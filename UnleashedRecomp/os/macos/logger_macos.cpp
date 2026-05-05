#include <os/logger.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <system_error>
#include <string_view>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

static std::mutex s_logMutex;

#if defined(__APPLE__) && TARGET_OS_IPHONE
static FILE* s_logFile = nullptr;

static std::filesystem::path GetIOSLogPath()
{
    const char* home = std::getenv("HOME");
    if (home == nullptr || *home == '\0')
        return {};

    return std::filesystem::path(home) / "Documents" / "UnleashedRecomp" / "unleashedrecomp.log";
}
#endif

static void SafeLogPrint(const std::string_view str, const char* func)
{
    std::lock_guard lock(s_logMutex);

    try
    {
        if (func)
        {
            fmt::println("[{}] {}", func, str);
        }
        else
        {
            fmt::println("{}", str);
        }
    }
    catch (...)
    {
        if (func)
        {
            std::fprintf(stderr, "[%s] %.*s\n", func, static_cast<int>(str.size()), str.data());
        }
        else
        {
            std::fprintf(stderr, "%.*s\n", static_cast<int>(str.size()), str.data());
        }
    }

#if defined(__APPLE__) && TARGET_OS_IPHONE
    if (s_logFile != nullptr)
    {
        if (func)
        {
            std::fprintf(s_logFile, "[%s] %.*s\n", func, static_cast<int>(str.size()), str.data());
        }
        else
        {
            std::fprintf(s_logFile, "%.*s\n", static_cast<int>(str.size()), str.data());
        }

        std::fflush(s_logFile);
    }
#endif
}

void os::logger::Init()
{
#if defined(__APPLE__) && TARGET_OS_IPHONE
    std::lock_guard lock(s_logMutex);
    if (s_logFile != nullptr)
        return;

    auto logPath = GetIOSLogPath();
    if (logPath.empty())
        return;

    std::error_code ec;
    std::filesystem::create_directories(logPath.parent_path(), ec);

    s_logFile = std::fopen(logPath.c_str(), "a");
    if (s_logFile != nullptr)
    {
        std::setvbuf(s_logFile, nullptr, _IOLBF, 0);
        std::fprintf(s_logFile, "\n--- UnleashedRecomp log start ---\n");
        std::fflush(s_logFile);
    }
#endif
}

void os::logger::Log(const std::string_view str, ELogType type, const char* func)
{
    (void)type;
    SafeLogPrint(str, func);
}
