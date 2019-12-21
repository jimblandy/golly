// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "ruleloaderalgo.h"

#include "util.h"       // for lifegetuserrules, lifegetrulesdir, lifefatal

#include <string.h>     // for strcmp, strchr
#include <string>       // for std::string

const char* noTABLEorTREE = "No @TABLE or @TREE section found in .rule file.";

int ruleloaderalgo::NumCellStates()
{
    if (rule_type == TABLE)
        return LocalRuleTable->NumCellStates();
    else // rule_type == TREE
        return LocalRuleTree->NumCellStates();
}

static FILE* OpenRuleFile(std::string& rulename, const char* dir)
{
    // try to open rulename.rule in given dir
    std::string path = dir;
    int istart = (int)path.size();
    path += rulename + ".rule";
    // change "dangerous" characters to underscores
    for (unsigned int i=istart; i<path.size(); i++)
        if (path[i] == '/' || path[i] == '\\') path[i] = '_';
    return fopen(path.c_str(), "rt");
}

void ruleloaderalgo::SetAlgoVariables(RuleTypes ruletype)
{
    // yuk -- we wouldn't need to copy all these variables if we merged
    // the RuleTable and RuleTree code into one algo
    rule_type = ruletype;
    if (rule_type == TABLE) {
        maxCellStates = LocalRuleTable->NumCellStates();
        grid_type = LocalRuleTable->getgridtype();
        gridwd = LocalRuleTable->gridwd;
        gridht = LocalRuleTable->gridht;
        gridleft = LocalRuleTable->gridleft;
        gridright = LocalRuleTable->gridright;
        gridtop = LocalRuleTable->gridtop;
        gridbottom = LocalRuleTable->gridbottom;
        boundedplane = LocalRuleTable->boundedplane;
        sphere = LocalRuleTable->sphere;
        htwist = LocalRuleTable->htwist;
        vtwist = LocalRuleTable->vtwist;
        hshift = LocalRuleTable->hshift;
        vshift = LocalRuleTable->vshift;
    } else {
        // rule_type == TREE
        maxCellStates = LocalRuleTree->NumCellStates();
        grid_type = LocalRuleTree->getgridtype();
        gridwd = LocalRuleTree->gridwd;
        gridht = LocalRuleTree->gridht;
        gridleft = LocalRuleTree->gridleft;
        gridright = LocalRuleTree->gridright;
        gridtop = LocalRuleTree->gridtop;
        gridbottom = LocalRuleTree->gridbottom;
        boundedplane = LocalRuleTree->boundedplane;
        sphere = LocalRuleTree->sphere;
        htwist = LocalRuleTree->htwist;
        vtwist = LocalRuleTree->vtwist;
        hshift = LocalRuleTree->hshift;
        vshift = LocalRuleTree->vshift;
    }
    
    // need to clear cache
    ghashbase::setrule("not used");
}

const char* ruleloaderalgo::LoadTableOrTree(FILE* rulefile, const char* rule)
{
    const char *err;
    const int MAX_LINE_LEN = 4096;
    char line_buffer[MAX_LINE_LEN+1];
    int lineno = 0;

    linereader lr(rulefile);

    // find line starting with @TABLE or @TREE
    while (lr.fgets(line_buffer,MAX_LINE_LEN) != 0) {
        lineno++;
        if (strcmp(line_buffer, "@TABLE") == 0) {
            err = LocalRuleTable->LoadTable(rulefile, lineno, '@', rule);
            // err is the result of setrule(rule)
            if (err == NULL) {
                SetAlgoVariables(TABLE);
            }
            // LoadTable has closed rulefile so don't do lr.close()
            return err;
        }
        if (strcmp(line_buffer, "@TREE") == 0) {
            err = LocalRuleTree->LoadTree(rulefile, lineno, '@', rule);
            // err is the result of setrule(rule)
            if (err == NULL) {
                SetAlgoVariables(TREE);
            }
            // LoadTree has closed rulefile so don't do lr.close()
            return err;
        }
    }
    
    lr.close();
    return noTABLEorTREE;
}

