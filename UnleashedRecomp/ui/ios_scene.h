#pragma once

// iOS 27 won't launch apps that don't use the UIScene lifecycle, which SDL2 doesn't support.
// The app declares a scene in its Info.plist, and SDL's window is attached to it here.
namespace ios_scene
{
    // Runs the main run loop until the app's scene has connected. Call at startup, before creating any windows.
    void WaitForScene();

    // Attaches a UIWindow created by SDL to the app's scene and shows it. If the scene hasn't
    // connected yet, the window is attached as soon as it does.
    void AttachWindow(void* uiWindow);
}
