#include <stdafx.h>
#ifdef __x86_64__
#include <cpuid.h>
#endif
#include <cpu/guest_thread.h>
#include <gpu/video.h>
#include <kernel/function.h>
#include <kernel/memory.h>
#include <kernel/heap.h>
#include <kernel/xam.h>
#include <kernel/io/file_system.h>
#include <file.h>
#include <xex.h>
#include <app.h>
#include <apu/audio.h>
#include <hid/hid.h>
#include <user/config.h>
#include <user/paths.h>
#include <user/persistent_storage_manager.h>
#include <user/registry.h>
#include <kernel/xdbf.h>
#include <install/installer.h>
#include <install/update_checker.h>
#include <os/logger.h>
#include <os/process.h>
#include <os/registry.h>
#include <ui/game_window.h>
#include <ui/installer_wizard.h>
#include <mod/mod_loader.h>
#include <preload_executable.h>
#include <SDL.h>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#ifdef _WIN32
#include <timeapi.h>
#endif

#if defined(_WIN32) && defined(UNLEASHED_RECOMP_D3D12)
static std::array<std::string_view, 3> g_D3D12RequiredModules =
{
    "D3D12/D3D12Core.dll",
    "dxcompiler.dll",
    "dxil.dll"
};
#endif

const size_t XMAIOBegin = 0x7FEA0000;
const size_t XMAIOEnd = XMAIOBegin + 0x0000FFFF;

Memory g_memory;
Heap g_userHeap;
XDBFWrapper g_xdbfWrapper;
std::unordered_map<uint16_t, GuestTexture*> g_xdbfTextureCache;

void HostStartup()
{
#ifdef _WIN32
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
#endif

    hid::Init();
}

// Name inspired from nt's entry point
void KiSystemStartup()
{
    if (g_memory.base == nullptr)
    {
        LOGN_ERROR("Failed to reserve the 4 GiB guest memory space.");
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, GameWindow::GetTitle(), Localise("System_MemoryAllocationFailed").c_str(), GameWindow::s_pWindow);
        std::_Exit(1);
    }

    LOGFN("Guest memory base: {}", static_cast<void*>(g_memory.base));
    g_userHeap.Init();

    const auto gameContent = XamMakeContent(XCONTENTTYPE_RESERVED, "Game");
    const auto updateContent = XamMakeContent(XCONTENTTYPE_RESERVED, "Update");
    const std::string gamePath = (const char*)(GetGamePath() / "game").u8string().c_str();
    const std::string updatePath = (const char*)(GetGamePath() / "update").u8string().c_str();
    XamRegisterContent(gameContent, gamePath);
    XamRegisterContent(updateContent, updatePath);

    const auto saveFilePath = GetSaveFilePath(true);
    bool saveFileExists = std::filesystem::exists(saveFilePath);

    if (!saveFileExists)
    {
        // Copy base save data to modded save as fallback.
        std::error_code ec;
        std::filesystem::create_directories(saveFilePath.parent_path(), ec);

        if (!ec)
        {
            std::filesystem::copy_file(GetSaveFilePath(false), saveFilePath, ec);
            saveFileExists = !ec;
        }
    }

    if (saveFileExists)
    {
        std::u8string savePathU8 = saveFilePath.parent_path().u8string();
        XamRegisterContent(XamMakeContent(XCONTENTTYPE_SAVEDATA, "SYS-DATA"), (const char*)(savePathU8.c_str()));
    }

    // Mount game
    XamContentCreateEx(0, "game", &gameContent, OPEN_EXISTING, nullptr, nullptr, 0, 0, nullptr);
    XamContentCreateEx(0, "update", &updateContent, OPEN_EXISTING, nullptr, nullptr, 0, 0, nullptr);

    // OS mounts game data to D:
    XamContentCreateEx(0, "D", &gameContent, OPEN_EXISTING, nullptr, nullptr, 0, 0, nullptr);

    std::error_code ec;
    for (auto& file : std::filesystem::directory_iterator(GetGamePath() / "dlc", ec))
    {
        if (file.is_directory())
        {
            std::u8string fileNameU8 = file.path().filename().u8string();
            std::u8string filePathU8 = file.path().u8string();
            XamRegisterContent(XamMakeContent(XCONTENTTYPE_DLC, (const char*)(fileNameU8.c_str())), (const char*)(filePathU8.c_str()));
        }
    }

    XAudioInitializeSystem();
}

