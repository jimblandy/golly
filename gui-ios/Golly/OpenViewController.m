// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include <string>       // for std::string
#include <list>         // for std::list
#include <set>          // for std::set
#include <algorithm>    // for std::count

#include "prefs.h"      // for userdir, recentpatterns, SavePrefs, etc
#include "utils.h"      // for Warning, RemoveFile, FixURLPath, MoveFile, etc
#include "file.h"       // for OpenFile

#import "InfoViewController.h"      // for ShowTextFile
#import "OpenViewController.h"

@implementation OpenViewController

// -----------------------------------------------------------------------------

static NSString *options[] =    // data for optionTable
{
	@"Supplied",
	@"Recent",
	@"Saved",
    @"Downloaded"
};
const int NUM_OPTIONS = sizeof(options) / sizeof(options[0]);

static int curroption;                      // current index in optionTable
static CGPoint curroffset[NUM_OPTIONS];     // current offset in scroll view for each option

const char* HTML_HEADER = "<html><font size=+2 color='black'><b>";
const char* HTML_FOOTER = "</b></font></html>";
const char* HTML_INDENT = "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;";

static std::set<std::string> opendirs;      // set of open directories in Supplied patterns

// -----------------------------------------------------------------------------

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self) {
        self.title = @"Open";
        self.tabBarItem.image = [UIImage imageNamed:@"open.png"];
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

static void AppendHtmlData(std::string& htmldata, const std::string& dir,
                           const std::string& prefix, const std::string& title, bool candelete)
{
    NSFileManager *fm = [NSFileManager defaultManager];
    NSString *pattdir = [NSString stringWithCString:dir.c_str() encoding:NSUTF8StringEncoding];
    NSDirectoryEnumerator *dirEnum = [fm enumeratorAtPath:pattdir];
    NSString *path;
    
    int closedlevel = 0;
    
    htmldata = HTML_HEADER;
    htmldata += title;
    htmldata += "<br><br>";
    
    while (path = [dirEnum nextObject]) {
        // path is relative to given dir (eg. "Life/Bounded-Grids/agar-p3.rle" if patternsdir)
        std::string pstr = [path cStringUsingEncoding:NSUTF8StringEncoding];

        // indent level is number of separators in path
        int indents = (int)std::count(pstr.begin(), pstr.end(), '/');
        
        if (indents <= closedlevel) {
            NSString *fullpath = [pattdir stringByAppendingPathComponent:path];
            BOOL isDir;
            if ([fm fileExistsAtPath:fullpath isDirectory:&isDir] && isDir) {
                // path is to a directory
                std::string imgname;
                if (opendirs.find(pstr) == opendirs.end()) {
                    closedlevel = indents;
                    imgname = "triangle-right.png";
                } else {
                    closedlevel = indents+1;
                    imgname = "triangle-down.png";
                }
                for (int i = 0; i < indents; i++) htmldata += HTML_INDENT;
                htmldata += "<a href=\"toggledir:";
                htmldata += pstr;
                htmldata += "\"><img src='";
                htmldata += imgname;
                htmldata += "' border=0/><font size=+2 color='gray'>";
                size_t lastsep = pstr.rfind('/');
                if (lastsep == std::string::npos) {
                    htmldata += pstr;
                } else {
                    htmldata += pstr.substr(lastsep+1);
                }
                htmldata += "</font></a><br>";
            } else {
                // path is to a file
                for (int i = 0; i < indents; i++) htmldata += HTML_INDENT;
                if (candelete) {
                    // allow user to delete file
                    htmldata += "<a href=\"delete:";
                    htmldata += prefix;
                    htmldata += pstr;
                    htmldata += "\"><font size=-1 color='red'>DELETE</font></a>&nbsp;&nbsp;&nbsp;";
                    // allow user to edit file
                    htmldata += "<a href=\"edit:";
                    htmldata += prefix;
                    htmldata += pstr;
                    htmldata += "\"><font size=-1 color='green'>EDIT</font></a>&nbsp;&nbsp;&nbsp;";
                } else {
                    // allow user to read file (a supplied pattern)
                    htmldata += "<a href=\"edit:";
                    htmldata += prefix;
                    htmldata += pstr;
                    htmldata += "\"><font size=-1 color='green'>READ</font></a>&nbsp;&nbsp;&nbsp;";
                }
                htmldata += "<a href=\"open:";
                htmldata += prefix;
                htmldata += pstr;
                htmldata += "\">";
                size_t lastsep = pstr.rfind('/');
                if (lastsep == std::string::npos) {
                    htmldata += pstr;
                } else {
                    htmldata += pstr.substr(lastsep+1);
                }
                htmldata += "</a><br>";
            }
        }
    }
    
    htmldata += HTML_FOOTER;
}

// -----------------------------------------------------------------------------

- (void)showSuppliedPatterns
{
    std::string htmldata;
    AppendHtmlData(htmldata, patternsdir, "Patterns/", "Supplied patterns:", false);
    [htmlView loadHTMLString:[NSString stringWithCString:htmldata.c_str() encoding:NSUTF8StringEncoding]
                             // the following base URL is needed for img links to work
                     baseURL:[NSURL fileURLWithPath:[[NSBundle mainBundle] bundlePath]]];
}

// -----------------------------------------------------------------------------

- (void)showSavedPatterns
{
    std::string htmldata;
    AppendHtmlData(htmldata, savedir, "Documents/Saved/", "Saved patterns:", true);
    [htmlView loadHTMLString:[NSString stringWithCString:htmldata.c_str() encoding:NSUTF8StringEncoding]
                     baseURL:nil];
}

// -----------------------------------------------------------------------------

- (void)showDownloadedPatterns
{
    std::string htmldata;
    AppendHtmlData(htmldata, downloaddir, "Documents/Downloads/", "Downloaded patterns:", true);
    [htmlView loadHTMLString:[NSString stringWithCString:htmldata.c_str() encoding:NSUTF8StringEncoding]
                     baseURL:nil];
}

// -----------------------------------------------------------------------------

- (void)showRecentPatterns
{
    std::string htmldata = HTML_HEADER;
    if (recentpatterns.empty()) {
        htmldata += "There are no recent patterns.";
    } else {
        htmldata += "Recently opened patterns:<br><br>";
        std::list<std::string>::iterator next = recentpatterns.begin();
        while (next != recentpatterns.end()) {
            std::string path = *next;
            if (path.find("Patterns/") == 0 || FileExists(userdir + path)) {
                htmldata += "<a href=\"open:";
                htmldata += path;
                htmldata += "\">";
                // nicer not to show Patterns/ or Documents/
                size_t firstsep = path.find('/');
                if (firstsep != std::string::npos) {
                    path.erase(0, firstsep+1);
                }
                htmldata += path;
                htmldata += "</a><br>";
            }
            next++;
        }
    }
    htmldata += HTML_FOOTER;
    [htmlView loadHTMLString:[NSString stringWithCString:htmldata.c_str() encoding:NSUTF8StringEncoding]
                     baseURL:nil];
}

// -----------------------------------------------------------------------------

- (void)viewDidLoad
{
    [super viewDidLoad];
    
    htmlView.delegate = self;
    
    // init all offsets to top left
    for (int i=0; i<NUM_OPTIONS; i++) {
        curroffset[i].x = 0.0;
        curroffset[i].y = 0.0;
    }
    
    // show supplied patterns initially
    curroption = 0;
    [optionTable selectRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]
                             animated:NO
                       scrollPosition:UITableViewScrollPositionNone];
    [self showSuppliedPatterns];
}

