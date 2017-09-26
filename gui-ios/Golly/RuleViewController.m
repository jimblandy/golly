// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "prefs.h"      // for allowundo, SavePrefs
#include "algos.h"      // for algo_type, NumAlgos, etc
#include "layer.h"      // for currlayer, etc
#include "undo.h"       // for currlayer->undoredo
#include "view.h"       // for SaveCurrentSelection
#include "control.h"    // for ChangeAlgorithm, CreateRuleFiles
#include "file.h"       // for OpenFile

#import "InfoViewController.h"      // for ShowTextFile
#import "OpenViewController.h"      // for MoveSharedFiles
#import "RuleViewController.h"

@implementation RuleViewController

// -----------------------------------------------------------------------------

const char* RULE_IF_EMPTY = "B3/S23";   // use Life if ruleText is empty

static algo_type algoindex;             // index of currently displayed algorithm
static std::string oldrule;             // original rule on entering Rule screen
static int oldmaxstate;                 // maximum cell state in original rule

static CGPoint curroffset[MAX_ALGOS];   // current offset in scroll view for each algoindex

static NSString *namedRules[] =         // data for rulePicker
{
    // keep names in alphabetical order, and rules must be in canonical form for findNamedRule and GetRuleName
	@"3-4 Life",            @"B34/S34",
    @"AntiLife",            @"B0123478/S01234678",
    @"Banners",             @"2367/3457/5",
    @"Bloomerang",          @"234/34678/24",
    @"Brian's Brain",       @"/2/3",
    @"Caterpillars",        @"124567/378/4",
    @"Cooties",             @"23/2/8",
    @"Day and Night",       @"B3678/S34678",
    @"Diamoeba",            @"B35678/S5678",
    @"Fireworks",           @"2/13/21",
    @"Fredkin",             @"B1357/S02468",
    @"Frogs",               @"12/34/3",
	@"HighLife",            @"B36/S23",
    @"Hutton32",            @"Hutton32",
    @"JvN29",               @"JvN29",
    @"Lava",                @"12345/45678/8",
	@"Life",                @"B3/S23",
	@"LifeHistory",         @"LifeHistory",
    @"Life without Death",  @"B3/S012345678",
    @"Lines",               @"012345/458/3",
	@"LongLife",            @"B345/S5",
    @"Morley",              @"B368/S245",
    @"Nobili32",            @"Nobili32",
    @"Persian Rug",         @"B234/S",
    @"Plow World",          @"B378/S012345678",
    @"Replicator",          @"B1357/S1357",
    @"Seeds",               @"B2/S",
    @"Star Wars",           @"345/2/4",
    @"Sticks",              @"3456/2/6",
    @"Transers",            @"345/26/5",
	@"WireWorld",           @"WireWorld",
	@"Wolfram 22",          @"W22",
	@"Wolfram 30",          @"W30",
	@"Wolfram 110",         @"W110",
    @"Xtasy",               @"1456/2356/16",
    @"UNNAMED",             @"Bug if we see this!"  // UNNAMED must be last
};
const int NUM_ROWS = (sizeof(namedRules) / sizeof(namedRules[0])) / 2;
const int UNNAMED_ROW = NUM_ROWS - 1;

// -----------------------------------------------------------------------------

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self) {
        self.title = @"Rule";
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
    
    htmlView.delegate = self;
    
    // init all offsets to top left
    for (int i=0; i<MAX_ALGOS; i++) {
        curroffset[i].x = 0.0;
        curroffset[i].y = 0.0;
    }
    algoindex = 0;
}

// -----------------------------------------------------------------------------

- (void)viewDidUnload
{
    [super viewDidUnload];
    
    // release all outlets
    algoButton = nil;
    ruleText = nil;
    unknownLabel = nil;
    rulePicker = nil;
    htmlView.delegate = nil;
    htmlView = nil;
}

// -----------------------------------------------------------------------------

