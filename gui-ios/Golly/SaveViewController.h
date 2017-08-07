// This file is part of Golly.
// See docs/License.html for the copyright notice.

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
