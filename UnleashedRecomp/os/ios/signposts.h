#pragma once

#if defined(UNLEASHED_RECOMP_IOS_SIGNPOSTS)
#include <os/log.h>
#include <os/signpost.h>
#endif

namespace ios_signposts
{
    enum class IntervalKind
    {
        RendererStartup,
        Installer,
        CacheTrim,
    };

    enum class EventKind
    {
        Background,
        Foreground,
        MemoryWarning,
        Restart,
    };

#if defined(UNLEASHED_RECOMP_IOS_SIGNPOSTS)

    inline os_log_t GetLog()
    {
        static os_log_t log = os_log_create("io.github.hedgedev.unleashedrecomp", "Performance");
        return log;
    }

    class Interval
    {
        os_log_t m_log;
        os_signpost_id_t m_id;
        IntervalKind m_kind;

    public:
        explicit Interval(IntervalKind kind) : m_log(GetLog()), m_id(os_signpost_id_generate(m_log)), m_kind(kind)
        {
            switch (m_kind)
            {
            case IntervalKind::RendererStartup:
                os_signpost_interval_begin(m_log, m_id, "Renderer startup");
                break;
            case IntervalKind::Installer:
                os_signpost_interval_begin(m_log, m_id, "Installer");
                break;
            case IntervalKind::CacheTrim:
                os_signpost_interval_begin(m_log, m_id, "Cache trim");
                break;
            }
        }

        ~Interval()
        {
            switch (m_kind)
            {
            case IntervalKind::RendererStartup:
                os_signpost_interval_end(m_log, m_id, "Renderer startup");
                break;
            case IntervalKind::Installer:
                os_signpost_interval_end(m_log, m_id, "Installer");
                break;
            case IntervalKind::CacheTrim:
                os_signpost_interval_end(m_log, m_id, "Cache trim");
                break;
            }
        }

        Interval(const Interval&) = delete;
        Interval& operator=(const Interval&) = delete;
    };

    inline void Emit(EventKind kind)
    {
        switch (kind)
        {
        case EventKind::Background:
            os_signpost_event_emit(GetLog(), OS_SIGNPOST_ID_EXCLUSIVE, "Application backgrounded");
            break;
        case EventKind::Foreground:
            os_signpost_event_emit(GetLog(), OS_SIGNPOST_ID_EXCLUSIVE, "Application foregrounded");
            break;
        case EventKind::MemoryWarning:
            os_signpost_event_emit(GetLog(), OS_SIGNPOST_ID_EXCLUSIVE, "Memory warning");
            break;
        case EventKind::Restart:
            os_signpost_event_emit(GetLog(), OS_SIGNPOST_ID_EXCLUSIVE, "Restart requested");
            break;
        }
    }

#else

    class Interval
    {
    public:
        explicit Interval(IntervalKind) { }
    };

    inline void Emit(EventKind) { }

#endif
}