- (int)findNamedRule:(NSString *)rule
{
    for (int row = 0; row < UNNAMED_ROW; row++) {
        if ([rule isEqualToString:namedRules[row * 2 + 1]]) {
            return row;
        }
    }
    return UNNAMED_ROW;
}

// -----------------------------------------------------------------------------

static void ConvertOldRules()
{
    NSFileManager *fm = [NSFileManager defaultManager];
    NSString *rdir = [NSString stringWithCString:userrules.c_str() encoding:NSUTF8StringEncoding];
    NSDirectoryEnumerator *dirEnum = [fm enumeratorAtPath:rdir];
    NSString *path;
    std::list<std::string> deprecated, keeprules;
    
    while (path = [dirEnum nextObject]) {
        std::string filename = [path cStringUsingEncoding:NSUTF8StringEncoding];
        if (IsRuleFile(filename)) {
            if (EndsWith(filename,".rule")) {
                // .rule file exists, so tell CreateRuleFiles not to change it
                keeprules.push_back(filename);
            } else {
                // this is a deprecated .table/tree/colors/icons file
                deprecated.push_back(filename);
            }
        }
    }

    if (deprecated.size() > 0) {
        // convert deprecated files into new .rule files (if not in keeprules)
        // and then delete all the deprecated files
        CreateRuleFiles(deprecated, keeprules);
    }
}

// -----------------------------------------------------------------------------

static void CreateRuleLinks(std::string& htmldata, const std::string& dir,
                            const std::string& prefix, const std::string& title, bool candelete)
{
    htmldata += "<p>";
    htmldata += title;
    htmldata += "</p><p>";

    NSFileManager *fm = [NSFileManager defaultManager];
    NSString *rdir = [NSString stringWithCString:dir.c_str() encoding:NSUTF8StringEncoding];
    NSDirectoryEnumerator *dirEnum = [fm enumeratorAtPath:rdir];
    NSString *path;
    
    while (path = [dirEnum nextObject]) {
        std::string pstr = [path cStringUsingEncoding:NSUTF8StringEncoding];
        size_t extn = pstr.rfind(".rule");
        if (extn != std::string::npos) {
            // path is to a .rule file
            std::string rulename = pstr.substr(0,extn);
            if (candelete) {
                // allow user to delete .rule file
                htmldata += "<a href=\"delete:";
                htmldata += prefix;
                htmldata += pstr;
                htmldata += "\"><font size=-1 color='red'>DELETE</font></a>&nbsp;&nbsp;&nbsp;";
                // allow user to edit .rule file
                htmldata += "<a href=\"edit:";
                htmldata += prefix;
                htmldata += pstr;
                htmldata += "\"><font size=-1 color='green'>EDIT</font></a>&nbsp;&nbsp;&nbsp;";
            } else {
                // allow user to read supplied .rule file
                htmldata += "<a href=\"edit:";
                htmldata += prefix;
                htmldata += pstr;
                htmldata += "\"><font size=-1 color='green'>READ</font></a>&nbsp;&nbsp;&nbsp;";
            }
            // use "open:" link rather than "rule:" link so dialog closes immediately
            htmldata += "<a href=\"open:";
            htmldata += prefix;
            htmldata += pstr;
            htmldata += "\">";
            htmldata += rulename;
            htmldata += "</a><br>";
        }
    }
}

// -----------------------------------------------------------------------------

