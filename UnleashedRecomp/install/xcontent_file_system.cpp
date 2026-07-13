// Referenced from: https://github.com/xenia-canary/xenia-canary/blob/canary_experimental/src/xenia/vfs/devices/xcontent_container_device.cc

/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2023 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */


#include "xcontent_file_system.h"
#include "xbox.h"

#include <bit>
#include <cstring>
#include <limits>
#include <set>
#include <stack>
#include <unordered_set>

enum class XContentPackageType
{
    CON = 0x434F4E20,
    PIRS = 0x50495253,
    LIVE = 0x4C495645,
};

struct XContentLicense
{
    be<uint64_t> licenseId;
    be<uint32_t> licenseBits;
    be<uint32_t> licenseFlags;
};

#pragma pack(push, 1)
struct XContentHeader
{
    be<uint32_t> magic;
    uint8_t signature[0x228];
    XContentLicense licenses[0x10];
    uint8_t contentId[0x14];
    be<uint32_t> headerSize;
};
static_assert(sizeof(XContentHeader) == 0x344);

struct StfsVolumeDescriptor
{
    uint8_t descriptorLength;
    uint8_t version;

    union
    {
        uint8_t asByte;
        struct
        {
            uint8_t readOnlyFormat : 1;
            uint8_t rootActiveIndex : 1;
            uint8_t directoryOverallocated : 1;
            uint8_t directoryIndexBoundsValid : 1;
        } bits;
    } flags;

    uint16_t fileTableBlockCount;
    uint8_t fileTableBlockNumberRaw[3];
    uint8_t topHashTableHash[0x14];
    be<uint32_t> totalBlockCount;
    be<uint32_t> freeBlockCount;
};
static_assert(sizeof(StfsVolumeDescriptor) == 0x24);

struct StfsDirectoryEntry {
    char name[40];

    struct
    {
        uint8_t nameLength : 6;
        uint8_t contiguous : 1;
        uint8_t directory : 1;
    } flags;

    uint8_t validDataBlocksRaw[3];
    uint8_t allocatedDataBlocksRaw[3];
    uint8_t startBlockNumberRaw[3];
    be<uint16_t> directoryIndex;
    be<uint32_t> length;
    be<uint16_t> createDate;
    be<uint16_t> createTime;
    be<uint16_t> modifiedDate;
    be<uint16_t> modifiedTime;
};
static_assert(sizeof(StfsDirectoryEntry) == 0x40);

struct StfsDirectoryBlock {
    StfsDirectoryEntry entries[0x40];
};
static_assert(sizeof(StfsDirectoryBlock) == 0x1000);

struct StfsHashEntry {
    uint8_t sha1[0x14];
    be<uint32_t> infoRaw;
};
static_assert(sizeof(StfsHashEntry) == 0x18);

struct StfsHashTable {
    StfsHashEntry entries[170];
    be<uint32_t> numBlocks;
    uint8_t padding[12];
};
static_assert(sizeof(StfsHashTable) == 0x1000);

struct SvodDeviceDescriptor {
    uint8_t descriptorLength;
    uint8_t blockCacheElementCount;
    uint8_t workerThreadProcessor;
    uint8_t workerThreadPriority;
    uint8_t firstFragmentHashEntry[0x14];
    union {
        uint8_t asByte;
        struct {
            uint8_t mustBeZeroForFutureUsage : 6;
            uint8_t enhancedGdfLayout : 1;
            uint8_t zeroForDownlevelClients : 1;
        } bits;
    } features;
    uint8_t numDataBlocksRaw[3];
    uint8_t startDataBlockRaw[3];
    uint8_t reserved[5];
};
static_assert(sizeof(SvodDeviceDescriptor) == 0x24);

struct SvodDirectoryEntry {
    uint16_t nodeL;
    uint16_t nodeR;
    uint32_t dataBlock;
    uint32_t length;
    uint8_t attributes;
    uint8_t nameLength;
};
static_assert(sizeof(SvodDirectoryEntry) == 0xE);

struct XContentMetadata
{
    be<uint32_t> contentType;
    be<uint32_t> metadataVersion;
    be<uint64_t> contentSize;
    uint8_t executionInfo[24];
    uint8_t consoleId[5];
    be<uint64_t> profileId;

