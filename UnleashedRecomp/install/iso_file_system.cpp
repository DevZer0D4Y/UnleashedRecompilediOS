// Referenced from: https://github.com/xenia-canary/xenia-canary/blob/canary_experimental/src/xenia/vfs/devices/disc_image_device.cc

/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2023 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "iso_file_system.h"

#include <cstring>
#include <fstream>
#include <limits>
#include <stack>
#include <unordered_set>

namespace
{
    static bool rangeWithin(size_t totalSize, size_t offset, size_t byteCount)
    {
        return offset <= totalSize && byteCount <= (totalSize - offset);
    }

    static bool readFileRange(const std::filesystem::path& filePath, size_t offset, void* outBytes, size_t byteCount)
    {
        if ((outBytes == nullptr && byteCount != 0)
            || offset > static_cast<size_t>(std::numeric_limits<std::streamoff>::max())
            || byteCount > static_cast<size_t>(std::numeric_limits<std::streamsize>::max()))
        {
            return false;
        }

        std::ifstream input(filePath, std::ios::binary);
        if (!input.is_open())
        {
            return false;
        }

        input.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
        if (!input.good())
        {
            return false;
        }

        input.read(static_cast<char*>(outBytes), static_cast<std::streamsize>(byteCount));
        return static_cast<size_t>(input.gcount()) == byteCount;
    }
}

ISOFileSystem::ISOFileSystem(const std::filesystem::path &isoPath)
{
    sourcePath = isoPath;
    std::error_code ec;
    const uintmax_t fileSize = std::filesystem::file_size(sourcePath, ec);
    if (ec || fileSize == 0 || fileSize > std::numeric_limits<size_t>::max())
    {
        return;
    }
    sourceSize = static_cast<size_t>(fileSize);

    mappedFile.open(isoPath);
    const bool usingMappedFile = mappedFile.isOpen();
    if (usingMappedFile)
    {
        sourceSize = mappedFile.size();
    }

    if (sourceSize == 0)
    {
        return;
    }

    name = (const char *)(isoPath.filename().u8string().data());

    const uint8_t* mappedFileData = usingMappedFile ? mappedFile.data() : nullptr;
    auto readBytes = [&](size_t offset, void* outBytes, size_t byteCount) -> bool
    {
        if (!rangeWithin(sourceSize, offset, byteCount))
        {
            return false;
        }

        if (usingMappedFile)
        {
            std::memcpy(outBytes, &mappedFileData[offset], byteCount);
            return true;
        }

        return readFileRange(sourcePath, offset, outBytes, byteCount);
    };

    // Find root sector.
    uint32_t gameOffset = 0;
    const size_t XeSectorSize = 2048;
    auto getSectorOffset = [&](size_t baseOffset, uint32_t sector, size_t& outOffset) -> bool
    {
        if (baseOffset > sourceSize
            || sector > ((std::numeric_limits<size_t>::max() - baseOffset) / XeSectorSize))
        {
            return false;
        }

        outOffset = baseOffset + (static_cast<size_t>(sector) * XeSectorSize);
        return outOffset <= sourceSize;
    };

    static const size_t PossibleOffsets[] = { 0x00000000, 0x0000FB20, 0x00020600, 0x02080000, 0x0FD90000, };
    bool magicFound = false;
    const char RefMagic[] = "MICROSOFT*XBOX*MEDIA";
    char magicBuffer[sizeof(RefMagic)]{};
    for (size_t i = 0; i < std::size(PossibleOffsets); i++)
    {
        size_t fileOffset = PossibleOffsets[i] + (32 * XeSectorSize);
        constexpr size_t magicSize = sizeof(RefMagic) - 1;
        if (!rangeWithin(sourceSize, fileOffset, magicSize))
        {
            continue;
        }

        if (!readBytes(fileOffset, magicBuffer, magicSize))
        {
            continue;
        }

        if (std::memcmp(magicBuffer, RefMagic, magicSize) == 0)
        {
            gameOffset = PossibleOffsets[i];
            magicFound = true;
        }
    }

    size_t rootInfoOffset = gameOffset + (32 * XeSectorSize) + 20;
    if (!magicFound || !rangeWithin(sourceSize, rootInfoOffset, 8))
    {
        return;
    }

    // Parse root information.
    uint32_t rootSector = 0;
    uint32_t rootSize = 0;
    if (!readBytes(rootInfoOffset + 0, &rootSector, sizeof(rootSector))
        || !readBytes(rootInfoOffset + 4, &rootSize, sizeof(rootSize)))
    {
        return;
    }

    size_t rootOffset = 0;
    const uint32_t MinRootSize = 13;
    const uint32_t MaxRootSize = 32 * 1024 * 1024;
    if ((rootSize < MinRootSize) || (rootSize > MaxRootSize)
        || !getSectorOffset(gameOffset, rootSector, rootOffset)
        || !rangeWithin(sourceSize, rootOffset, rootSize))
    {
        return;
    }

    struct IterationStep
    {
        std::string fileNameBase;
        size_t nodeOffset = 0;
        size_t nodeSize = 0;
        size_t entryOffset = 0;

        IterationStep() = default;
        IterationStep(std::string fileNameBase, size_t nodeOffset, size_t nodeSize, size_t entryOffset)
            : fileNameBase(std::move(fileNameBase)), nodeOffset(nodeOffset), nodeSize(nodeSize), entryOffset(entryOffset) { }
    };

    std::stack<IterationStep> iterationStack;
    iterationStack.emplace("", rootOffset, rootSize, 0);
    std::unordered_set<size_t> visitedEntries;
    constexpr size_t MaxEntryCount = 1'000'000;
    constexpr size_t MaxPathLength = 4096;

    IterationStep step;
    uint16_t nodeL, nodeR;
    uint32_t sector, length;
    uint8_t attributes, nameLength;
    uint8_t entryHeader[14];
    char fileName[256];
    const uint8_t FileAttributeDirectory = 0x10;
    while (!iterationStack.empty())
    {
        step = iterationStack.top();
        iterationStack.pop();

        if (!rangeWithin(step.nodeSize, step.entryOffset, sizeof(entryHeader))
            || step.entryOffset > (std::numeric_limits<size_t>::max() - step.nodeOffset))
        {
            return;
        }

        size_t infoOffset = step.nodeOffset + step.entryOffset;
        if (!rangeWithin(sourceSize, infoOffset, sizeof(entryHeader))
            || visitedEntries.size() >= MaxEntryCount
            || !visitedEntries.insert(infoOffset).second)
        {
            return;
        }

        if (!readBytes(infoOffset, entryHeader, sizeof(entryHeader)))
        {
            return;
        }

        std::memcpy(&nodeL, &entryHeader[0], sizeof(nodeL));
        std::memcpy(&nodeR, &entryHeader[2], sizeof(nodeR));
        std::memcpy(&sector, &entryHeader[4], sizeof(sector));
        std::memcpy(&length, &entryHeader[8], sizeof(length));
        attributes = entryHeader[12];
        nameLength = entryHeader[13];

        size_t nameOffset = infoOffset + sizeof(entryHeader);
        if (nameLength == 0
            || !rangeWithin(step.nodeSize, step.entryOffset + sizeof(entryHeader), nameLength)
            || !rangeWithin(sourceSize, nameOffset, nameLength))
        {
            return;
        }

        if (!readBytes(nameOffset, fileName, nameLength))
        {
            return;
        }

        fileName[nameLength] = '\0';

        if (nodeL)
        {
            iterationStack.emplace(step.fileNameBase, step.nodeOffset, step.nodeSize, nodeL * 4);
        }

        if (nodeR)
        {
            iterationStack.emplace(step.fileNameBase, step.nodeOffset, step.nodeSize, nodeR * 4);
        }

        std::string fileNameUTF8 = step.fileNameBase + fileName;
        if (fileNameUTF8.size() > MaxPathLength)
        {
            return;
        }

        if (attributes & FileAttributeDirectory)
        {
            if (length > 0)
            {
                size_t directoryOffset = 0;
                if (!getSectorOffset(gameOffset, sector, directoryOffset)
                    || !rangeWithin(sourceSize, directoryOffset, length))
                {
                    return;
                }

                iterationStack.emplace(fileNameUTF8 + "/", directoryOffset, length, 0);
            }
        }
        else
        {
            size_t fileOffset = 0;
            if (!getSectorOffset(gameOffset, sector, fileOffset)
                || !rangeWithin(sourceSize, fileOffset, length))
            {
                continue;
            }

            fileMap[fileNameUTF8] = { fileOffset, length };
        }
    }
}