- (void)showAlgoHelp
{
    NSString *algoname = [NSString stringWithCString:GetAlgoName(algoindex) encoding:NSUTF8StringEncoding];
    if ([algoname isEqualToString:@"RuleLoader"]) {
        // first check for any .rule/tree/table files in Documents
        // (created by iTunes file sharing) and move them into Documents/Rules/
        MoveSharedFiles();
        
        // now convert any .tree/table files to .rule files
        ConvertOldRules();
        
        // create html data with links to the user's .rule files and Golly's supplied .rule files
        std::string htmldata;
        htmldata += "<html><body bgcolor=\"#FFFFCE\"><a name=\"top\"></a>";
        htmldata += "<p>The RuleLoader algorithm allows CA rules to be specified in .rule files.";
        htmldata += " Given the rule string \"Foo\", RuleLoader will search for a file called Foo.rule";
        htmldata += " in your rules folder (Documents/Rules/), then in the rules folder supplied with Golly.";
        htmldata += "</p><font size=+2 color='black'><b>";
        CreateRuleLinks(htmldata, userrules, "Documents/Rules/", "Your .rule files:", true);
        CreateRuleLinks(htmldata, rulesdir, "Rules/", "Supplied .rule files:", false);
        htmldata += "</b></font><p><center><a href=\"#top\"><b>TOP</b></a></center></body></html>";
        [htmlView loadHTMLString:[NSString stringWithCString:htmldata.c_str() encoding:NSUTF8StringEncoding]
                         baseURL:[NSURL fileURLWithPath:[[NSBundle mainBundle] bundlePath]]];
                                 // the above base URL is needed for links like <img src='foo.png'> to work
    } else {
        // replace any spaces with underscores
        algoname = [algoname stringByReplacingOccurrencesOfString:@" " withString:@"_"];
        NSBundle *bundle=[NSBundle mainBundle];
        NSString *filePath = [bundle pathForResource:algoname
                                              ofType:@"html"
                                         inDirectory:@"Help/Algorithms"];
        if (filePath) {
            NSURL *fileUrl = [NSURL fileURLWithPath:filePath];
            NSURLRequest *request = [NSURLRequest requestWithURL:fileUrl];
            [htmlView loadRequest:request];
        } else {
            [htmlView loadHTMLString:@"<html><center><font size=+4 color='red'>Failed to find html file!</font></center></html>"
                             baseURL:nil];
        }
    }
}

// -----------------------------------------------------------------------------

static bool keepalgoindex = false;

- (void)viewWillAppear:(BOOL)animated
{
    [super viewWillAppear:animated];

    if (keepalgoindex) {
        // ShowTextFile has finished displaying a modal dialog so we don't want to change algoindex
        keepalgoindex = false;
        [self showAlgoHelp];        // user might have created a new .rule file
        return;
    }

    // save current location
    curroffset[algoindex] = htmlView.scrollView.contentOffset;
    
    // show current algo and rule
    algoindex = currlayer->algtype;
    [algoButton setTitle:[NSString stringWithCString:GetAlgoName(algoindex) encoding:NSUTF8StringEncoding]
                forState:UIControlStateNormal];
    [self showAlgoHelp];
    
    oldrule = currlayer->algo->getrule();
    oldmaxstate = currlayer->algo->NumCellStates() - 1;
    [ruleText setText:[NSString stringWithCString:currlayer->algo->getrule() encoding:NSUTF8StringEncoding]];
    unknownLabel.hidden = YES;
    
    // show corresponding name for current rule (or UNNAMED if there isn't one)
    [rulePicker selectRow:[self findNamedRule:ruleText.text] inComponent:0 animated:NO];

    // selection might change if grid becomes smaller, so save current selection
    // if RememberRuleChange/RememberAlgoChange is called later
    SaveCurrentSelection();
}

// -----------------------------------------------------------------------------

- (void)viewWillDisappear:(BOOL)animated
{
	[super viewWillDisappear:animated];

    [htmlView stopLoading];  // in case the web view is still loading its content

    // save current location of htmlView
    curroffset[algoindex] = htmlView.scrollView.contentOffset;
}

// -----------------------------------------------------------------------------

- (IBAction)changeAlgorithm:(id)sender
{
    UIActionSheet *sheet = [[UIActionSheet alloc]
                            initWithTitle:nil
                            delegate:self
                            cancelButtonTitle:nil
                            destructiveButtonTitle:nil
                            otherButtonTitles:nil];
    
    for (int i=0; i<NumAlgos(); i++) {
        [sheet addButtonWithTitle:[NSString stringWithCString:GetAlgoName(i) encoding:NSUTF8StringEncoding]];
    }
    
    [sheet showFromRect:algoButton.frame inView:algoButton.superview animated:NO];
}