    union {
        StfsVolumeDescriptor stfsVolumeDescriptor;
        SvodDeviceDescriptor svodDeviceDescriptor;
    };

    be<uint32_t> dataFileCount;
    be<uint64_t> dataFileSize;
    be<uint32_t> volumeType;
    be<uint64_t> onlineCreator;
    be<uint32_t> category;
};
static_assert(sizeof(XContentMetadata) == 0x75);

#pragma pack(pop)

struct XContentContainerHeader
{
    XContentHeader contentHeader;
    XContentMetadata contentMetadata;
};

const uint32_t StfsBlockSize = 0x1000;
const uint32_t StfsBlocksHashLevelAmount = 3;
const uint32_t StfsBlocksPerHashLevel[StfsBlocksHashLevelAmount] = { 170, 28900, 4913000 };
const uint32_t StfsEndOfChain = 0xFFFFFF;
const uint32_t StfsEntriesPerDirectoryBlock = StfsBlockSize / sizeof(StfsDirectoryEntry);

static bool rangeWithin(size_t totalSize, size_t offset, size_t byteCount)
{
    return offset <= totalSize && byteCount <= (totalSize - offset);
}

uint32_t parseUint24(const uint8_t *bytes) {
    return bytes[0] | (bytes[1] << 8U) | (bytes[2] << 16U);
}

size_t blockIndexToOffset(uint64_t baseOffset, uint64_t blockIndex)
{
    uint64_t block = blockIndex;
    for (uint32_t i = 0; i < StfsBlocksHashLevelAmount; i++)
    {
        uint32_t levelBase = StfsBlocksPerHashLevel[i];
        block += ((blockIndex + levelBase) / levelBase);
        if (blockIndex < levelBase)
        {
            break;
        }
    }

    return baseOffset + (block << 12);
}

uint32_t blockIndexToHashBlockNumber(uint32_t blockIndex) {
    if (blockIndex < StfsBlocksPerHashLevel[0])
    {
        return 0;
    }

    uint32_t block = (blockIndex / StfsBlocksPerHashLevel[0]) * (StfsBlocksPerHashLevel[0] + 1);
    block += ((blockIndex / StfsBlocksPerHashLevel[1]) + 1);
    if (blockIndex < StfsBlocksPerHashLevel[1])
    {
        return block;
    }

    return block + 1;
}

size_t blockIndexToHashBlockOffset(uint64_t baseOffset, uint32_t blockIndex)
{
    size_t blockNumber = blockIndexToHashBlockNumber(blockIndex);
    return baseOffset + (blockNumber << 12);
}

const StfsHashEntry* hashEntryFromBlockIndex(const uint8_t* fileData, size_t fileSize, uint64_t baseOffset, uint64_t blockIndex)
{
    size_t hashOffset = blockIndexToHashBlockOffset(baseOffset, blockIndex);
    if (!rangeWithin(fileSize, hashOffset, sizeof(StfsHashTable)))
        return nullptr;

    const StfsHashTable *hashTable = (const StfsHashTable *)(&fileData[hashOffset]);
    return &hashTable->entries[blockIndex % StfsBlocksPerHashLevel[0]];
}

bool blockToOffsetAndFile(SvodLayoutType svodLayoutType, size_t svodStartDataBlock, size_t svodBaseOffset, size_t block, size_t &outOffset, size_t &outFileIndex)
{
    const size_t BlockSize = 0x800;
    const size_t HashBlockSize = 0x1000;
    const size_t BlocksPerL0Hash = 0x198;
    const size_t HashesPerL1Hash = 0xA1C4;
    const size_t BlocksPerFile = 0x14388;
    const size_t MaxFileSize = 0xA290000;
    if (svodStartDataBlock > (std::numeric_limits<size_t>::max() / 2))
        return false;

    const size_t firstDataBlock = svodStartDataBlock * 2;
    if (block < firstDataBlock)
        return false;

    size_t trueBlock = block - firstDataBlock;
    if (svodLayoutType == SvodLayoutType::EnhancedGDF)
    {
        trueBlock += 0x2;
    }

    size_t fileBlock = trueBlock % BlocksPerFile;
    outFileIndex = trueBlock / BlocksPerFile;

    size_t offset = 0;
    size_t level0TableCount = (fileBlock / BlocksPerL0Hash) + 1;
    offset += level0TableCount * HashBlockSize;

    size_t level1TableCount = (level0TableCount / HashesPerL1Hash) + 1;
    offset += level1TableCount * HashBlockSize;

    if (svodLayoutType == SvodLayoutType::SingleFile)
    {
        offset += svodBaseOffset;
    }

    outOffset = (fileBlock * BlockSize) + offset;
    if (outOffset >= MaxFileSize)
    {
        outOffset = (outOffset % MaxFileSize) + 0x2000;
        outFileIndex++;
    }

    return true;
}

