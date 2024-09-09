// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "prefs.h"      // for showicons

#import "PatternViewController.h"   // for UpdateEditBar, UpdatePattern
#import "StatePickerController.h"

// -----------------------------------------------------------------------------

@implementation StatePickerController

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self) {
        self.title = @"StatePicker";
    }
    return self;
}

// -----------------------------------------------------------------------------

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // release any cached data, images, etc that aren't in use
}

// -----------------------------------------------------------------------------

- (void)viewDidLoad
{
    [super viewDidLoad];
}

// -----------------------------------------------------------------------------

- (void)viewWillAppear:(BOOL)animated
{
    [super viewWillAppear:animated];
    [iconsSwitch setOn:showicons animated:NO];
}

// -----------------------------------------------------------------------------

- (IBAction)toggleIcons:(id)sender
{
    showicons = !showicons;
    [spView setNeedsDisplay];
    UpdateEditBar();
    UpdatePattern();
}

// -----------------------------------------------------------------------------

@end
