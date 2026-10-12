#include <mod/ios_mod_manager.h>
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;
using namespace ios_mod_manager;

static void write(const fs::path& path, const std::string& contents)
{
    fs::create_directories(path.parent_path());
    std::ofstream(path) << contents;
}

int main(int argc, char** argv)
{
    assert(argc == 2); // Use a disposable scratch directory.
    const fs::path root = fs::absolute(argv[1]);
    assert(!fs::exists(root));
    fs::create_directories(root);
    const auto source = root / "source";
    const auto user = root / "user";
    const std::string asset = "[Main]\nIncludeDirCount=1\nIncludeDir0=\".\"\n[Desc]\nTitle=\"Test mod\"\n";
    write(source / "First" / "mod.ini", asset);
    write(source / "First" / "+Sonic.ar.00", "fixture");
    std::string error;
    assert(Import(user, source / "First", error));
    assert(fs::exists(user / "mods/First/+Sonic.ar.00"));
    assert(!Import(user, source / "First", error));
    auto entries = Scan(user);
    assert(entries.size() == 1 && !entries[0].enabled && entries[0].issue.empty());
    entries[0].enabled = true;
    assert(Save(user, entries, error));
    entries = Scan(user);
    assert(entries[0].enabled);

    write(user / "mods/Second/mod.ini", asset);
    write(user / "mods/DLL/mod.ini", "[Main]\nDLLFile=plugin.dll\nIncludeDirCount=1\nIncludeDir0=.\n");
    write(user / "mods/Code/mod.ini", "[Main]\nCodeFile=custom.hmm\n");
    write(user / "mods/Escape/mod.ini", "[Main]\nIncludeDirCount=1\nIncludeDir0=../..\n");
    write(user / "mods/Windows/mod.ini", "[Main]\nIncludeDirCount=1\nIncludeDir0=C:\\mods\\foo\n");
    entries = Scan(user);
    assert(entries[0].enabled && entries[0].iniPath.filename() == "mod.ini");
    for (auto& entry : entries)
        if (entry.iniPath.parent_path().filename() != "First" && entry.iniPath.parent_path().filename() != "Second")
            assert(!entry.issue.empty() && !entry.enabled);
    auto second = std::find_if(entries.begin(), entries.end(), [](const Entry& e) { return e.iniPath.parent_path().filename() == "Second"; });
    second->enabled = true;
    std::rotate(entries.begin(), second, second + 1);
    assert(Save(user, entries, error));
    entries = Scan(user);
    assert(entries[0].iniPath.parent_path().filename() == "Second" && entries[0].enabled);
    // Removing earlier entries must not lose later entries' settings/order.
    fs::remove_all(user / "mods/Second");
    entries = Scan(user);
    assert(entries[0].iniPath.parent_path().filename() == "First" && entries[0].enabled);

    assert(ResolveRelative(user, "mods\\First\\mod.ini") == user / "mods/First/mod.ini");
    assert(ResolveRelative(user, "../escape").empty());
    assert(ResolveRelative(user, "/absolute").empty());
    assert(ResolveRelative(user, "C:\\absolute").empty());
    write(user / "mods/Save/mod.ini", "[Main]\nIncludeDirCount=1\nIncludeDir0=.\nSaveFile=../../save\n");
    assert(!CompatibilityIssue(user / "mods/Save/mod.ini").empty());
    write(source / "Symlink/mod.ini", asset);
    fs::create_symlink(source / "First/+Sonic.ar.00", source / "Symlink/linked");
    assert(!Import(user, source / "Symlink", error));
    assert(!fs::exists(user / "mods/Symlink"));

    write(user / "mods/Dependency/mod.ini", "[Main]\nID=base\nIncludeDirCount=1\nIncludeDir0=.\n[Desc]\nVersion=1.2.0\n");
    write(user / "mods/Dependent/mod.ini", "[Main]\nIncludeDirCount=1\nIncludeDir0=.\nDependsCount=1\nDepends0=\"base|Base mod|https://example.com|1.2\"\n");
    entries = Scan(user);
    for (auto& e : entries) e.enabled = e.iniPath.parent_path().filename() == "Dependent";
    assert(!SelectionIssue(entries).empty());
    assert(!Save(user, entries, error));
    for (auto& e : entries) if (e.iniPath.parent_path().filename() == "Dependency") e.enabled = true;
    assert(SelectionIssue(entries).empty());
    assert(Save(user, entries, error));
    write(user / "mods/Dependency/mod.ini", "[Main]\nID=base\nIncludeDirCount=1\nIncludeDir0=.\n[Desc]\nVersion=1.1.9\n");
    assert(!SelectionIssue(entries).empty());

    // Relative database paths continue to work after the sandbox path changes.
    const auto moved = root / "moved-user";
    fs::rename(user, moved);
    entries = Scan(moved);
    for (const auto& e : entries) assert(e.iniPath.string().starts_with(moved.string()));
    fs::remove_all(root);
    std::cout << "iOS mod manager tests passed\n";
}
