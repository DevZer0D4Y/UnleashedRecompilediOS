#include <os/logger.h>
#include <os/process.h>

#include <cstdio>

#define FOREGROUND_WHITE  (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)
#define FOREGROUND_YELLOW (FOREGROUND_RED | FOREGROUND_GREEN)

static HANDLE g_hStandardOutput;

void os::logger::Init()
{
    g_hStandardOutput = GetStdHandle(STD_OUTPUT_HANDLE);
}

void os::logger::Flush()
{
    std::fflush(nullptr);
}

void os::logger::Shutdown()
{
    std::fflush(nullptr);
}

void os::logger::Log(const std::string_view str, ELogType type, const char* func)
{
    if (!os::process::g_consoleVisible)
        return;

    switch (type)
    {
        case ELogType::Utility:
            SetConsoleTextAttribute(g_hStandardOutput, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
            break;

        case ELogType::Warning:
            SetConsoleTextAttribute(g_hStandardOutput, FOREGROUND_YELLOW | FOREGROUND_INTENSITY);
            break;

        case ELogType::Error:
            SetConsoleTextAttribute(g_hStandardOutput, FOREGROUND_RED | FOREGROUND_INTENSITY);
            break;

        default:
            SetConsoleTextAttribute(g_hStandardOutput, FOREGROUND_WHITE);
            break;
    }

    if (func)
    {
        fmt::println("[{}] {}", func, str);
    }
    else
    {
        fmt::println("{}", str);
    }

    SetConsoleTextAttribute(g_hStandardOutput, FOREGROUND_WHITE);
}
