#include "ios_mod_manager.h"
#include <mod/ios_mod_manager.h>
#include <user/paths.h>
#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

@interface RecompModManager : UITableViewController <UIDocumentPickerDelegate>
{
    std::vector<ios_mod_manager::Entry> _mods;
    std::vector<ModLoader::Code> _codes;
    bool _scanFailed;
}
@property(nonatomic, copy) void (^completion)(void);
@end

@implementation RecompModManager
- (void)viewDidLoad
{
    [super viewDidLoad];
    self.title = @"Mods — top wins conflicts";
    self.tableView.allowsSelectionDuringEditing = YES;
    self.tableView.editing = YES;
    self.tableView.rowHeight = 76;
    _codes = ModLoader::GetBuiltInCodes();
    self.navigationItem.leftBarButtonItem = [[UIBarButtonItem alloc] initWithTitle:@"Import folder" style:UIBarButtonItemStylePlain target:self action:@selector(importFolder)];
    self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc] initWithTitle:@"Play" style:UIBarButtonItemStyleDone target:self action:@selector(play)];
    UILabel* footer = [[UILabel alloc] initWithFrame:CGRectMake(0, 0, 600, 90)];
    footer.numberOfLines = 0;
    footer.textAlignment = NSTextAlignmentCenter;
    footer.text = @"Tap to enable or disable; drag to reorder. Import an extracted Unleashed Recompiled mod folder containing mod.ini. DLL and custom code mods require ports.\nChanges apply when you tap Play.";
    footer.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    self.tableView.tableFooterView = footer;
    [self reloadMods];
}
- (void)showError:(NSString*)message
{
    UIAlertController* alert = [UIAlertController alertControllerWithTitle:@"Mod manager" message:message preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:nil]];
    [self presentViewController:alert animated:YES completion:nil];
}
- (void)reloadMods
{
    try { _mods = ios_mod_manager::Scan(GetUserPath()); _scanFailed = false; }
    catch (const std::exception& ex) { _scanFailed = true; [self showError:[NSString stringWithUTF8String:ex.what()]]; }
    [self.tableView reloadData];
}
- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView { return 2; }
- (NSString*)tableView:(UITableView*)tableView titleForHeaderInSection:(NSInteger)section { return section == 0 ? @"Installed mods" : @"Built-in codes"; }
- (NSInteger)tableView:(UITableView*)tableView numberOfRowsInSection:(NSInteger)section { return section == 0 ? _mods.size() : _codes.size(); }
- (UITableViewCell*)tableView:(UITableView*)tableView cellForRowAtIndexPath:(NSIndexPath*)indexPath
{
    UITableViewCell* cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:nil];
    if (indexPath.section == 1)
    {
        const auto& code = _codes[indexPath.row];
        NSString* title = [NSString stringWithUTF8String:code.name.c_str()];
        title = [title stringByReplacingOccurrencesOfString:@"([A-Z]+)([A-Z][a-z])" withString:@"$1 $2" options:NSRegularExpressionSearch range:NSMakeRange(0, title.length)];
        title = [title stringByReplacingOccurrencesOfString:@"([a-z])([A-Z])" withString:@"$1 $2" options:NSRegularExpressionSearch range:NSMakeRange(0, title.length)];
        cell.textLabel.text = title;
        cell.textLabel.numberOfLines = 2;
        cell.detailTextLabel.text = code.enabled ? @"Enabled" : @"Disabled";
        cell.accessoryType = code.enabled ? UITableViewCellAccessoryCheckmark : UITableViewCellAccessoryNone;
        return cell;
    }
    const auto& mod = _mods[indexPath.row];
    cell.textLabel.text = [NSString stringWithUTF8String:mod.title.c_str()];
    const auto subtitle = mod.issue.empty() ? (mod.enabled ? "Enabled • " : "Disabled • ") + mod.author : mod.issue;
    cell.detailTextLabel.text = [NSString stringWithUTF8String:subtitle.c_str()];
    cell.detailTextLabel.numberOfLines = 0;
    cell.accessoryType = mod.enabled ? UITableViewCellAccessoryCheckmark : UITableViewCellAccessoryNone;
    cell.textLabel.textColor = mod.issue.empty() ? UIColor.labelColor : UIColor.secondaryLabelColor;
    return cell;
}
- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
    if (indexPath.section == 1)
    {
        _codes[indexPath.row].enabled = !_codes[indexPath.row].enabled;
        [tableView deselectRowAtIndexPath:indexPath animated:YES];
        [tableView reloadData];
        return;
    }
    auto& mod = _mods[indexPath.row];
    if (mod.issue.empty()) mod.enabled = !mod.enabled;
    else [self showError:[NSString stringWithUTF8String:mod.issue.c_str()]];
    [tableView deselectRowAtIndexPath:indexPath animated:YES];
    [tableView reloadData];
}
- (BOOL)tableView:(UITableView*)tableView canMoveRowAtIndexPath:(NSIndexPath*)indexPath { return indexPath.section == 0; }
- (NSIndexPath*)tableView:(UITableView*)tableView targetIndexPathForMoveFromRowAtIndexPath:(NSIndexPath*)source toProposedIndexPath:(NSIndexPath*)destination
{
    return destination.section == 0 ? destination : source;
}
- (UITableViewCellEditingStyle)tableView:(UITableView*)tableView editingStyleForRowAtIndexPath:(NSIndexPath*)indexPath { return UITableViewCellEditingStyleNone; }
- (BOOL)tableView:(UITableView*)tableView shouldIndentWhileEditingRowAtIndexPath:(NSIndexPath*)indexPath { return NO; }
- (void)tableView:(UITableView*)tableView moveRowAtIndexPath:(NSIndexPath*)source toIndexPath:(NSIndexPath*)destination
{
    auto entry = std::move(_mods[source.row]);
    _mods.erase(_mods.begin() + source.row);
    _mods.insert(_mods.begin() + destination.row, std::move(entry));
}
- (BOOL)saveMods
{
    if (_scanFailed) { [self showError:@"The mods folder could not be scanned. Fix its permissions using Files and relaunch."]; return NO; }
    std::string error;
    if (ios_mod_manager::Save(GetUserPath(), _mods, error)) { ModLoader::SaveBuiltInCodes(_codes); return YES; }
    [self showError:[NSString stringWithUTF8String:error.c_str()]];
    return NO;
}
- (void)play
{
    if (![self saveMods]) return;
    [self.navigationController dismissViewControllerAnimated:YES completion:^{ if (self.completion) self.completion(); }];
}
- (void)importFolder
{
    if (![self saveMods]) return;
    UIDocumentPickerViewController* picker = [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:@[UTTypeFolder] asCopy:NO];
    picker.delegate = self;
    picker.allowsMultipleSelection = NO;
    [self presentViewController:picker animated:YES completion:nil];
}
- (void)documentPicker:(UIDocumentPickerViewController*)controller didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls
{
    NSURL* url = urls.firstObject;
    if (!url || !url.isFileURL) return;
    BOOL scoped = [url startAccessingSecurityScopedResource];
    self.tableView.userInteractionEnabled = NO;
    self.title = @"Importing mod…";
    self.navigationItem.leftBarButtonItem.enabled = NO;
    self.navigationItem.rightBarButtonItem.enabled = NO;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        std::string error;
        const bool success = ios_mod_manager::Import(GetUserPath(), std::filesystem::path(url.path.UTF8String), error);
        if (scoped) [url stopAccessingSecurityScopedResource];
        NSString* message = [NSString stringWithUTF8String:error.c_str()];
        dispatch_async(dispatch_get_main_queue(), ^{
            self.tableView.userInteractionEnabled = YES;
            self.title = @"Mods — top wins conflicts";
            self.navigationItem.leftBarButtonItem.enabled = YES;
            self.navigationItem.rightBarButtonItem.enabled = YES;
            [self reloadMods];
            if (!success) [self showError:message];
        });
    });
}
@end

