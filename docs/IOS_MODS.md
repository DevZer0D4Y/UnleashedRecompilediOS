# iOS mod support

The iOS build has a native startup mod manager. It uses the existing Unleashed Recompiled asset loader; it does not run HedgeModManager itself or convert arbitrary executable mods.

## Installing and using mods

1. Obtain a mod specifically made for **Unleashed Recompiled**. Mods for Sonic Generations, Frontiers, or other games are not interchangeable.
2. Extract its archive using Files or another archive tool. Find the folder containing `mod.ini`.
3. Launch the iOS build and tap **Import folder** in the startup manager. Select that extracted folder. The manager copies it into the app's Documents storage; the original folder can remain wherever it is.
4. Tap the mod to enable it. New imports start disabled. Unsupported declared requirements appear beneath the mod title.
5. Drag rows to set priority. The first mod wins file replacement conflicts. Enable required dependencies too; the manager checks IDs and minimum numeric versions before starting.
6. Optionally toggle the **Built-in codes** that ship with the game (such as skipping intro logos or homing attack on jump). These settings are saved in the game configuration.
7. Tap **Play** to save the selection and start the game. Open the app again to change mods for another launch.

You can also place extracted folders directly in `On My iPhone/iPad → UnleashedRecomp → UnleashedRecomp → mods`. Each immediate child must contain `mod.ini`. Remove installed mod folders with Files while the game is closed. Importing a duplicate folder name does not overwrite an existing mod or its saves.

## Supported loading paths

- HMM `mod.ini` metadata and multiple relative `IncludeDir` paths.
- Asset file replacements handled by the existing game loader.
- HMM append archives (`+*.arl`, `+*.ar.*`) and loose `work` files through the existing archive hooks.
- Existing UMM asset loading and merge behavior. This does not run UMM's original file-patching installer.
- Mod-specific save redirection within the mod folder.
- Enable/disable settings, load order, and dependency ID/minimum-version checks.

The manager stores `ios-mods.ini` beside the game data. Its paths are relative to the user directory, allowing app updates and iOS sandbox relocations. The native database controls iOS selection; desktop `cpkredir.ini` settings do not override it. Desktop loader behavior remains available on other platforms.

## Compatibility limits

DLL plugins, arbitrary `CodeFile` scripts, and mods for other games require individual source ports. Declared DLL/code requirements prevent a mod from being enabled. This check cannot establish that every asset in a package is compatible with the renderer or the Xbox 360 game data. No actual community mod packages have been tested on a device in this change.

There is no online catalog, automatic downloading/updating, archive extraction, or HMM configuration-schema editor. Extract archives before importing. The built-in codes section exposes the codes implemented in this executable; it does not execute downloaded HMM scripts.

Dependency versions use numeric dotted version comparison (such as `1.2.0`); other version formats require an exact match. Symbolic links and include/save paths that escape a mod's directory are rejected.

## Validation

`tests/ios_mod_manager_tests.cpp` exercises importing and collisions, persistence, order after removing a mod, unsupported declarations, path confinement, symlinks, dependency versions, and sandbox relocation. It passed locally with AddressSanitizer and UndefinedBehaviorSanitizer. The manager UI and backend passed ARM64 iOS 15 SDK compilation checks with warnings treated as errors.

After initializing the project's dependencies, run the standalone backend tests from the repository root:

```sh
xcrun clang++ -std=c++20 -DXXH_INLINE_ALL -fsanitize=address,undefined \
  -IUnleashedRecomp -Ithirdparty/unordered_dense/include \
  -Itools/XenonRecomp/thirdparty/xxHash \
  UnleashedRecomp/mod/ios_mod_manager.cpp tests/ios_mod_manager_tests.cpp \
  -o /tmp/unleashed-ios-mod-tests
/tmp/unleashed-ios-mod-tests /tmp/unleashed-ios-mod-fixtures
```

The fixture path must not already exist. Successful tests remove their fixture directory.

A complete app build and device smoke test remain required; no IPA was produced during validation. Restoring dependencies in an isolated checkout failed because the build machine ran out of disk space. The supplied source checkout also lacks the private game inputs (`default.xex`, `default.xexp`, and `shader.ar`) and the pre-generated PPC/shader sources. The regular iOS build script requires those inputs to produce the executable.

Format reference: [HedgeModManager's mod structure documentation](https://github.com/thesupersonic16/HedgeModManager/wiki/Basic-Mod-File-Structure).
