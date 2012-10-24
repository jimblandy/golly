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

#include "ruleloaderalgo.h"

#include "util.h"       // for lifegetuserrules, lifegetrulesdir, lifefatal
#include <string>       // for std::string

int ruleloaderalgo::NumCellStates()
{
    if (rule_type == TABLE)
        return LocalRuleTable->NumCellStates();
    else // rule_type == TREE
        return LocalRuleTree->NumCellStates();
}

static FILE* OpenRuleFile(std::string& rulename, const char* dir, std::string& path)
{
    // look for rulename.rule in given dir and set path
    path = dir;
    int istart = (int)path.size();
    path += rulename + ".rule";
    // change "dangerous" characters to underscores
    for (unsigned int i=istart; i<path.size(); i++)
        if (path[i] == '/' || path[i] == '\\') path[i] = '_';
    return fopen(path.c_str(), "rt");
}

void ruleloaderalgo::SetAlgoVariables(RuleTypes ruletype)
{
    // yuk!!! too error prone, so rethink???
    // (wouldn't neeed this stuff if we merged the RuleTable and RuleTree code into one algo)
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
        if (strncmp(line_buffer, "@TABLE", 6) == 0) {
            err = LocalRuleTable->LoadTable(rulefile, lineno, '@', rule);
            // err is the result of setrule(rule)
            if (err == NULL) {
                SetAlgoVariables(TABLE);
            }
            // LoadTable has closed rulefile so don't do lr.close()
            return err;
        }
        if (strncmp(line_buffer, "@TREE", 5) == 0) {
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
    return "No @TABLE or @TREE section found in .rule file.";
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
        LocalRuleTable->setrule(s);
        SetAlgoVariables(TABLE);
        return NULL;
    }
    if (LocalRuleTree->IsDefaultRule(rulename.c_str())) {
        LocalRuleTree->setrule(s);
        SetAlgoVariables(TREE);
        return NULL;
    }
    
    // look for .rule file in user's rules dir then in Golly's rules dir
    std::string fullpath;
    FILE* rulefile = OpenRuleFile(rulename, lifegetuserrules(), fullpath);
    if (!rulefile)
        rulefile = OpenRuleFile(rulename, lifegetrulesdir(), fullpath);
    if (rulefile) {
        return LoadTableOrTree(rulefile, s);
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
        
    return err;
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
    
    if (LocalRuleTable == NULL) lifefatal("RuleLoader failed to create local RuleTable!");
    if (LocalRuleTree == NULL) lifefatal("RuleLoader failed to create local RuleTree!");

    // probably don't need to do anything else, but play safe
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
