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
    int wd = CGImageGetWidth(icon);
    int ht = CGImageGetHeight(icon);
    int bytesPerPixel = 4;
    int bytesPerRow = bytesPerPixel * wd;
    int bitsPerComponent = 8;

    // allocate memory to store icon's RGBA bitmap data
    unsigned char* pxldata = (unsigned char*) calloc(wd * ht * 4, 1);
    if (pxldata == NULL) return;

    CGColorSpaceRef colorspace = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(pxldata, wd, ht,
        bitsPerComponent, bytesPerRow, colorspace,
        kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
    CGContextDrawImage(ctx, CGRectMake(0, 0, wd, ht), icon);
    CGContextRelease(ctx);
    
    bool multicolor = currlayer->multicoloricons;

    // pxldata now contains the icon bitmap in RGBA pixel format,
    // so convert black pixels to given dead cell color and non-black pixels
    // to given live cell color (but not if icon is multi-colored)
    int byte = 0;
    for (int i = 0; i < wd*ht; i++) {
        unsigned char r = pxldata[byte];
        unsigned char g = pxldata[byte+1];
        unsigned char b = pxldata[byte+2];
        if (r || g || b) {
            // non-black pixel
            if (multicolor) {
                // use non-black pixel in multi-colored icon
                if (swapcolors) {
                    pxldata[byte]   = 255 - pxldata[byte];
                    pxldata[byte+1] = 255 - pxldata[byte+1];
                    pxldata[byte+2] = 255 - pxldata[byte+2];
                }
            } else {
                // replace non-black pixel with live cell color
                pxldata[byte]   = liver;
                pxldata[byte+1] = liveg;
                pxldata[byte+2] = liveb;
            }
        } else {
            // replace black pixel with dead cell color
            pxldata[byte]   = deadr;
            pxldata[byte+1] = deadg;
            pxldata[byte+2] = deadb;
        }
        pxldata[byte + 3] = 255;    // ensure alpha channel is opaque
        byte += 4;
    }
    
    // create new icon image using modified pixel data and display it upside down
    CGContextSaveGState(context);
    CGContextTranslateCTM(context, x, y);
    CGContextScaleCTM(context, 1, -1);

    ctx = CGBitmapContextCreate(pxldata, wd, ht, 8, wd * 4, colorspace,
        kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
    CGImageRef newicon = CGBitmapContextCreateImage(ctx);
    CGContextRelease(ctx);
    CGColorSpaceRelease(colorspace);
    
    // draw the image (use 0,0 and -ve height because of above translation and scale by 1,-1)
    CGContextDrawImage(context, CGRectMake(0,0,wd,-ht), newicon);
    
    // clean up
    free(pxldata);
    CGImageRelease(newicon);
    CGContextRestoreGState(context);
}

// -----------------------------------------------------------------------------

- (void)drawRect:(CGRect)rect
{
    CGContextRef context = UIGraphicsGetCurrentContext();
    int state = currlayer->drawingstate;
    CGImageRef* iconmaps = currlayer->icons31x31;

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