const char* ruleloaderalgo::setrule(const char* s)
{
    const char *err;
    const char *colonptr = strchr(s,':');
    std::string rulename(s);
    if (colonptr) rulename.assign(s,colonptr);
    
    // first check if rulename is the default rule for RuleTable or RuleTree
    // in which case there is no need to look for a .rule/table/tree file
    if (LocalRuleTable->IsDefaultRule(rulename.c_str())) {
        err = LocalRuleTable->setrule(s);
        if (err) return err;
        SetAlgoVariables(TABLE);
        return NULL;
    }
    if (LocalRuleTree->IsDefaultRule(rulename.c_str())) {
        err = LocalRuleTree->setrule(s);
        if (err) return err;
        SetAlgoVariables(TREE);
        return NULL;
    }
    
    // look for .rule file in user's rules dir then in Golly's rules dir
    bool inuser = true;
    FILE* rulefile = OpenRuleFile(rulename, lifegetuserrules());
    if (!rulefile) {
        inuser = false;
        rulefile = OpenRuleFile(rulename, lifegetrulesdir());
    }
    if (rulefile) {
        err = LoadTableOrTree(rulefile, s);
        if (inuser && err && (strcmp(err, noTABLEorTREE) == 0)) {
            // if .rule file was found in user's rules dir but had no
            // @TABLE or @TREE section then we look in Golly's rules dir
            // (this lets user override the colors/icons in a supplied .rule
            // file without having to copy the entire file)
            rulefile = OpenRuleFile(rulename, lifegetrulesdir());
            if (rulefile) err = LoadTableOrTree(rulefile, s);
        }
        return err;
    }

    // no .rule file so try to load .table file
    err = LocalRuleTable->setrule(s);
    if (err == NULL) {
        SetAlgoVariables(TABLE);
        return NULL;
    }
    
    // no .table file so try to load .tree file
    err = LocalRuleTree->setrule(s);
    if (err == NULL) {
        SetAlgoVariables(TREE);
        return NULL;
    }
    
    // make sure we show given rule string in final error msg (probably "File not found")
    static std::string badrule;
    badrule = err;
    badrule += "\nGiven rule: ";
    badrule += s;
    return badrule.c_str();
}

const char* ruleloaderalgo::getrule() {
    if (rule_type == TABLE)
        return LocalRuleTable->getrule();
    else // rule_type == TREE
        return LocalRuleTree->getrule();
}

const char* ruleloaderalgo::DefaultRule() {
    // use RuleTree's default rule (B3/S23)
    return LocalRuleTree->DefaultRule();
}

ruleloaderalgo::ruleloaderalgo()
{
    LocalRuleTable = new ruletable_algo();
    LocalRuleTree = new ruletreealgo();

    // initialize rule_type
    LocalRuleTree->setrule( LocalRuleTree->DefaultRule() );
    SetAlgoVariables(TREE);
}

ruleloaderalgo::~ruleloaderalgo()
{
    delete LocalRuleTable;
    delete LocalRuleTree;
}

state ruleloaderalgo::slowcalc(state nw, state n, state ne, state w, state c,
                               state e, state sw, state s, state se) 
{
    if (rule_type == TABLE)
        return LocalRuleTable->slowcalc(nw, n, ne, w, c, e, sw, s, se);
    else // rule_type == TREE
        return LocalRuleTree->slowcalc(nw, n, ne, w, c, e, sw, s, se);
}

static lifealgo* creator()
{
    return new ruleloaderalgo();
}

void ruleloaderalgo::doInitializeAlgoInfo(staticAlgoInfo &ai) 
{
    ghashbase::doInitializeAlgoInfo(ai);
    ai.setAlgorithmName("RuleLoader");
    ai.setAlgorithmCreator(&creator);
    ai.minstates = 2;
    ai.maxstates = 256;
    // init default color scheme
    ai.defgradient = true;              // use gradient
    ai.defr1 = 255;                     // start color = red
    ai.defg1 = 0;
    ai.defb1 = 0;
    ai.defr2 = 255;                     // end color = yellow
    ai.defg2 = 255;
    ai.defb2 = 0;
    // if not using gradient then set all states to white
    for (int i=0; i<256; i++) {
        ai.defr[i] = ai.defg[i] = ai.defb[i] = 255;
    }
}
