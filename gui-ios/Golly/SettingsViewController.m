// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "utils.h"      // for Beep
#include "status.h"     // for ClearMessage
#include "prefs.h"      // for SavePrefs, showgridlines, etc
#include "layer.h"      // for currlayer, etc
#include "undo.h"       // for currlayer->undoredo->...
#include "view.h"       // for ToggleCellColors
#include "control.h"    // for generating

#import "SettingsViewController.h"

@implementation SettingsViewController

// -----------------------------------------------------------------------------

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self) {
        self.title = @"Settings";
        self.tabBarItem.image = [UIImage imageNamed:@"settings.png"];
    }
    return self;
}

// -----------------------------------------------------------------------------

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Release any cached data, images, etc that aren't in use.
}

// -----------------------------------------------------------------------------

- (void)viewDidLoad
{
    [super viewDidLoad];
    // Do any additional setup after loading the view from its nib.
}

// -----------------------------------------------------------------------------

- (void)viewDidUnload
{
    [super viewDidUnload];

    // release outlets
    modeButton = nil;
    percentageText = nil;
    memoryText = nil;
    percentageSlider = nil;
    memorySlider = nil;
    gridSwitch = nil;
    timingSwitch = nil;
    beepSwitch = nil;
    colorsSwitch = nil;
    iconsSwitch = nil;
    undoSwitch = nil;
    hashingSwitch = nil;
}

// -----------------------------------------------------------------------------

static bool oldcolors;      // detect if user changed swapcolors
static bool oldundo;        // detect if user changed allowundo
static bool oldhashinfo;    // detect if user changed currlayer->showhashinfo
static int oldhashmem;      // detect if user changed maxhashmem

- (void)viewWillAppear:(BOOL)animated
{
    [super viewWillAppear:animated];
    
    // make sure various controls agree with current settings:
    
    [modeButton setTitle:[NSString stringWithCString:GetPasteMode() encoding:NSUTF8StringEncoding]
                forState:UIControlStateNormal];
    
    [percentageText setText:[NSString stringWithFormat:@"%d", randomfill]];
    percentageSlider.minimumValue = 1;
    percentageSlider.maximumValue = 100;
    percentageSlider.value = randomfill;

    [memoryText setText:[NSString stringWithFormat:@"%d", maxhashmem]];
    memorySlider.minimumValue = MIN_MEM_MB;
    memorySlider.maximumValue = MAX_MEM_MB;
    memorySlider.value = maxhashmem;
    
    [gridSwitch setOn:showgridlines animated:NO];
    [timingSwitch setOn:showtiming animated:NO];
    [beepSwitch setOn:allowbeep animated:NO];
    [colorsSwitch setOn:swapcolors animated:NO];
    [iconsSwitch setOn:showicons animated:NO];
    [undoSwitch setOn:allowundo animated:NO];
    [hashingSwitch setOn:currlayer->showhashinfo animated:NO];
    
    oldcolors = swapcolors;
    oldundo = allowundo;
    oldhashmem = maxhashmem;
    oldhashinfo = currlayer->showhashinfo;
}

// -----------------------------------------------------------------------------

- (void)viewWillDisappear:(BOOL)animated
{
	[super viewWillDisappear:animated];
    
    if (swapcolors != oldcolors) ToggleCellColors();

    if (allowundo != oldundo) {
        if (allowundo) {
            if (currlayer->algo->getGeneration() > currlayer->startgen) {
                // undo list is empty but user can Reset, so add a generating change
                // to undo list so user can Undo or Reset (and then Redo if they wish)
                currlayer->undoredo->AddGenChange();
            }
        } else {
            currlayer->undoredo->ClearUndoRedo();
        }
    }

    if (currlayer->showhashinfo != oldhashinfo) {
        // we only show hashing info while generating
        if (generating) lifealgo::setVerbose(currlayer->showhashinfo);
    }

    // need to call setMaxMemory if maxhashmem changed
    if (maxhashmem != oldhashmem) {
        for (int i = 0; i < numlayers; i++) {
            Layer* layer = GetLayer(i);
            if (algoinfo[layer->algtype]->canhash) {
                layer->algo->setMaxMemory(maxhashmem);
            }
            // non-hashing algos (QuickLife) use their default memory setting
        }
    }

    // this is a good place to save the current settings
    SavePrefs();
}