// -----------------------------------------------------------------------------

static NSInteger globalButton;

- (void)doDelayedAction
{
    if (globalButton >= 0 && globalButton < NumAlgos()) {

        // save current location
        curroffset[algoindex] = htmlView.scrollView.contentOffset;
        
        // save chosen algo for later use
        algoindex = (algo_type)globalButton;
        
        // display the chosen algo name
        [algoButton setTitle:[NSString stringWithCString:GetAlgoName(algoindex) encoding:NSUTF8StringEncoding]
                    forState:UIControlStateNormal];
        
        // display help for chosen algo
        [self showAlgoHelp];
        
        // if current rule (in ruleText) is valid in the chosen algo then don't change rule,
        // otherwise switch to the algo's default rule
        std::string thisrule = [ruleText.text cStringUsingEncoding:NSUTF8StringEncoding];
        if (thisrule.empty()) thisrule = RULE_IF_EMPTY;
        
        lifealgo* tempalgo = CreateNewUniverse(algoindex);
        const char* err = tempalgo->setrule(thisrule.c_str());
        if (err) {
            // switch to tempalgo's default rule
            std::string defrule = tempalgo->DefaultRule();
            size_t thispos = thisrule.find(':');
            if (thispos != std::string::npos) {
                // preserve valid topology so we can do things like switch from
                // "LifeHistory:T30,20" in RuleLoader to "B3/S23:T30,20" in QuickLife
                size_t defpos = defrule.find(':');
                if (defpos != std::string::npos) {
                    // default rule shouldn't have a suffix but play safe and remove it
                    defrule = defrule.substr(0, defpos);
                }
                defrule += ":";
                defrule += thisrule.substr(thispos+1);
            }
            [ruleText setText:[NSString stringWithCString:defrule.c_str() encoding:NSUTF8StringEncoding]];
            [rulePicker selectRow:[self findNamedRule:ruleText.text] inComponent:0 animated:NO];
            unknownLabel.hidden = YES;
        }
        delete tempalgo;
    }
}

// -----------------------------------------------------------------------------

// called when the user selects an item from UIActionSheet created in changeAlgorithm

- (void)actionSheet:(UIActionSheet *)sheet didDismissWithButtonIndex:(NSInteger)buttonIndex
{
    // user interaction is disabled at this moment, which is a problem if Warning gets called
    // (the OK button won't work) so we have to call the appropriate action code AFTER this
    // callback has finished and user interaction restored
    globalButton = buttonIndex;
    [self performSelector:@selector(doDelayedAction) withObject:nil afterDelay:0.01];
}

// -----------------------------------------------------------------------------

- (void)checkRule
{
    // check displayed rule and show message if it's not valid in any algo,
    // or if the rule is valid then displayed algo might need to change
    std::string thisrule = [ruleText.text cStringUsingEncoding:NSUTF8StringEncoding];
    if (thisrule.empty()) {
        thisrule = RULE_IF_EMPTY;
        [ruleText setText:[NSString stringWithCString:thisrule.c_str() encoding:NSUTF8StringEncoding]];
        [rulePicker selectRow:[self findNamedRule:ruleText.text] inComponent:0 animated:NO];
    }
    
    // 1st check if rule is valid in displayed algo
    lifealgo* tempalgo = CreateNewUniverse(algoindex);
    const char* err = tempalgo->setrule(thisrule.c_str());
    if (err) {
        // check rule in other algos
        for (int newindex = 0; newindex < NumAlgos(); newindex++) {
            if (newindex != algoindex) {
                delete tempalgo;
                tempalgo = CreateNewUniverse(newindex);
                err = tempalgo->setrule(thisrule.c_str());
                if (!err) {
                    // save current location
                    curroffset[algoindex] = htmlView.scrollView.contentOffset;
                    algoindex = newindex;
                    [algoButton setTitle:[NSString stringWithCString:GetAlgoName(algoindex) encoding:NSUTF8StringEncoding]
                                forState:UIControlStateNormal];
                    [self showAlgoHelp];
                    break;
                }
            }
        }
    }
    
    unknownLabel.hidden = (err == nil);
    if (err) {
        // unknown rule must be unnamed
        [rulePicker selectRow:UNNAMED_ROW inComponent:0 animated:NO];
        Beep();
    } else {
        // need canonical rule for comparison in findNamedRule and GetRuleName
        NSString *canonrule = [NSString stringWithCString:tempalgo->getrule() encoding:NSUTF8StringEncoding];
        [rulePicker selectRow:[self findNamedRule:canonrule] inComponent:0 animated:NO];
    }

    delete tempalgo;
}