XContentFileSystem::XContentFileSystem(const std::filesystem::path &contentPath)
{
    mappedFiles.emplace_back();

    MemoryMappedFile &rootMappedFile = mappedFiles.back();
    rootMappedFile.open(contentPath);
    if (!rootMappedFile.isOpen())
    {
        return;
    }

    name = (const char *)(contentPath.filename().u8string().data());

    const uint8_t *rootMappedFileData = rootMappedFile.data();
    if (sizeof(XContentContainerHeader) > rootMappedFile.size())
    {
        mappedFiles.clear();
        return;
    }

    XContentContainerHeader contentContainerHeader;
    std::memcpy(&contentContainerHeader, rootMappedFileData, sizeof(contentContainerHeader));
    XContentPackageType packageType = XContentPackageType(contentContainerHeader.contentHeader.magic.get());
    if (packageType != XContentPackageType::CON && packageType != XContentPackageType::LIVE && packageType != XContentPackageType::PIRS)
    {
        mappedFiles.clear();
        return;
    }

    const XContentMetadata &metadata = contentContainerHeader.contentMetadata;
    volumeType = XContentVolumeType(metadata.volumeType.get());
    if (volumeType == XContentVolumeType::STFS)
    {
        const StfsVolumeDescriptor &descriptor = metadata.stfsVolumeDescriptor;
        if (descriptor.descriptorLength != sizeof(StfsVolumeDescriptor) || !descriptor.flags.bits.readOnlyFormat)
        {
            mappedFiles.clear();
            return;
        }

        const uint64_t headerSize = contentContainerHeader.contentHeader.headerSize.get();
        baseOffset = ((headerSize + StfsBlockSize - 1) / StfsBlockSize) * StfsBlockSize;
        if (baseOffset > rootMappedFile.size())
        {
            mappedFiles.clear();
            return;
        }

        uint32_t entryCount = 0;
        uint32_t tableBlockIndex = parseUint24(descriptor.fileTableBlockNumberRaw);
        uint32_t tableBlockCount = descriptor.fileTableBlockCount;
        std::map<uint32_t, std::string> directoryNames;
        for (uint32_t i = 0; i < tableBlockCount; i++)
        {
            size_t offset = blockIndexToOffset(baseOffset, tableBlockIndex);
            if (!rangeWithin(rootMappedFile.size(), offset, sizeof(StfsDirectoryBlock)))
            {
                mappedFiles.clear();
                return;
            }

            StfsDirectoryBlock *directoryBlock = (StfsDirectoryBlock *)(&rootMappedFileData[offset]);
            for (uint32_t j = 0; j < StfsEntriesPerDirectoryBlock; j++)
            {
                const StfsDirectoryEntry &directoryEntry = directoryBlock->entries[j];
                if (directoryEntry.name[0] == '\0')
                {
                    break;
                }

                const size_t nameLength = directoryEntry.flags.nameLength & 0x3F;
                if (nameLength == 0 || nameLength > sizeof(directoryEntry.name))
                {
                    mappedFiles.clear();
                    return;
                }

                std::string fileNameBase = directoryNames[directoryEntry.directoryIndex];
                std::string fileName(directoryEntry.name, nameLength);
                if (fileNameBase.size() + fileName.size() > 4096)
                {
                    mappedFiles.clear();
                    return;
                }
                if (directoryEntry.flags.directory)
                {
                    directoryNames[entryCount++] = fileNameBase + fileName + "/";
                    continue;
                }

                uint32_t fileBlockIndex = parseUint24(directoryEntry.startBlockNumberRaw);
                uint32_t fileBlockCount = parseUint24(directoryEntry.allocatedDataBlocksRaw);
                fileMap[fileNameBase + fileName] = { directoryEntry.length, fileBlockIndex, fileBlockCount };
                entryCount++;
            }

            const StfsHashEntry *hashEntry = hashEntryFromBlockIndex(rootMappedFileData, rootMappedFile.size(), baseOffset, tableBlockIndex);
            if (hashEntry == nullptr)
            {
                mappedFiles.clear();
                return;
            }
            tableBlockIndex = hashEntry->infoRaw & 0xFFFFFF;
            if (tableBlockIndex == StfsEndOfChain)
            {
                break;
            }
        }
    }
    else if (volumeType == XContentVolumeType::SVOD)
    {
        mappedFiles.clear();

        // Close the root file and open all the files inside the directory with the same name instead.
        std::filesystem::path dataDirectory(contentPath.u8string() + u8".data");
        if (!std::filesystem::is_directory(dataDirectory))
        {
            return;
        }

        // Find all data files inside the directory.
        std::set<std::filesystem::path> orderedPaths;
        for (auto &entry : std::filesystem::directory_iterator(dataDirectory))
        {
            if (!entry.is_regular_file())
            {
                continue;
            }

            orderedPaths.emplace(entry.path());
        }

        // Memory map all the files that were found.
        for (auto &path : orderedPaths)
        {
            mappedFiles.emplace_back();
            if (!mappedFiles.back().open(path))
            {
                mappedFiles.clear();
                return;
            }
        }

        if (mappedFiles.empty())
        {
            return;
        }

        // Determine the layout of the SVOD from the first file.
        MemoryMappedFile &firstMappedFile = mappedFiles.front();
        const uint8_t *firstMappedFileData = firstMappedFile.data();
        const char *RefMagic = "MICROSOFT*XBOX*MEDIA";
        size_t RefXSFMagicOffset = 0x12000;
        size_t SingleFileMagicOffset = 0xD000;
        const size_t refMagicLength = std::strlen(RefMagic);
        if (metadata.svodDeviceDescriptor.features.bits.enhancedGdfLayout)
        {
            size_t EGDFMagicOffset = 0x2000;
            if (!rangeWithin(firstMappedFile.size(), EGDFMagicOffset, refMagicLength)
                || std::memcmp(&firstMappedFileData[EGDFMagicOffset], RefMagic, refMagicLength) != 0)
            {
                mappedFiles.clear();
                return;
            }

            svodBaseOffset = 0;
            svodMagicOffset = EGDFMagicOffset;
            svodLayoutType = SvodLayoutType::EnhancedGDF;
        }
        else if (rangeWithin(firstMappedFile.size(), RefXSFMagicOffset, refMagicLength)
            && std::memcmp(&firstMappedFileData[RefXSFMagicOffset], RefMagic, refMagicLength) == 0)
        {
            const char *XSFMagic = "XSF";
            size_t XSFMagicOffset = 0x2000;
            svodBaseOffset = 0x10000;
            svodMagicOffset = 0x12000;

            if (rangeWithin(firstMappedFile.size(), XSFMagicOffset, std::strlen(XSFMagic))
                && std::memcmp(&firstMappedFileData[XSFMagicOffset], XSFMagic, std::strlen(XSFMagic)) == 0)
            {
                svodLayoutType = SvodLayoutType::XSF;
            }
            else
            {
                svodLayoutType = SvodLayoutType::Unknown;
            }
        }
        else if (rangeWithin(firstMappedFile.size(), SingleFileMagicOffset, refMagicLength)
            && std::memcmp(&firstMappedFileData[SingleFileMagicOffset], RefMagic, refMagicLength) == 0)
        {
            svodBaseOffset = 0xB000;
            svodMagicOffset = 0xD000;
            svodLayoutType = SvodLayoutType::SingleFile;
        }
        else {
            mappedFiles.clear();
            return;
        }

        svodStartDataBlock = parseUint24(metadata.svodDeviceDescriptor.startDataBlockRaw);

        struct IterationStep
        {
            std::string fileNameBase;
            uint32_t blockIndex = 0;
            uint32_t ordinalIndex = 0;

            IterationStep() = default;
            IterationStep(std::string fileNameBase, uint32_t blockIndex, uint32_t ordinalIndex) : fileNameBase(fileNameBase), blockIndex(blockIndex), ordinalIndex(ordinalIndex) { }
        };

        std::stack<IterationStep> iterationStack;
        if (!rangeWithin(firstMappedFile.size(), svodMagicOffset + 0x14, sizeof(uint32_t)))
        {
            mappedFiles.clear();
            return;
        }

        uint32_t rootBlock = 0;
        std::memcpy(&rootBlock, &firstMappedFileData[svodMagicOffset + 0x14], sizeof(rootBlock));
        iterationStack.emplace("", rootBlock, 0);
        std::unordered_set<uint64_t> visitedEntries;
        constexpr size_t MaxEntryCount = 1'000'000;

        IterationStep step;
        size_t fileOffset, fileIndex;
        char fileName[256];
        const uint8_t FileAttributeDirectory = 0x10;
        while (!iterationStack.empty())
        {
            step = iterationStack.top();
            iterationStack.pop();

            size_t ordinalOffset = size_t(step.ordinalIndex) * 0x4;
            size_t blockOffset = ordinalOffset / 0x800;
            size_t trueOrdinalOffset = ordinalOffset % 0x800;
            if (step.blockIndex > (std::numeric_limits<size_t>::max() - blockOffset)
                || !blockToOffsetAndFile(svodLayoutType, svodStartDataBlock, svodBaseOffset, step.blockIndex + blockOffset, fileOffset, fileIndex)
                || fileOffset > (std::numeric_limits<size_t>::max() - trueOrdinalOffset))
            {
                mappedFiles.clear();
                return;
            }
            fileOffset += trueOrdinalOffset;
            if (fileIndex >= mappedFiles.size())
            {
                mappedFiles.clear();
                return;
            }

            const MemoryMappedFile &mappedFile = mappedFiles[fileIndex];
            if (!rangeWithin(mappedFile.size(), fileOffset, sizeof(SvodDirectoryEntry)))
            {
                mappedFiles.clear();
                return;
            }

            const uint8_t *mappedFileData = mappedFile.data();
            const SvodDirectoryEntry *directoryEntry = (const SvodDirectoryEntry *)(&mappedFileData[fileOffset]);
            size_t nameOffset = fileOffset + sizeof(SvodDirectoryEntry);
            if (directoryEntry->nameLength == 0
                || !rangeWithin(mappedFile.size(), nameOffset, directoryEntry->nameLength)
                || visitedEntries.size() >= MaxEntryCount)
            {
                mappedFiles.clear();
                return;
            }

            const uint64_t entryKey = (uint64_t(fileIndex) << 32) ^ uint64_t(fileOffset);
            if (!visitedEntries.insert(entryKey).second)
            {
                mappedFiles.clear();
                return;
            }

            memcpy(fileName, &mappedFileData[nameOffset], directoryEntry->nameLength);
            fileName[directoryEntry->nameLength] = '\0';

            if (directoryEntry->nodeL)
            {
                iterationStack.emplace(step.fileNameBase, step.blockIndex, directoryEntry->nodeL);
            }

            if (directoryEntry->nodeR)
            {
                iterationStack.emplace(step.fileNameBase, step.blockIndex, directoryEntry->nodeR);
            }

            std::string fileNameUTF8 = step.fileNameBase + fileName;
            if (fileNameUTF8.size() > 4096)
            {
                mappedFiles.clear();
                return;
            }
            if (directoryEntry->attributes & FileAttributeDirectory)
            {
                if (directoryEntry->length > 0)
                {
                    iterationStack.emplace(fileNameUTF8 + "/", directoryEntry->dataBlock, 0);
                }
            }
            else
            {
                fileMap[fileNameUTF8] = { directoryEntry->length, directoryEntry->dataBlock, 0 };
            }
        }
    }
    else
    {
        mappedFiles.clear();
    }
}

