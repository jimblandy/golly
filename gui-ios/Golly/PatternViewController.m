// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "viewport.h"    // for MAX_MAG

#include "utils.h"       // for Warning, event_checker
#include "status.h"      // for SetMessage, DisplayMessage
#include "algos.h"       // for InitAlgorithms, algoinfo
#include "prefs.h"       // for GetPrefs, SavePrefs, userdir, etc
#include "layer.h"       // for AddLayer, currlayer
#include "file.h"        // for NewPattern
#include "control.h"     // for generating, StartGenerating, etc
#include "view.h"        // for nopattupdate, SmallScroll, drawingcells
#include "undo.h"        // for currlayer->undoredo->...

#import "GollyAppDelegate.h"        // for EnableTabBar
#import "InfoViewController.h"
#import "SaveViewController.h"
#import "RuleViewController.h"
#import "PatternViewController.h"

@implementation PatternViewController

// -----------------------------------------------------------------------------

// global stuff needed in UpdatePattern, UpdateStatus, etc

static PatternViewController *globalController = nil;
static PatternView *globalPatternView = nil;
static StatusView *globalStatusView = nil;
static StateView *globalStateView = nil;
static UIView *globalProgress = nil;
static UILabel *globalTitle = nil;

static bool cancelProgress = false;     // cancel progress dialog?
static double progstart, prognext;      // for progress timing
static int progresscount = 0;           // if > 0 then BeginProgress has been called
static int pausecount = 0;              // if > 0 then genTimer needs to be restarted

// -----------------------------------------------------------------------------

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self) {
        self.title = @"Pattern";
        self.tabBarItem.image = [UIImage imageNamed:@"pattern.png"];
    }
    return self;
}

// -----------------------------------------------------------------------------

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    
    // free up as much memory as possible
    if (numlayers > 0) DeleteOtherLayers();
    if (waitingforpaste) AbortPaste();
    [self doNew:self];
    
    // Warning hangs if called here, so use DisplayMessage for now!!!
    // (may need non-modal version via extra flag???)
    // Warning("Memory warning occurred, so empty universe created.");
    DisplayMessage("Memory warning occurred, so empty universe created.");
}

// -----------------------------------------------------------------------------

static void CreateDocSubdir(NSString *subdirname)
{
    // create given subdirectory inside Documents
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *path = [[paths objectAtIndex:0] stringByAppendingPathComponent:subdirname];
    if ([[NSFileManager defaultManager] fileExistsAtPath:path]) {
        // do nothing if subdir already exists
    } else {
        NSError *error;
        if (![[NSFileManager defaultManager] createDirectoryAtPath:path
                                       withIntermediateDirectories:NO
                                                        attributes:nil
                                                             error:&error]) {
            NSLog(@"Error creating %@ subdirectory: %@", subdirname, error);
        }
    }
}

// -----------------------------------------------------------------------------

static void InitPaths()
{
    // init userdir to directory containing user's data
    userdir = [NSHomeDirectory() cStringUsingEncoding:NSUTF8StringEncoding];
    userdir += "/";

    // init savedir to userdir/Documents/Saved/
    savedir = userdir + "Documents/Saved/";
    CreateDocSubdir(@"Saved");

    // init downloaddir to userdir/Documents/Downloads/
    downloaddir = userdir + "Documents/Downloads/";
    CreateDocSubdir(@"Downloads");

    // init userrules to userdir/Documents/Rules/
    userrules = userdir + "Documents/Rules/";
    CreateDocSubdir(@"Rules");

    // supplied patterns, rules, help are bundled inside Golly.app
    NSString *appdir = [[NSBundle mainBundle] resourcePath];
    supplieddir = [appdir cStringUsingEncoding:NSUTF8StringEncoding];
    supplieddir += "/";
    patternsdir = supplieddir + "Patterns/";
    rulesdir = supplieddir + "Rules/";
    helpdir = supplieddir + "Help/";

    // init tempdir to userdir/tmp/
    tempdir = userdir + "tmp/";

    // init path to file that stores clipboard data
    clipfile = tempdir + "golly_clipboard";
    
    // init path to file that stores user preferences
    prefsfile = userdir + "Library/Preferences/GollyPrefs";

#if 0
    NSLog(@"userdir =     %s", userdir.c_str());
    NSLog(@"savedir =     %s", savedir.c_str());
    NSLog(@"downloaddir = %s", downloaddir.c_str());
    NSLog(@"userrules =   %s", userrules.c_str());
    NSLog(@"supplieddir = %s", supplieddir.c_str());
    NSLog(@"patternsdir = %s", patternsdir.c_str());
    NSLog(@"rulesdir =    %s", rulesdir.c_str());
    NSLog(@"helpdir =     %s", helpdir.c_str());
    NSLog(@"tempdir =     %s", tempdir.c_str());
    NSLog(@"clipfile =    %s", clipfile.c_str());
    NSLog(@"prefsfile =   %s", prefsfile.c_str());
#endif
}

