#include <os/logger.h>

#include <cstdio>

static void SafeLogPrint(const std::string_view str, const char* func)
{
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
}

void os::logger::Init()
{
}

void os::logger::Log(const std::string_view str, ELogType type, const char* func)
{
    SafeLogPrint(str, func);
}
