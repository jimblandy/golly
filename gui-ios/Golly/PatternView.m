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

#include "bigint.h"
#include "lifealgo.h"
#include "viewport.h"

#include "utils.h"      // for event_checker
#include "prefs.h"      // for tilelayers, etc
#include "status.h"     // for ClearMessage
#include "render.h"     // for stacklayers, tilelayers
#include "layer.h"      // for currlayer, numlayers, etc
#include "control.h"    // for generating
#include "render.h"     // for InitOGLES2, DrawPattern
#include "view.h"       // for TouchBegan, TouchMoved, TouchEnded, etc

#import <QuartzCore/QuartzCore.h>
#import <OpenGLES/EAGLDrawable.h>
#import <OpenGLES/EAGL.h>
#import <OpenGLES/ES2/gl.h>
#import <OpenGLES/ES2/glext.h>

#import "PatternViewController.h"   // for PauseGenerating, ResumeGenerating, StopIfGenerating
#import "PatternView.h"

@implementation PatternView

// -----------------------------------------------------------------------------

// golbals for drawing with OpenGL ES

static EAGLContext *context;
static GLuint viewRenderbuffer = 0;
static GLuint viewFramebuffer = 0;

// -----------------------------------------------------------------------------

// override the default layer class (which is [CALayer class]) so that
// this view will be backed by a layer using OpenGL ES rendering

+ (Class)layerClass
{
	return [CAEAGLLayer class];
}

// -----------------------------------------------------------------------------

- (void)createFramebuffer
{
	// generate IDs for a framebuffer object and a color renderbuffer
	glGenFramebuffers(1, &viewFramebuffer);
	glGenRenderbuffers(1, &viewRenderbuffer);
	
	glBindFramebuffer(GL_FRAMEBUFFER, viewFramebuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, viewRenderbuffer);
    
	// associate the storage for the current render buffer with our CAEAGLLayer
	// so we can draw into a buffer that will later be rendered to screen
	[context renderbufferStorage:GL_RENDERBUFFER fromDrawable:(id<EAGLDrawable>)self.layer];
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, viewRenderbuffer);
    
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		NSLog(@"Error creating framebuffer object: %x", glCheckFramebufferStatus(GL_FRAMEBUFFER));
		Fatal("Failed to create frame buffer!");
	}
}

// -----------------------------------------------------------------------------

- (void)destroyFramebuffer
{
    // clean up any buffers we have allocated
	glDeleteFramebuffers(1, &viewFramebuffer);
	viewFramebuffer = 0;
	glDeleteRenderbuffers(1, &viewRenderbuffer);
	viewRenderbuffer = 0;
}

// -----------------------------------------------------------------------------

- (void)layoutSubviews
{
    // this is called when view is resized (on start up or when device is rotated or when toggling full screen)
    // so we need to create the framebuffer to be the same size
    
    // NSLog(@"layoutSubviews: wd=%f ht=%f", self.bounds.size.width, self.bounds.size.height);

    [EAGLContext setCurrentContext:context];
    
    CGRect frame = self.bounds;
    glViewport(0, 0, frame.size.width, frame.size.height);
    
    if (viewFramebuffer != 0 || viewRenderbuffer != 0) {
        [self destroyFramebuffer];
    }
    [self createFramebuffer];

    [self refreshPattern];
}

// -----------------------------------------------------------------------------

- (void)refreshPattern
{
    // x and y are always 0
    // float x = self.bounds.origin.x;
    // float y = self.bounds.origin.y;
    int wd = int(self.bounds.size.width);
    int ht = int(self.bounds.size.height);
    
    //!!! import from view.h if we ever support tiled layers
    int tileindex = -1;
    
    if ( numclones > 0 && numlayers > 1 && (stacklayers || tilelayers) ) {
        SyncClones();
    }
    if ( numlayers > 1 && tilelayers ) {
        if ( tileindex >= 0 && ( wd != GetLayer(tileindex)->view->getwidth() ||
                                 ht != GetLayer(tileindex)->view->getheight() ) ) {
            GetLayer(tileindex)->view->resize(wd, ht);
        }
    } else if ( wd != currlayer->view->getwidth() || ht != currlayer->view->getheight() ) {
        // set viewport size
        ResizeLayers(wd, ht);
    }

    [EAGLContext setCurrentContext:context];

    DrawPattern(tileindex);
    
    // display the buffer
    glBindRenderbuffer(GL_RENDERBUFFER, viewRenderbuffer);
    [context presentRenderbuffer:GL_RENDERBUFFER];
}

