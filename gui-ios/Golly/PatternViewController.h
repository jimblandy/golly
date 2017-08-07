// This file is part of Golly.
// See docs/License.html for the copyright notice.

#import <UIKit/UIKit.h>
#import "PatternView.h"
#import "StatusView.h"
#import "StateView.h"

// This is the view controller for the Pattern tab.

@interface PatternViewController : UIViewController
{
    IBOutlet PatternView *pattView;
    IBOutlet StatusView *statView;
    IBOutlet UIButton *startStopButton;
    IBOutlet UIButton *restoreButton;
    IBOutlet UIBarButtonItem *resetButton;
    IBOutlet UIBarButtonItem *undoButton;
    IBOutlet UIBarButtonItem *redoButton;
    IBOutlet UIBarButtonItem *actionButton;
    IBOutlet UIBarButtonItem *infoButton;
    IBOutlet UISegmentedControl *stepControl;
    IBOutlet UISegmentedControl *scaleControl;
    IBOutlet UISegmentedControl *modeControl;
    IBOutlet UILabel *stateLabel;
    IBOutlet StateView *stateView;
    IBOutlet UIToolbar *topBar;
    IBOutlet UIToolbar *editBar;
    IBOutlet UIToolbar *bottomBar;
    IBOutlet UIView *progressView;
    IBOutlet UILabel *progressTitle;
    IBOutlet UILabel *progressMessage;
    IBOutlet UIProgressView *progressBar;
    IBOutlet UIButton *cancelButton;
    
    NSTimer *genTimer;
}

- (IBAction)doReset:(id)sender;
- (IBAction)doStartStop:(id)sender;
- (IBAction)doNext:(id)sender;
- (IBAction)doStep:(id)sender;
- (IBAction)doFit:(id)sender;
- (IBAction)doChangeStep:(id)sender;
- (IBAction)doChangeScale:(id)sender;
- (IBAction)doChangeMode:(id)sender;
- (IBAction)doUndo:(id)sender;
- (IBAction)doRedo:(id)sender;
- (IBAction)doMiddle:(id)sender;
- (IBAction)doSelectAll:(id)sender;
- (IBAction)doAction:(id)sender;
- (IBAction)doPaste:(id)sender;
- (IBAction)doRule:(id)sender;
- (IBAction)doNew:(id)sender;
- (IBAction)doInfo:(id)sender;
- (IBAction)doSave:(id)sender;
- (IBAction)doCancel:(id)sender;
- (IBAction)toggleFullScreen:(id)sender;

- (void)updateDrawingState;
- (void)updateButtons;
- (void)toggleStartStopButton;
- (void)stopIfGenerating;
- (void)startGenTimer;
- (void)stopGenTimer;
- (void)doGeneration:(NSTimer*)theTimer;

@end

// other modules need these routines:
void UpdatePattern();
void UpdateStatus();
void UpdateEditBar();
void CloseStatePicker();
void PauseGenTimer();
void RestartGenTimer();
void PauseGenerating();
void ResumeGenerating();
void StopIfGenerating();
void BeginProgress(const char* title);
bool AbortProgress(double fraction_done, const char* message);
void EndProgress();