// -----------------------------------------------------------------------------

- (IBAction)cancelRuleChange:(id)sender
{
    [self dismissViewControllerAnimated:YES completion:nil];
}

// -----------------------------------------------------------------------------

- (IBAction)doRuleChange:(id)sender
{
    // clear first responder if necessary (ie. remove keyboard)
    [self.view endEditing:YES];
    
    if (unknownLabel.hidden == NO) {
        Warning("Given rule is not valid in any algorithm.");
        // don't dismiss modal view
        return;
    }
    
    // dismiss modal view first in case ChangeAlgorithm calls BeginProgress
    [self dismissViewControllerAnimated:YES completion:nil];
    
    // get current rule in ruleText
    std::string newrule = [ruleText.text cStringUsingEncoding:NSUTF8StringEncoding];
    if (newrule.empty()) newrule = RULE_IF_EMPTY;
    
    if (algoindex == currlayer->algtype) {
        // check if new rule is valid in current algorithm
        // (if not then revert to original rule saved in viewWillAppear)
        const char* err = currlayer->algo->setrule(newrule.c_str());
        if (err) RestoreRule(oldrule.c_str());
        
        // convert newrule to canonical form for comparison with oldrule, then
        // check if the rule string changed or if the number of states changed
        newrule = currlayer->algo->getrule();
        int newmaxstate = currlayer->algo->NumCellStates() - 1;
        if (oldrule != newrule || oldmaxstate != newmaxstate) {
		
			// if pattern exists and is at starting gen then ensure savestart is true
			// so that SaveStartingPattern will save pattern to suitable file
			// (and thus undo/reset will work correctly)
			if (currlayer->algo->getGeneration() == currlayer->startgen && !currlayer->algo->isEmpty()) {
				currlayer->savestart = true;
			}

            // if grid is bounded then remove any live cells outside grid edges
            if (currlayer->algo->gridwd > 0 || currlayer->algo->gridht > 0) {
                ClearOutsideGrid();
            }
            
            // rule change might have changed the number of cell states;
            // if there are fewer states then pattern might change
            if (newmaxstate < oldmaxstate && !currlayer->algo->isEmpty()) {
                ReduceCellStates(newmaxstate);
            }
            
            if (allowundo) {
                currlayer->undoredo->RememberRuleChange(oldrule.c_str());
            }
        }
        UpdateLayerColors();
        UpdateEverything();
        
    } else {
        // change the current algorithm and switch to the new rule
        // (if the new rule is invalid then the algo's default rule will be used);
        // this also calls UpdateLayerColors, UpdateEverything and RememberAlgoChange
        ChangeAlgorithm(algoindex, newrule.c_str());
    }
    
    SavePrefs();
}

// -----------------------------------------------------------------------------

// UITextFieldDelegate methods:

- (void)textFieldDidBeginEditing:(UITextField *)tf
{
    // probably best to clear any message about unknown rule
    unknownLabel.hidden = YES;
}

