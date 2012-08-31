/*** /
 
 This file is part of Golly, a Game of Life Simulator.
 Copyright (C) 2012 Andrew Trevorrow and Tomas Rokicki.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 
 Web site:  http://sourceforge.net/projects/golly
 Authors:   rokicki@gmail.com  andrew@trevorrow.com
 
 / ***/

#import <UIKit/UIKit.h>

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
- (IBAction)toggleUndo:(id)sender;
- (IBAction)toggleHashing:(id)sender;

@end