// -----------------------------------------------------------------------------

- (void)viewDidUnload
{
    [super viewDidUnload];
    
    // release all outlets
    optionTable = nil;
    htmlView.delegate = nil;
    htmlView = nil;
}

// -----------------------------------------------------------------------------

- (void)viewWillAppear:(BOOL)animated
{
    [super viewWillAppear:animated];
    
    MoveSharedFiles();

    if (curroption > 0) {
        // recent/saved/downloaded patterns might have changed
        switch (curroption) {
            case 1: [self showRecentPatterns]; break;
            case 2: [self showSavedPatterns]; break;
            case 3: [self showDownloadedPatterns]; break;
            default: Warning("Bug detected in viewWillAppear!");
        }
    }
}

// -----------------------------------------------------------------------------

- (void)viewWillDisappear:(BOOL)animated
{
	[super viewWillDisappear:animated];
    
    // no need for this as we're only displaying html data in strings
    // [htmlView stopLoading];

    if (curroption > 0) {
        // save current location of recent/saved/downloaded view
        curroffset[curroption] = htmlView.scrollView.contentOffset;
    }
}

// -----------------------------------------------------------------------------

// UITableViewDelegate and UITableViewDataSource methods:

- (NSInteger)numberOfSectionsInTableView:(UITableView *)TableView
{
	return 1;
}

// -----------------------------------------------------------------------------

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)component
{
	return NUM_OPTIONS;
}

// -----------------------------------------------------------------------------

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    // check for a reusable cell first and use that if it exists
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:@"UITableViewCell"];
    
    // if there is no reusable cell of this type, create a new one
    if (!cell) cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault
                                             reuseIdentifier:@"UITableViewCell"];

    [[cell textLabel] setText:options[[indexPath row]]];
	return cell;
}

// -----------------------------------------------------------------------------

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
    // save current location
    curroffset[curroption] = htmlView.scrollView.contentOffset;
    
    // display selected set of files
    curroption = (int)[indexPath row];
    switch (curroption) {
        case 0: [self showSuppliedPatterns]; break;
        case 1: [self showRecentPatterns]; break;
        case 2: [self showSavedPatterns]; break;
        case 3: [self showDownloadedPatterns]; break;
        default: Warning("Bug: unexpected row!"); 
    }
}

// -----------------------------------------------------------------------------

// UIWebViewDelegate methods:

- (void)webViewDidStartLoad:(UIWebView *)webView
{
    // show the activity indicator in the status bar
    [UIApplication sharedApplication].networkActivityIndicatorVisible = YES;
}

// -----------------------------------------------------------------------------

