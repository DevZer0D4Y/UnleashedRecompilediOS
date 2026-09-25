#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
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

template<typename T>
static std::optional<std::string> FindPathByCandidates(const std::map<std::string, T>& fileMap, const std::vector<std::string>& candidates)
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

static bool WriteFileFromVfs(VirtualFileSystem& vfs, const std::string& sourcePath, const std::filesystem::path& outPath)
{
    // Empty files are valid, but the loader treats a size of zero as a failure.
    std::vector<uint8_t> bytes;
    if (vfs.getSize(sourcePath) > 0 && !vfs.load(sourcePath, bytes))
        return false;

    std::filesystem::create_directories(outPath.parent_path());
    std::ofstream file(outPath, std::ios::binary);
    if (!file.good())
        return false;

    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return file.good();
}

// Extracts every file in a container, keeping its folder structure.
template<typename T>
static bool ExtractAll(VirtualFileSystem& vfs, const std::map<std::string, T>& fileMap, const std::filesystem::path& outDir)
{
    size_t count = 0;

    for (const auto& [path, _] : fileMap)
    {
        std::string relativePath = path;
        std::replace(relativePath.begin(), relativePath.end(), '\\', '/');

        while (!relativePath.empty() && relativePath.front() == '/')
            relativePath.erase(relativePath.begin());

        if (relativePath.empty())
            continue;

        if (!WriteFileFromVfs(vfs, path, outDir / relativePath))
        {
            std::cerr << "Failed to extract " << path << "\n";
            return false;
        }

        count++;
    }

    std::cout << "Extracted " << count << " files to " << outDir.string() << "\n";
    return true;
}

int main(int argc, char** argv)
{
    std::filesystem::path isoPath;
    std::filesystem::path contentPath;
    std::filesystem::path outDir;
    std::filesystem::path isoAllOutDir;
    std::filesystem::path contentAllOutDir;

    for (int i = 1; i < argc; ++i)
    {
        std::string_view arg = argv[i];
        if (arg == "--iso" && i + 1 < argc)
            isoPath = argv[++i];
        else if ((arg == "--content" || arg == "--update-container") && i + 1 < argc)
            contentPath = argv[++i];
        else if ((arg == "--out" || arg == "--out-dir") && i + 1 < argc)
            outDir = argv[++i];
        else if (arg == "--iso-extract-all" && i + 1 < argc)
            isoAllOutDir = argv[++i];
        else if (arg == "--content-extract-all" && i + 1 < argc)
            contentAllOutDir = argv[++i];
        else if (arg == "--help" || arg == "-h")
        {
            std::cout << "Usage: x_content_extract [--out <private-dir>] [--iso <game.iso>] [--content <update-container>]\n"
                         "                         [--iso-extract-all <dir>] [--content-extract-all <dir>]\n"
                         "\n"
                         "  --out                   Extracts default.xex and shader.ar from the ISO, and default.xexp from the container.\n"
                         "  --iso-extract-all       Extracts every file in the ISO to a folder.\n"
                         "  --content-extract-all   Extracts every file in the container to a folder.\n";
            return 0;
        }
        else
        {
            std::cerr << "Unknown or incomplete argument: " << arg << "\n";
            return 2;
        }
    }

    if (outDir.empty() && isoAllOutDir.empty() && contentAllOutDir.empty())
    {
        std::cerr << "Nothing to do: provide --out, --iso-extract-all and/or --content-extract-all\n";
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
    bool extractedAll = true;

    if (!isoPath.empty())
    {
        auto iso = ISOFileSystem::create(isoPath);
        if (!iso)
        {
            std::cerr << "Failed to open ISO: " << isoPath << "\n";
            return 3;
        }

        if (!outDir.empty())
        {
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

        if (!isoAllOutDir.empty())
            extractedAll &= ExtractAll(*iso, iso->fileMap, isoAllOutDir);
    }

    if (!contentPath.empty())
    {
        auto content = XContentFileSystem::create(contentPath);
        if (!content)
        {
            std::cerr << "Failed to open content container: " << contentPath << "\n";
            return 4;
        }

        if (!outDir.empty())
        {
            std::optional<std::string> xexpPath = FindPathByCandidates(content->fileMap, { "default.xexp" });
            if (xexpPath)
            {
                extractedXexp = WriteFileFromVfs(*content, *xexpPath, outDir / "default.xexp");
                std::cout << "Extracted default.xexp from content path: " << *xexpPath << "\n";
            }
        }

        if (!contentAllOutDir.empty())
            extractedAll &= ExtractAll(*content, content->fileMap, contentAllOutDir);
    }

    if (!extractedAll)
        return 1;

    // Only report files that were asked for, from the inputs that were given.
    bool success = true;

    if (!outDir.empty() && !isoPath.empty())
    {
        if (!extractedXex)
            std::cerr << "default.xex not found in the ISO\n";

        if (!extractedShader)
            std::cerr << "shader.ar not found in the ISO\n";

        success &= extractedXex && extractedShader;
    }

    if (!outDir.empty() && !contentPath.empty())
    {
        if (!extractedXexp)
            std::cerr << "default.xexp not found in the container\n";

        success &= extractedXexp;
    }

    return success ? 0 : 1;
}
