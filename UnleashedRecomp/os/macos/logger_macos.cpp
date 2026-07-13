#include <os/logger.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <system_error>
#include <string_view>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if defined(__APPLE__) && TARGET_OS_IPHONE && defined(UNLEASHED_RECOMP_IOS_DETAILED_LOGGING)
#include <array>
#include <mutex>

static std::mutex s_fileMutex;
static FILE* s_logFile = nullptr;
static std::array<char, 64 * 1024> s_logBuffer;

static std::filesystem::path GetIOSLogPath()
{
    const char* home = std::getenv("HOME");
    if (home == nullptr || *home == '\0')
        return {};

    return std::filesystem::path(home) / "Documents" / "UnleashedRecomp" / "unleashedrecomp.log";
}
#endif

static const char* GetLogPrefix(os::logger::ELogType type)
{
    switch (type)
    {
        case os::logger::ELogType::Utility: return "[utility] ";
        case os::logger::ELogType::Warning: return "[warning] ";
        case os::logger::ELogType::Error: return "[error] ";
        default: return "";
    }
}

static void PrintLogLine(FILE* output, const std::string_view str, os::logger::ELogType type, const char* func)
{
    const char* prefix = GetLogPrefix(type);
    if (func != nullptr)
    {
        std::fprintf(output, "%s[%s] %.*s\n", prefix, func, static_cast<int>(str.size()), str.data());
    }
    else
    {
        std::fprintf(output, "%s%.*s\n", prefix, static_cast<int>(str.size()), str.data());
    }
}

static void SafeLogPrint(const std::string_view str, os::logger::ELogType type, const char* func)
{
    FILE* console = type == os::logger::ELogType::Error ? stderr : stdout;

#if defined(__APPLE__) && TARGET_OS_IPHONE && defined(UNLEASHED_RECOMP_IOS_DETAILED_LOGGING)
    std::lock_guard lock(s_fileMutex);
#endif

    PrintLogLine(console, str, type, func);

#if defined(__APPLE__) && TARGET_OS_IPHONE && defined(UNLEASHED_RECOMP_IOS_DETAILED_LOGGING)
    if (s_logFile != nullptr)
    {
        PrintLogLine(s_logFile, str, type, func);
        if (type == os::logger::ELogType::Error)
            std::fflush(s_logFile);
    }
#endif

    if (type == os::logger::ELogType::Error)
        std::fflush(console);
}

void os::logger::Init()
{
#if defined(__APPLE__) && TARGET_OS_IPHONE && defined(UNLEASHED_RECOMP_IOS_DETAILED_LOGGING)
    std::lock_guard lock(s_fileMutex);
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
        std::setvbuf(s_logFile, s_logBuffer.data(), _IOFBF, s_logBuffer.size());
        std::fprintf(s_logFile, "\n--- UnleashedRecomp log start ---\n");
    }
#endif
}

void os::logger::Flush()
{
#if defined(__APPLE__) && TARGET_OS_IPHONE && defined(UNLEASHED_RECOMP_IOS_DETAILED_LOGGING)
    std::lock_guard lock(s_fileMutex);
    if (s_logFile != nullptr)
        std::fflush(s_logFile);
#else
    std::fflush(nullptr);
#endif
}

void os::logger::Shutdown()
{
#if defined(__APPLE__) && TARGET_OS_IPHONE && defined(UNLEASHED_RECOMP_IOS_DETAILED_LOGGING)
    std::lock_guard lock(s_fileMutex);
    if (s_logFile != nullptr)
    {
        std::fprintf(s_logFile, "--- UnleashedRecomp log end ---\n");
        std::fflush(s_logFile);
        std::fclose(s_logFile);
        s_logFile = nullptr;
    }
#else
    std::fflush(nullptr);
#endif
}

void os::logger::Log(const std::string_view str, ELogType type, const char* func)
{
    SafeLogPrint(str, type, func);
}