// -----------------------------------------------------------------------------

- (void)viewDidLoad
{
    [super viewDidLoad];
    
	// now do additional setup after loading the view from the xib file
    
    // init global pointers
    globalController = self;
    globalPatternView = pattView;
    globalStatusView = statView;
    globalStateView = stateView;
    globalProgress = progressView;
    globalTitle = progressTitle;
    
    static bool firstload = true;
    if (firstload) {
        firstload = false;          // only do the following once
        SetMessage("This is Golly 1.2 for iOS.  Copyright 2017 The Golly Gang.");
        MAX_MAG = 5;                // maximum cell size = 32x32
        InitAlgorithms();           // must initialize algoinfo first
        InitPaths();                // init userdir, etc (must be before GetPrefs)
        GetPrefs();                 // load user's preferences
        SetMinimumStepExponent();   // for slowest speed
        AddLayer();                 // create initial layer (sets currlayer)
        NewPattern();               // create new, empty universe
    }
    
    // ensure pattView is composited underneath toolbar, otherwise
    // bottom toolbar can be obscured when device is rotated left/right
    // (no longer needed)
    // [[pattView layer] setZPosition:-1];

    // we draw background areas of pattView and statView so set to transparent (presumably a tad faster)
    [pattView setBackgroundColor:nil];
    [statView setBackgroundColor:nil];
    
    progressView.hidden = YES;

    genTimer = nil;
    
    // init buttons
    [self toggleStartStopButton];
    resetButton.enabled = NO;
    undoButton.enabled = NO;
    redoButton.enabled = NO;
    actionButton.enabled = NO;
    infoButton.enabled = NO;
    restoreButton.hidden = YES;

    // deselect step/scale controls (these have Momentary attribute set)
    [stepControl setSelectedSegmentIndex:-1];
    [scaleControl setSelectedSegmentIndex:-1];
    
    // init touch mode
    [modeControl setSelectedSegmentIndex:currlayer->touchmode];

    [self updateDrawingState];
}

// -----------------------------------------------------------------------------

- (void)viewDidUnload
{
    [super viewDidUnload];
    
    SavePrefs();
    
    // release all outlets
    pattView = nil;
    statView = nil;
    startStopButton = nil;
    restoreButton = nil;
    resetButton = nil;
    undoButton = nil;
    redoButton = nil;
    actionButton = nil;
    infoButton = nil;
    stepControl = nil;
    scaleControl = nil;
    modeControl = nil;
    stateLabel = nil;
    stateView = nil;
    topBar = nil;
    editBar = nil;
    bottomBar = nil;
    progressView = nil;
    progressTitle = nil;
    progressMessage = nil;
    progressBar = nil;
    cancelButton = nil;
}

// -----------------------------------------------------------------------------

