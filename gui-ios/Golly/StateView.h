// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "algos.h"      // for gBitmapPtr

@interface StateView : UIView <UIGestureRecognizerDelegate, UIPopoverControllerDelegate>
{
    UIPopoverController *statePopover;
}

// close statePopover when user clicks on a drawing state
- (void)dismissStatePopover;

@end

void DrawOneIcon(CGContextRef context, int x, int y, gBitmapPtr icon,
                 unsigned char deadr, unsigned char deadg, unsigned char deadb,
                 unsigned char liver, unsigned char liveg, unsigned char liveb);