// -----------------------------------------------------------------------------

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldReceiveTouch:(UITouch *)touch
{
    if ([touch.view isKindOfClass:[UIButton class]]){
        // allow Restore button to work (when in fullscreen mode)
        return NO;
    }
    return YES;
}

// -----------------------------------------------------------------------------

- (void)zoomView:(UIPinchGestureRecognizer *)gestureRecognizer
{
    static CGFloat oldscale = 1.0;
    static CGPoint zoompt;
    
    UIGestureRecognizerState state = [gestureRecognizer state];
    
    if (state == UIGestureRecognizerStateBegan || state == UIGestureRecognizerStateChanged) {
        if (state == UIGestureRecognizerStateBegan) {
            // less confusing if we only get zoom point at start of pinch
            zoompt = [gestureRecognizer locationInView:self];
        }
        CGFloat newscale = [gestureRecognizer scale];
        if (newscale - oldscale < -0.1) {
            // fingers moved closer, so zoom out
            ZoomOutPos(zoompt.x, zoompt.y);
            oldscale = newscale;
        } else if (newscale - oldscale > 0.1) {
            // fingers moved apart, so zoom in
            ZoomInPos(zoompt.x, zoompt.y);
            oldscale = newscale;
        }
        
    } else if (state == UIGestureRecognizerStateEnded) {
        // reset scale
        [gestureRecognizer setScale:1.0];
        oldscale = 1.0;
    }
}

// -----------------------------------------------------------------------------

static int startx, starty;

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
    if ([touches count] == 1) {
        // get the only touch and remember its location for use in singleDrag
        UITouch *t = [touches anyObject];
        CGPoint currpt = [t locationInView:self];
        startx = (int)currpt.x;
        starty = (int)currpt.y;
    }
}

// -----------------------------------------------------------------------------

- (void)singleDrag:(UIPanGestureRecognizer *)gestureRecognizer
{    
    static CGPoint prevpt, delta;

    UIGestureRecognizerState state = [gestureRecognizer state];
    
    if (state == UIGestureRecognizerStateBegan) {
        prevpt = [gestureRecognizer locationInView:self];
        ClearMessage();
        
        // the current location is not where the initial touch occurred
        // so use the starting location saved in touchesBegan (otherwise
        // problems will occur when dragging a small paste image)
        TouchBegan(startx, starty);
        TouchMoved(prevpt.x, prevpt.y);
        
    } else if (state == UIGestureRecognizerStateChanged && [gestureRecognizer numberOfTouches] == 1) {
        CGPoint newpt = [gestureRecognizer locationInView:self];
        TouchMoved(newpt.x, newpt.y);
        if (currlayer->touchmode == movemode) {
            delta.x = newpt.x - prevpt.x;
            delta.y = newpt.y - prevpt.y;
            prevpt = newpt;
        }
        
    } else if (state == UIGestureRecognizerStateEnded) {
        if (currlayer->touchmode == movemode) {
            // if velocity is high then move further in current direction
            CGPoint velocity = [gestureRecognizer velocityInView:self];
            if (abs(velocity.x) > 1000.0 || abs(velocity.y) > 1000.0) {
                TouchMoved(prevpt.x + delta.x * abs(velocity.x/100.0), prevpt.y + delta.y * abs(velocity.y/100.0));
            }
        }
        TouchEnded();
        
    } else if (state == UIGestureRecognizerStateCancelled) {
        TouchEnded();
    }
}

// -----------------------------------------------------------------------------

- (void)moveView:(UIPanGestureRecognizer *)gestureRecognizer
{
    static TouchModes oldmode;
    static CGPoint prevpt, delta;
    
    UIGestureRecognizerState state = [gestureRecognizer state];
    
    if (state == UIGestureRecognizerStateBegan) {
        prevpt = [gestureRecognizer locationInView:self];
        ClearMessage();
        oldmode = currlayer->touchmode;
        currlayer->touchmode = movemode;
        TouchBegan(prevpt.x, prevpt.y);
        
    } else if (state == UIGestureRecognizerStateChanged && [gestureRecognizer numberOfTouches] == 2) {
        CGPoint newpt = [gestureRecognizer locationInView:self];
        TouchMoved(newpt.x, newpt.y);
        delta.x = newpt.x - prevpt.x;
        delta.y = newpt.y - prevpt.y;
        prevpt = newpt;

    } else if (state == UIGestureRecognizerStateEnded) {
        // if velocity is high then move further in current direction
        CGPoint velocity = [gestureRecognizer velocityInView:self];
        if (abs(velocity.x) > 1000.0 || abs(velocity.y) > 1000.0) {
            TouchMoved(prevpt.x + delta.x * abs(velocity.x/100.0), prevpt.y + delta.y * abs(velocity.y/100.0));
        }
        TouchEnded();
        currlayer->touchmode = oldmode;
        
    } else if (state == UIGestureRecognizerStateCancelled) {
        TouchEnded();
        currlayer->touchmode = oldmode;
    }
}