bool XContentFileSystem::stream(const std::string& path, const StreamCallback& callback) const
{
    auto it = fileMap.find(path);
    if (it == fileMap.end() || it->second.size == 0 || mappedFiles.empty())
    {
        return false;
    }

    constexpr size_t BufferSize = 1024 * 1024;
    std::vector<uint8_t> buffer(std::min(BufferSize, it->second.size));
    size_t bufferSize = 0;
    auto appendBytes = [&](const uint8_t* bytes, size_t byteCount) -> bool
    {
        while (byteCount != 0)
        {
            const size_t copySize = std::min(byteCount, buffer.size() - bufferSize);
            std::memcpy(buffer.data() + bufferSize, bytes, copySize);
            bufferSize += copySize;
            bytes += copySize;
            byteCount -= copySize;

            if (bufferSize == buffer.size())
            {
                if (!callback(std::span<const uint8_t>(buffer.data(), bufferSize)))
                    return false;
                bufferSize = 0;
            }
        }

        return true;
    };

    size_t remainingSize = it->second.size;
    if (volumeType == XContentVolumeType::STFS)
    {
        const MemoryMappedFile& rootMappedFile = mappedFiles.back();
        const uint8_t* rootMappedFileData = rootMappedFile.data();
        uint32_t fileBlockIndex = it->second.blockIndex;
        for (uint32_t i = 0; i < it->second.blockCount && fileBlockIndex != StfsEndOfChain && remainingSize != 0; i++)
        {
            const size_t blockSize = std::min(size_t(StfsBlockSize), remainingSize);
            const size_t blockOffset = blockIndexToOffset(baseOffset, fileBlockIndex);
            if (!rangeWithin(rootMappedFile.size(), blockOffset, blockSize)
                || !appendBytes(&rootMappedFileData[blockOffset], blockSize))
            {
                return false;
            }

            const StfsHashEntry* hashEntry = hashEntryFromBlockIndex(
                rootMappedFileData, rootMappedFile.size(), baseOffset, fileBlockIndex);
            if (hashEntry == nullptr)
                return false;

            fileBlockIndex = hashEntry->infoRaw & 0xFFFFFF;
            remainingSize -= blockSize;
        }

        if (remainingSize != 0)
            return false;
    }
    else if (volumeType == XContentVolumeType::SVOD)
    {
        size_t currentBlock = it->second.blockIndex;
        while (remainingSize > 0)
        {
            size_t blockFileOffset, blockFileIndex;
            if (!blockToOffsetAndFile(svodLayoutType, svodStartDataBlock, svodBaseOffset, currentBlock, blockFileOffset, blockFileIndex))
                return false;
            if (blockFileIndex >= mappedFiles.size())
            {
                return false;
            }

            const MemoryMappedFile& mappedFile = mappedFiles[blockFileIndex];
            const uint8_t* mappedFileData = mappedFile.data();
            const size_t blockSize = std::min(size_t(0x800), remainingSize);
            if (!rangeWithin(mappedFile.size(), blockFileOffset, blockSize)
                || !appendBytes(&mappedFileData[blockFileOffset], blockSize))
            {
                return false;
            }

            remainingSize -= blockSize;
            currentBlock++;
        }
    }
    else
    {
        return false;
    }

    return bufferSize == 0 || callback(std::span<const uint8_t>(buffer.data(), bufferSize));
}

