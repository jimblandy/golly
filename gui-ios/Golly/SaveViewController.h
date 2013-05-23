/*** /
 
 This file is part of Golly, a Game of Life Simulator.
 Copyright (C) 2013 Andrew Trevorrow and Tomas Rokicki.
 
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

#import "InfoViewController.h"

// This view controller creates a modal dialog that appears
// when the user taps the Pattern tab's Save button.
// It is also used (via SaveTextFile) to save a text file
// when the user is editing a pattern file or .rule file.

@interface SaveViewController : UIViewController <UITextFieldDelegate, UITableViewDelegate, UITableViewDataSource>
{
    IBOutlet UITextField *nameText;
    IBOutlet UITableView *typeTable;
    IBOutlet UILabel *topLabel;
    IBOutlet UILabel *botLabel;
}

- (IBAction)doCancel:(id)sender;
- (IBAction)doSave:(id)sender;

@end

// Ask user to save given text file currently being edited.
void SaveTextFile(const char* filepath, const char* contents, InfoViewController* currentView);
