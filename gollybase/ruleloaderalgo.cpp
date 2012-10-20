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

int ruleloaderalgo::NumCellStates()
{
    if (rule_type == TABLE)
        return LocalRuleTable->NumCellStates();
    else // rule_type == TREE
        return LocalRuleTree->NumCellStates();
}

const char* ruleloaderalgo::setrule(const char* s)
{
    // try to load rule.table first
    const char *err = LocalRuleTable->setrule(s);
    if (err == NULL) {
        rule_type = TABLE;
        maxCellStates = LocalRuleTable->NumCellStates();
        return NULL;
    }
    
    // now try to load rule.tree
    err = LocalRuleTree->setrule(s);
    if (err == NULL) {
        rule_type = TREE;
        maxCellStates = LocalRuleTree->NumCellStates();
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

    // probably don't need to do anything else, but play safe
    LocalRuleTree->setrule( LocalRuleTree->DefaultRule() );
    rule_type = TREE;
    maxCellStates = LocalRuleTree->NumCellStates();
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
