#import "AppDelegate.h"
#import "MIStore.h"
#import "MITheme.h"
#import "MILang.h"
#import "MICrash.h"
#import "MIViews.h"
#import "MIQueueController.h"
#import "MIImportedController.h"
#import "MISettingsController.h"

@interface AppDelegate ()
{
    MIQueueController *_queue;
}
@end

@implementation AppDelegate

- (UINavigationController *)navWith:(UIViewController *)root title:(NSString *)title kind:(int)kind
{
    UINavigationController *nav = [[UINavigationController alloc] initWithRootViewController:root];
    MIStyleNavigationBar(nav.navigationBar);
    nav.tabBarItem = [[UITabBarItem alloc] initWithTitle:title image:MITabIcon(kind) tag:kind];
    MIStyleTabItem(nav.tabBarItem, kind);
    return nav;
}

/* cały interfejs od zera: przy starcie oraz po zmianie motywu albo języka */
- (UITabBarController *)buildTabs
{
    _queue = [[MIQueueController alloc] init];
    NSArray *controllers = [NSArray arrayWithObjects:
        [self navWith:_queue title:GL(@"Kolejka") kind:0],
        [self navWith:[[MIImportedController alloc] init] title:GL(@"Zaimportowane") kind:1],
        [self navWith:[[MISettingsController alloc] init] title:GL(@"Ustawienia") kind:3], nil];
    UITabBarController *tabs = [[UITabBarController alloc] init];
    tabs.viewControllers = controllers;
    MIStyleTabBar(tabs.tabBar);
    return tabs;
}

- (void)appearanceChanged:(NSNotification *)n
{
    /* po wyjściu z obsługi dotknięcia, żeby nie usuwać kontrolera, który właśnie działa */
    [self performSelector:@selector(rebuildInterface) withObject:nil afterDelay:0.0];
}

- (void)rebuildInterface
{
    UITabBarController *old = (UITabBarController *)self.window.rootViewController;
    NSUInteger selected = [old isKindOfClass:[UITabBarController class]] ? old.selectedIndex : 0;
    [old dismissViewControllerAnimated:NO completion:nil];
    UITabBarController *tabs = [self buildTabs];
    tabs.selectedIndex = selected;
    [UIView transitionWithView:self.window duration:0.3 options:UIViewAnimationOptionTransitionCrossDissolve
                    animations:^{ self.window.rootViewController = tabs; } completion:nil];
}

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    NSString *lastReport = MITakeLastRunReport();
    MICrashInstall();
    MICrumb(@"start aplikacji");
    [MIStore shared];
    MICrumb(@"ustawienia wczytane");
    [application setStatusBarStyle:UIStatusBarStyleBlackOpaque];
    self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    self.window.backgroundColor = [UIColor blackColor];

    self.window.rootViewController = [self buildTabs];
    MICrumb(@"interfejs zbudowany");
    [self.window makeKeyAndVisible];
    MICrumb(@"okno pokazane");
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(appearanceChanged:) name:MIAppearanceDidChangeNotification object:nil];

    /* ekran powitalny: ten sam obraz co Default.png, przytrzymany chwilę i płynnie zgaszony */
    CGRect bounds = [[UIScreen mainScreen] bounds];
    UIImage *splash = [UIImage imageNamed:(bounds.size.height > 480.0f ? @"Default-568h" : @"Default")];
    if (splash) {
        UIImageView *cover = [[UIImageView alloc] initWithFrame:bounds];
        cover.image = splash;
        cover.contentMode = UIViewContentModeScaleAspectFill;
        cover.clipsToBounds = YES;
        cover.userInteractionEnabled = YES;
        [self.window addSubview:cover];
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.2 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            [UIView animateWithDuration:0.4 animations:^{ cover.alpha = 0.0f; }
                             completion:^(BOOL finished) { [cover removeFromSuperview]; }];
        });
    }
    if (lastReport) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(2.0 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            UIAlertView *a = [[UIAlertView alloc] initWithTitle:@"Music Impactor – diagnostyka" message:lastReport delegate:nil cancelButtonTitle:@"OK" otherButtonTitles:nil];
            [a show];
        });
    }
    return YES;
}

/* "Otwórz w..." - plik trafia do kolejki */
- (void)handleOpenedURL:(NSURL *)url
{
    if (![url isFileURL]) return;
    NSString *path = [url path];
    NSString *err = nil;
    NSString *name = [path lastPathComponent];
    if (![[MIStore shared] addFileAtPath:path displayName:name move:YES error:&err]) {
        MIAlert(name, err);
    }
    UITabBarController *tabs = (UITabBarController *)self.window.rootViewController;
    if ([tabs isKindOfClass:[UITabBarController class]]) {
        [tabs dismissViewControllerAnimated:NO completion:nil];
        tabs.selectedIndex = 0;
        UINavigationController *nav = [tabs.viewControllers objectAtIndex:0];
        [nav popToRootViewControllerAnimated:NO];
    }
}

- (BOOL)application:(UIApplication *)application openURL:(NSURL *)url sourceApplication:(NSString *)sourceApplication annotation:(id)annotation
{
    [self handleOpenedURL:url];
    return YES;
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
    MICrumb(@"aplikacja aktywna");
}

- (void)applicationDidEnterBackground:(UIApplication *)application { [[MIStore shared] save]; MICrumbCleanExit(); }
- (void)applicationWillTerminate:(UIApplication *)application { [[MIStore shared] save]; MICrumbCleanExit(); }

@end
