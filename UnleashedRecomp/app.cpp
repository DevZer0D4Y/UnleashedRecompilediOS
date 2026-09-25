#include "app.h"
#include <api/SWA.h>
#include <gpu/video.h>
#include <install/installer.h>
#include <kernel/function.h>
#include <locale/locale.h>
#include <os/logger.h>
#include <os/process.h>
#include <patches/audio_patches.h>
#include <patches/inspire_patches.h>
#include <ui/game_window.h>
#include <user/config.h>
#include <user/paths.h>
#include <user/registry.h>

static std::filesystem::path GetPendingLaunchArgumentsPath()
{
    return GetUserPath() / "pending_launch_arguments.txt";
}

void App::Restart(std::vector<std::string> restartArgs)
{
#ifdef UNLEASHED_RECOMP_IOS
    // iOS apps cannot launch a new instance of themselves. Save the arguments so the
    // next launch picks them up (e.g. to open the DLC installer) and let the user reopen the app.
    {
        std::ofstream file(GetPendingLaunchArgumentsPath(), std::ios::trunc);
        for (auto& arg : restartArgs)
            file << arg << '\n';

        if (!file)
            LOGN_ERROR("Failed to save the arguments for the next launch.");
    }

    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, GameWindow::GetTitle(), Localise("System_iOS_ReopenRequired").c_str(), GameWindow::s_pWindow);
#else
    os::process::StartProcess(os::process::GetExecutablePath(), restartArgs, os::process::GetWorkingDirectory());
#endif

    Exit();
}

std::vector<std::string> App::ConsumePendingLaunchArguments()
{
    std::vector<std::string> args;
    std::filesystem::path path = GetPendingLaunchArgumentsPath();

    std::error_code ec;
    if (!std::filesystem::exists(path, ec))
        return args;

    {
        std::ifstream file(path);
        std::string line;
        while (std::getline(file, line))
        {
            if (!line.empty())
                args.push_back(line);
        }
    }

    std::filesystem::remove(path, ec);
    return args;
}

void App::Exit()
{
    Config::Save();

#ifdef _WIN32
    timeEndPeriod(1);
#endif

    std::_Exit(0);
}

// SWA::CApplication::CApplication
PPC_FUNC_IMPL(__imp__sub_824EB490);
PPC_FUNC(sub_824EB490)
{
    App::s_isInit = true;
    App::s_isMissingDLC = !Installer::checkAllDLC(GetGamePath());
    App::s_language = Config::Language;

    SWA::SGlobals::Init();
    Registry::Save();

    __imp__sub_824EB490(ctx, base);
}

static std::thread::id g_mainThreadId = std::this_thread::get_id();

// SWA::CApplication::Update
PPC_FUNC_IMPL(__imp__sub_822C1130);
PPC_FUNC(sub_822C1130)
{
    Video::WaitOnSwapChain();

    // Correct small delta time errors.
    if (Config::FPS >= FPS_MIN && Config::FPS < FPS_MAX)
    {
        double targetDeltaTime = 1.0 / Config::FPS;

        if (abs(ctx.f1.f64 - targetDeltaTime) < 0.00001)
            ctx.f1.f64 = targetDeltaTime;
    }

    App::s_deltaTime = ctx.f1.f64;
    App::s_time += App::s_deltaTime;

    // This function can also be called by the loading thread,
    // which SDL does not like. To prevent the OS from thinking
    // the process is unresponsive, we will flush while waiting
    // for the pipelines to finish compiling in video.cpp.
    if (std::this_thread::get_id() == g_mainThreadId)
    {
        SDL_PumpEvents();
        SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
        GameWindow::Update();
    }

    AudioPatches::Update(App::s_deltaTime);
    InspirePatches::Update();

    // Apply subtitles option.
    if (auto pApplicationDocument = SWA::CApplicationDocument::GetInstance())
        pApplicationDocument->m_InspireSubtitles = Config::Subtitles;

    if (Config::EnableEventCollisionDebugView)
        *SWA::SGlobals::ms_IsTriggerRender = true;

    if (Config::EnableGIMipLevelDebugView)
        *SWA::SGlobals::ms_VisualizeLoadedLevel = true;

    if (Config::EnableObjectCollisionDebugView)
        *SWA::SGlobals::ms_IsObjectCollisionRender = true;

    if (Config::EnableStageCollisionDebugView)
        *SWA::SGlobals::ms_IsCollisionRender = true;

    __imp__sub_822C1130(ctx, base);
}

