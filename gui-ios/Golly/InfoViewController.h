// This file is part of Golly.
// See docs/License.html for the copyright notice.

#import <UIKit/UIKit.h>

// This view controller is used to display comments in the currently loaded pattern.
// It can also be used (via ShowTextFile) to display the contents of a text file
// and edit that text if the file is located somewhere inside Documents/*.

@interface InfoViewController : UIViewController <UITextViewDelegate, UIGestureRecognizerDelegate>
{
    IBOutlet UITextView *fileView;
    IBOutlet UIBarButtonItem *saveButton;
}

- (IBAction)doCancel:(id)sender;
- (IBAction)doSave:(id)sender;

- (void)saveSucceded:(const char*)savedpath;

@end

// Display the given text file.
void ShowTextFile(const char* filepath, UIViewController* currentView=nil);
