#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace ios_mod_manager
{
    struct Entry
    {
        std::filesystem::path iniPath;
        std::string title;
        std::string author;
        std::string issue;
        bool enabled = false;
    };

    // The database stores paths relative to the user directory, surviving app updates.
    std::filesystem::path DatabasePath(const std::filesystem::path& userPath);
    std::filesystem::path ResolveRelative(const std::filesystem::path& base, std::string value);
    std::string CompatibilityIssue(const std::filesystem::path& modIni);
    std::vector<Entry> Scan(const std::filesystem::path& userPath);
    std::string SelectionIssue(const std::vector<Entry>& entries);
    bool Save(const std::filesystem::path& userPath, const std::vector<Entry>& entries, std::string& error);
    bool Import(const std::filesystem::path& userPath, const std::filesystem::path& folder, std::string& error);
}
