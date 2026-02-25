#include "ios_file_picker.h"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if defined(__APPLE__) && TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
#import <objc/runtime.h>
#include <atomic>

@interface RecompDocumentPickerDelegate : NSObject <UIDocumentPickerDelegate>
@property (nonatomic, copy) void (^completion)(NSArray<NSURL*>* urls, NSError* error, BOOL cancelled);
@end

@implementation RecompDocumentPickerDelegate
- (void)documentPicker:(UIDocumentPickerViewController*)controller didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls
{
    if (self.completion)
        self.completion(urls, nil, NO);
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController*)controller
{
    if (self.completion)
        self.completion(@[], nil, YES);
}
@end

static UIWindow* GetActiveWindow()
{
    UIApplication* app = UIApplication.sharedApplication;

    if (@available(iOS 13.0, *))
    {
        for (UIScene* scene in app.connectedScenes)
        {
            if (![scene isKindOfClass:[UIWindowScene class]])
                continue;

            UIWindowScene* windowScene = (UIWindowScene*)scene;
            if (windowScene.activationState != UISceneActivationStateForegroundActive)
                continue;

            for (UIWindow* window in windowScene.windows)
            {
                if (window.isKeyWindow)
                    return window;
            }

            if (windowScene.windows.count > 0)
                return windowScene.windows.firstObject;
        }
    }

    if (app.windows.count > 0)
        return app.windows.firstObject;

    return nil;
}

namespace ios_file_picker
{
    static std::atomic<bool> s_pickerInFlight = false;
    static NSMutableArray<NSURL*>* s_activeScopedURLs = nil;

    static void ClearActiveScopedURLs()
    {
        if (s_activeScopedURLs == nil)
            return;

        NSArray* urlsToStop = [NSArray arrayWithArray:s_activeScopedURLs];
        for (id scopedObject in urlsToStop)
        {
            if (![scopedObject isKindOfClass:[NSURL class]])
                continue;

            NSURL* scopedURL = (NSURL*)scopedObject;
            [scopedURL stopAccessingSecurityScopedResource];
        }

        [s_activeScopedURLs removeAllObjects];
    }

    static bool IsTrackingScopedURL(NSURL* url)
    {
        if (url == nil || s_activeScopedURLs == nil)
            return false;

        for (id scopedObject in s_activeScopedURLs)
        {
            if (![scopedObject isKindOfClass:[NSURL class]])
                continue;

            NSURL* scopedURL = (NSURL*)scopedObject;
            if ([scopedURL isEqual:url])
                return true;
        }

        return false;
    }