bool ISOFileSystem::stream(const std::string& path, const StreamCallback& callback) const
{
    auto it = fileMap.find(path);
    if (it == fileMap.end())
    {
        return false;
    }

    const size_t fileOffset = std::get<0>(it->second);
    const size_t fileSize = std::get<1>(it->second);
    if (fileSize == 0 || !rangeWithin(sourceSize, fileOffset, fileSize))
    {
        return false;
    }

    constexpr size_t BufferSize = 1024 * 1024;
    if (mappedFile.isOpen())
    {
        const uint8_t* mappedFileData = mappedFile.data();
        size_t offset = 0;
        while (offset != fileSize)
        {
            const size_t chunkSize = std::min(BufferSize, fileSize - offset);
            if (!callback(std::span<const uint8_t>(&mappedFileData[fileOffset + offset], chunkSize)))
                return false;
            offset += chunkSize;
        }

        return true;
    }

    if (fileOffset > static_cast<size_t>(std::numeric_limits<std::streamoff>::max()))
        return false;

    std::ifstream input(sourcePath, std::ios::binary);
    if (!input.is_open())
        return false;

    input.seekg(static_cast<std::streamoff>(fileOffset), std::ios::beg);
    if (!input.good())
        return false;

    std::vector<uint8_t> buffer(std::min(BufferSize, fileSize));
    size_t remaining = fileSize;
    while (remaining != 0)
    {
        const size_t chunkSize = std::min(remaining, buffer.size());
        input.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(chunkSize));
        if (static_cast<size_t>(input.gcount()) != chunkSize
            || !callback(std::span<const uint8_t>(buffer.data(), chunkSize)))
        {
            return false;
        }

        remaining -= chunkSize;
    }

    return true;
}

size_t ISOFileSystem::getSize(const std::string &path) const
{
    auto it = fileMap.find(path);
    if (it != fileMap.end())
    {
        return std::get<1>(it->second);
    }
    else
    {
        return 0;
    }
}

bool ISOFileSystem::exists(const std::string &path) const
{
    return fileMap.find(path) != fileMap.end();
}

const std::string &ISOFileSystem::getName() const
{
    return name;
}

bool ISOFileSystem::empty() const 
{
    return fileMap.empty();
}

std::unique_ptr<ISOFileSystem> ISOFileSystem::create(const std::filesystem::path &isoPath) {
    std::unique_ptr<ISOFileSystem> isoFs = std::make_unique<ISOFileSystem>(isoPath);
    if (!isoFs->empty())
    {
        return isoFs;
    }
    else
    {
        return nullptr;
    }
}
