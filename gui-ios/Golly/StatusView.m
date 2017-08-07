// This file is part of Golly.
// See docs/License.html for the copyright notice.

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
    int wd = self.bounds.size.width;
    int ht = self.bounds.size.height;
    
    // background color of status area depends on current algorithm
    [[UIColor colorWithRed:algoinfo[currlayer->algtype]->statusrgb.r / 255.0
                     green:algoinfo[currlayer->algtype]->statusrgb.g / 255.0
                      blue:algoinfo[currlayer->algtype]->statusrgb.b / 255.0
                     alpha:1.0] setFill];
    CGContextFillRect(context, dirtyrect);
    
    // draw thin gray line along top
    [[UIColor grayColor] setStroke];
    CGContextSetLineWidth(context, 1.0);
    CGContextMoveToPoint(context, 0, 0);
    CGContextAddLineToPoint(context, wd, 0);
    CGContextStrokePath(context);
    
    // use black for drawing text
    [[UIColor blackColor] setFill];
    
    // add code to limit amount of drawing if dirtyrect's height is smaller than given ht???

    NSString *line1 = [NSString stringWithCString:status1.c_str() encoding:NSUTF8StringEncoding];
    NSString *line2 = [NSString stringWithCString:status2.c_str() encoding:NSUTF8StringEncoding];
    
    // get font to draw lines (do only once)
    static UIFont *font = nil;
    if (font == nil) font = [UIFont systemFontOfSize:14];
    
    // set position of text in each line
    float lineht = ht / 3.0;
    CGRect rect1, rect2;
    rect1.size = [line1 sizeWithAttributes:@{NSFontAttributeName:font}];
    rect2.size = [line2 sizeWithAttributes:@{NSFontAttributeName:font}];
    rect1.origin.x = 5.0;
    rect2.origin.x = 5.0;
    rect1.origin.y = (lineht / 2.0) - (rect1.size.height / 2.0);
    rect2.origin.y = rect1.origin.y + lineht;
    
    // draw the top 2 lines
    [line1 drawInRect:rect1 withAttributes:@{NSFontAttributeName:font}];
    [line2 drawInRect:rect2 withAttributes:@{NSFontAttributeName:font}];

    if (status3.length() > 0) {
        // display message on bottom line
        NSString *line3 = [NSString stringWithCString:status3.c_str() encoding:NSUTF8StringEncoding];
        CGRect rect3;
        rect3.size = [line3 sizeWithAttributes:@{NSFontAttributeName:font}];
        rect3.origin.x = 5.0;
        rect3.origin.y = rect2.origin.y + lineht;
        [line3 drawInRect:rect3 withAttributes:@{NSFontAttributeName:font}];
    }
}

// -----------------------------------------------------------------------------

@end
