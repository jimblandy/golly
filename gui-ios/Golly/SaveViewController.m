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

#include "writepattern.h"   // for pattern_format, output_compression

#include "utils.h"          // for FileExists
#include "prefs.h"          // for datadir
#include "layer.h"          // for currlayer, etc
#include "file.h"           // for SavePattern

#import "SaveViewController.h"

@implementation SaveViewController

// -----------------------------------------------------------------------------

static NSString *filetypes[] =      // data for typeTable
{
    // WARNING: code below assumes this ordering
	@"Run Length Encoded (*.rle)",
    @"Compressed RLE (*.rle.gz)",
	@"Macrocell (*.mc)",
	@"Compressed MC (*.mc.gz)"
};
const int NUM_TYPES = sizeof(filetypes) / sizeof(filetypes[0]);

static int currtype = 0;            // current index in typeTable

// -----------------------------------------------------------------------------

- (void)checkFileType
{
    std::string filename = [[nameText text] cStringUsingEncoding:NSUTF8StringEncoding];
    if (!filename.empty()) {
        // might need to change currtype to match an explicit extension
        int oldtype = currtype;
        size_t len = filename.length();
        if        (len >= 4 && len - 4 == filename.rfind(".rle")) {
            currtype = 0;
        } else if (len >= 7 && len - 7 == filename.rfind(".rle.gz")) {
            currtype = 1;
        } else if (len >= 3 && len - 3 == filename.rfind(".mc")) {
            currtype = 2;
        } else if (len >= 6 && len - 6 == filename.rfind(".mc.gz")) {
            currtype = 3;
        } else {
            // extension is unknown or not supplied, so append appropriate extension
            size_t dotpos = filename.find('.');
            if (currtype == 0) filename = filename.substr(0,dotpos) + ".rle";
            if (currtype == 1) filename = filename.substr(0,dotpos) + ".rle.gz";
            if (currtype == 2) filename = filename.substr(0,dotpos) + ".mc";
            if (currtype == 3) filename = filename.substr(0,dotpos) + ".mc.gz";
            [nameText setText:[NSString stringWithCString:filename.c_str()
                                                 encoding:NSUTF8StringEncoding]];
        }
        if (currtype != oldtype) {
            [typeTable selectRowAtIndexPath:[NSIndexPath indexPathForRow:currtype inSection:0]
                                   animated:NO
                             scrollPosition:UITableViewScrollPositionNone];
        }
    }
}

// -----------------------------------------------------------------------------

- (void)checkFileName
{
    std::string filename = [[nameText text] cStringUsingEncoding:NSUTF8StringEncoding];
    if (!filename.empty()) {
        // might need to change filename to match currtype
        size_t dotpos = filename.find('.');
        std::string ext = (dotpos == std::string::npos) ? "" : filename.substr(dotpos);
        
        if (currtype == 0 && ext != ".rle")    filename = filename.substr(0,dotpos) + ".rle";
        if (currtype == 1 && ext != ".rle.gz") filename = filename.substr(0,dotpos) + ".rle.gz";
        if (currtype == 2 && ext != ".mc")     filename = filename.substr(0,dotpos) + ".mc";
        if (currtype == 3 && ext != ".mc.gz")  filename = filename.substr(0,dotpos) + ".mc.gz";
        
        [nameText setText:[NSString stringWithCString:filename.c_str()
                                             encoding:NSUTF8StringEncoding]];
    }
}

// -----------------------------------------------------------------------------

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self) {
        self.title = @"Save";
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
    // return YES for supported orientations
	return YES;
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

    // release all outlets
    nameText = nil;
    typeTable = nil;
}

// -----------------------------------------------------------------------------

- (void)viewWillAppear:(BOOL)animated
{
    [super viewWillAppear:animated];

    // [nameText setText:[NSString stringWithCString:currlayer->currname.c_str() encoding:NSUTF8StringEncoding]];
    // probably nicer not to show current name
    [nameText setText:@""];

    // init file type
    if (currlayer->algo->hyperCapable()) {
        // RLE is allowed but macrocell format is better
        if (currtype < 2) currtype = 2;
    } else {
        // algo doesn't support macrocell format
        if (currtype > 1) currtype = 0;
    }
    [typeTable selectRowAtIndexPath:[NSIndexPath indexPathForRow:currtype inSection:0]
                           animated:NO
                     scrollPosition:UITableViewScrollPositionNone];
    
    [self checkFileName];
    
    // show keyboard immediately
    [nameText becomeFirstResponder];
}

// -----------------------------------------------------------------------------

- (void)viewWillDisappear:(BOOL)animated
{
	[super viewWillDisappear:animated];
}

// -----------------------------------------------------------------------------

- (IBAction)doCancel:(id)sender
{
    [self dismissModalViewControllerAnimated:YES];
}

// -----------------------------------------------------------------------------

- (IBAction)doSave:(id)sender
{
    // clear first responder if necessary (ie. remove keyboard)
    [self.view endEditing:YES];
    
    std::string filename = [[nameText text] cStringUsingEncoding:NSUTF8StringEncoding];
    if (filename.empty()) {
        // no need??? Warning("Please enter a file name.");
        // better to beep and show keyboard
        Beep();
        [nameText becomeFirstResponder];
        return;
    }
    
    std::string fullpath = datadir + filename;
    if (FileExists(fullpath)) {
        // ask user if it's ok to replace an existing file
        if (!YesNo("A file with that name already exists.\nDo you want to replace that file?"))
            return;
    }
    
    // dismiss modal view first in case SavePattern calls BeginProgress
    [self dismissModalViewControllerAnimated:YES];
    
    pattern_format format = currtype < 2 ? XRLE_format : MC_format;
    output_compression compression = currtype % 2 == 0 ? no_compression : gzip_compression;
    
    SavePattern(fullpath, format, compression);
}

// -----------------------------------------------------------------------------

// UITextFieldDelegate methods:

- (void)textFieldDidEndEditing:(UITextField *)tf
{
    // called when rule editing has ended (ie. keyboard disappears)
    [self checkFileType];
}

- (BOOL)textFieldShouldReturn:(UITextField *)tf
{
    // called when user hits Done button, so remove keyboard
    // (note that textFieldDidEndEditing will then be called)
    [tf resignFirstResponder];
    return YES;
}

- (BOOL)disablesAutomaticKeyboardDismissal
{
    // this allows keyboard to be dismissed if modal view uses UIModalPresentationFormSheet
    return NO;
}

// -----------------------------------------------------------------------------

// UITableViewDelegate and UITableViewDataSource methods:

- (NSInteger)numberOfSectionsInTableView:(UITableView *)TableView
{
	return 1;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)component
{
    if (!currlayer->algo->hyperCapable()) {
        // algo doesn't support macrocell format
        return 2;
    } else {
        return NUM_TYPES;
    }
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    // check for a reusable cell first and use that if it exists
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:@"UITableViewCell"];
    
    // if there is no reusable cell of this type, create a new one
    if (!cell) cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault
                                             reuseIdentifier:@"UITableViewCell"];
    
    [[cell textLabel] setText:filetypes[[indexPath row]]];
	return cell;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
    // change selected file type
    int newtype = [indexPath row];
    if (newtype < 0 || newtype >= NUM_TYPES) {
        Warning("Bug: unexpected row!");
    } else {
        currtype = newtype;
        [self checkFileName];
    }
}

// -----------------------------------------------------------------------------

@end
