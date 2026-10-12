#include "ios_mod_manager.h"
#include <algorithm>
#include <charconv>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <xxhash.h>
#include <ankerl/unordered_dense.h>
#include "ini_file.h"

namespace ios_mod_manager
{
    std::filesystem::path DatabasePath(const std::filesystem::path& userPath)
    {
        return userPath / "ios-mods.ini";
    }

    std::filesystem::path ResolveRelative(const std::filesystem::path& base, std::string value)
    {
        std::replace(value.begin(), value.end(), '\\', '/');
        std::filesystem::path path = std::u8string_view(reinterpret_cast<const char8_t*>(value.c_str()));
        // Windows absolute paths cannot be transplanted into an iOS sandbox.
        if (value.find(':') != std::string::npos || path.is_absolute())
            return {};
        auto result = (base / path).lexically_normal();
        auto relative = result.lexically_relative(base.lexically_normal());
        if (relative.empty() || *relative.begin() == "..")
            return {};
        return result;
    }

    std::string CompatibilityIssue(const std::filesystem::path& modIni)
    {
        try
        {
            IniFile ini;
            if (std::filesystem::is_symlink(modIni) || std::filesystem::is_symlink(modIni.parent_path()))
                return "Symbolic links are not supported in iOS mods.";
            if (!ini.read(modIni))
                return "Cannot read mod.ini.";
            if (!ini.getString("Main", "DLLFile", "").empty())
                return "Requires a native plugin; this mod needs an iOS port.";
            if (!ini.getString("Main", "CodeFile", "").empty())
                return "Custom HMM code is not supported by this loader.";
            if (ini.get<size_t>("Main", "DependsCount", 0) > 256)
                return "Invalid dependency count.";
            const auto base = modIni.parent_path();
            if (!ini.contains("Details") && !ini.contains("Filesystem"))
            {
                auto count = ini.get<size_t>("Main", "IncludeDirCount", 0);
                if (count == 0 || count > 256)
                    return "Missing or invalid include directories.";
                for (size_t i = 0; i < count; ++i)
                {
                    auto value = ini.getString("Main", "IncludeDir" + std::to_string(i), "");
                    auto path = ResolveRelative(base, value);
                    std::error_code ec;
                    if (value.empty() || path.empty() || !std::filesystem::is_directory(path, ec))
                        return "Include directory is missing or outside the mod folder.";
                }
            }
            // Reject symlinks, including those leading outside the imported folder.
            for (const auto& file : std::filesystem::recursive_directory_iterator(base))
                if (file.is_symlink())
                    return "Symbolic links are not supported in iOS mods.";
            const auto save = ini.contains("Details") || ini.contains("Filesystem")
                ? ini.getString("Details", "Save", ini.getString("Filesystem", "Save", ""))
                : ini.getString("Main", "SaveFile", "");
            if (!save.empty() && ResolveRelative(base, save).empty())
                return "Save redirection must stay inside the mod folder.";
            return {};
        }
        catch (const std::exception& ex) { return std::string("Cannot inspect mod: ") + ex.what(); }
    }

