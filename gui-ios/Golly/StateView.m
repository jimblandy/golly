// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "prefs.h"      // for showicons
#include "layer.h"      // for currlayer
#include "algos.h"      // for gBitmapPtr
#include "status.h"     // for ClearMessage

#import "PatternViewController.h"   // for UpdateEditBar
#import "StatePickerController.h"
#import "StateView.h"

@implementation StateView

// -----------------------------------------------------------------------------

- (void)singleTap:(UITapGestureRecognizer *)gestureRecognizer
{
    if ([gestureRecognizer state] == UIGestureRecognizerStateEnded) {
        ClearMessage();

        int numstates = currlayer->algo->NumCellStates();
        if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPad) {
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
            // allow for switch button and label
            if (wd < 196) wd = 196;
            ht += 40;
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

void DrawOneIcon(CGContextRef context, int x, int y, gBitmapPtr icon,
                 unsigned char deadr, unsigned char deadg, unsigned char deadb,
                 unsigned char liver, unsigned char liveg, unsigned char liveb)
{
    // allocate memory to copy icon's RGBA data
    int wd = icon->wd;
    int ht = icon->ht;
    int iconbytes = wd * ht * 4;
    unsigned char* pxldata = (unsigned char*) malloc(iconbytes);
    if (pxldata == NULL) return;
    memcpy(pxldata, icon->pxldata, iconbytes);
    
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
                    pxldata[byte]   = 255 - r;
                    pxldata[byte+1] = 255 - g;
                    pxldata[byte+2] = 255 - b;
                }
            } else {
                // grayscale icon
                if (r == 255) {
                    // replace white pixel with live cell color
                    pxldata[byte]   = liver;
                    pxldata[byte+1] = liveg;
                    pxldata[byte+2] = liveb;
                } else {
                    // replace gray pixel with appropriate shade between
                    // live and dead cell colors
                    float frac = (float)r / 255.0;
                    pxldata[byte]   = (int)(deadr + frac * (liver - deadr) + 0.5);
                    pxldata[byte+1] = (int)(deadg + frac * (liveg - deadg) + 0.5);
                    pxldata[byte+2] = (int)(deadb + frac * (liveb - deadb) + 0.5);
                }
            }
        } else {
            // replace black pixel with dead cell color
            pxldata[byte]   = deadr;
            pxldata[byte+1] = deadg;
            pxldata[byte+2] = deadb;
        }
        pxldata[byte+3] = 255;      // ensure alpha channel is opaque
        byte += 4;
    }
    
    // create new icon image using modified pixel data and display it upside down
    CGColorSpaceRef colorspace = CGColorSpaceCreateDeviceRGB();
    CGContextSaveGState(context);
    CGContextTranslateCTM(context, x, y);
    CGContextScaleCTM(context, 1, -1);

    CGContextRef ctx = CGBitmapContextCreate(pxldata, wd, ht, 8, wd * 4, colorspace,
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
    
    gBitmapPtr* iconmaps = currlayer->icons31x31;

    // leave a 1 pixel border (color is set in xib)
    CGRect box = CGRectMake(1, 1, self.bounds.size.width-2, self.bounds.size.height-2);
    
    if (showicons && iconmaps && iconmaps[state]) {
        // fill box with background color then draw icon
        CGColorSpaceRef colorspace = CGColorSpaceCreateDeviceRGB();
        CGFloat components[4];
        components[0] = currlayer->cellr[0] / 255.0;
        components[1] = currlayer->cellg[0] / 255.0;
        components[2] = currlayer->cellb[0] / 255.0;
        components[3] = 1.0;    // alpha
        CGColorRef colorref = CGColorCreate(colorspace, components);
        CGColorSpaceRelease(colorspace);
        CGContextSetFillColorWithColor(context, colorref);
        CGContextFillRect(context, box);
        CGColorRelease(colorref);
        DrawOneIcon(context, 1, 1, iconmaps[state],
                    currlayer->cellr[0],     currlayer->cellg[0],     currlayer->cellb[0],
                    currlayer->cellr[state], currlayer->cellg[state], currlayer->cellb[state]);
    } else {
        // fill box with color of current drawing state
        CGColorSpaceRef colorspace = CGColorSpaceCreateDeviceRGB();
        CGFloat components[4];
        components[0] = currlayer->cellr[state] / 255.0;
        components[1] = currlayer->cellg[state] / 255.0;
        components[2] = currlayer->cellb[state] / 255.0;
        components[3] = 1.0;    // alpha
        CGColorRef colorref = CGColorCreate(colorspace, components);
        CGColorSpaceRelease(colorspace);
        CGContextSetFillColorWithColor(context, colorref);
        CGContextFillRect(context, box);
        CGColorRelease(colorref);
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
