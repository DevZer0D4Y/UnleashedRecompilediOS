<p align="center">
    <img src="https://raw.githubusercontent.com/hedge-dev/UnleashedRecompResources/refs/heads/main/images/logo/Logo.png" width="512"/>
</p>

---

UnleashedRecompiled for iOS is an iOS port of the Xbox 360 version of Sonic Unleashed created through the process of static recompilation. The port offers iOS support with numerous built-in enhancements such as high resolutions, ultrawide support, high frame rates, improved performance and modding.

**This project does not include any game assets. You must provide the files from your own legally acquired copy of the game to install or build UnleashedRecompiled for iOS.**


[XenonRecomp](https://github.com/hedge-dev/XenonRecomp) and [XenosRecomp](https://github.com/hedge-dev/XenosRecomp) are the main recompilers used for converting the game's original PowerPC code and Xenos shaders into compatible C++ and HLSL code respectively.

If you want to support me and remain up-to-date about this and other projects, you can in different ways:

Ko-Fi: https://ko-fi.com/dev_zer0

Discord: https://discord.gg/uFChheZEWX

YouTube: https://www.youtube.com/@develop_erZ

## Table of Contents

- [Minimum System Requirements](#minimum-system-requirements)
- [How to Install](#how-to-install)
    - [Installing on iPhone/iPad without a PC](#installing-on-iphoneipad-without-a-pc)
- [How do I change the game's language?](#how-do-i-change-the-games-language)
- [What's New in This Port](#whats-new-in-this-port)
- [Features](#features)
- [Credits](#credits)

## Minimum System Requirements

- Device:
  - iPhone or iPad with a 64-bit Apple chip and Metal support.
- Memory:
  - 4 GB of RAM minimum.
  - 6 GB recommended (e.g. iPhone 13 Pro, iPhone 14 or newer), as 4 GB devices are more likely to be closed by iOS when memory runs low.
- Operating System:
  - iOS / iPadOS 15.0 or newer.
- Controls:
  - Built-in touch controls, or an MFi, Xbox or PlayStation controller (recommended). See [the controls FAQ](#what-are-the-controls-on-ios).
- Storage:
  - With DLC: 10 GiB required
  - Without DLC: 6 GiB required

> [!NOTE]
> Extra space is needed for the app itself, and temporarily for any `.zip` or other archive you copy to your device before extracting the game files.

## How to Install

1) You must have access to the following:

    - Xbox 360 (modifications not necessary)
    - Xbox 360 Storage Device (either an Xbox 360 hard drive or an external USB storage device)
    - Xbox 360 Hard Drive Transfer Cable or a compatible SATA to USB adapter (only required for dumping from an Xbox 360 hard drive)
    - Sonic Unleashed for Xbox 360 (US or EU, **JP is not supported**)
        - Retail Disc or Digital Copy (can be purchased and downloaded from the [Xbox Store](https://www.xbox.com/en-US/games/store/sonic-unleashed/c098fgnmts8f)).
        - Title Update required.
        - All available DLC (Adventure Packs) are optional, but **highly recommended**. **The DLC includes high quality lighting for the entire game**.

> [!TIP]
> If you do not have the Xbox 360 Hard Drive Transfer Cable, please ensure that you purchase the correct revision of it for your console.
>
> The latest revision works with both original Xbox 360 and Xbox 360 S|E hard drives, but the first revision only works with original Xbox 360 hard drives.
>
> To know which is which, the first revision cable is gray, whereas the latest revision (which supports any Xbox 360 hard drive) is black.

2) **Before proceeding with the installation**, make sure to follow the guide on how to acquire the game files from your Xbox 360.

    - Xbox 360 Hard Drive Dumping Guide
        - [English](/docs/DUMPING-en.md)
    - Xbox 360 USB Dumping Guide
        - [English](/docs/DUMPING-USB-en.md)

3) Download [the latest release](https://github.com/DevZer0D4Y/UnleashedRecompilediOS//releases/latest) of UnleashedRecompiled for iOS and extract it to where you'd like the game to be installed.

4) Run the executable and you will be guided through the installation process. You will be asked to provide the files you acquired in the previous step. When presented with options for how to do this:

    - **Add Files** will only allow you to provide **containers or images dumped from an Xbox 360**. These often come in the form of very large files without associated extensions. Don't worry if you're not aware of what's inside of them, the installer will automatically detect what type of content is inside the container.

    - **Add Folder** will only allow you to provide a **directory with the game's raw files** corresponding to the piece of content that is requested. **It will NOT scan your folder for compatible content!**

### Installing on iPhone/iPad without a PC

1) Install the `.ipa` from [the latest release](https://github.com/DevZer0D4Y/UnleashedRecompilediOS/releases/latest) with your sideloading tool of choice and open the app once. It will tell you that no game files were found and create its folders.

2) Open the **Files** app and go to **On My iPhone** (or **On My iPad**) → **Unleashed** → **UnleashedRecomp**. This is the same folder that holds the settings and `.toml` files.

3) Copy your extracted game files into it so it looks like this:

    ```
    UnleashedRecomp/
    ├── game/      <- the base game's files (default.xex, ...)
    ├── update/    <- the title update's files (default.xexp, ...)
    └── dlc/       <- optional, one folder per DLC (each containing DLC.xml)
    ```

    DLC folders can have any name, they'll be renamed to what the game expects automatically.

4) Open the app again. It will check the files, create the patched executable and start the game, no installer needed.

> [!NOTE]
> Please note that it is **not possible** to complete the installation if your files have been **modified**. In case of other problems such as black screens or crashes, **do not try to reinstall the game** as it is not possible for the process to result in an invalid installation.

## What's New in This Port

A list of everything fixed and added in this iOS port, on top of the original Unleashed Recompiled.

### New Features

| Feature | What it does |
|---|---|
| **Install without a PC** | Copy your `game`, `update` and `dlc` folders into the app's folder with the Files app and open the game. It checks the files, sets itself up and starts, with no installer screens. DLC folders can have any name. See [Installing on iPhone/iPad without a PC](#installing-on-iphoneipad-without-a-pc). |
| **Touch controls** | An on-screen gamepad modelled on [XeniOS](https://github.com/xenios-jp/XeniOS): a stick with a D-Pad ring, swipe anywhere to move the camera, all face, shoulder and trigger buttons, and haptic feedback. They hide when you use a controller and come back when you touch the screen. See [What are the controls on iOS?](#what-are-the-controls-on-ios). |
| **Touch layout editor** | Tap **EDIT** to move, resize and hide controls and change their transparency. Layouts are saved to `touch_layout.toml`. |
| **Your phone's language on first launch** | The game starts in your iPhone's language if it's supported, instead of always starting in English. |
| **Up to 120 Hz** | High refresh rates are unlocked on ProMotion iPhones and iPads. |

### iOS Fixes

| Problem | Fix |
|---|---|
| **Stuck after the installer** | The game could hang forever when the installer handed over to the game. The wait on the graphics card now uses its own dedicated path, so it no longer hangs. |
| **Restarting the game did nothing** | iOS apps can't relaunch themselves. The game now saves its launch settings, asks you to reopen it, and picks them up on the next start. |
| **Broken textures** | Most iPhones can't read the Xbox's compressed texture formats (BC1 to BC7). They're now converted while loading, on devices that need it. The converters were checked against reference decoders. |
| **Memory growing during long sessions** | Freed memory is handed back to iOS, and temporary graphics objects that used to pile up are cleaned up. A warning is logged when memory runs low. |
| **Random freezes** | Some thread synchronisation code could fail at random on Apple's ARM chips and leave the game stuck. It now retries properly. |
| **Going to the background** | Rendering pauses while the app is in the background and resumes when you come back, as iOS doesn't allow graphics work in the background. Ported from upstream. |
| **Crashes on older and 4 GB devices** | The app requests Apple's Extended Virtual Addressing and Increased Memory Limit permissions, needed for the game's 4 GB memory reservation and to give low-RAM devices more room. |
| **Wrong button prompts** | Touching the screen no longer switches the button prompts to keyboard and mouse icons. |

### Graphics Backend (plume)

| Change | Details |
|---|---|
| **iOS build support** | The Metal backend uses [a fork of plume](https://github.com/DevZer0D4Y/plume/tree/ios-unleashed) that builds for iOS. |
| **Metal memory leaks** | Fixed, ported from upstream plume. |
| **Buffer overrun** | Fixed, ported from upstream plume. |
| **iOS versions older than 16** | A Metal feature only available on iOS 16 and newer is no longer used on older versions. |

### Fixes From Upstream Unleashed Recompiled

| Problem | Fix |
|---|---|
| **Werehog rotating into walls at high frame rates** | Fixed when leaving walls at frame rates above 60 FPS. |
| **Rumble too strong** | Controller rumble strength is no longer multiplied. |

### Documentation

| Change | Details |
|---|---|
| **New guides** | Installing without a PC, the touch controls and their editor, and changing the language. |
| **System requirements** | Corrected for iPhone and iPad. |

### Can't Be Fixed From This Project

| Issue | Why |
|---|---|
| **Stutters the first time effects appear** | They come from the game engine itself. |
| **Some high frame rate glitches** | The known ones are fixed; others need specific reports to track down. |
| **Ultrawide cutscene issues** | The cutscenes were made for 16:9 and are locked to it by default. |
| **Bugs in the original game** | The port keeps the original game's behaviour. |
| **Japanese copies of the game** | They would need their own recompilation. US and EU copies both work. |

## Features

### Easy to Use Installer

A built-in installation wizard will guide you through the process of installing the game with many integrity checks to ensure the process goes as smoothly as possible. The installer can also be accessed from the title screen, if you wish to add the DLC at a later time.

### Options Menu

A completely new options menu accessible from the title screen or pause menu, with an unprecedented level of fidelity to the game's design language. Get access to many quality of life features and graphics options directly within the game with full controller navigation.

### Achievements

You will be rewarded with achievements as you progress through the game just like on its original platform. Achievements are recreated with integrated notifications and a new menu also very faithful to the game's design language. Get all of them and you will be rewarded with a gold trophy!

### Custom Localization

All of the new menus in the port fully support localization for each of the game's originally supported languages; English, Japanese, German, French, Spanish and Italian. As a bonus, switching the game to Japanese also changes the title screen logo to its original Japanese counterpart.

### High Fidelity

Special care and attention was taken to recreate the game's visuals as accurately as possible and was always compared to the game running on the original hardware. The colors are as vibrant as those of the PlayStation 3 version, which did not use the color correction filter present in the Xbox 360 version, as was originally intended. Nevertheless, an option to recreate the original warm filter from the Xbox 360 version has been included as an option.

### High Resolution Enhancements

Many improvements have been provided to accompany support for higher resolutions:

- Multisample Anti-Aliasing (MSAA) levels beyond the 2x used by the original game.
- Higher quality Depth of Field (DoF), an effect which commonly breaks in emulators when increasing the game's resolution. The original effect's formula was reverse-engineered and versions with 5x5, 7x7 and 9x9 taps were added based on the target resolution.
- An "Enhanced" Motion Blur option which uses more samples than the original for a smoother blur.
- Support for "Alpha to Coverage" Anti-Aliasing to enhance the visual result of transparent textures with hard edges.
- A Bicubic Texture Filtering option which greatly enhances the visual result of Global Illumination textures.
- A more precise implementation of the game's reverse Z technique, eliminating most Z-fighting issues and fixing the jittery motion blur in stages like Jungle Joyride.

### High Performance Renderer

A new renderer was written from scratch to translate the game's draw calls to modern APIs in a highly efficient way while also taking advantage of multi-threading.

As emulation of the Xbox 360's GPU is not required in a recompilation, many decisions were made to skip quirks of the original hardware that are not required in an iOS port, resulting in great improvements to performance.

Modern rendering techniques such as bindless textures and shader specialization are also used to maximize the performance on systems with modern hardware whilst also supporting a wide range of lower end hardware.

Multiple optimizations were made to the game's shadow map rendering and special techniques were developed to automatically detect and skip unnecessary texture copies that are no longer needed in modern APIs.

Additionally, to support the game's extensive use of asset streaming, parallel transfer queues are leveraged, a feature only available in modern APIs that enable efficient use of available PCI-E bandwidth.

### High Frame Rate Support

The game's frame rate cap has been increased by default to 60 FPS, with support for higher targets and unlocked frame rate being available from the options menu. A vast amount of glitches that usually occur at higher frame rates have been fixed and are included as part of the recompilation.

> [!NOTE]
> While the game is considered to be beatable at frame rates higher than 60 FPS, please note that [some issues](#high-frame-rate-glitches) can still occur. Some of these issues may be addressed in future updates.

### Ultrawide Support

Aspect ratios for ultrawide displays (such as 21:9 or even wider) are supported out of the box, with options to adjust the alignment of the UI to the edges or to the original 16:9 safe area, if desired. No external codes are required!

> [!NOTE]
> By default, cutscenes are locked to their original aspect ratio to prevent [presentation issues](#ultrawide-visual-issues), as the game was not designed to present these scenes with wider aspect ratios. However, you can use the included option to unlock this feature if you don't mind these issues.

### Extended Controller Features

Support for the D-Pad has been added to various parts of the game, allowing the full game to be completed using it over the analog stick, if you so desire.

If you have a DualShock 4 or DualSense controller, the LED will dynamically change color depending on the game context and support for the touchpad has been added to the World Map, allowing you to spin the planet freely!

### Low Input Latency

Modern input latency reduction techniques are included to improve the game's responsiveness as much as possible.

### Asynchronous Shader Compilation

One of the biggest improvements that the recompilation features over emulators is the fact that Pipeline Compilation (commonly known as "Shader Compilation") is directly integrated into the game as part of the asset loading process. This means there's **no stutters during gameplay the first time new objects or effects appear**.

The renderer will traverse the game's rendering structures and automatically determine what pipelines must be compiled before it considers the asset is loaded. As a result, pipeline compilation is performed in parallel as part of the game's background workers that take care of streaming in the assets, either during regular gameplay or as part of the game's loading screens. This is an improvement that'd never be possible with an emulator, as it requires direct modification of the game to implement this feature.

While this system is very extensive, some special shaders such as post-processing effects or 2D elements are not detected ahead of time, so a list of pre-determined pipelines are compiled during the game's boot sequence. However, the amount is so low that this is done completely in the background and it was determined that there was no need to show a shader compilation screen on most systems that were tested.

> [!NOTE]
> While stutters from shader compilation are non-existent, please be aware that [some stutters](#unavoidable-stutters) may be encountered due to the way the game was programmed. If you encounter these, please keep in mind that **these are not related to shader compilation**. Some of these issues may be addressed in future updates.

### Support for Xbox and PlayStation Controller Icons

You can freely choose whether to use Xbox 360 or PlayStation 3 controller icons. By default, the game will automatically detect which to use based on your controller, but you can select a different option based on your personal preference from within the options menu.

Game objects that display controller icons such as Reaction Plates or Jump Selectors will automatically switch their textures to match the option in use. Even small details such as the Tornado Defense missions using different colors for the missiles have been accounted for.

### Quality of Life Options

Many options have been integrated to address some common quality of life improvements that were deemed to be essential to the port:

- Hint rings and other types of hints provided by the game during exploration or boss fights can be disabled.
- Control tutorials (as referred to by current Sonic games) can be disabled to remove button prompts that show up during gameplay to teach the player how to use certain moves.
- The Werehog's Battle Theme, commonly considered to be an annoyance in the original game due to its frequency and interruption of the stage's background music, can now be disabled.
- The day/night transformation cutscene in towns can use either the Xbox 360 or PlayStation 3 version, with the Xbox version artificially extending loading times for the full video play out, whilst the PlayStation version ends as soon as it's done loading.
- Music Attenuation is a feature that was originally present in the Xbox 360 version of the game, where it'd automatically mute the background music if the console's media player was in use.

### Why does the installer say my files are invalid?

The installer may display this error for several reasons. Please check the following to ensure your files are valid:

- Please read the [How to Install](#how-to-install) section and make sure you've acquired all of the necessary files correctly.

- Verify that you're not trying to add compressed files such as `.zip`, `.7z`, `.rar` or other formats.

- Only use the **Add Folder** option if you're sure you have a directory with the content's files already extracted, which means it'll only contain files like `.xex`, `.ar.00`, `.arl` and others. **This option will not scan your folder for compatible content**.

- Ensure that the files you've acquired correspond to the same region. **Discs and Title Updates from different regions can't be used together** and will fail to generate a patch.

- The installer will only accept **original and unmodified files**. Do not attempt to provide modified files to the installer.

### What are the controls on iOS?

The game has on-screen touch controls, laid out like the ones in XeniOS:

- **Left stick:** drag inside the circle on the bottom left. Tap the arrows on its outer ring for the D-Pad.
- **Camera (right stick):** swipe anywhere on the screen that isn't a button.
- **A, B, X, Y:** the diamond of buttons on the right.
- **LT, RT:** the wide buttons above the stick and above the face buttons.
- **LB, RB, BACK, START:** along the top of the screen.

A controller (MFi, Xbox or PlayStation) is still the best way to play. The touch controls hide themselves as soon as you use a controller, and come back when you touch the screen.

#### Customising the touch controls

Pause the game, then tap **EDIT** at the top of the screen to open the layout editor:

- **Move** a control by dragging it.
- **Resize** it by tapping it to select it, then using **SIZE -** and **SIZE +**.
- **Hide** a control you don't need by selecting it and tapping **HIDE**. Hidden controls stay faintly visible in the editor, so you can select them and tap **SHOW** to bring them back.
- **ALPHA -** and **ALPHA +** make all the controls more or less see-through.
- **RESET** restores the default layout.
- **DONE** saves your layout and goes back to the game.

Your layout is saved to `touch_layout.toml`, in the same folder as `config.toml`. Delete that file to go back to the default layout.

You can change them in `config.toml` (see [How do I change the game's language?](#how-do-i-change-the-games-language) for where to find it), under `[Input]`:

```toml
[Input]
TouchControls = false       # turn the touch controls off completely
TouchControlsOpacity = 0.5  # from 0.0 (invisible) to 1.0 (default)
```

### How do I change the game's language?

On first launch, the game uses your iPhone's language if it's one of the supported ones (English, Japanese, German, French, Spanish or Italian), and English otherwise. You can switch the text and voices at any time, and your choice is saved.

**From the game:**

1) From the title screen or the pause menu, open **Options** → **System**.
2) Change **Language** for the text and menus: English, Japanese, German, French, Spanish or Italian.
3) Change **Voice Language** for the voice acting: English or Japanese.

**From the Files app (no controller needed):**

1) Launch the game once so it creates its settings file, then fully close it.
2) Open **Files** → **On My iPhone** (or **On My iPad**) → **Unleashed** → **UnleashedRecomp** and open `config.toml`.
3) Find the `[System]` section and change these lines, for example:

    ```toml
    [System]
    Language = "Spanish"
    VoiceLanguage = "Japanese"
    ```

    `Language` accepts `"English"`, `"Japanese"`, `"German"`, `"French"`, `"Spanish"` or `"Italian"`. `VoiceLanguage` accepts `"English"` or `"Japanese"`. Keep the quotes and the capital letter.

4) Save the file and open the game again.

> [!TIP]
> Setting the language to Japanese also changes the title screen logo to the original Japanese *Sonic World Adventure* logo.

### I want to update the game. How can I avoid losing my save data? Do I need to reinstall the game?

Updating the game can be done by simply installing the newer .ipa on top of your existing installation. **Your save data and configuration will not be lost.** You won't need to reinstall the game, as the game files will always remain the same across versions of UnleashedRecompiled for iOS.

### How can I force the game to store the save data and configuration in the installation folder?

You can make the game ignore the [default configuration paths](#where-is-the-save-data-and-configuration-file-stored) and force it to save everything in the installation directory by creating an empty `portable.txt` file. You are directly responsible for the safekeeping of your save data and configuration if you choose this option.

### Can I install the game with a PlayStation 3 copy?

**You cannot use the files from the PlayStation 3 version of the game.** Supporting these files would require an entirely new recompilation, as they have proprietary formatting that only works on PS3 and the code for these formats is only present in that version. All significant differences present in the PS3 version of the game have been included in this project as options.

### Can I install the game with a Japanese copy?

The Japanese version of Sonic Unleashed has some minor differences in both file structure and content that make this version of the game incompatible with the international release. Furthermore, the US and EU versions of the game already support Japanese. Supporting this version would only cause mod compatibility issues in the future, so it is unlikely to be added to the update roadmap as it would also require its own recompilation.

### What other platforms will be supported?

This project does not plan to support any more platforms other than iOS. Any contributors who wish to support more platforms should do so through my discord.

### Do you have plans to recompile other Xbox 360 games or Sonic games?

After Minecraft Xbox 360 Edition for iOS and Sonic Unleashed for iOS, maybe a Toy Story 3 port could be made.

## Credits

### Unleashed Recompiled
- [Skyth](https://github.com/blueskythlikesclouds): Creator and Lead Developer of the recompilation, as well as the developer of technologies created for it such as [XenonRecomp](https://github.com/hedge-dev/XenonRecomp) and [XenosRecomp](https://github.com/hedge-dev/XenosRecomp). Other responsibilities include the creation of the graphics and audio backends for the project, alongside custom menus, dynamic UI aspect ratio and various patches and new features added to the game.

- [Sajid](https://github.com/Sajidur78): Co-creator and Developer of the recompilation, as well as the developer of [XenonAnalyse](https://github.com/hedge-dev/XenonRecomp/?tab=readme-ov-file#XenonAnalyse). Other responsibilities include the implementation of core components for the project, like the Xbox 360 kernel translation layer used to make the game function.

- [Hyper](https://github.com/hyperbx): Developer of system level features, such as achievement support and the custom menus, alongside various other patches and features to make the game feel right at home on modern systems. Aided in the creation of concept art and the final options menu thumbnails.

- [Darío](https://github.com/DarioSamo): Creator of the graphics hardware abstraction layer [plume](https://github.com/renderbag/plume), used by the project's graphics backend. Alongside providing consultation for graphics and aiding with shader research and development, other responsibilities include the installer wizard and Linux support. Provided Spanish localization for the custom menus.

- [ĐeäTh](https://github.com/DeaTh-G): Supervisor of game accurate design philosophy regarding the custom menus. Aided in the implementation of annotation support for Japanese localization, whilst providing minor support for all localization.

- [RadiantDerg](https://github.com/RadiantDerg): Lead Artist behind the thumbnails used in the options menu. Other responsibilities include the creation of several debugging related codes for Hedge Mod Manager and providing aid with the research of the game's internals.

- [PTKay](https://github.com/PTKay): Lead Concept Artist for the custom menus. Aided in the development of the installer wizard's visuals.

- [SuperSonic16](https://github.com/thesupersonic16): Lead Developer of [Hedge Mod Manager](https://github.com/thesupersonic16/HedgeModManager), providing compatibility for modding with the recompilation. Aided in the creation of the deployment system for Linux builds.

- [NextinHKRY](https://github.com/NextinMono): Aided in researching the game's internals and creating concept art for some options menu thumbnails used in the final release. Provided Italian localization for the custom menus.

- [NerunSmarts](https://github.com/NerunSmarts): Added support for building to iOS, and fixed *most* issues resulting from that including textures and file management.

- [LadyLunanova](https://linktr.ee/ladylunanova): Artist behind the achievement trophy sprite and the keyboard and mouse icons used in the installer wizard. 

- [LJSTAR](https://github.com/LJSTARbird): Artist behind the project logo, along with several thumbnail designs used in the options menu and created new icons for the button guide for opening the achievements menu. Provided French localization for the custom menus.

- [saguinee](https://twitter.com/saguinee): Artist behind thumbnail designs used in the options menu such as Hints and Battle Theme.

- [Goalringmod27](https://linktr.ee/goalringmod27): Concept Artist behind the achievements overlay shown during gameplay. Aided in the creation of the Transparency Anti-Aliasing thumbnail.

- [RagdollClash](https://github.com/RagdollClash): Provisional support for dynamic UI aspect ratio.

- [DaGuAr](https://twitter.com/TheDaguar): Provided Spanish localization for the custom menus alongside Darío.

- [brianuuuSonic](https://github.com/brianuuu): Provided Japanese localization for the custom menus.

- [Kitzuku](https://github.com/Kitzuku): Provided German localization for the custom menus.