namespace ios_mod_ui
{
    bool Run()
    {
        __block bool finished = false;
        __block bool presented = false;
        dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
        auto present = ^{
            UIWindow* window = nil;
            for (UIScene* scene in UIApplication.sharedApplication.connectedScenes)
            {
                if (![scene isKindOfClass:UIWindowScene.class] || scene.activationState != UISceneActivationStateForegroundActive) continue;
                for (UIWindow* candidate in ((UIWindowScene*)scene).windows)
                    if (candidate.isKeyWindow) window = candidate;
            }
            // SDL2 uses the application delegate window without a scene manifest.
            if (!window && [UIApplication.sharedApplication.delegate respondsToSelector:@selector(window)])
                window = UIApplication.sharedApplication.delegate.window;
            UIViewController* presenter = window.rootViewController;
            while (presenter.presentedViewController) presenter = presenter.presentedViewController;
            if (!presenter) { finished = true; dispatch_semaphore_signal(semaphore); return; }
            RecompModManager* manager = [[RecompModManager alloc] initWithStyle:UITableViewStyleInsetGrouped];
            manager.completion = ^{ finished = true; dispatch_semaphore_signal(semaphore); };
            UINavigationController* navigation = [[UINavigationController alloc] initWithRootViewController:manager];
            navigation.modalPresentationStyle = UIModalPresentationFullScreen;
            navigation.modalInPresentation = YES;
            [presenter presentViewController:navigation animated:NO completion:nil];
            presented = true;
        };
        if (NSThread.isMainThread)
        {
            present();
            while (!finished)
                [NSRunLoop.currentRunLoop runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
        }
        else
        {
            dispatch_sync(dispatch_get_main_queue(), present);
            dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
        }
        return presented;
    }
}
