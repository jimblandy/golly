// This file is part of Golly.
// See docs/License.html for the copyright notice.

#import <UIKit/UIKit.h>

// This is the view controller for the Open tab.

@interface OpenViewController : UIViewController <UITableViewDelegate, UITableViewDataSource, UIWebViewDelegate>
{
    IBOutlet UITableView *optionTable;
    IBOutlet UIWebView *htmlView;
}

@end

// if any files exist in the Documents folder (created by iTunes file sharing)
// then move them into Documents/Rules/ if their names end with
// .rule/tree/table, otherwise assume they are patterns
// and move them into Documents/Saved/
void MoveSharedFiles();
