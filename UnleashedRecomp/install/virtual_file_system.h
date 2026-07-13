#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <span>
#include <vector>

struct VirtualFileSystem {
    using StreamCallback = std::function<bool(std::span<const uint8_t>)>;

    virtual ~VirtualFileSystem() { };
    virtual bool stream(const std::string &path, const StreamCallback& callback) const = 0;
    virtual size_t getSize(const std::string &path) const = 0;
    virtual bool exists(const std::string &path) const = 0;
    virtual const std::string &getName() const = 0;

    bool load(const std::string& path, uint8_t* fileData, size_t fileDataMaxByteCount) const
    {
        const size_t fileSize = getSize(path);
        if (fileSize == 0 || fileSize > fileDataMaxByteCount || fileData == nullptr)
        {
            return false;
        }

        size_t offset = 0;
        const bool streamed = stream(path, [&](std::span<const uint8_t> bytes)
        {
            if (bytes.size() > (fileSize - offset))
                return false;

            std::memcpy(fileData + offset, bytes.data(), bytes.size());
            offset += bytes.size();
            return true;
        });

        return streamed && offset == fileSize;
    }

    // Concrete implementation shortcut.
    bool load(const std::string &path, std::vector<uint8_t> &fileData)
    {
        size_t fileDataSize = getSize(path);
        if (fileDataSize == 0)
        {
            return false;
        }

        fileData.resize(fileDataSize);
        return load(path, fileData.data(), fileDataSize);
    }
};
