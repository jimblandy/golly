/*** /
 
 This file is part of Golly, a Game of Life Simulator.
 Copyright (C) 2012 Andrew Trevorrow and Tomas Rokicki.
 
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

#include "utils.h"      // for Beep, Warning, FixURLPath
#include "prefs.h"      // for gollydir, downloaddir
#include "file.h"       // for OpenFile, UnzipFile, GetURL, DownloadFile, LoadLexiconPattern
#include "control.h"    // for ChangeRule

#import "GollyAppDelegate.h"        // for SwitchToPatternTab, SwitchToHelpTab
#import "InfoViewController.h"      // for ShowTextFile
#import "HelpViewController.h"

@implementation HelpViewController

// -----------------------------------------------------------------------------

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self) {
        self.title = @"Help";
        self.tabBarItem.image = [UIImage imageNamed:@"help.png"];
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

- (void)showContentsPage
{
    NSBundle *bundle=[NSBundle mainBundle];
    NSString *filePath = [bundle pathForResource:@"index" ofType:@"html" inDirectory:@"Help"];
    if (filePath) {
        NSURL *fileUrl = [NSURL fileURLWithPath:filePath];
        NSURLRequest *request = [NSURLRequest requestWithURL:fileUrl];
        [htmlView loadRequest:request];
    } else {
        [htmlView loadHTMLString:@"<html><center><font size=+4 color='red'>Failed to find index.html!</font></center></html>"
                         baseURL:nil];
    }
}

// -----------------------------------------------------------------------------

static UIWebView *globalHtmlView = nil;     // for ShowHelp

- (void)viewDidLoad
{
    [super viewDidLoad];
	
    globalHtmlView = htmlView;
    htmlView.delegate = self;
    backButton.enabled = NO;
    nextButton.enabled = NO;
    
    [self showContentsPage];
}

// -----------------------------------------------------------------------------

- (void)viewDidUnload
{
    [super viewDidUnload];
    
    // release delegate and outlets
    htmlView.delegate = nil;
    htmlView = nil;
    backButton = nil;
    nextButton = nil;
    contentsButton = nil;
}

// -----------------------------------------------------------------------------

- (void)viewWillAppear:(BOOL)animated
{
    [super viewWillAppear:animated];
}

// -----------------------------------------------------------------------------

- (void)viewDidAppear:(BOOL)animated
{
    [super viewDidAppear:animated];
}

// -----------------------------------------------------------------------------

- (void)viewWillDisappear:(BOOL)animated
{
	[super viewWillDisappear:animated];

    [htmlView stopLoading];  // in case the web view is still loading its content
}

// -----------------------------------------------------------------------------

- (void)viewDidDisappear:(BOOL)animated
{
	[super viewDidDisappear:animated];
}

// -----------------------------------------------------------------------------

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
    // return YES for supported orientations
    return YES;
}

// -----------------------------------------------------------------------------

- (IBAction)doBack:(id)sender
{
    [htmlView goBack];
}

// -----------------------------------------------------------------------------

- (IBAction)doNext:(id)sender
{
    [htmlView goForward];
}

// -----------------------------------------------------------------------------

- (IBAction)doContents:(id)sender
{
    [self showContentsPage];
}

// -----------------------------------------------------------------------------

// UIWebViewDelegate methods:

- (void)webViewDidStartLoad:(UIWebView *)webView
{
    [UIApplication sharedApplication].networkActivityIndicatorVisible = YES;
}

// -----------------------------------------------------------------------------

static std::string pageurl;

- (void)webViewDidFinishLoad:(UIWebView *)webView
{
    [UIApplication sharedApplication].networkActivityIndicatorVisible = NO;
    backButton.enabled = htmlView.canGoBack;
    nextButton.enabled = htmlView.canGoForward;
    
    // need URL of this page for relative "get:" links
    NSString *str = [htmlView stringByEvaluatingJavaScriptFromString:@"window.location.href"];
    // note that htmlView.request.mainDocumentURL.absoluteString returns the
    // same URL string, but with "%20" instead of spaces (GetURL wants spaces)
    pageurl = [str cStringUsingEncoding:NSUTF8StringEncoding];
}

// -----------------------------------------------------------------------------

- (void)webView:(UIWebView *)webView didFailLoadWithError:(NSError *)error
{
    [UIApplication sharedApplication].networkActivityIndicatorVisible = NO;
    // we can safely ignore -999 errors (eg. when ShowHelp is called before user switches to Help tab)
    if (error.code == NSURLErrorCancelled) return;
    Warning([error.localizedDescription cStringUsingEncoding:NSUTF8StringEncoding]);
}

// -----------------------------------------------------------------------------

- (BOOL)webView:(UIWebView *)webView shouldStartLoadWithRequest:(NSURLRequest *)request navigationType:(UIWebViewNavigationType)navigationType
{
    if (navigationType == UIWebViewNavigationTypeLinkClicked) {
        NSURL *url = [request URL];
        NSString *link = [url absoluteString];
        // NSLog(@"url = %@", link);
        
        // look for special prefixes used by Golly (and return NO if found)
        if ([link hasPrefix:@"open:"]) {
            // open specified file
            std::string path = [[link substringFromIndex:5] cStringUsingEncoding:NSUTF8StringEncoding];
            OpenFile(path.c_str());
            // OpenFile will switch to the appropriate tab
            return NO;
        }
        if ([link hasPrefix:@"rule:"]) {
            // switch to specified rule
            std::string newrule = [[link substringFromIndex:5] cStringUsingEncoding:NSUTF8StringEncoding];
            SwitchToPatternTab();
            ChangeRule(newrule);
            return NO;
        }
        if ([link hasPrefix:@"lexpatt:"]) {
            // user tapped on pattern in Life Lexicon
            std::string pattern = [[link substringFromIndex:8] cStringUsingEncoding:NSUTF8StringEncoding];
            std::replace(pattern.begin(), pattern.end(), '$', '\n');
            LoadLexiconPattern(pattern);
            return NO;
        }
        if ([link hasPrefix:@"edit:"]) {
            std::string path = [[link substringFromIndex:5] cStringUsingEncoding:NSUTF8StringEncoding];
            ShowTextFile(path.c_str());
            return NO;
        }
        if ([link hasPrefix:@"get:"]) {
            std::string geturl = [[link substringFromIndex:4] cStringUsingEncoding:NSUTF8StringEncoding];
            // download file specifed in link (possibly relative to a previous full url)
            GetURL(geturl, pageurl);
            return NO;
        }
        if ([link hasPrefix:@"unzip:"]) {
            std::string zippath = [[link substringFromIndex:6] cStringUsingEncoding:NSUTF8StringEncoding];
            FixURLPath(zippath);
            std::string entry = zippath.substr(zippath.rfind(':') + 1);
            zippath = zippath.substr(0, zippath.rfind(':'));
            UnzipFile(zippath, entry);
            return NO;
        }
        
        // no special prefix, so look for file with .zip/rle/lif/mc extension
        std::string path = [link cStringUsingEncoding:NSUTF8StringEncoding];
        path = path.substr(path.rfind('/')+1);
        size_t dotpos = path.rfind('.');
        std::string ext = "";
        if (dotpos != std::string::npos) ext = path.substr(dotpos+1);
        if ( (IsZipFile(path) ||
                strcasecmp(ext.c_str(),"rle") == 0 ||
                strcasecmp(ext.c_str(),"life") == 0 ||
                strcasecmp(ext.c_str(),"mc") == 0)
                // also check for '?' to avoid opening links like ".../detail?name=foo.zip"
                && path.find('?') == std::string::npos) {
            // download file to downloaddir and open it
            path = downloaddir + path;
            std::string url = [link cStringUsingEncoding:NSUTF8StringEncoding];
            if (DownloadFile(url, path)) {
                OpenFile(path.c_str());
            }
            return NO;
        }
    }
    return YES;
}

@end

// =============================================================================

void ShowHelp(const char* filepath)
{
    SwitchToHelpTab();
    NSURL *fileUrl = [NSURL fileURLWithPath:[NSString stringWithCString:filepath encoding:NSUTF8StringEncoding]];
    NSURLRequest *request = [NSURLRequest requestWithURL:fileUrl];
    [globalHtmlView loadRequest:request];
}