uint32_t LdrLoadModule(const std::filesystem::path &path)
{
    auto loadResult = LoadFile(path);
    if (loadResult.empty())
    {
        LOGFN_ERROR("Failed to load module: {}", (const char*)path.u8string().c_str());
        return 0;
    }

    auto* header = reinterpret_cast<const Xex2Header*>(loadResult.data());
    auto* security = reinterpret_cast<const Xex2SecurityInfo*>(loadResult.data() + header->securityOffset);
    const auto* fileFormatInfo = reinterpret_cast<const Xex2OptFileFormatInfo*>(getOptHeaderPtr(loadResult.data(), XEX_HEADER_FILE_FORMAT_INFO));
    auto entry = *reinterpret_cast<const uint32_t*>(getOptHeaderPtr(loadResult.data(), XEX_HEADER_ENTRY_POINT));
    ByteSwapInplace(entry);

    auto srcData = loadResult.data() + header->headerSize;
    auto destData = reinterpret_cast<uint8_t*>(g_memory.Translate(security->loadAddress));

    if (fileFormatInfo->compressionType == XEX_COMPRESSION_NONE)
    {
        memcpy(destData, srcData, security->imageSize);
    }
    else if (fileFormatInfo->compressionType == XEX_COMPRESSION_BASIC)
    {
        auto* blocks = reinterpret_cast<const Xex2FileBasicCompressionBlock*>(fileFormatInfo + 1);
        const size_t numBlocks = (fileFormatInfo->infoSize / sizeof(Xex2FileBasicCompressionInfo)) - 1;

        for (size_t i = 0; i < numBlocks; i++)
        {
            memcpy(destData, srcData, blocks[i].dataSize);

            srcData += blocks[i].dataSize;
            destData += blocks[i].dataSize;

            memset(destData, 0, blocks[i].zeroSize);
            destData += blocks[i].zeroSize;
        }
    }
    else
    {
        assert(false && "Unknown compression type.");
    }

    auto res = reinterpret_cast<const Xex2ResourceInfo*>(getOptHeaderPtr(loadResult.data(), XEX_HEADER_RESOURCE_INFO));

    g_xdbfWrapper = XDBFWrapper((uint8_t*)g_memory.Translate(res->offset.get()), res->sizeOfData);

    return entry;
}

#ifdef __x86_64__
__attribute__((constructor(101), target("no-avx,no-avx2"), noinline))
void init()
{
    uint32_t eax, ebx, ecx, edx;

    // Execute CPUID for processor info and feature bits.
    __get_cpuid(1, &eax, &ebx, &ecx, &edx);

    // Check for AVX support.
    if ((ecx & (1 << 28)) == 0)
    {
        printf("[*] CPU does not support the AVX instruction set.\n");

#ifdef _WIN32
        MessageBoxA(nullptr, "Your CPU does not meet the minimum system requirements.", "Unleashed Recompiled", MB_ICONERROR);
#endif

        std::_Exit(1);
    }
}
#endif

