#include "paths.h"
#include <os/process.h>
#if defined(__APPLE__)
#include <TargetConditionals.h>
#include <SDL.h>
#endif
#if defined(__linux__) || defined(__APPLE__)
#include <pwd.h>
#include <unistd.h>
#endif

std::filesystem::path g_executableRoot = os::process::GetExecutableRoot();
std::filesystem::path g_userPath = BuildUserPath();

bool CheckPortable()
{
    std::error_code ec;
    return std::filesystem::exists(g_executableRoot / "portable.txt", ec);
}

std::filesystem::path BuildUserPath()
{
    if (CheckPortable())
        return g_executableRoot;

    std::filesystem::path userPath;

#if defined(_WIN32)
    PWSTR knownPath = NULL;
    if (SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &knownPath) == S_OK)
        userPath = std::filesystem::path{ knownPath } / USER_DIRECTORY;

    CoTaskMemFree(knownPath);
#elif defined(__linux__) || defined(__APPLE__)
#if defined(__APPLE__) && TARGET_OS_IPHONE
    const char* homeDir = getenv("HOME");
    if (homeDir == nullptr)
    {
        if (const passwd* pw = getpwuid(getuid()); pw != nullptr)
            homeDir = pw->pw_dir;
    }

    if (homeDir != nullptr)
    {
        userPath = std::filesystem::path(homeDir) / "Documents" / USER_DIRECTORY;
    }
    else if (char* prefPath = SDL_GetPrefPath("hedge_dev", USER_DIRECTORY); prefPath != nullptr)
    {
        userPath = prefPath;
        SDL_free(prefPath);
    }
#else
    const char* homeDir = getenv("HOME");
    if (homeDir == nullptr)
    {
        if (const passwd* pw = getpwuid(getuid()); pw != nullptr)
            homeDir = pw->pw_dir;
    }

    if (userPath.empty() && homeDir != nullptr)
    {
        // Prefer to store in the .config directory if it exists. Use the home directory otherwise.
        std::filesystem::path homePath = homeDir;
#if defined(__linux__)
        std::filesystem::path configPath = homePath / ".config";
#else
        std::filesystem::path configPath = homePath / "Library" / "Application Support";
#endif
        std::error_code ec;
        if (std::filesystem::exists(configPath, ec))
            userPath = configPath / USER_DIRECTORY;
        else
            userPath = homePath / ("." USER_DIRECTORY);
    }
#endif

    if (userPath.empty())
    {
        std::error_code ec;
        const auto tempPath = std::filesystem::temp_directory_path(ec);
        if (!ec)
            userPath = tempPath / USER_DIRECTORY;
    }
#else
    static_assert(false, "GetUserPath() not implemented for this platform.");
#endif

    return userPath;
}

const std::filesystem::path& GetUserPath()
{
    return g_userPath;
}
