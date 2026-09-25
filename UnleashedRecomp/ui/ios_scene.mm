#include "ios_scene.h"

#import <UIKit/UIKit.h>
#import <objc/runtime.h>

static UIWindowScene* g_scene = nil;
static UIWindow* g_pendingWindow = nil;

static void AttachToScene(UIWindow* window)
{
    if (window == nil || g_scene == nil)
        return;

    if (window.windowScene != g_scene)
    {
        window.windowScene = g_scene;
        window.frame = g_scene.coordinateSpace.bounds;
    }

    [window makeKeyAndVisible];
}

// Windows created by SDL (the game window, and the extra window message boxes use) don't know about scenes,
// and a window without a scene is never shown. Attach every new window to the app's scene.
@implementation UIWindow (UnleashedScene)

+ (void)load
{
    Class windowClass = [UIWindow class];
    SEL originalSelector = @selector(initWithFrame:);
    SEL replacementSelector = @selector(unleashed_initWithFrame:);
    Method original = class_getInstanceMethod(windowClass, originalSelector);
    Method replacement = class_getInstanceMethod(windowClass, replacementSelector);

    // If UIWindow only inherits initWithFrame: from UIView, add it to UIWindow instead of swapping UIView's.
    if (class_addMethod(windowClass, originalSelector, method_getImplementation(replacement), method_getTypeEncoding(replacement)))
        class_replaceMethod(windowClass, replacementSelector, method_getImplementation(original), method_getTypeEncoding(original));
    else
        method_exchangeImplementations(original, replacement);
}

- (instancetype)unleashed_initWithFrame:(CGRect)frame
{
    // Calls the original initWithFrame:, as the implementations are swapped.
    UIWindow* window = [self unleashed_initWithFrame:frame];

    if (window != nil && window.windowScene == nil && g_scene != nil)
        window.windowScene = g_scene;

    return window;
}

@end

// Referenced by name from UIApplicationSceneManifest in Info.plist.
@interface UnleashedSceneDelegate : UIResponder <UIWindowSceneDelegate>
@property (nonatomic, retain) UIWindow* window;
@end

@implementation UnleashedSceneDelegate

- (void)scene:(UIScene*)scene willConnectToSession:(UISceneSession*)session options:(UISceneConnectionOptions*)connectionOptions
{
    if (![scene isKindOfClass:[UIWindowScene class]])
        return;

    g_scene = (UIWindowScene*)scene;

    // SDL may have created its window before the scene connected.
    UIWindow* window = g_pendingWindow;
    if (window == nil)
    {
        id<UIApplicationDelegate> appDelegate = UIApplication.sharedApplication.delegate;
        if ([appDelegate respondsToSelector:@selector(window)])
            window = appDelegate.window;
    }

    if (window != nil)
    {
        self.window = window;
        AttachToScene(window);
    }
}

- (void)sceneDidDisconnect:(UIScene*)scene
{
    if (scene == g_scene)
        g_scene = nil;
}

- (void)dealloc
{
    self.window = nil;
    [super dealloc];
}

@end

namespace ios_scene
{
    void WaitForScene()
    {
        // The scene connects while the run loop runs, shortly after launch. Wait for it (up to a few
        // seconds) so that windows and message boxes created at startup can be shown.
        for (int i = 0; i < 300 && g_scene == nil; i++)
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, true);
    }

    void AttachWindow(void* uiWindow)
    {
        UIWindow* window = (UIWindow*)uiWindow;
        if (window == nil)
            return;

        if (g_pendingWindow != window)
        {
            [g_pendingWindow release];
            g_pendingWindow = [window retain];
        }

        AttachToScene(window);

        if (g_scene != nil && [g_scene.delegate isKindOfClass:[UnleashedSceneDelegate class]])
            ((UnleashedSceneDelegate*)g_scene.delegate).window = window;
    }
}