int main(int argc, char *argv[])
{
#ifdef _WIN32
    timeBeginPeriod(1);
#endif

    os::process::CheckConsole();

    if (!os::registry::Init())
        LOGN_WARNING("OS does not support registry.");

    os::logger::Init();

    PreloadContext preloadContext;
    preloadContext.PreloadExecutable();

    bool forceInstaller = false;
    bool forceDLCInstaller = false;
    bool useDefaultWorkingDirectory = false;
    bool forceInstallationCheck = false;
    bool graphicsApiRetry = false;
    const char *sdlVideoDriver = nullptr;

    std::vector<std::string> args(argv + 1, argv + argc);

    // Arguments of a restart the app could not perform by itself (e.g. on iOS).
    for (auto& arg : App::ConsumePendingLaunchArguments())
    {
        LOGFN("Applying launch argument from the previous session: {}", arg);
        args.push_back(arg);
    }

    for (size_t i = 0; i < args.size(); i++)
    {
        forceInstaller = forceInstaller || (args[i] == "--install");
        forceDLCInstaller = forceDLCInstaller || (args[i] == "--install-dlc");
        useDefaultWorkingDirectory = useDefaultWorkingDirectory || (args[i] == "--use-cwd");
        forceInstallationCheck = forceInstallationCheck || (args[i] == "--install-check");
        graphicsApiRetry = graphicsApiRetry || (args[i] == "--graphics-api-retry");

        if (args[i] == "--sdl-video-driver")
        {
            if ((i + 1) < args.size())
                sdlVideoDriver = args[++i].c_str();
            else
                LOGN_WARNING("No argument was specified for --sdl-video-driver. Option will be ignored.");
        }
    }

    if (!useDefaultWorkingDirectory)
    {
        // Set the current working directory to the executable's path.
        std::error_code ec;
        std::filesystem::current_path(os::process::GetExecutableRoot(), ec);
    }

    Config::Load();

    LOGFN("Resolved user path: {}", (const char*)GetUserPath().u8string().c_str());
    LOGFN("Resolved game path: {}", (const char*)GetGamePath().u8string().c_str());

    if (forceInstallationCheck)
    {
        // Create the console to show progress to the user, otherwise it will seem as if the game didn't boot at all.
        os::process::ShowConsole();

        Journal journal;
        double lastProgressMiB = 0.0;
        double lastTotalMib = 0.0;
        Installer::checkInstallIntegrity(GAME_INSTALL_DIRECTORY, journal, [&]()
        {
            constexpr double MiBDivisor = 1024.0 * 1024.0;
            constexpr double MiBProgressThreshold = 128.0;
            double progressMiB = double(journal.progressCounter) / MiBDivisor;
            double totalMiB = double(journal.progressTotal) / MiBDivisor;
            if (journal.progressCounter > 0)
            {
                if ((progressMiB - lastProgressMiB) > MiBProgressThreshold)
                {
                    fprintf(stdout, "Checking files: %0.2f MiB / %0.2f MiB\n", progressMiB, totalMiB);
                    lastProgressMiB = progressMiB;
                }
            }
            else
            {
                if ((totalMiB - lastTotalMib) > MiBProgressThreshold)
                {
                    fprintf(stdout, "Scanning files: %0.2f MiB\n", totalMiB);
                    lastTotalMib = totalMiB;
                }
            }

            return true;
        });

        char resultText[512];
        uint32_t messageBoxStyle;
        if (journal.lastResult == Journal::Result::Success)
        {
            snprintf(resultText, sizeof(resultText), "%s", Localise("IntegrityCheck_Success").c_str());
            fprintf(stdout, "%s\n", resultText);
            messageBoxStyle = SDL_MESSAGEBOX_INFORMATION;
        }
        else
        {
            snprintf(resultText, sizeof(resultText), Localise("IntegrityCheck_Failed").c_str(), journal.lastErrorMessage.c_str());
            fprintf(stderr, "%s\n", resultText);
            messageBoxStyle = SDL_MESSAGEBOX_ERROR;
        }

        SDL_ShowSimpleMessageBox(messageBoxStyle, GameWindow::GetTitle(), resultText, GameWindow::s_pWindow);
        std::_Exit(int(journal.lastResult));
    }

#if defined(_WIN32) && defined(UNLEASHED_RECOMP_D3D12)
    for (auto& dll : g_D3D12RequiredModules)
    {
        if (!std::filesystem::exists(g_executableRoot / dll))
        {
            char text[512];
            snprintf(text, sizeof(text), Localise("System_Win32_MissingDLLs").c_str(), dll.data());
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, GameWindow::GetTitle(), text, GameWindow::s_pWindow);
            std::_Exit(1);
        }
    }
#endif

    // Check the time since the last time an update was checked. Store the new time if the difference is more than six hours.
    constexpr double TimeBetweenUpdateChecksInSeconds = 6 * 60 * 60;
    time_t timeNow = std::time(nullptr);
    double timeDifferenceSeconds = difftime(timeNow, Config::LastChecked);
    if (timeDifferenceSeconds > TimeBetweenUpdateChecksInSeconds)
    {
        UpdateChecker::initialize();
        UpdateChecker::start();
        Config::LastChecked = timeNow;
        Config::Save();
    }

    if (Config::ShowConsole)
        os::process::ShowConsole();

    HostStartup();

    const std::filesystem::path gameRoot = GetGamePath();
    const std::filesystem::path patchedExecutablePath = gameRoot / "patched" / "default.xex";
    const std::filesystem::path updatePatchPath = gameRoot / "update" / "default.xexp";
    const std::filesystem::path gameExecutablePath = gameRoot / "game" / "default.xex";
    const bool hasPatchedExecutable = std::filesystem::exists(patchedExecutablePath);
    const bool hasUpdatePatch = std::filesystem::exists(updatePatchPath);
    const bool hasGameExecutable = std::filesystem::exists(gameExecutablePath);
    LOGFN("Install files present - patched/default.xex: {}, update/default.xexp: {}, game/default.xex: {}", hasPatchedExecutable, hasUpdatePatch, hasGameExecutable);

    std::filesystem::path modulePath;
    bool isGameInstalled = Installer::checkGameInstall(gameRoot, modulePath);

#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
    constexpr bool isMobileSetup = true;
#else
    constexpr bool isMobileSetup = false;