- (void)viewWillDisappear:(BOOL)animated
{
	[super viewWillDisappear:animated];

    SetMessage("");

    // stop generating, but only if PauseGenTimer() hasn't been called
    if (pausecount == 0) [self stopIfGenerating];
}

// -----------------------------------------------------------------------------

- (void)viewDidAppear:(BOOL)animated
{
	[super viewDidAppear:animated];
    
    UpdateEverything();     // calls [pattView refreshPattern]
    
    // restart genTimer if PauseGenTimer() was called earlier
    if (pausecount > 0) RestartGenTimer();
}

// -----------------------------------------------------------------------------

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
    // return YES for supported orientations
    return YES;
}

// -----------------------------------------------------------------------------

- (void)updateDrawingState;
{
    // reset drawing state if it's no longer valid (due to algo/rule change)
    if (currlayer->drawingstate >= currlayer->algo->NumCellStates()) {
        currlayer->drawingstate = 1;
    }

    [stateLabel setText:[NSString stringWithFormat:@"State=%d", currlayer->drawingstate]];
    [stateView setNeedsDisplay];
}

// -----------------------------------------------------------------------------

- (void)updateButtons
{
    resetButton.enabled = currlayer->algo->getGeneration() > currlayer->startgen;
    undoButton.enabled = currlayer->undoredo->CanUndo();
    redoButton.enabled = currlayer->undoredo->CanRedo();
    actionButton.enabled = SelectionExists();
    infoButton.enabled = currlayer->currname != "untitled";
    [modeControl setSelectedSegmentIndex:currlayer->touchmode];
}

// -----------------------------------------------------------------------------

- (void)toggleStartStopButton
{
    if (generating) {
        [startStopButton setTitle:@"STOP"
                         forState:UIControlStateNormal];
        [startStopButton setTitleColor:[UIColor redColor]
                              forState:UIControlStateNormal];
    } else {
        [startStopButton setTitle:@"START"
                         forState:UIControlStateNormal];
        [startStopButton setTitleColor:[UIColor colorWithRed:0.2 green:0.7 blue:0.0 alpha:1.0]
                              forState:UIControlStateNormal];
    }
}

// -----------------------------------------------------------------------------

- (void)stopIfGenerating
{
    if (generating) {
        [self stopGenTimer];
        StopGenerating();
        // generating now false
        [self toggleStartStopButton];
    }
}

// -----------------------------------------------------------------------------

- (IBAction)doNew:(id)sender
{
    if (drawingcells) return;
    
    // undo/redo history is about to be cleared so no point calling RememberGenFinish
    // if we're generating a (possibly large) pattern
    bool saveundo = allowundo;
    allowundo = false;
    [self stopIfGenerating];
    allowundo = saveundo;

    if (event_checker > 0) {
        // try again after a short delay that gives time for NextGeneration() to terminate
        [self performSelector:@selector(doNew:) withObject:sender afterDelay:0.01];
        return;
    }
    
    ClearMessage();
    NewPattern();
    
    [modeControl setSelectedSegmentIndex:currlayer->touchmode];
    
    [self updateDrawingState];
    [self updateButtons];
    
    [statView setNeedsDisplay];
    [pattView refreshPattern];
}

// -----------------------------------------------------------------------------

- (IBAction)doInfo:(id)sender
{
    if (drawingcells) return;
    
    // if generating then just pause the timer so doGeneration won't be called;
    // best not to call PauseGenerating() because it calls StopGenerating()
    // which might result in a lengthy file save for undo/redo
    PauseGenTimer();
    
    ClearMessage();
    
    // display contents of current pattern file in a modal view
    InfoViewController *modalInfoController = [[InfoViewController alloc] initWithNibName:nil bundle:nil];
    
    [modalInfoController setModalPresentationStyle:UIModalPresentationFullScreen];
    [self presentViewController:modalInfoController animated:YES completion:nil];
    
    modalInfoController = nil;
    
    // RestartGenTimer() will be called in viewDidAppear
}

// -----------------------------------------------------------------------------