- (void)textFieldDidEndEditing:(UITextField *)tf
{
    // called when rule editing has ended (ie. keyboard disappears)
    [self checkRule];
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

// UIPickerViewDelegate and UIPickerViewDataSource methods:

- (NSInteger)numberOfComponentsInPickerView:(UIPickerView *)pickerView
{
	return 1;
}

- (NSInteger)pickerView:(UIPickerView *)pickerView numberOfRowsInComponent:(NSInteger)component
{
	return NUM_ROWS;
}

- (NSString *)pickerView:(UIPickerView *)pickerView titleForRow:(NSInteger)row forComponent:(NSInteger)component;
{
	return namedRules[row * 2];
}

- (CGFloat)pickerView:(UIPickerView *)pickerView rowHeightForComponent:(NSInteger)component
{
    return 38.0;   // nicer spacing
}

- (void)pickerView:(UIPickerView *)pickerView didSelectRow:(NSInteger)row inComponent:(NSInteger)component
{
    if (row < UNNAMED_ROW) {
        [ruleText setText:namedRules[row * 2 + 1]];
        [self checkRule];
    }
}

// -----------------------------------------------------------------------------

// UIWebViewDelegate methods:

- (void)webViewDidStartLoad:(UIWebView *)webView
{
    // show the activity indicator in the status bar
    [UIApplication sharedApplication].networkActivityIndicatorVisible = YES;
}

- (void)webViewDidFinishLoad:(UIWebView *)webView
{
    // hide the activity indicator in the status bar
    [UIApplication sharedApplication].networkActivityIndicatorVisible = NO;
    // restore old offset here
    htmlView.scrollView.contentOffset = curroffset[algoindex];
}

- (void)webView:(UIWebView *)webView didFailLoadWithError:(NSError *)error
{
    // hide the activity indicator in the status bar and display error message
    [UIApplication sharedApplication].networkActivityIndicatorVisible = NO;
    // we can safely ignore -999 errors
    if (error.code == NSURLErrorCancelled) return;
    Warning([error.localizedDescription cStringUsingEncoding:NSUTF8StringEncoding]);
}

- (BOOL)webView:(UIWebView *)webView shouldStartLoadWithRequest:(NSURLRequest *)request navigationType:(UIWebViewNavigationType)navigationType
{
    if (navigationType == UIWebViewNavigationTypeLinkClicked) {
        NSURL *url = [request URL];
        NSString *link = [url absoluteString];
        
        // look for special prefixes used by Golly (and return NO if found)
        if ([link hasPrefix:@"open:"]) {
            // open specified file, but dismiss modal view first in case OpenFile calls BeginProgress
            [self dismissViewControllerAnimated:YES completion:nil];
            std::string path = [[link substringFromIndex:5] cStringUsingEncoding:NSUTF8StringEncoding];
            OpenFile(path.c_str());
            SavePrefs();
            return NO;
        }
        if ([link hasPrefix:@"rule:"]) {
            // copy specified rule into ruleText
            [ruleText setText:[link substringFromIndex:5]];
            [self checkRule];
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
                curroffset[algoindex] = htmlView.scrollView.contentOffset;
                [self showAlgoHelp];
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
            // we pass self to ShowTextFile so it doesn't use current tab's view controller
            ShowTextFile(fullpath.c_str(), self);
            // tell viewWillAppear not to reset algoindex to currlayer->algtype
            keepalgoindex = true;
            return NO;
        }
    }
    return YES;
}

@end

// =============================================================================

std::string GetRuleName(const std::string& rule)
{
    std::string result = "";
    NSString* nsrule = [NSString stringWithCString:rule.c_str() encoding:NSUTF8StringEncoding];
    
    for (int row = 0; row < UNNAMED_ROW; row++) {
        if ([nsrule isEqualToString:namedRules[row * 2 + 1]]) {
            result = [namedRules[row * 2] cStringUsingEncoding:NSUTF8StringEncoding];
            break;
        }
    }

    return result;
}
