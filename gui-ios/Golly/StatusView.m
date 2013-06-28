/*** /
 
 This file is part of Golly, a Game of Life Simulator.
 Copyright (C) 2013 Andrew Trevorrow and Tomas Rokicki.
 
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

#include "status.h"     // for UpdateStatusLines, status1, status2, status3
#include "algos.h"      // for algoinfo
#include "layer.h"      // for currlayer

#import "StatusView.h"

@implementation StatusView

// -----------------------------------------------------------------------------

- (void)drawRect:(CGRect)dirtyrect
{
    UpdateStatusLines();    // sets status1 and status2
    
    CGContextRef context = UIGraphicsGetCurrentContext();
    // int wd = self.bounds.size.width;
    int ht = self.bounds.size.height;
    
    // background color of status area depends on current algorithm
    [[UIColor colorWithRed:algoinfo[currlayer->algtype]->statusrgb.r / 255.0
                     green:algoinfo[currlayer->algtype]->statusrgb.g / 255.0
                      blue:algoinfo[currlayer->algtype]->statusrgb.b / 255.0
                     alpha:1.0] setFill];
    CGContextFillRect(context, dirtyrect);
    
    // draw thin gray line along bottom
    /* only do this if edit toolbar is ever hidden
    [[UIColor grayColor] setStroke];
    CGContextSetLineWidth(context, 1.0);
    CGContextMoveToPoint(context, 0, ht);
    CGContextAddLineToPoint(context, wd, ht);
    CGContextStrokePath(context);
    */
    
    // use black for drawing text
    [[UIColor blackColor] setFill];
    
    // add code to limit amount of drawing if dirtyrect's height is smaller than given ht???

    NSString *line1 = [NSString stringWithCString:status1.c_str() encoding:NSUTF8StringEncoding];
    NSString *line2 = [NSString stringWithCString:status2.c_str() encoding:NSUTF8StringEncoding];
    
    // get font to draw lines (do only once)
    static UIFont *font = nil;
    if (font == nil) font = [UIFont systemFontOfSize:13];
    
    // set position of text in each line
    float lineht = ht / 3.0;
    CGRect rect1, rect2;
    rect1.size = [line1 sizeWithFont:font];
    rect2.size = [line2 sizeWithFont:font];
    rect1.origin.x = 5.0;
    rect2.origin.x = 5.0;
    rect1.origin.y = (lineht / 2.0) - (rect1.size.height / 2.0);
    rect2.origin.y = rect1.origin.y + lineht;
    
    // draw the top 2 lines
    [line1 drawInRect:rect1 withFont:font];
    [line2 drawInRect:rect2 withFont:font];

    if (status3.length() > 0) {
        // display message on bottom line
        NSString *line3 = [NSString stringWithCString:status3.c_str() encoding:NSUTF8StringEncoding];
        CGRect rect3;
        rect3.size = [line3 sizeWithFont:font];
        rect3.origin.x = 5.0;
        rect3.origin.y = rect2.origin.y + lineht;
        [line3 drawInRect:rect3 withFont:font];
    }
}

// -----------------------------------------------------------------------------

@end