- (IBAction)doSave:(id)sender
{
    if (drawingcells) return;
    
    [self stopIfGenerating];
    
    if (event_checker > 0) {
        // try again after a short delay
        [self performSelector:@selector(doSave:) withObject:sender afterDelay:0.01];
        return;
    }
    
    ClearMessage();
    
    // let user save current pattern in a file via a modal view
    SaveViewController *modalSaveController = [[SaveViewController alloc] initWithNibName:nil bundle:nil];
    
    [modalSaveController setModalPresentationStyle:UIModalPresentationFormSheet];
    [self presentViewController:modalSaveController animated:YES completion:nil];
    
    modalSaveController = nil;
}

// -----------------------------------------------------------------------------

- (IBAction)doUndo:(id)sender
{
    if (drawingcells) return;
    
    [self stopIfGenerating];
    
    if (event_checker > 0) {
        // try again after a short delay
        [self performSelector:@selector(doUndo:) withObject:sender afterDelay:0.01];
        return;
    }
    
    ClearMessage();
    currlayer->undoredo->UndoChange();
    UpdateEverything();
}

// -----------------------------------------------------------------------------

- (IBAction)doRedo:(id)sender
{
    if (drawingcells) return;
    
    [self stopIfGenerating];
    
    if (event_checker > 0) {
        // try again after a short delay
        [self performSelector:@selector(doRedo:) withObject:sender afterDelay:0.01];
        return;
    }
    
    ClearMessage();
    currlayer->undoredo->RedoChange();
    UpdateEverything();
}

// -----------------------------------------------------------------------------

- (IBAction)doReset:(id)sender
{
    if (drawingcells) return;
    
    [self stopIfGenerating];
    
    if (event_checker > 0) {
        // try again after a short delay
        [self performSelector:@selector(doReset:) withObject:sender afterDelay:0.01];
        return;
    }
    
    ClearMessage();
    ResetPattern();
    UpdateEverything();
}

// -----------------------------------------------------------------------------

- (IBAction)doStartStop:(id)sender
{
    if (drawingcells) return;
    // this can happen on iPad if user taps Start/Stop button while another finger
    // is currently drawing cells
    
    ClearMessage();
    if (generating) {
        [self stopGenTimer];
        StopGenerating();
        // generating is now false
        [self toggleStartStopButton];
        // can't call [self updateButtons] here because if event_checker is > 0
        // then StopGenerating hasn't called RememberGenFinish, and so CanUndo/CanRedo
        // might not return correct results
        bool canreset = currlayer->algo->getGeneration() > currlayer->startgen;
        resetButton.enabled = canreset;
        undoButton.enabled = allowundo && (canreset || currlayer->undoredo->CanUndo());
        redoButton.enabled = NO;
    
    } else if (StartGenerating()) {
        // generating is now true
        [self toggleStartStopButton];            
        [self startGenTimer];
        // don't call [self updateButtons] here because we want user to
        // be able to stop generating by tapping Reset/Undo buttons
        resetButton.enabled = YES;
        undoButton.enabled = allowundo;
        redoButton.enabled = NO;
    }
    pausecount = 0;     // play safe
}

// -----------------------------------------------------------------------------

- (void)startGenTimer
{
    float interval = 1.0f/60.0f;
    
    // increase interval if user wants a delay
    if (currlayer->currexpo < 0) {
        interval = GetCurrentDelay() / 1000.0f;
    }
    
    // create a repeating timer
    genTimer = [NSTimer scheduledTimerWithTimeInterval:interval
                                                target:self
                                              selector:@selector(doGeneration:)
                                              userInfo:nil
                                               repeats:YES];
}

// -----------------------------------------------------------------------------

- (void)stopGenTimer
{
    [genTimer invalidate];
    genTimer = nil;
}

// -----------------------------------------------------------------------------

// called after genTimer fires

