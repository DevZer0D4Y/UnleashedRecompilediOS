#pragma once

#include <filesystem>
#include <fstream>
#include <limits>
#include <vector>

#include "virtual_file_system.h"

struct DirectoryFileSystem : VirtualFileSystem
{
    std::filesystem::path directoryPath;
    std::string name;

    DirectoryFileSystem(const std::filesystem::path &directoryPath)
    {
        this->directoryPath = directoryPath;
        name = (const char *)(directoryPath.filename().u8string().data());
    }

    bool stream(const std::string& path, const StreamCallback& callback) const override
    {
        const std::filesystem::path filePath = directoryPath / std::filesystem::path(std::u8string_view((const char8_t *)(path.c_str())));
        std::error_code ec;
        const uintmax_t fileSize = std::filesystem::file_size(filePath, ec);
        if (ec || fileSize == 0 || fileSize > std::numeric_limits<size_t>::max())
        {
            return false;
        }

        std::ifstream fileStream(filePath, std::ios::binary);
        if (!fileStream.is_open())
        {
            return false;
        }

        constexpr size_t BufferSize = 1024 * 1024;
        std::vector<uint8_t> buffer(std::min<size_t>(static_cast<size_t>(fileSize), BufferSize));
        size_t remaining = static_cast<size_t>(fileSize);
        while (remaining != 0)
        {
            const size_t chunkSize = std::min(remaining, buffer.size());
            fileStream.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(chunkSize));
            if (static_cast<size_t>(fileStream.gcount()) != chunkSize
                || !callback(std::span<const uint8_t>(buffer.data(), chunkSize)))
            {
                return false;
            }

            remaining -= chunkSize;
        }

        return true;
    }

    size_t getSize(const std::string &path) const override
    {
        std::error_code ec;
        size_t fileSize = std::filesystem::file_size(directoryPath / std::filesystem::path(std::u8string_view((const char8_t *)(path.c_str()))), ec);
        if (!ec)
        {
            return fileSize;
        }
        else
        {
            return 0;
        }
    }

    bool exists(const std::string &path) const override
    {
        if (path.empty())
        {
            return false;
        }

        std::error_code ec;
        bool result = std::filesystem::exists(directoryPath / std::filesystem::path(std::u8string_view((const char8_t *)(path.c_str()))), ec);
        return !ec && result;
    }

    const std::string &getName() const override
    {
        return name;
    }

    static std::unique_ptr<VirtualFileSystem> create(const std::filesystem::path &directoryPath)
    {
        std::error_code ec;
        bool exists = std::filesystem::exists(directoryPath, ec);
        bool isDirectory = std::filesystem::is_directory(directoryPath, ec);
        if (!ec && exists && isDirectory)
        {
            return std::make_unique<DirectoryFileSystem>(directoryPath);
        }
        else
        {
            return nullptr;
        }
    }
};
