// This file is part of Golly.
// See docs/License.html for the copyright notice.

// This is the view used to display a pattern (within the Pattern tab).

@interface PatternView : UIView <UIGestureRecognizerDelegate, UIActionSheetDelegate>

- (void)doPasteAction;
- (void)doSelectionAction;
- (void)refreshPattern;

@end