- (void)doGeneration:(NSTimer*)timer
{
    if (event_checker > 0 || progresscount > 0) return;
    
    // advance by current step size
    NextGeneration(true);
    
    [statView setNeedsDisplay];
    [pattView refreshPattern];
}

// -----------------------------------------------------------------------------

- (IBAction)doNext:(id)sender
{
    if (drawingcells) return;
    
    [self stopIfGenerating];
    
    if (event_checker > 0) {
        // try again after a short delay
        [self performSelector:@selector(doNext:) withObject:sender afterDelay:0.01];
        return;
    }
    
    ClearMessage();
    NextGeneration(false);
    
    [statView setNeedsDisplay];
    [pattView refreshPattern];
    [self updateButtons];
}

// -----------------------------------------------------------------------------

- (IBAction)doStep:(id)sender
{
    if (drawingcells) return;
    
    [self stopIfGenerating];
    
    if (event_checker > 0) {
        // try again after a short delay
        [self performSelector:@selector(doStep:) withObject:sender afterDelay:0.01];
        return;
    }
    
    ClearMessage();
    NextGeneration(true);
    
    [statView setNeedsDisplay];
    [pattView refreshPattern];
    [self updateButtons];
}

// -----------------------------------------------------------------------------

- (IBAction)doFit:(id)sender
{
    ClearMessage();
    FitInView(1);

    [statView setNeedsDisplay];
    [pattView refreshPattern];
}

// -----------------------------------------------------------------------------

- (IBAction)doChangeStep:(id)sender
{
    ClearMessage();
    switch([sender selectedSegmentIndex])
    {
        case 0:
        {
            // go slower by decrementing step exponent
            if (currlayer->currexpo > minexpo) {
                currlayer->currexpo--;
                SetGenIncrement();
                if (generating && currlayer->currexpo < 0) {
                    // increase timer interval
                    [self stopGenTimer];
                    [self startGenTimer];
                }
            } else {
                Beep();
            }
        } break;
        
        case 1:
        {
            // reset step exponent to 0
            currlayer->currexpo = 0;
            SetGenIncrement();
            if (generating) {
                // reset timer interval to max speed
                [self stopGenTimer];
                [self startGenTimer];
            }
        } break;
        
        case 2:
        {
            // go faster by incrementing step exponent
            currlayer->currexpo++;
            SetGenIncrement();
            if (generating && currlayer->currexpo <= 0) {
                // reduce timer interval
                [self stopGenTimer];
                [self startGenTimer];
            }
        } break;
    }

    // only need to update status info
    [statView setNeedsDisplay];
}

// -----------------------------------------------------------------------------

- (IBAction)doChangeScale:(id)sender
{
    ClearMessage();
    switch([sender selectedSegmentIndex])
    {
        case 0:
        {
            // zoom out
            currlayer->view->unzoom();
            [statView setNeedsDisplay];
            [pattView refreshPattern];
        } break;
        
        case 1:
        {
            // set scale to 1:1
            if (currlayer->view->getmag() != 0) {
                currlayer->view->setmag(0);
                [statView setNeedsDisplay];
                [pattView refreshPattern];
            }
        } break;
        
        case 2:
        {
            // zoom in
            if (currlayer->view->getmag() < MAX_MAG) {
                currlayer->view->zoom();
                [statView setNeedsDisplay];
                [pattView refreshPattern];
            } else {
                Beep();
            }
        } break;
    }
}

// -----------------------------------------------------------------------------

- (IBAction)doChangeMode:(id)sender
{
    ClearMessage();
    switch([sender selectedSegmentIndex])
    {
        case 0: currlayer->touchmode = drawmode; break;
        case 1: currlayer->touchmode = pickmode; break;
        case 2: currlayer->touchmode = selectmode; break;
        case 3: currlayer->touchmode = movemode; break;
    }
}

// -----------------------------------------------------------------------------