// -----------------------------------------------------------------------------

static UIActionSheet *selsheet;

- (void)doSelectionAction
{
    selsheet = [[UIActionSheet alloc]
        initWithTitle:nil
        delegate:self
        cancelButtonTitle:nil
        destructiveButtonTitle:nil
        otherButtonTitles:
        @"Remove",
        @"Cut",
        @"Copy",
        @"Clear",
        @"Clear Outside",
        @"Shrink",
        @"Fit",
        [NSString stringWithFormat:@"Random Fill (%d%%)", randomfill],
        @"Flip Top-Bottom",
        @"Flip Left-Right",
        @"Rotate Clockwise",
        @"Rotate Anticlockwise",
        @"Advance",
        @"Advance Outside",
        nil];
    
    [selsheet showInView:self];
}

// -----------------------------------------------------------------------------

static UIActionSheet *pastesheet;

- (void)doPasteAction
{
    pastesheet = [[UIActionSheet alloc]
        initWithTitle:nil
        delegate:self
        cancelButtonTitle:nil
        destructiveButtonTitle:nil
        otherButtonTitles:
        @"Abort",
        [NSString stringWithFormat:@"Paste (%@)",
            [NSString stringWithCString:GetPasteMode() encoding:NSUTF8StringEncoding]],
        @"Paste to Selection",
        @"Flip Top-Bottom",
        @"Flip Left-Right",
        @"Rotate Clockwise",
        @"Rotate Anticlockwise",
        nil];
    
    [pastesheet showInView:self];
}

// -----------------------------------------------------------------------------

static UIActionSheet *globalSheet;
static int globalButton;

- (void)doDelayedAction
{
    if (globalSheet == selsheet) {
        if (generating && globalButton >= 1 && globalButton <= 13 &&
            globalButton != 2 && globalButton != 5 && globalButton != 6) {
            // temporarily stop generating for all actions except Remove, Copy, Shrink, Fit
            PauseGenerating();
            if (event_checker > 0) {
                // try again after a short delay that gives time for NextGeneration() to terminate
                [self performSelector:@selector(doDelayedAction) withObject:nil afterDelay:0.01];
                return;
            }
        }
        switch (globalButton) {
            case 0:  RemoveSelection(); break;                      // WARNING: above test assumes Remove is index 0
            case 1:  CutSelection(); break;
            case 2:  CopySelection(); break;                        // WARNING: above test assumes Copy is index 2
            case 3:  ClearSelection(); break;
            case 4:  ClearOutsideSelection(); break;
            case 5:  ShrinkSelection(false); break;                 // WARNING: above test assumes Shrink is index 5
            case 6:  FitSelection(); break;                         // WARNING: above test assumes Fit is index 6
            case 7:  RandomFill(); break;
            case 8:  FlipSelection(true); break;
            case 9:  FlipSelection(false); break;
            case 10: RotateSelection(true); break;
            case 11: RotateSelection(false); break;
            case 12: currlayer->currsel.Advance(); break;
            case 13: currlayer->currsel.AdvanceOutside(); break;    // WARNING: above test assumes 13 is last index
            default: break;
        }
        ResumeGenerating();
        
    } else if (globalSheet == pastesheet) {
        switch (globalButton) {
            case 0:  AbortPaste(); break;
            case 1:  DoPaste(false); break;
            case 2:  DoPaste(true); break;
            case 3:  FlipPastePattern(true); break;
            case 4:  FlipPastePattern(false); break;
            case 5:  RotatePastePattern(true); break;
            case 6:  RotatePastePattern(false); break;
            default: break;
        }
        UpdateEverything();
    }
}

// -----------------------------------------------------------------------------

// called when the user selects an option in a UIActionSheet

- (void)actionSheet:(UIActionSheet *)sheet didDismissWithButtonIndex:(NSInteger)buttonIndex
{
    // user interaction is disabled at this moment, which is a problem if Warning or BeginProgress
    // gets called (their OK/Cancel buttons won't work) so we have to call the appropriate
    // action code AFTER this callback has finished and user interaction restored
    globalSheet = sheet;
    globalButton = buttonIndex;
    [self performSelector:@selector(doDelayedAction) withObject:nil afterDelay:0.01];
}

// -----------------------------------------------------------------------------

