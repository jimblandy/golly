// This file is part of Golly.
// See docs/License.html for the copyright notice.

#import <UIKit/UIKit.h>
#import "StatePickerView.h"

@interface StatePickerController : UIViewController
{
    IBOutlet StatePickerView *spView;
    IBOutlet UISwitch *iconsSwitch;
}

- (IBAction)toggleIcons:(id)sender;

@end
