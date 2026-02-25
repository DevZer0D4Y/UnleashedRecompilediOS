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
#include <stack>

namespace
{
    static bool readFileRange(const std::filesystem::path& filePath, size_t offset, void* outBytes, size_t byteCount)
    {
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
        return input.good() || input.eof();
    }
}

ISOFileSystem::ISOFileSystem(const std::filesystem::path &isoPath)
{
    sourcePath = isoPath;
    std::error_code ec;
    sourceSize = static_cast<size_t>(std::filesystem::file_size(sourcePath, ec));
    if (ec || sourceSize == 0)
    {
        return;
    }

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
        if ((offset + byteCount) > sourceSize)
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
    static const size_t PossibleOffsets[] = { 0x00000000, 0x0000FB20, 0x00020600, 0x02080000, 0x0FD90000, };
    bool magicFound = false;
    const char RefMagic[] = "MICROSOFT*XBOX*MEDIA";
    char magicBuffer[sizeof(RefMagic)]{};
    for (size_t i = 0; i < std::size(PossibleOffsets); i++)
    {
        size_t fileOffset = PossibleOffsets[i] + (32 * XeSectorSize);
        constexpr size_t magicSize = sizeof(RefMagic) - 1;
        if ((fileOffset + magicSize) > sourceSize)
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
    if (!magicFound || (rootInfoOffset + 8) > sourceSize)
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

    size_t rootOffset = gameOffset + (rootSector * XeSectorSize);
    const uint32_t MinRootSize = 13;
    const uint32_t MaxRootSize = 32 * 1024 * 1024;
    if ((rootSize < MinRootSize) || (rootSize > MaxRootSize))
    {
        return;
    }

    struct IterationStep
    {
        std::string fileNameBase;
        size_t nodeOffset = 0;
        size_t entryOffset = 0;

        IterationStep() = default;
        IterationStep(std::string fileNameBase, size_t nodeOffset, size_t entryOffset) : fileNameBase(fileNameBase), nodeOffset(nodeOffset), entryOffset(entryOffset) { }
    };

    std::stack<IterationStep> iterationStack;
    iterationStack.emplace("", rootOffset, 0);

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

        size_t infoOffset = step.nodeOffset + step.entryOffset;
        if ((infoOffset + sizeof(entryHeader)) > sourceSize)
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

        size_t nameOffset = infoOffset + 14;
        if (nameLength == 0 || (nameOffset + nameLength) > sourceSize)
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
            iterationStack.emplace(step.fileNameBase, step.nodeOffset, nodeL * 4);
        }

        if (nodeR)
        {
            iterationStack.emplace(step.fileNameBase, step.nodeOffset, nodeR * 4);
        }

        std::string fileNameUTF8 = step.fileNameBase + fileName;
        if (attributes & FileAttributeDirectory)
        {
            if (length > 0)
            {
                iterationStack.emplace(fileNameUTF8 + "/", gameOffset + sector * XeSectorSize, 0);
            }
        }
        else
        {
            if ((gameOffset + sector * XeSectorSize + length) > sourceSize)
            {
                continue;
            }

            fileMap[fileNameUTF8] = { gameOffset + sector * XeSectorSize, length};
        }
    }
}

bool ISOFileSystem::load(const std::string &path, uint8_t *fileData, size_t fileDataMaxByteCount) const
{
    auto it = fileMap.find(path);
    if (it != fileMap.end())
    {
        if (fileDataMaxByteCount < std::get<1>(it->second))
        {
            return false;
        }

        const size_t fileOffset = std::get<0>(it->second);
        const size_t fileSize = std::get<1>(it->second);

        if (mappedFile.isOpen())
        {
            const uint8_t *mappedFileData = mappedFile.data();
            memcpy(fileData, &mappedFileData[fileOffset], fileSize);
            return true;
        }

        if (!readFileRange(sourcePath, fileOffset, fileData, fileSize))
        {
            return false;
        }

        return true;
    }
    else
    {
        return false;
    }
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
