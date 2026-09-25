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
- [Features](#features)
- [Credits](#credits)

## Minimum System Requirements

- CPU with support for the AVX instruction set:
tested on A16 Bionic, may support older.
- GPU with support for Direct3D 12.0 (Shader Model 6) or Vulkan 1.2:
A16 Bionic (iPhone 14 Pro) for 61% resolution scale @ 30-60FPS
- Memory:
  4 GB for iOS, recommend 6.
- Operating System:
  iOS 15.0
- Storage:
  - With DLC: 10 GiB required
  - Without DLC: 6 GiB required

> [!NOTE]
> More storage space may be required if uncompressed game files are provided during installation.

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

> [!NOTE]
> Please note that it is **not possible** to complete the installation if your files have been **modified**. In case of other problems such as black screens or crashes, **do not try to reinstall the game** as it is not possible for the process to result in an invalid installation.

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

### I can't do anything on iOS, what are the controls?

Currently, there are no touch controls for iOS implemented outside of the installer. If you'd like them, please make a feature suggestion in my discord (if there's not one already and I can probably add some basic ones), but for now I have no plans on doing them.

### I want to update the game. How can I avoid losing my save data? Do I need to reinstall the game?

Updating the game can be done by simply installing the newer .ipa on top of your existing installation. **Your save data and configuration will not be lost.** You won't need to reinstall the game, as the game files will always remain the same across versions of UnleashedRecompiled for iOS.

### How can I force the game to store the save data and configuration in the installation folder?

You can make the game ignore the [default configuration paths](#where-is-the-save-data-and-configuration-file-stored) and force it to save everything in the installation directory by creating an empty `portable.txt` file. You are directly responsible for the safekeeping of your save data and configuration if you choose this option.

### Can I install the game with a PlayStation 3 copy?

**You cannot use the files from the PlayStation 3 version of the game.** Supporting these files would require an entirely new recompilation, as they have proprietary formatting that only works on PS3 and the code for these formats is only present in that version. All significant differences present in the PS3 version of the game have been included in this project as options.

### Can I install the game with a Japanese copy?

The Japanese version of Sonic Unleashed has some minor differences in both file structure and content that make this version of the game incompatible with the international release. Furthermore, the US and EU versions of the game already support Japanese. Supporting this version would only cause mod compatibility issues in the future, so it is unlikely to be added to the update roadmap as it would also require its own recompilation.

### Will macOS be supported?

macOS is officially supported in this fork.

### What other platforms will be supported?

This project does not plan to support any more platforms other than iOS and macOS at the moment. Any contributors who wish to support more platforms should do so through my discord.

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
