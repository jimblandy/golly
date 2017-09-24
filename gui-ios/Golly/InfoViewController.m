// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "readpattern.h"    // for readcomments
#include "layer.h"          // for currlayer, etc
#include "prefs.h"          // for userdir

#import "GollyAppDelegate.h"        // for CurrentViewController
#import "SaveViewController.h"      // for SaveTextFile
#import "InfoViewController.h"

@implementation InfoViewController

// -----------------------------------------------------------------------------

static std::string textfile = "";   // set in ShowTextFile
static bool textchanged = false;    // true if user changed text

// -----------------------------------------------------------------------------

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self) {
        self.title = @"Info";
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

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
    // Return YES for supported orientations
	return YES;
}

// -----------------------------------------------------------------------------

- (void)scaleText:(UIPinchGestureRecognizer *)pinchGesture
{
    // very slow for large files; better to use A- and A+ buttons to dec/inc font size???
    UIGestureRecognizerState state = pinchGesture.state;
    if (state == UIGestureRecognizerStateBegan || state == UIGestureRecognizerStateChanged) {
        CGFloat ptsize = fileView.font.pointSize * pinchGesture.scale;
        if (ptsize < 5.0) ptsize = 5.0;
        if (ptsize > 50.0) ptsize = 50.0;
        fileView.font = [UIFont fontWithName:fileView.font.fontName size:ptsize];
    } else if (state == UIGestureRecognizerStateEnded) {
        // do nothing
    }
}

// -----------------------------------------------------------------------------

- (void)viewDidLoad
{
    [super viewDidLoad];

    // pinch gestures will change text size
    UIPinchGestureRecognizer *pinchGesture = [[UIPinchGestureRecognizer alloc]
                                              initWithTarget:self action:@selector(scaleText:)];
    pinchGesture.delegate = self;
    [fileView addGestureRecognizer:pinchGesture];
}

// -----------------------------------------------------------------------------

- (void)viewDidUnload
{
    [super viewDidUnload];
    
    // release all outlets
    fileView = nil;
    saveButton = nil;
}

// -----------------------------------------------------------------------------

- (void)viewWillAppear:(BOOL)animated
{
	[super viewWillAppear:animated];
    
    textchanged = false;
    
    if (textfile.find(userdir + "Documents/") == 0) {
        // allow user to edit this file
        fileView.editable = YES;
        // show Save button
        saveButton.style = UIBarButtonItemStylePlain;
        saveButton.enabled = YES;
        saveButton.title = @"Save";
    } else {
        // textfile is empty (ie. not in ShowTextFile) or file is read-only
        fileView.editable = NO;
        // hide Save button
        saveButton.style = UIBarButtonItemStylePlain;
        saveButton.enabled = NO;
        saveButton.title = nil;
    }
    
    // best to use a fixed-width font
    // fileView.font = [UIFont fontWithName:@"CourierNewPSMT" size:14];
    // bold is a bit nicer
    fileView.font = [UIFont fontWithName:@"CourierNewPS-BoldMT" size:14];
    
    if (!textfile.empty()) {
        // display contents of textfile
        NSError *err = nil;
        NSString *filePath = [NSString stringWithCString:textfile.c_str() encoding:NSUTF8StringEncoding];
        NSString *fileContents = [NSString stringWithContentsOfFile:filePath encoding:NSUTF8StringEncoding error:&err];
        if (fileContents == nil) {
            fileView.text = [NSString stringWithFormat:@"Error reading text file:\n%@", err];
            fileView.textColor = [UIColor redColor];
        } else {
            fileView.text = fileContents;
        }
        
    } else if (currlayer->currfile.empty()) {
        // should never happen
        fileView.text = @"There is no current pattern file!";
        fileView.textColor = [UIColor redColor];

    } else {
        // display comments in current pattern file
        char *commptr = NULL;
        // readcomments will allocate commptr
        const char *err = readcomments(currlayer->currfile.c_str(), &commptr);
        if (err) {
            fileView.text = [NSString stringWithCString:err encoding:NSUTF8StringEncoding];
            fileView.textColor = [UIColor redColor];
        } else if (commptr[0] == 0) {
            fileView.text = @"No comments found.";
        } else {
            fileView.text = [NSString stringWithCString:commptr encoding:NSUTF8StringEncoding];
        }
        if (commptr) free(commptr);
    }
}

// -----------------------------------------------------------------------------

- (void)viewWillDisappear:(BOOL)animated
{
	[super viewWillDisappear:animated];

    textfile.clear();   // viewWillAppear will display comments in currlayer->currfile
}

// -----------------------------------------------------------------------------

- (IBAction)doCancel:(id)sender
{
    if (textchanged && YesNo("Do you want to save your changes?")) {
        [self doSave:sender];
        return;
    }
    
    [self dismissViewControllerAnimated:YES completion:nil];
}

// -----------------------------------------------------------------------------

static std::string contents;

- (IBAction)doSave:(id)sender
{
    contents = [fileView.text cStringUsingEncoding:NSUTF8StringEncoding];
    SaveTextFile(textfile.c_str(), contents.c_str(), self);
}

// -----------------------------------------------------------------------------

- (void)saveSucceded:(const char*)savedpath
{
    textfile = savedpath;
    textchanged = false;
}

// -----------------------------------------------------------------------------

// UITextViewDelegate method:

- (void)textViewDidChange:(UITextView *)textView
{
    textchanged = true;
}

@end

// =============================================================================

void ShowTextFile(const char* filepath, UIViewController* currentView)
{
    // check if path ends with .gz or .zip
    if (EndsWith(filepath,".gz") || EndsWith(filepath,".zip")) {
        Warning("Editing a compressed file is not supported.");
        return;
    }

    textfile = filepath;    // viewWillAppear will display this file
    
    InfoViewController *modalInfoController = [[InfoViewController alloc] initWithNibName:nil bundle:nil];
    
    [modalInfoController setModalPresentationStyle:UIModalPresentationFullScreen];
    
    if (currentView == nil) currentView = CurrentViewController();
    [currentView presentViewController:modalInfoController animated:YES completion:nil];
    
    modalInfoController = nil;
    
    // only reset textfile in viewWillDisappear
}
