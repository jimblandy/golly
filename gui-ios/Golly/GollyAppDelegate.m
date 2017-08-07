// This file is part of Golly.
// See docs/License.html for the copyright notice.

#import "prefs.h"   // for SavePrefs

#import "PatternViewController.h"
#import "OpenViewController.h"
#import "SettingsViewController.h"
#import "HelpViewController.h"
#import "GollyAppDelegate.h"

@implementation GollyAppDelegate

@synthesize window = _window;

// -----------------------------------------------------------------------------

static UITabBarController *tabBarController = nil;      // for SwitchToPatternTab, etc

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    // set variable seed for later rand() calls
    srand((unsigned int)time(0));
    
    self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];

    UIViewController *vc0 = [[PatternViewController alloc] initWithNibName:nil bundle:nil];
    UIViewController *vc1 = [[OpenViewController alloc] initWithNibName:nil bundle:nil];
    UIViewController *vc2 = [[SettingsViewController alloc] initWithNibName:nil bundle:nil];
    UIViewController *vc3 = [[HelpViewController alloc] initWithNibName:nil bundle:nil];
    
    tabBarController = [[UITabBarController alloc] init];
    tabBarController.viewControllers = [NSArray arrayWithObjects:vc0, vc1, vc2, vc3, nil];
    
    self.window.rootViewController = tabBarController;
    
    [self.window makeKeyAndVisible];
    return YES;
}

// -----------------------------------------------------------------------------

- (void)applicationWillResignActive:(UIApplication *)application
{
    // this is called for certain types of temporary interruptions (such as an incoming phone call or SMS message)
    // or when the user quits the application and it begins the transition to the background state
    PauseGenTimer();
}

// -----------------------------------------------------------------------------

- (void)applicationDidEnterBackground:(UIApplication *)application
{
    // called when user hits home button
    PauseGenTimer();
    SavePrefs();
}

// -----------------------------------------------------------------------------

- (void)applicationWillEnterForeground:(UIApplication *)application
{
    // undo any changes made in applicationDidEnterBackground
    RestartGenTimer();
}

// -----------------------------------------------------------------------------

- (void)applicationDidBecomeActive:(UIApplication *)application
{
    // restart any tasks that were paused in applicationWillResignActive
    RestartGenTimer();
}

// -----------------------------------------------------------------------------

- (void)applicationWillTerminate:(UIApplication *)application
{
    // application is about to terminate
    // (never called in iOS 5, so use applicationDidEnterBackground)
}

@end

// =============================================================================

void SwitchToPatternTab()
{
    tabBarController.selectedIndex = 0;
}

// -----------------------------------------------------------------------------

void SwitchToOpenTab()
{
    tabBarController.selectedIndex = 1;
}

// -----------------------------------------------------------------------------

void SwitchToSettingsTab()
{
    tabBarController.selectedIndex = 2;
}

// -----------------------------------------------------------------------------

void SwitchToHelpTab()
{
    tabBarController.selectedIndex = 3;
}

// -----------------------------------------------------------------------------

UIViewController* CurrentViewController()
{
    return tabBarController.selectedViewController;
}

// -----------------------------------------------------------------------------

void EnableTabBar(bool enable)
{
    tabBarController.tabBar.userInteractionEnabled = enable;
}

// -----------------------------------------------------------------------------

void ShowTabBar(bool show)
{
    tabBarController.tabBar.hidden = !show;
}

// -----------------------------------------------------------------------------

CGFloat TabBarHeight()
{
    return tabBarController.tabBar.frame.size.height;
}
