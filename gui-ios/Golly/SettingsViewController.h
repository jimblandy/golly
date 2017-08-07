// This file is part of Golly.
// See docs/License.html for the copyright notice.

#import <UIKit/UIKit.h>

// This is the view controller for the Settings tab.

@interface SettingsViewController : UIViewController <UIActionSheetDelegate, UITextFieldDelegate>
{
    IBOutlet UIButton *modeButton;
    IBOutlet UITextField *percentageText;
    IBOutlet UITextField *memoryText;
    IBOutlet UISlider *percentageSlider;
    IBOutlet UISlider *memorySlider;
    IBOutlet UISwitch *gridSwitch;
    IBOutlet UISwitch *timingSwitch;
    IBOutlet UISwitch *beepSwitch;
    IBOutlet UISwitch *colorsSwitch;
    IBOutlet UISwitch *iconsSwitch;
    IBOutlet UISwitch *undoSwitch;
    IBOutlet UISwitch *hashingSwitch;
}

- (IBAction)changePasteMode:(id)sender;
- (IBAction)changePercentage:(id)sender;
- (IBAction)changeMemory:(id)sender;
- (IBAction)toggleGrid:(id)sender;
- (IBAction)toggleTiming:(id)sender;
- (IBAction)toggleBeep:(id)sender;
- (IBAction)toggleColors:(id)sender;
- (IBAction)toggleIcons:(id)sender;
- (IBAction)toggleUndo:(id)sender;
- (IBAction)toggleHashing:(id)sender;

@end