- (IBAction)doMiddle:(id)sender
{
    ClearMessage();
    if (currlayer->originx == bigint::zero && currlayer->originy == bigint::zero) {
        currlayer->view->center();
    } else {
        // put cell saved by ChangeOrigin (not yet implemented!!!) in middle
        currlayer->view->setpositionmag(currlayer->originx, currlayer->originy,
                                        currlayer->view->getmag());
    }
    [statView setNeedsDisplay];
    [pattView refreshPattern];
}

// -----------------------------------------------------------------------------

- (IBAction)doSelectAll:(id)sender
{
    ClearMessage();
    SelectAll();
}

// -----------------------------------------------------------------------------

- (IBAction)doAction:(id)sender
{
    ClearMessage();
    [pattView doSelectionAction];
}

// -----------------------------------------------------------------------------

- (IBAction)doPaste:(id)sender
{
    if (drawingcells) return;
    
    [self stopIfGenerating];
    
    if (event_checker > 0) {
        // try again after a short delay
        [self performSelector:@selector(doPaste:) withObject:sender afterDelay:0.01];
        return;
    }
    
    ClearMessage();
    if (waitingforpaste) {
        [pattView doPasteAction];
    } else {
        PasteClipboard();
        [pattView refreshPattern];
    }
}

// -----------------------------------------------------------------------------

- (IBAction)doRule:(id)sender
{
    if (drawingcells) return;
    
    [self stopIfGenerating];
    
    if (event_checker > 0) {
        // try again after a short delay
        [self performSelector:@selector(doRule:) withObject:sender afterDelay:0.01];
        return;
    }

    ClearMessage();

    // let user change current algo/rule in a modal view
    RuleViewController *modalRuleController = [[RuleViewController alloc] initWithNibName:nil bundle:nil];

    [modalRuleController setModalPresentationStyle:UIModalPresentationFullScreen];
    [self presentViewController:modalRuleController animated:YES completion:nil];
    
    modalRuleController = nil;
}

// -----------------------------------------------------------------------------

- (IBAction)toggleFullScreen:(id)sender
{
    if (fullscreen) {
        ShowTabBar(YES);
        topBar.hidden = NO;
        editBar.hidden = NO;
        bottomBar.hidden = NO;
        statView.hidden = NO;
        // recalculate pattView's frame from scratch (in case device was rotated)
        CGFloat barht = topBar.bounds.size.height;
        CGFloat topht = barht * 2 + statView.bounds.size.height;
        CGFloat x = pattView.superview.frame.origin.x;
        CGFloat y = pattView.superview.frame.origin.y + topht;
        CGFloat wd = pattView.superview.frame.size.width;
        CGFloat ht = pattView.superview.frame.size.height - (topht + barht + TabBarHeight());
        pattView.frame = CGRectMake(x, y, wd, ht);
        [statView setNeedsDisplay];
    } else {
        ShowTabBar(NO);
        topBar.hidden = YES;
        editBar.hidden = YES;
        bottomBar.hidden = YES;
        statView.hidden = YES;
        pattView.frame = pattView.superview.frame;
    }
    
    fullscreen = !fullscreen;
    restoreButton.hidden = !fullscreen;
    [pattView refreshPattern];
    UpdateEditBar();
}

// -----------------------------------------------------------------------------

- (IBAction)doCancel:(id)sender
{
    cancelProgress = true;
}

// -----------------------------------------------------------------------------

- (void)enableInteraction:(BOOL)enable
{
    topBar.userInteractionEnabled = enable;
    editBar.userInteractionEnabled = enable;
    bottomBar.userInteractionEnabled = enable;
    pattView.userInteractionEnabled = enable;
    EnableTabBar(enable);
}

// -----------------------------------------------------------------------------

- (void)updateProgressBar:(float)fraction
{
    progressBar.progress = fraction;
    
    // show estimate of time remaining
    double elapsecs = TimeInSeconds() - progstart;
    double remsecs = fraction > 0.0 ? (elapsecs / fraction) - elapsecs : 999.0;
    [progressMessage setText:[NSString stringWithFormat:@"Estimated time remaining (in secs): %d", int(remsecs+0.5)]];
    
    // need following to see progress view and bar update
    event_checker++;
    [[NSRunLoop currentRunLoop] runUntilDate:[NSDate date]];
    event_checker--;
}

