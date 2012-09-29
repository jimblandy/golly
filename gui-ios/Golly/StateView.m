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

#include "prefs.h"      // for showicons
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

void DrawOneIcon(CGContextRef context, int x, int y, CGImageRef icon,
                 unsigned char deadr, unsigned char deadg, unsigned char deadb,
                 unsigned char liver, unsigned char liveg, unsigned char liveb)
{
    //!!! color is wrong (always white), and image is upside down
    CGContextDrawImage(context, CGRectMake(x,y,31,31), icon);

    // copy pixels from icon but convert black pixels to given dead cell color
    // and convert non-black pixels to given live cell color
    /*!!!
    int wd = icon->GetWidth();
    int ht = icon->GetHeight();
    bool multicolor = icon->GetDepth() > 1;
    wxBitmap pixmap(wd, ht, 32);
    
    wxAlphaPixelData pxldata(pixmap);
    if (pxldata) {
        wxAlphaPixelData::Iterator p(pxldata);
        wxAlphaPixelData icondata(*icon);
        if (icondata) {
            wxAlphaPixelData::Iterator iconpxl(icondata);
            for (int i = 0; i < ht; i++) {
                wxAlphaPixelData::Iterator pixmaprow = p;
                wxAlphaPixelData::Iterator iconrow = iconpxl;
                for (int j = 0; j < wd; j++) {
                    if (iconpxl.Red() || iconpxl.Green() || iconpxl.Blue()) {
                        if (multicolor) {
                            // use non-black pixel in multi-colored icon
                            if (swapcolors) {
                                p.Red()   = 255 - iconpxl.Red();
                                p.Green() = 255 - iconpxl.Green();
                                p.Blue()  = 255 - iconpxl.Blue();
                            } else {
                                p.Red()   = iconpxl.Red();
                                p.Green() = iconpxl.Green();
                                p.Blue()  = iconpxl.Blue();
                            }
                        } else {
                            // replace non-black pixel with live cell color
                            p.Red()   = liver;
                            p.Green() = liveg;
                            p.Blue()  = liveb;
                        }
                    } else {
                        // replace black pixel with dead cell color
                        p.Red()   = deadr;
                        p.Green() = deadg;
                        p.Blue()  = deadb;
                    }
                    p++;
                    iconpxl++;
                }
                // move to next row of pixmap
                p = pixmaprow;
                p.OffsetY(pxldata, 1);
                // move to next row of icon bitmap
                iconpxl = iconrow;
                iconpxl.OffsetY(icondata, 1);
            }
        }
    }
    dc.DrawBitmap(pixmap, x, y);
    !!!*/
}

// -----------------------------------------------------------------------------

- (void)drawRect:(CGRect)rect
{
    CGContextRef context = UIGraphicsGetCurrentContext();
    int state = currlayer->drawingstate;
    CGImageRef* iconmaps = currlayer->icons15x15;  //!!! 31x31

    // leave a 1 pixel border (color is set in xib)
    CGRect box = CGRectMake(1, 1, self.bounds.size.width-2, self.bounds.size.height-2);
    
    if (showicons && iconmaps && iconmaps[state]) {
        // fill box with background color then draw icon
        CGContextSetFillColorWithColor(context, currlayer->colorref[0]);
        CGContextFillRect(context, box);
        DrawOneIcon(context, 1, 1, iconmaps[state],
                    currlayer->cellr[0],     currlayer->cellg[0],     currlayer->cellb[0],
                    currlayer->cellr[state], currlayer->cellg[state], currlayer->cellb[state]);        
    } else {
        // fill box with color of current drawing state
        CGContextSetFillColorWithColor(context, currlayer->colorref[state]);
        CGContextFillRect(context, box);
    }
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