    bool PickPaths(bool folderMode, std::list<std::filesystem::path>& outPaths, std::string& outError)
    {
        @autoreleasepool
        {
            bool expected = false;
            if (!s_pickerInFlight.compare_exchange_strong(expected, true))
            {
                outError = "A file picker request is already active.";
                return false;
            }

            __block BOOL finished = NO;
            __block BOOL cancelled = NO;
            __block NSError* pickerError = nil;
            __block NSArray<NSURL*>* selectedURLs = nil;
            dispatch_semaphore_t completionSemaphore = dispatch_semaphore_create(0);

            if (s_activeScopedURLs == nil)
            {
                s_activeScopedURLs = [[NSMutableArray alloc] init];
            }

            auto presentPicker = ^{
                UIWindow* window = GetActiveWindow();
                UIViewController* presenter = window.rootViewController;

                while (presenter != nil && presenter.presentedViewController != nil)
                    presenter = presenter.presentedViewController;

                if (presenter == nil)
                {
                    pickerError = [NSError errorWithDomain:@"UnleashedRecomp" code:1 userInfo:@{NSLocalizedDescriptionKey: @"No active presenter for file picker."}];
                    finished = YES;
                    dispatch_semaphore_signal(completionSemaphore);
                    return;
                }

                NSArray<NSString*>* documentTypes = folderMode ? @[@"public.folder"] : @[@"public.data"];
                UIDocumentPickerViewController* picker = [[UIDocumentPickerViewController alloc] initWithDocumentTypes:documentTypes inMode:UIDocumentPickerModeOpen];
                picker.allowsMultipleSelection = YES;

                RecompDocumentPickerDelegate* delegate = [RecompDocumentPickerDelegate new];
                delegate.completion = ^(NSArray<NSURL*>* urls, NSError* error, BOOL wasCancelled)
                {
                    selectedURLs = urls != nil ? [urls copy] : @[];
                    pickerError = error;
                    cancelled = wasCancelled;
                    finished = YES;
                    dispatch_semaphore_signal(completionSemaphore);
                };

                picker.delegate = delegate;
                objc_setAssociatedObject(picker, @selector(PickPaths), delegate, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
                [presenter presentViewController:picker animated:YES completion:nil];
            };

            if ([NSThread isMainThread])
            {
                presentPicker();
                while (!finished)
                {
                    [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
                }
            }
            else
            {
                dispatch_sync(dispatch_get_main_queue(), presentPicker);
                dispatch_semaphore_wait(completionSemaphore, DISPATCH_TIME_FOREVER);
            }

            if (pickerError != nil)
            {
                outError = [[pickerError localizedDescription] UTF8String];
                s_pickerInFlight.store(false);
                return false;
            }

            if (cancelled)
            {
                s_pickerInFlight.store(false);
                return true;
            }

            if (selectedURLs != nil && ![selectedURLs isKindOfClass:[NSArray class]])
            {
                outError = "Document picker returned invalid selection data.";
                s_pickerInFlight.store(false);
                return false;
            }

            bool foundValidPath = false;
            NSArray* selectedURLArray = selectedURLs != nil ? selectedURLs : @[];
            for (id pickedObject in selectedURLArray)
            {
                if (![pickedObject isKindOfClass:[NSURL class]])
                    continue;

                NSURL* url = (NSURL*)pickedObject;
                if (url == nil)
                    continue;

                BOOL startedScopedAccess = [url startAccessingSecurityScopedResource];
                if (startedScopedAccess && !IsTrackingScopedURL(url))
                    [s_activeScopedURLs addObject:url];

                NSURL* fileURL = url;
                if (![fileURL isFileURL])
                {
                    NSURL* filePathURL = [fileURL filePathURL];
                    if (filePathURL != nil)
                        fileURL = filePathURL;
                }

                NSString* path = fileURL.path;
                if ((path == nil || path.length == 0) && url.standardizedURL != nil)
                    path = url.standardizedURL.path;

                if ((path == nil || path.length == 0) && url.absoluteString != nil)
                {
                    NSURL* absoluteURL = [NSURL URLWithString:url.absoluteString];
                    if (absoluteURL != nil)
                        path = absoluteURL.path;
                }

                if (path != nil && path.length > 0)
                {
                    outPaths.emplace_back(std::filesystem::path([path UTF8String]));
                    foundValidPath = true;
                }
            }

            if (!foundValidPath && selectedURLArray.count > 0)
            {
                outError = "Document picker returned items without usable local file paths. Try selecting a local file under 'On My iPhone' and ensure the file is fully downloaded.";
                s_pickerInFlight.store(false);
                return false;
            }

            if (!foundValidPath && selectedURLArray.count == 0)
            {
                s_pickerInFlight.store(false);
                return true;
            }

            if (outPaths.empty())
            {
                outError = "No readable file paths were returned by the picker.";
                s_pickerInFlight.store(false);
                return false;
            }

            s_pickerInFlight.store(false);
            return true;
        }
    }

    void ReleaseAllAccess()
    {
        @autoreleasepool
        {
            ClearActiveScopedURLs();
        }
    }
}

#else

namespace ios_file_picker
{
    bool PickPaths(bool, std::list<std::filesystem::path>&, std::string& outError)
    {
        outError = "iOS picker is only available on iOS builds.";
        return false;
    }

    void ReleaseAllAccess()
    {
    }
}

#endif