    std::vector<Entry> Scan(const std::filesystem::path& userPath)
    {
        const auto modsPath = userPath / "mods";
        std::filesystem::create_directories(modsPath);
        IniFile database;
        database.read(DatabasePath(userPath));
        std::vector<Entry> entries;
        for (const auto& folder : std::filesystem::directory_iterator(modsPath))
        {
            if (folder.is_symlink() || !folder.is_directory())
                continue;
            const auto iniPath = folder.path() / "mod.ini";
            if (!std::filesystem::is_regular_file(iniPath) || std::filesystem::is_symlink(iniPath))
                continue;
            IniFile ini;
            ini.read(iniPath);
            Entry entry;
            entry.iniPath = iniPath;
            entry.title = ini.getString("Desc", "Title", ini.getString("Details", "Title", folder.path().filename().string()));
            entry.author = ini.getString("Desc", "Author", ini.getString("Details", "Author", ""));
            entry.issue = CompatibilityIssue(iniPath);
            entries.emplace_back(std::move(entry));
        }
        std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) { return a.iniPath < b.iniPath; });
        // Restore order including disabled mods; newly discovered mods start disabled.
        std::vector<Entry> ordered;
        auto count = std::min(database.get<size_t>("Installed", "Count", 0), size_t(10000));
        for (size_t i = 0; i < count; ++i)
        {
            auto path = ResolveRelative(userPath, database.getString("Installed", "Mod" + std::to_string(i), ""));
            auto found = std::find_if(entries.begin(), entries.end(), [&](const Entry& e) { return e.iniPath == path; });
            if (found == entries.end())
                continue;
            found->enabled = found->issue.empty() && database.getBool("Installed", "Enabled" + std::to_string(i), false);
            ordered.emplace_back(std::move(*found));
            entries.erase(found);
        }
        ordered.insert(ordered.end(), entries.begin(), entries.end());
        return ordered;
    }

    static bool VersionAtLeast(std::string actual, std::string required)
    {
        if (required.empty() || actual == required)
            return true;
        auto parse = [](const std::string& text, std::vector<unsigned>& result)
        {
            size_t start = 0;
            while (start < text.size())
            {
                const auto end = text.find('.', start);
                const auto length = end == std::string::npos ? text.size() - start : end - start;
                unsigned value = 0;
                const auto conversion = std::from_chars(text.data() + start, text.data() + start + length, value);
                if (conversion.ec != std::errc{} || conversion.ptr != text.data() + start + length)
                    return false;
                result.push_back(value);
                if (end == std::string::npos) break;
                start = end + 1;
                if (start == text.size()) return false;
            }
            return !result.empty();
        };
        std::vector<unsigned> a, b;
        if (!parse(actual, a) || !parse(required, b)) return false;
        a.resize(std::max(a.size(), b.size()));
        b.resize(a.size());
        return a >= b;
    }

    std::string SelectionIssue(const std::vector<Entry>& entries)
    {
        for (const auto& entry : entries)
        {
            if (!entry.enabled) continue;
            IniFile ini;
            if (!ini.read(entry.iniPath)) return "Cannot read " + entry.title + ".";
            const auto count = ini.get<size_t>("Main", "DependsCount", 0);
            if (count > 256) return "Invalid dependency count in " + entry.title + ".";
            for (size_t i = 0; i < count; ++i)
            {
                const auto value = ini.getString("Main", "Depends" + std::to_string(i), "");
                std::vector<std::string> fields;
                size_t start = 0;
                for (;;)
                {
                    const auto end = value.find('|', start);
                    fields.push_back(value.substr(start, end == std::string::npos ? end : end - start));
                    if (end == std::string::npos) break;
                    start = end + 1;
                }
                if (fields.size() < 3 || fields[0].empty()) return "Invalid dependency declaration in " + entry.title + ".";
                bool satisfied = false;
                for (const auto& candidate : entries)
                {
                    if (!candidate.enabled || candidate.iniPath == entry.iniPath || !candidate.issue.empty()) continue;
                    IniFile dependency;
                    if (dependency.read(candidate.iniPath) && dependency.getString("Main", "ID", "") == fields[0]
                        && VersionAtLeast(dependency.getString("Desc", "Version", ""), fields.size() > 3 ? fields[3] : ""))
                    {
                        satisfied = true;
                        break;
                    }
                }
                if (!satisfied) return entry.title + " requires an enabled compatible dependency: " + fields[1]
                    + (fields.size() > 3 && !fields[3].empty() ? " (version " + fields[3] + " or newer)." : ".");
            }
        }
        return {};
    }

    bool Save(const std::filesystem::path& userPath, const std::vector<Entry>& entries, std::string& error)
    {
        try
        {
            for (const auto& entry : entries)
                if (entry.enabled)
                {
                    const auto issue = CompatibilityIssue(entry.iniPath);
                    if (!issue.empty()) throw std::runtime_error(entry.title + ": " + issue);
                }
            const auto selectionIssue = SelectionIssue(entries);
            if (!selectionIssue.empty()) throw std::runtime_error(selectionIssue);
            auto temporary = DatabasePath(userPath);
            temporary += ".tmp";
            std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
            stream << "[Installed]\nCount=" << entries.size() << '\n';
            std::vector<size_t> active;
            for (size_t i = 0; i < entries.size(); ++i)
            {
                auto path = entries[i].iniPath.lexically_relative(userPath).generic_string();
                if (path.find_first_of("\"\r\n") != std::string::npos || ResolveRelative(userPath, path) != entries[i].iniPath)
                    throw std::runtime_error("Invalid mod path.");
                const bool enabled = entries[i].enabled && CompatibilityIssue(entries[i].iniPath).empty();
                stream << "Mod" << i << "=\"" << path << "\"\nEnabled" << i << '=' << enabled << '\n';
                if (enabled)
                    active.push_back(i);
            }
            stream << "[Main]\nActiveModCount=" << active.size() << '\n';
            for (size_t i = 0; i < active.size(); ++i)
                stream << "ActiveMod" << i << "=ios" << active[i] << '\n';
            stream << "[Mods]\n";
            for (size_t i : active)
                stream << "ios" << i << "=\"" << entries[i].iniPath.lexically_relative(userPath).generic_string() << "\"\n";
            stream.close();
            if (!stream)
                throw std::runtime_error("Could not write mod settings.");
            std::filesystem::rename(temporary, DatabasePath(userPath));
            return true;
        }
        catch (const std::exception& ex) { error = ex.what(); return false; }
    }

    bool Import(const std::filesystem::path& userPath, const std::filesystem::path& folder, std::string& error)
    {
        std::filesystem::path staging;
        bool ownsStaging = false;
        try
        {
            if (!std::filesystem::is_regular_file(folder / "mod.ini") || std::filesystem::is_symlink(folder))
                throw std::runtime_error("Select the extracted folder containing mod.ini.");
            for (const auto& file : std::filesystem::recursive_directory_iterator(folder))
                if (file.is_symlink() || (!file.is_regular_file() && !file.is_directory()))
                    throw std::runtime_error("Mod folders must contain ordinary files and folders.");
            const auto destination = userPath / "mods" / folder.filename();
            if (folder.filename().empty() || folder.filename().string().find_first_of("\"\r\n") != std::string::npos)
                throw std::runtime_error("Invalid mod folder name.");
            std::filesystem::create_directories(destination.parent_path());
            if (std::filesystem::exists(destination))
                throw std::runtime_error("A mod with this folder name is already installed. Rename the source folder first.");
            staging = userPath / "mod-import.tmp";
            if (std::filesystem::exists(staging))
                throw std::runtime_error("An unfinished import exists at mod-import.tmp. Remove it using Files and try again.");
            ownsStaging = true;
            std::filesystem::copy(folder, staging, std::filesystem::copy_options::recursive);
            std::filesystem::rename(staging, destination);
            return true;
        }
        catch (const std::exception& ex)
        {
            error = ex.what();
            if (ownsStaging)
            {
                std::error_code ec;
                std::filesystem::remove_all(staging, ec);
            }
            return false;
        }
    }
}