@end

// =============================================================================

// global routines used by external code:

void UpdatePattern()
{
    [globalPatternView refreshPattern];
}

// -----------------------------------------------------------------------------

void UpdateStatus()
{
    if (inscript || currlayer->undoredo->doingscriptchanges) return;
    if (!fullscreen) {
        [globalStatusView setNeedsDisplay];
    }
}

// -----------------------------------------------------------------------------

void UpdateEditBar()
{
    if (!fullscreen) {
        [globalController updateButtons];
        [globalController updateDrawingState];
    }
}

// -----------------------------------------------------------------------------

void CloseStatePicker()
{
    [globalStateView dismissStatePopover];
}

// -----------------------------------------------------------------------------

void PauseGenTimer()
{
    if (generating) {
        // use pausecount to handle nested calls
        if (pausecount == 0) [globalController stopGenTimer];
        pausecount++;
    }
}

// -----------------------------------------------------------------------------

void RestartGenTimer()
{
    if (pausecount > 0) {
        pausecount--;
        if (pausecount == 0) [globalController startGenTimer];
    }
}

// -----------------------------------------------------------------------------

static bool wasgenerating = false;      // generating needs to be resumed?

void PauseGenerating()
{
    if (generating) {
        // stop generating without changing STOP button
        [globalController stopGenTimer];
        StopGenerating();
        // generating now false
        wasgenerating = true;
    }
}

// -----------------------------------------------------------------------------

void ResumeGenerating()
{
    if (wasgenerating) {
        // generating should be false but play safe
        if (!generating) {
            if (StartGenerating()) {
                // generating now true
                [globalController startGenTimer];
            } else {
                // this can happen if pattern is empty
                [globalController toggleStartStopButton];
                [globalController updateButtons];
            }
        }
        wasgenerating = false;
    }
}

// -----------------------------------------------------------------------------

void StopIfGenerating()
{
    [globalController stopIfGenerating];
}

// -----------------------------------------------------------------------------

void BeginProgress(const char* title)
{
    if (progresscount == 0) {
        // disable interaction with all views but don't show progress view just yet
        [globalController enableInteraction:NO];
        [globalTitle setText:[NSString stringWithCString:title encoding:NSUTF8StringEncoding]];
        cancelProgress = false;
        progstart = TimeInSeconds();
    }
    progresscount++;    // handles nested calls
}

// -----------------------------------------------------------------------------

bool AbortProgress(double fraction_done, const char* message)
{
    if (progresscount <= 0) Fatal("Bug detected in AbortProgress!");
    double secs = TimeInSeconds() - progstart;
    if (!globalProgress.hidden) {
        if (secs < prognext) return false;
        prognext = secs + 0.1;     // update progress bar about 10 times per sec
        if (fraction_done < 0.0) {
            // show indeterminate progress gauge???
        } else {
            [globalController updateProgressBar:fraction_done];
        }
        return cancelProgress;
    } else {
        // note that fraction_done is not always an accurate estimator for how long
        // the task will take, especially when we use nextcell for cut/copy
        if ( (secs > 1.0 && fraction_done < 0.3) || secs > 2.0 ) {
            // task is probably going to take a while so show progress view
            globalProgress.hidden = NO;
            [globalController updateProgressBar:fraction_done];
        }
        prognext = secs + 0.01;     // short delay until 1st progress update
    }
    return false;
}

// -----------------------------------------------------------------------------

void EndProgress()
{
    if (progresscount <= 0) Fatal("Bug detected in EndProgress!");
    progresscount--;
    if (progresscount == 0) {
        // hide the progress view and enable interaction with other views
        globalProgress.hidden = YES;
        [globalController enableInteraction:YES];
    }
}