#endif

    if (isMobileSetup)
    {
        // Create the folders players copy their game files into, so they show up in the Files app.
        std::error_code ec;
        std::filesystem::create_directories(gameRoot / "game", ec);
        std::filesystem::create_directories(gameRoot / "update", ec);
        std::filesystem::create_directories(gameRoot / "dlc", ec);
    }

    // The game and update folders were copied in by hand (e.g. through the Files app): finish the setup without the installer.
    if (!isGameInstalled && !forceInstaller && hasGameExecutable && hasUpdatePatch)
    {
        LOGN("Found copied game files, creating patched executable.");

        Journal journal;
        if (Installer::setupCopiedFiles(gameRoot, journal))
        {
            isGameInstalled = Installer::checkGameInstall(gameRoot, modulePath);
        }
        else
        {
            LOGFN_ERROR("Setup from copied game files failed: {}", journal.lastErrorMessage);

            if (isMobileSetup)
            {
                std::string message = "The game files in the UnleashedRecomp folder could not be set up.\n\n" + journal.lastErrorMessage +
                    "\n\nCopy the full contents of your game into the \"game\" folder and the title update into the \"update\" folder, then open the app again.";

                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, GameWindow::GetTitle(), message.c_str(), GameWindow::s_pWindow);
                std::_Exit(1);
            }
        }
    }

    if (isMobileSetup && !isGameInstalled && !forceInstaller)
    {
        const SDL_MessageBoxButtonData buttons[] =
        {
            { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Close" },
            { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Open Installer" }
        };

        SDL_MessageBoxData messageBox{};
        messageBox.flags = SDL_MESSAGEBOX_INFORMATION;
        messageBox.window = GameWindow::s_pWindow;
        messageBox.title = GameWindow::GetTitle();
        messageBox.message =
            "Game files not found.\n\n"
            "Open the Files app and go to On My iPhone > Unleashed > UnleashedRecomp. Copy your game files into the \"game\" folder, "
            "the title update files into the \"update\" folder and any DLC folders into the \"dlc\" folder, then open the app again.";
        messageBox.numbuttons = SDL_arraysize(buttons);
        messageBox.buttons = buttons;

        int buttonId = 0;
        if (SDL_ShowMessageBox(&messageBox, &buttonId) != 0 || buttonId != 1)
            std::_Exit(0);
    }
    bool runInstallerWizard = forceInstaller || forceDLCInstaller || !isGameInstalled;
    LOGFN("Install state - gameInstalled: {}, runInstallerWizard: {}, candidateModulePath: {}", isGameInstalled, runInstallerWizard, (const char*)modulePath.u8string().c_str());
    if (runInstallerWizard)
    {
        if (!Video::CreateHostDevice(sdlVideoDriver, graphicsApiRetry))
        {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, GameWindow::GetTitle(), Localise("Video_BackendError").c_str(), GameWindow::s_pWindow);
            std::_Exit(1);
        }

        if (!InstallerWizard::Run(GetGamePath(), isGameInstalled && forceDLCInstaller))
        {
            std::_Exit(0);
        }

        isGameInstalled = Installer::checkGameInstall(gameRoot, modulePath);
        LOGFN("Post-installer state - gameInstalled: {}, modulePath: {}", isGameInstalled, (const char*)modulePath.u8string().c_str());
        if (!isGameInstalled)
        {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, GameWindow::GetTitle(), "Install data is still incomplete after installer. Ensure game/update sources are selected and installation finishes.", GameWindow::s_pWindow);
            std::_Exit(1);
        }
    }

    ModLoader::Init();

    if (!PersistentStorageManager::LoadBinary())
        LOGFN_ERROR("Failed to load persistent storage binary... (status code {})", (int)PersistentStorageManager::BinStatus);

    LOGN("Starting guest system initialization.");
    KiSystemStartup();

    LOGFN("Loading module: {}", (const char*)modulePath.u8string().c_str());
    uint32_t entry = LdrLoadModule(modulePath);
    if (entry == 0)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, GameWindow::GetTitle(), "Failed to load game executable (patched/default.xex). Re-run installer and verify game/update files.", GameWindow::s_pWindow);
        std::_Exit(1);
    }

    if (!runInstallerWizard)
    {
        if (!Video::CreateHostDevice(sdlVideoDriver, graphicsApiRetry))
        {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, GameWindow::GetTitle(), Localise("Video_BackendError").c_str(), GameWindow::s_pWindow);
            std::_Exit(1);
        }
    }

    Video::StartPipelinePrecompilation();

    LOGFN("Starting guest thread at entry: 0x{:08X}", entry);
    GuestThread::Start({ entry, 0, 0 });

    return 0;
}

GUEST_FUNCTION_STUB(__imp__vsprintf);
GUEST_FUNCTION_STUB(__imp___vsnprintf);
GUEST_FUNCTION_STUB(__imp__sprintf);
GUEST_FUNCTION_STUB(__imp___snprintf);
GUEST_FUNCTION_STUB(__imp___snwprintf);
GUEST_FUNCTION_STUB(__imp__vswprintf);
GUEST_FUNCTION_STUB(__imp___vscwprintf);
GUEST_FUNCTION_STUB(__imp__swprintf);