static double prevtime = 0.0;   // used to detect a double tap

- (void)singleTap:(UITapGestureRecognizer *)gestureRecognizer
{
    if ([gestureRecognizer state] == UIGestureRecognizerStateEnded) {
        CGPoint pt = [gestureRecognizer locationInView:self];
        ClearMessage();
        if (TimeInSeconds() - prevtime < 0.3) {
            // double tap
            if (waitingforpaste && PointInPasteImage(pt.x, pt.y) && !drawingcells) {
                // if generating then stop (consistent with doPaste in PatternViewController.m)
                StopIfGenerating();
                ClearMessage();
                // now display action sheet that lets user abort/paste/flip/rotate/etc
                [self doPasteAction];
            
            } else if (currlayer->touchmode == selectmode && SelectionExists() && PointInSelection(pt.x, pt.y)) {
                // display action sheet that lets user remove/cut/copy/clear/etc
                [self doSelectionAction];
            
            } else if (currlayer->touchmode == drawmode) {
                // do the drawing
                TouchBegan(pt.x, pt.y);
                TouchEnded();
            }
            prevtime = 0.0;
        
        } else {
            // single tap
            TouchBegan(pt.x, pt.y);
            TouchEnded();
            prevtime = TimeInSeconds();
        }
    }
}

// -----------------------------------------------------------------------------

- (id)initWithCoder:(NSCoder *)c
{
    self = [super initWithCoder:c];
    if (self) {
        
        // init the OpenGL ES stuff (based on Apple's GLPaint sample code):
        
		CAEAGLLayer *eaglLayer = (CAEAGLLayer *)self.layer;
		eaglLayer.opaque = YES;
        
        // note that we're using OpenGL ES 2.0
		context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
		if (!context || ![EAGLContext setCurrentContext:context]) {
			self = nil;
			return self;
		}
		
		// set the view's scale factor
		self.contentScaleFactor = 1.0;
        
        if (!InitOGLES2()) Warning("InitOGLES2 failed!");
        
        // we only do 2D drawing
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_DITHER);
        glDisable(GL_STENCIL_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        // add gesture recognizers to this view:
        
        [self setMultipleTouchEnabled:YES];
        
        // pinch gestures will zoom in/out
        UIPinchGestureRecognizer *pinchGesture = [[UIPinchGestureRecognizer alloc]
                                                  initWithTarget:self action:@selector(zoomView:)];
        pinchGesture.delegate = self;
        [self addGestureRecognizer:pinchGesture];
        
        // one-finger pan gestures will be used to draw/pick/select/etc, depending on the current touch mode
        UIPanGestureRecognizer *pan1Gesture = [[UIPanGestureRecognizer alloc]
                                               initWithTarget:self action:@selector(singleDrag:)];
        pan1Gesture.minimumNumberOfTouches = 1;
        pan1Gesture.maximumNumberOfTouches = 1;
        pan1Gesture.delegate = self;
        [self addGestureRecognizer:pan1Gesture];
        
        // two-finger pan gestures will move the view
        UIPanGestureRecognizer *pan2Gesture = [[UIPanGestureRecognizer alloc]
                                               initWithTarget:self action:@selector(moveView:)];
        pan2Gesture.minimumNumberOfTouches = 2;
        pan2Gesture.maximumNumberOfTouches = 2;
        pan2Gesture.delegate = self;
        [self addGestureRecognizer:pan2Gesture];
        
        // single-taps will do various actions depending on the current touch mode
        UITapGestureRecognizer *tap1Gesture = [[UITapGestureRecognizer alloc]
                                               initWithTarget:self action:@selector(singleTap:)];
        tap1Gesture.numberOfTapsRequired = 1;
        tap1Gesture.numberOfTouchesRequired = 1;
        tap1Gesture.delegate = self;
        [self addGestureRecognizer:tap1Gesture];
        
        /* too many problems if we have gesture recognizers for single-taps and double-taps:
        
        // double-taps will do various actions depending on the location
        UITapGestureRecognizer *tap2Gesture = [[UITapGestureRecognizer alloc]
                                               initWithTarget:self action:@selector(doubleTap:)];
        tap2Gesture.numberOfTapsRequired = 2;
        tap2Gesture.numberOfTouchesRequired = 1;
        tap2Gesture.delegate = self;
        [self addGestureRecognizer:tap2Gesture];
        
        // only do a single-tap if double-tap is not detected
        // (this works but the delay is way too long)
        // [tap1Gesture requireGestureRecognizerToFail:tap2Gesture];
        
        */
    }
    return self;
}

// -----------------------------------------------------------------------------

@end