// -----------------------------------------------------------------------------

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
    // return YES for supported orientations
	return YES;
}

// -----------------------------------------------------------------------------

- (IBAction)changePasteMode:(id)sender
{
    UIActionSheet *sheet = [[UIActionSheet alloc]
                            initWithTitle:nil
                            delegate:self
                            cancelButtonTitle:nil
                            destructiveButtonTitle:nil
                            otherButtonTitles: @"AND", @"COPY", @"OR", @"XOR", nil];
    
    [sheet showFromRect:modeButton.frame inView:modeButton.superview animated:NO];
}

// -----------------------------------------------------------------------------

- (void)actionSheet:(UIActionSheet *)sheet didDismissWithButtonIndex:(NSInteger)buttonIndex
{
    // called when the user selects an option from UIActionSheet created in changePasteMode
    switch (buttonIndex) {
        case 0:  [modeButton setTitle:@"AND"  forState:UIControlStateNormal]; pmode = And;  break;
        case 1:  [modeButton setTitle:@"COPY" forState:UIControlStateNormal]; pmode = Copy; break;
        case 2:  [modeButton setTitle:@"OR"   forState:UIControlStateNormal]; pmode = Or;   break;
        case 3:  [modeButton setTitle:@"XOR"  forState:UIControlStateNormal]; pmode = Xor;  break;
        default: break;
    }
}

// -----------------------------------------------------------------------------

- (IBAction)changePercentage:(id)sender
{
    randomfill = (int)percentageSlider.value;
    [percentageText setText:[NSString stringWithFormat:@"%d", randomfill]];
}

// -----------------------------------------------------------------------------

- (IBAction)changeMemory:(id)sender
{
    maxhashmem = (int)memorySlider.value;
    [memoryText setText:[NSString stringWithFormat:@"%d", maxhashmem]];
}

// -----------------------------------------------------------------------------

- (IBAction)toggleGrid:(id)sender
{
    showgridlines = !showgridlines;
}

// -----------------------------------------------------------------------------

- (IBAction)toggleTiming:(id)sender
{
    showtiming = !showtiming;
}

// -----------------------------------------------------------------------------

- (IBAction)toggleBeep:(id)sender
{
    allowbeep = !allowbeep;
}

// -----------------------------------------------------------------------------

- (IBAction)toggleColors:(id)sender
{
    swapcolors = !swapcolors;
}

// -----------------------------------------------------------------------------

- (IBAction)toggleIcons:(id)sender
{
    showicons = !showicons;
}

// -----------------------------------------------------------------------------

- (IBAction)toggleUndo:(id)sender
{
    allowundo = !allowundo;
}

// -----------------------------------------------------------------------------

- (IBAction)toggleHashing:(id)sender
{
    currlayer->showhashinfo = !currlayer->showhashinfo;
}

// -----------------------------------------------------------------------------

// UITextFieldDelegate methods:

- (void)textFieldDidEndEditing:(UITextField *)tf
{
    // called when editing has ended (ie. keyboard disappears)
    if (tf == percentageText) {
        if (tf.text.length > 0) {
            randomfill = (int)[tf.text integerValue];
            if (randomfill < 1) randomfill = 1;
            if (randomfill > 100) randomfill = 100;
        }
        [percentageText setText:[NSString stringWithFormat:@"%d", randomfill]];
        percentageSlider.value = randomfill;
    } else if (tf == memoryText) {
        if (tf.text.length > 0) {
            maxhashmem = (int)[tf.text integerValue];
            if (maxhashmem < MIN_MEM_MB) maxhashmem = MIN_MEM_MB;
            if (maxhashmem > MAX_MEM_MB) maxhashmem = MAX_MEM_MB;
        }
        [memoryText setText:[NSString stringWithFormat:@"%d", maxhashmem]];
        memorySlider.value = maxhashmem;
    }
}

- (BOOL)textFieldShouldReturn:(UITextField *)tf
{
    // called when user hits Done button, so remove keyboard
    // (note that textFieldDidEndEditing will then be called)
    [tf resignFirstResponder];
    return YES;
}

// -----------------------------------------------------------------------------

@end