size_t XContentFileSystem::getSize(const std::string &path) const
{
    auto it = fileMap.find(path);
    if (it != fileMap.end())
    {
        return it->second.size;
    }
    else
    {
        return 0;
    }
}

bool XContentFileSystem::exists(const std::string &path) const
{
    return fileMap.find(path) != fileMap.end();
}

const std::string &XContentFileSystem::getName() const
{
    return name;
}

bool XContentFileSystem::empty() const
{
    return mappedFiles.empty() || fileMap.empty();
}

std::unique_ptr<XContentFileSystem> XContentFileSystem::create(const std::filesystem::path &contentPath)
{
    std::unique_ptr<XContentFileSystem> xContentFS = std::make_unique<XContentFileSystem>(contentPath);
    if (!xContentFS->empty())
    {
        return xContentFS;
    }
    else
    {
        return nullptr;
    }
}

bool XContentFileSystem::check(const std::filesystem::path &contentPath)
{
    std::ifstream contentStream(contentPath, std::ios::binary);
    if (!contentStream.is_open())
    {
        return false;
    }

    uint32_t packageTypeUint = 0;
    contentStream.read((char *)(&packageTypeUint), sizeof(uint32_t));
    if (contentStream.gcount() != sizeof(uint32_t))
        return false;

    packageTypeUint = ByteSwap(packageTypeUint);
    XContentPackageType packageType = XContentPackageType(packageTypeUint);
    return packageType == XContentPackageType::CON || packageType == XContentPackageType::LIVE || packageType == XContentPackageType::PIRS;
}
