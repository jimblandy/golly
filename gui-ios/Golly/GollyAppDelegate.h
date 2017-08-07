// This file is part of Golly.
// See docs/License.html for the copyright notice.

#import <UIKit/UIKit.h>

@interface GollyAppDelegate : UIResponder <UIApplicationDelegate, UITabBarControllerDelegate>

@property (strong, nonatomic) UIWindow *window;

@end

// export these routines so we can switch tabs programmatically
void SwitchToPatternTab();
void SwitchToOpenTab();
void SwitchToSettingsTab();
void SwitchToHelpTab();

// return tab bar's current view controller
UIViewController* CurrentViewController();

// enable/disable tab bar
void EnableTabBar(bool enable);

// show/hide tab bar
void ShowTabBar(bool show);

// get height of tab bar
CGFloat TabBarHeight();
