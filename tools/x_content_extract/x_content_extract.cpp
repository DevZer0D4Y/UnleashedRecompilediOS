#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "install/iso_file_system.h"
#include "install/xcontent_file_system.h"

static std::string ToLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return value;
}

static bool EndsWithInsensitive(const std::string& value, const std::string& suffix)
{
    if (suffix.size() > value.size())
        return false;

    return ToLower(value.substr(value.size() - suffix.size())) == ToLower(suffix);
}

static std::optional<std::string> FindPathByCandidates(const std::map<std::string, std::tuple<size_t, size_t>>& fileMap, const std::vector<std::string>& candidates)
{
    for (const std::string& candidate : candidates)
    {
        if (fileMap.find(candidate) != fileMap.end())
            return candidate;
    }

    for (const auto& [path, _] : fileMap)
    {
        for (const std::string& candidate : candidates)
        {
            if (EndsWithInsensitive(path, candidate))
                return path;
        }
    }

    return std::nullopt;
}

static bool WriteFileFromVfs(const VirtualFileSystem& vfs, const std::string& sourcePath, const std::filesystem::path& outPath)
{
    std::vector<uint8_t> bytes;
    if (!vfs.load(sourcePath, bytes))
        return false;

    std::filesystem::create_directories(outPath.parent_path());
    std::ofstream file(outPath, std::ios::binary);
    if (!file.good())
        return false;

    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return file.good();
}

int main(int argc, char** argv)
{
    std::filesystem::path isoPath;
    std::filesystem::path contentPath;
    std::filesystem::path outDir;

    for (int i = 1; i < argc; ++i)
    {
        std::string_view arg = argv[i];
        if (arg == "--iso" && i + 1 < argc)
            isoPath = argv[++i];
        else if ((arg == "--content" || arg == "--update-container") && i + 1 < argc)
            contentPath = argv[++i];
        else if ((arg == "--out" || arg == "--out-dir") && i + 1 < argc)
            outDir = argv[++i];
        else if (arg == "--help" || arg == "-h")
        {
            std::cout << "Usage: x_content_extract --out <private-dir> [--iso <game.iso>] [--content <update-container>]\n";
            return 0;
        }
        else
        {
            std::cerr << "Unknown or incomplete argument: " << arg << "\n";
            return 2;
        }
    }

    if (outDir.empty())
    {
        std::cerr << "Missing required --out <private-dir> argument\n";
        return 2;
    }

    if (isoPath.empty() && contentPath.empty())
    {
        std::cerr << "At least one input must be provided: --iso and/or --content\n";
        return 2;
    }

    bool extractedXex = false;
    bool extractedShader = false;
    bool extractedXexp = false;

    if (!isoPath.empty())
    {
        auto iso = ISOFileSystem::create(isoPath);
        if (!iso)
        {
            std::cerr << "Failed to open ISO: " << isoPath << "\n";
            return 3;
        }

        std::optional<std::string> xexPath = FindPathByCandidates(iso->fileMap, { "default.xex" });
        std::optional<std::string> shaderPath = FindPathByCandidates(iso->fileMap, { "shader.ar" });

        if (xexPath)
        {
            extractedXex = WriteFileFromVfs(*iso, *xexPath, outDir / "default.xex");
            std::cout << "Extracted default.xex from ISO path: " << *xexPath << "\n";
        }

        if (shaderPath)
        {
            extractedShader = WriteFileFromVfs(*iso, *shaderPath, outDir / "shader.ar");
            std::cout << "Extracted shader.ar from ISO path: " << *shaderPath << "\n";
        }
    }

    if (!contentPath.empty())
    {
        auto content = XContentFileSystem::create(contentPath);
        if (!content)
        {
            std::cerr << "Failed to open content container: " << contentPath << "\n";
            return 4;
        }

        std::optional<std::string> xexpPath = FindPathByCandidates(content->fileMap, { "default.xexp" });
        if (xexpPath)
        {
            extractedXexp = WriteFileFromVfs(*content, *xexpPath, outDir / "default.xexp");
            std::cout << "Extracted default.xexp from content path: " << *xexpPath << "\n";
        }
    }

    if (!extractedXex)
        std::cerr << "default.xex not extracted (provide --iso with valid game image)\n";

    if (!extractedShader)
        std::cerr << "shader.ar not extracted (provide --iso with valid game image)\n";

    if (!extractedXexp)
        std::cerr << "default.xexp not extracted (provide --content with valid update container)\n";

    if (extractedXex && extractedShader && extractedXexp)
        return 0;

    return 1;
}
