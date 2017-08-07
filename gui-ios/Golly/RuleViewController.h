// This file is part of Golly.
// See docs/License.html for the copyright notice.

#import <UIKit/UIKit.h>

// This view controller is used when the Pattern tab's Rule button is tapped.

@interface RuleViewController : UIViewController <UIActionSheetDelegate, UITextFieldDelegate,
                                                  UIPickerViewDelegate, UIPickerViewDataSource,
                                                  UIWebViewDelegate>
{
    IBOutlet UIButton *algoButton;
    IBOutlet UITextField *ruleText;
    IBOutlet UILabel *unknownLabel;
    IBOutlet UIPickerView *rulePicker;
    IBOutlet UIWebView *htmlView;
}

- (IBAction)changeAlgorithm:(id)sender;
- (IBAction)cancelRuleChange:(id)sender;
- (IBAction)doRuleChange:(id)sender;

@end

std::string GetRuleName(const std::string& rule);
