// This file is part of Golly.
// See docs/License.html for the copyright notice.

#import <UIKit/UIKit.h>

// This is the view controller for the Help tab.

@interface HelpViewController : UIViewController <UIWebViewDelegate>
{
    IBOutlet UIWebView *htmlView;
    IBOutlet UIBarButtonItem *backButton;
    IBOutlet UIBarButtonItem *nextButton;
    IBOutlet UIBarButtonItem *contentsButton;
}

- (IBAction)doBack:(id)sender;
- (IBAction)doNext:(id)sender;
- (IBAction)doContents:(id)sender;

@end

// display given HTML file
void ShowHelp(const char* filepath);