- (void)webViewDidFinishLoad:(UIWebView *)webView
{
    // hide the activity indicator in the status bar
    [UIApplication sharedApplication].networkActivityIndicatorVisible = NO;
    // restore old offset here
    htmlView.scrollView.contentOffset = curroffset[curroption];
}

// -----------------------------------------------------------------------------

- (void)webView:(UIWebView *)webView didFailLoadWithError:(NSError *)error
{
    // hide the activity indicator in the status bar and display error message
    [UIApplication sharedApplication].networkActivityIndicatorVisible = NO;
    // we can safely ignore -999 errors
    if (error.code == NSURLErrorCancelled) return;
    Warning([error.localizedDescription cStringUsingEncoding:NSUTF8StringEncoding]);
}

// -----------------------------------------------------------------------------

- (BOOL)webView:(UIWebView *)webView shouldStartLoadWithRequest:(NSURLRequest *)request navigationType:(UIWebViewNavigationType)navigationType
{
    if (navigationType == UIWebViewNavigationTypeLinkClicked) {
        NSURL *url = [request URL];
        NSString *link = [url absoluteString];
        
        // link should have special prefix
        if ([link hasPrefix:@"open:"]) {
            // open specified file
            std::string path = [[link substringFromIndex:5] cStringUsingEncoding:NSUTF8StringEncoding];
            FixURLPath(path);
            OpenFile(path.c_str());
            SavePrefs();
            return NO;
        }
        if ([link hasPrefix:@"toggledir:"]) {
            // open/close directory
            std::string path = [[link substringFromIndex:10] cStringUsingEncoding:NSUTF8StringEncoding];
            FixURLPath(path);
            if (opendirs.find(path) == opendirs.end()) {
                // add directory path to opendirs
                opendirs.insert(path);
            } else {
                // remove directory path from opendirs
                opendirs.erase(path);
            }
            if (curroption == 0) {
                // save current location and update supplied patterns
                curroffset[0] = htmlView.scrollView.contentOffset;
                [self showSuppliedPatterns];
            } else {
                Warning("Bug: expected supplied patterns!");
            }
            return NO;
        }
        if ([link hasPrefix:@"delete:"]) {
            std::string path = [[link substringFromIndex:7] cStringUsingEncoding:NSUTF8StringEncoding];
            FixURLPath(path);
            std::string question = "Do you really want to delete " + path + "?";
            if (YesNo(question.c_str())) {
                // delete specified file
                path = userdir + path;
                RemoveFile(path);
                // save current location
                curroffset[curroption] = htmlView.scrollView.contentOffset;
                switch (curroption) {
                    // can't delete supplied or recent patterns
                    case 2: [self showSavedPatterns]; break;
                    case 3: [self showDownloadedPatterns]; break;
                    default: Warning("Bug: can't delete these files!");
                }
            }
            return NO;
        }
        if ([link hasPrefix:@"edit:"]) {
            std::string path = [[link substringFromIndex:5] cStringUsingEncoding:NSUTF8StringEncoding];
            FixURLPath(path);
            // convert path to a full path if necessary
            std::string fullpath = path;
            if (path[0] != '/') {
                if (fullpath.find("Patterns/") == 0 || fullpath.find("Rules/") == 0) {
                    // Patterns and Rules directories are inside supplieddir
                    fullpath = supplieddir + fullpath;
                } else {
                    fullpath = userdir + fullpath;
                }
            }
            ShowTextFile(fullpath.c_str());
            return NO;
        }
    }
    return YES;
}

@end

// =============================================================================

void MoveSharedFiles()
{
    // check for files in the Documents folder (created by iTunes file sharing)
    // and move any .rule/tree/table files into Documents/Rules/,
    // otherwise assume they are pattern files and move them into Documents/Saved/
    
    std::string docdir = userdir + "Documents/";
    NSFileManager *fm = [NSFileManager defaultManager];
    NSString *dirpath = [NSString stringWithCString:docdir.c_str() encoding:NSUTF8StringEncoding];
	NSArray* contents = [fm contentsOfDirectoryAtPath:dirpath error:nil];

	for (NSString* item in contents) {
        NSString *fullpath = [dirpath stringByAppendingPathComponent:item];
        BOOL isDir;
        if ([fm fileExistsAtPath:fullpath isDirectory:&isDir] && isDir) {
            // ignore path to subdirectory
        } else {
            // path is to a file
            std::string filename = [item cStringUsingEncoding:NSUTF8StringEncoding];
            if (filename[0] == '.' || EndsWith(filename,".colors") || EndsWith(filename,".icons")) {
                // ignore hidden file or .colors/icons file
            } else if (IsRuleFile(filename)) {
                // move .rule/tree/table file into Documents/Rules/
                std::string oldpath = docdir + filename;
                std::string newpath = userrules + filename;
                MoveFile(oldpath, newpath);
            } else {
                // assume this is a pattern file amd move it into Documents/Saved/
                std::string oldpath = docdir + filename;
                std::string newpath = savedir + filename;
                MoveFile(oldpath, newpath);
            }
        }
	}
}
