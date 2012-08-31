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

#include "layer.h"      // for currlayer
#include "status.h"     // for ClearMessage

#import "PatternViewController.h"   // for UpdateEditBar
#import "StatePickerController.h"
#import "StateView.h"

@implementation StateView

// -----------------------------------------------------------------------------

- (id)initWithCoder:(NSCoder *)c
{
    self = [super initWithCoder:c];
    if (self) {
        [self setMultipleTouchEnabled:YES];
        
        // add gesture recognizer to this view
        UITapGestureRecognizer *tap1Gesture = [[UITapGestureRecognizer alloc]
                                               initWithTarget:self action:@selector(singleTap:)];
        tap1Gesture.numberOfTapsRequired = 1;
        tap1Gesture.numberOfTouchesRequired = 1;
        tap1Gesture.delegate = self;
        [self addGestureRecognizer:tap1Gesture];
    }
    return self;
}

// -----------------------------------------------------------------------------

- (void)drawRect:(CGRect)rect
{
    // fill box with color of current drawing state
    CGContextRef context = UIGraphicsGetCurrentContext();
    CGContextSetFillColorWithColor(context, currlayer->colorref[currlayer->drawingstate]);
    
    // use a 2 pixel border (color set in xib)
    CGContextFillRect(context, CGRectMake(2, 2, self.bounds.size.width-4, self.bounds.size.height-4));
}

// -----------------------------------------------------------------------------

- (void)singleTap:(UITapGestureRecognizer *)gestureRecognizer
{
    if ([gestureRecognizer state] == UIGestureRecognizerStateEnded) {
        ClearMessage();

        int numstates = currlayer->algo->NumCellStates();
        if (numstates == 2) {
            // no need for popover; just toggle drawing state
            currlayer->drawingstate = 1 - currlayer->drawingstate;
            UpdateEditBar();
            
        } else if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPad) {
            // create a popover controller for picking the new drawing state
            
            StatePickerController *statePicker = [[StatePickerController alloc] initWithNibName:nil bundle:nil];
            
            statePopover = [[UIPopoverController alloc] initWithContentViewController:statePicker];
            
            statePopover.delegate = self;
            
            // set popover size depending on number of states
            int wd = numstates <= 16 ? numstates * 32 : 16 * 32;
            int ht = (numstates + 15) / 16 * 32;
            // add border of 10 pixels (must match border in xib),
            // and add 1 extra pixel because we draw boxes around each cell
            wd += 21;
            ht += 21;
            statePopover.popoverContentSize = CGSizeMake(wd, ht);
            
            [statePopover presentPopoverFromRect:self.bounds
                                          inView:self
                        permittedArrowDirections:UIPopoverArrowDirectionUp
                                        animated:NO];
        
            statePicker = nil;
        }
    }
}

// -----------------------------------------------------------------------------

- (void)dismissStatePopover
{
    [statePopover dismissPopoverAnimated:NO];
    statePopover = nil;
}

// -----------------------------------------------------------------------------

- (void)popoverControllerDidDismissPopover:(UIPopoverController *)popoverController
{
    statePopover = nil;
}

// -----------------------------------------------------------------------------

@end
