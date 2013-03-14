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

#include "bigint.h"
#include "lifealgo.h"
#include "qlifealgo.h"
#include "hlifealgo.h"
#include "writepattern.h"
#include "util.h"           // for linereader

#include "utils.h"          // for Warning, TimeInSeconds, Poller, event_checker, etc
#include "prefs.h"          // for allowundo, etc
#include "status.h"         // for ErrorMessage, etc
#include "file.h"           // for WritePattern, LoadPattern, etc
#include "algos.h"          // for *_ALGO, algo_type, CreateNewUniverse, etc
#include "layer.h"          // for currlayer, RestoreRule, etc
#include "view.h"           // for UpdatePatternAndStatus, draw_pending, etc
#include "undo.h"           // for UndoRedo
#include "control.h"

#include <stdexcept>        // for std::runtime_error and std::exception
#include <sstream>          // for std::ostringstream

#import "PatternViewController.h"   // for UpdateStatus, BeginProgress, etc

// -----------------------------------------------------------------------------

bool generating = false;        // currently generating pattern?
int minexpo = 0;                // step exponent at maximum delay (must be <= 0)
double begintime, endtime;      // for timing info
double begingen, endgen;        // ditto

const char* empty_pattern = "All cells are dead.";

// -----------------------------------------------------------------------------

// macros for checking if a certain string exists in a list of strings
#define FOUND(l,s) (std::find(l.begin(),l.end(),s) != l.end())
#define NOT_FOUND(l,s) (std::find(l.begin(),l.end(),s) == l.end())

// -----------------------------------------------------------------------------

bool SaveStartingPattern()
{
    if ( currlayer->algo->getGeneration() > currlayer->startgen ) {
        // don't do anything if current gen count > starting gen
        return true;
    }
    
    // save current rule, dirty flag, scale, location, etc
    currlayer->startname = currlayer->currname;
    currlayer->startrule = currlayer->algo->getrule();
    currlayer->startdirty = currlayer->dirty;
    currlayer->startmag = currlayer->view->getmag();
    currlayer->startx = currlayer->view->x;
    currlayer->starty = currlayer->view->y;
    currlayer->startbase = currlayer->currbase;
    currlayer->startexpo = currlayer->currexpo;
    currlayer->startalgo = currlayer->algtype;
    
    // if this layer is a clone then save some settings in other clones
    if (currlayer->cloneid > 0) {
        for ( int i = 0; i < numlayers; i++ ) {
            Layer* cloneptr = GetLayer(i);
            if (cloneptr != currlayer && cloneptr->cloneid == currlayer->cloneid) {
                cloneptr->startname = cloneptr->currname;
                cloneptr->startx = cloneptr->view->x;
                cloneptr->starty = cloneptr->view->y;
                cloneptr->startmag = cloneptr->view->getmag();
                cloneptr->startbase = cloneptr->currbase;
                cloneptr->startexpo = cloneptr->currexpo;
            }
        }
    }
    
    // save current selection
    currlayer->startsel = currlayer->currsel;
    
    if ( !currlayer->savestart ) {
        // no need to save pattern; ResetPattern will load currfile
        currlayer->startfile.clear();
        return true;
    }
    
    // save starting pattern in tempstart file
    if ( currlayer->algo->hyperCapable() ) {
        // much faster to save pattern in a macrocell file
        const char* err = WritePattern(currlayer->tempstart.c_str(), MC_format,
                                       no_compression, 0, 0, 0, 0);
        if (err) {
            ErrorMessage(err);
            // don't allow user to continue generating
            return false;
        }
    } else {
        // can only save as RLE if edges are within getcell/setcell limits
        bigint top, left, bottom, right;
        currlayer->algo->findedges(&top, &left, &bottom, &right);
        if ( OutsideLimits(top, left, bottom, right) ) {
            ErrorMessage("Starting pattern is outside +/- 10^9 boundary.");
            // don't allow user to continue generating
            return false;
        }
        int itop = top.toint();
        int ileft = left.toint();
        int ibottom = bottom.toint();
        int iright = right.toint();      
        // use XRLE format so the pattern's top left location and the current
        // generation count are stored in the file
        const char* err = WritePattern(currlayer->tempstart.c_str(), XRLE_format, no_compression,
                                       itop, ileft, ibottom, iright);
        if (err) {
            ErrorMessage(err);
            // don't allow user to continue generating
            return false;
        }
    }
    
    currlayer->startfile = currlayer->tempstart;   // ResetPattern will load tempstart
    return true;
}

// -----------------------------------------------------------------------------

void SetGenIncrement()
{
    if (currlayer->currexpo > 0) {
        bigint inc = 1;
        // set increment to currbase^currexpo
        int i = currlayer->currexpo;
        while (i > 0) {
            inc.mul_smallint(currlayer->currbase);
            i--;
        }
        currlayer->algo->setIncrement(inc);
    } else {
        currlayer->algo->setIncrement(1);
    }
}

// -----------------------------------------------------------------------------

void SetStepExponent(int newexpo)
{
    currlayer->currexpo = newexpo;
    if (currlayer->currexpo < minexpo) currlayer->currexpo = minexpo;
    SetGenIncrement();
}

// -----------------------------------------------------------------------------

void SetMinimumStepExponent()
{
    // set minexpo depending on mindelay and maxdelay
    minexpo = 0;
    if (mindelay > 0) {
        int d = mindelay;
        minexpo--;
        while (d < maxdelay) {
            d *= 2;
            minexpo--;
        }
    }
}

// -----------------------------------------------------------------------------

void ResetPattern(bool resetundo)
{
    if (currlayer->algo->getGeneration() == currlayer->startgen) return;
    
    if (currlayer->algo->getGeneration() < currlayer->startgen) {
        // if this happens then startgen logic is wrong
        Warning("Current gen < starting gen!");
        return;
    }
    
    if (currlayer->startfile.length() == 0 && currlayer->currfile.length() == 0) {
        // if this happens then savestart logic is wrong
        Warning("Starting pattern cannot be restored!");
        return;
    }
    
    // save current algo and rule
    algo_type oldalgo = currlayer->algtype;
    std::string oldrule = currlayer->algo->getrule();
    
    // restore pattern and settings saved by SaveStartingPattern;
    // first restore algorithm
    currlayer->algtype = currlayer->startalgo;
    
    // restore starting pattern
    if ( currlayer->startfile.length() == 0 ) {
        // restore pattern from currfile
        LoadPattern(currlayer->currfile.c_str(), "");
    } else {
        // restore pattern from startfile
        LoadPattern(currlayer->startfile.c_str(), "");
    }
    
    if (currlayer->algo->getGeneration() != currlayer->startgen) {
        // LoadPattern failed to reset the gen count to startgen
        // (probably because the user deleted the starting pattern)
        // so best to clear the pattern and reset the gen count
        CreateUniverse();
        currlayer->algo->setGeneration(currlayer->startgen);
    }
    
    // ensure savestart flag is correct
    currlayer->savestart = currlayer->startfile.length() != 0;
    
    // restore settings saved by SaveStartingPattern
    RestoreRule(currlayer->startrule.c_str());
    currlayer->currname = currlayer->startname;
    currlayer->dirty = currlayer->startdirty;
    if (restoreview) {
        currlayer->view->setpositionmag(currlayer->startx, currlayer->starty, currlayer->startmag);
    }
    
    // restore step size and set increment
    currlayer->currbase = currlayer->startbase;
    currlayer->currexpo = currlayer->startexpo;
    SetGenIncrement();
    
    // if this layer is a clone then restore some settings in other clones
    if (currlayer->cloneid > 0) {
        for ( int i = 0; i < numlayers; i++ ) {
            Layer* cloneptr = GetLayer(i);
            if (cloneptr != currlayer && cloneptr->cloneid == currlayer->cloneid) {
                cloneptr->currname = cloneptr->startname;
                if (restoreview) {
                    cloneptr->view->setpositionmag(cloneptr->startx, cloneptr->starty,
                                                   cloneptr->startmag);
                }
                cloneptr->currbase = cloneptr->startbase;
                cloneptr->currexpo = cloneptr->startexpo;
                // also synchronize dirty flags and update items in Layer menu
                cloneptr->dirty = currlayer->dirty;
                //!!! UpdateLayerItem(i);
            }
        }
    }
    
    // restore selection
    currlayer->currsel = currlayer->startsel;
    
    // switch to default colors if algo/rule changed
    std::string newrule = currlayer->algo->getrule();
    if (oldalgo != currlayer->algtype || oldrule != newrule) {
        UpdateLayerColors();
    }
    
    // update window title in case currname, rule or dirty flag changed
    //!!! SetWindowTitle(currlayer->currname);
    
    if (allowundo) {
        if (resetundo) {
            // wind back the undo history to the starting pattern
            currlayer->undoredo->SyncUndoHistory();
        }
    }
}

// -----------------------------------------------------------------------------

void RestorePattern(bigint& gen, const char* filename,
                    bigint& x, bigint& y, int mag, int base, int expo)
{
    // called to undo/redo a generating change
    if (gen == currlayer->startgen) {
        // restore starting pattern (false means don't call SyncUndoHistory)
        ResetPattern(false);
    } else {
        // restore pattern in given filename
        LoadPattern(filename, "");
        
        if (currlayer->algo->getGeneration() != gen) {
            // filename could not be loaded for some reason,
            // so best to clear the pattern and set the expected gen count
            CreateUniverse();
            currlayer->algo->setGeneration(gen);
        }
        
        // restore step size and set increment
        currlayer->currbase = base;
        currlayer->currexpo = expo;
        SetGenIncrement();
        
        // restore position and scale, if allowed
        if (restoreview) currlayer->view->setpositionmag(x, y, mag);
        
        UpdatePatternAndStatus();
    }
}

// -----------------------------------------------------------------------------

const char* ChangeGenCount(const char* genstring, bool inundoredo)
{
    // disallow alphabetic chars in genstring
    for (unsigned int i = 0; i < strlen(genstring); i++)
        if ( (genstring[i] >= 'a' && genstring[i] <= 'z') ||
             (genstring[i] >= 'A' && genstring[i] <= 'Z') )
            return "Alphabetic character is not allowed in generation string.";
    
    bigint oldgen = currlayer->algo->getGeneration();
    bigint newgen(genstring);
    
    if (genstring[0] == '+' || genstring[0] == '-') {
        // leading +/- sign so make newgen relative to oldgen
        bigint relgen = newgen;
        newgen = oldgen;
        newgen += relgen;
        if (newgen < bigint::zero) newgen = bigint::zero;
    }
    
    // set stop_after_script BEFORE testing newgen == oldgen so scripts
    // can call setgen("+0") to prevent further generating
    //!!! if (inscript) stop_after_script = true;
    
    if (newgen == oldgen) return NULL;
    
    if (!inundoredo && allowundo && !currlayer->stayclean && inscript) {
        // script called setgen()
        //!!! SavePendingChanges();
    }
    
    // need IsParityShifted() method???
    if (currlayer->algtype == QLIFE_ALGO && newgen.odd() != oldgen.odd()) {
        // qlife stores pattern in different bits depending on gen parity,
        // so we need to create a new qlife universe, set its gen, copy the
        // current pattern to the new universe, then switch to that universe
        bigint top, left, bottom, right;
        currlayer->algo->findedges(&top, &left, &bottom, &right);
        if ( OutsideLimits(top, left, bottom, right) ) {
            return "Pattern is too big to copy.";
        }
        // create a new universe of same type and same rule
        lifealgo* newalgo = CreateNewUniverse(currlayer->algtype);
        const char* err = newalgo->setrule(currlayer->algo->getrule());
        if (err) {
            delete newalgo;
            return "Current rule is no longer valid!";
        }
        newalgo->setGeneration(newgen);
        // copy pattern
        if ( !CopyRect(top.toint(), left.toint(), bottom.toint(), right.toint(),
                       currlayer->algo, newalgo, false, "Copying pattern") ) {
            delete newalgo;
            return "Failed to copy pattern.";
        }
        // switch to new universe
        delete currlayer->algo;
        currlayer->algo = newalgo;
        SetGenIncrement();
    } else {
        currlayer->algo->setGeneration(newgen);
    }
    
    if (!inundoredo) {
        // save some settings for RememberSetGen below
        bigint oldstartgen = currlayer->startgen;
        bool oldsave = currlayer->savestart;
        
        // may need to change startgen and savestart
        if (oldgen == currlayer->startgen || newgen <= currlayer->startgen) {
            currlayer->startgen = newgen;
            currlayer->savestart = true;
        }
        
        if (allowundo && !currlayer->stayclean) {
            currlayer->undoredo->RememberSetGen(oldgen, newgen, oldstartgen, oldsave);
        }
    }
    
    UpdateStatus();
    return NULL;
}

// -----------------------------------------------------------------------------

static void JoinTwistedEdges(lifealgo* curralgo)
{
    // set grid edges
    int gl = curralgo->gridleft.toint();
    int gt = curralgo->gridtop.toint();
    int gr = curralgo->gridright.toint();
    int gb = curralgo->gridbottom.toint();
    
    // border edges are 1 cell outside grid edges
    int bl = gl - 1;
    int bt = gt - 1;
    int br = gr + 1;
    int bb = gb + 1;
    
    if (curralgo->htwist && curralgo->vtwist) {
        // cross-surface
        //  eg. :C4,3
        //  a l k j i d
        //  l A B C D i
        //  h E F G H e
        //  d I J K L a
        //  i d c b a l
        
        for (int x = gl; x <= gr; x++) {
            int twistedx = gr - x + gl;
            int state = curralgo->getcell(twistedx, gt);
            if (state > 0) curralgo->setcell(x, bb, state);
            state = curralgo->getcell(twistedx, gb);
            if (state > 0) curralgo->setcell(x, bt, state);
        }
        
        for (int y = gt; y <= gb; y++) {
            int twistedy = gb - y + gt;
            int state = curralgo->getcell(gl, twistedy);
            if (state > 0) curralgo->setcell(br, y, state);
            state = curralgo->getcell(gr, twistedy);
            if (state > 0) curralgo->setcell(bl, y, state);
        }
        
        // copy grid's corner cells to SAME corners in border
        // (these cells are topologically different to non-corner cells)
        curralgo->setcell(bl, bt, curralgo->getcell(gl, gt));
        curralgo->setcell(br, bt, curralgo->getcell(gr, gt));
        curralgo->setcell(br, bb, curralgo->getcell(gr, gb));
        curralgo->setcell(bl, bb, curralgo->getcell(gl, gb));
        
    } else if (curralgo->htwist) {
        // Klein bottle with top and bottom edges twisted 180 degrees
        //  eg. :K4*,3
        //  i l k j i l
        //  d A B C D a
        //  h E F G H e
        //  l I J K L i
        //  a d c b a d
        
        for (int x = gl; x <= gr; x++) {
            int twistedx = gr - x + gl;
            int state = curralgo->getcell(twistedx, gt);
            if (state > 0) curralgo->setcell(x, bb, state);
            state = curralgo->getcell(twistedx, gb);
            if (state > 0) curralgo->setcell(x, bt, state);
        }
        
        for (int y = gt; y <= gb; y++) {
            // join left and right edges with no twist
            int state = curralgo->getcell(gl, y);
            if (state > 0) curralgo->setcell(br, y, state);
            state = curralgo->getcell(gr, y);
            if (state > 0) curralgo->setcell(bl, y, state);
        }
        
        // do corner cells
        curralgo->setcell(bl, bt, curralgo->getcell(gl, gb));
        curralgo->setcell(br, bt, curralgo->getcell(gr, gb));
        curralgo->setcell(bl, bb, curralgo->getcell(gl, gt));
        curralgo->setcell(br, bb, curralgo->getcell(gr, gt));
        
    } else { // curralgo->vtwist
        // Klein bottle with left and right edges twisted 180 degrees
        //  eg. :K4,3*
        //  d i j k l a
        //  l A B C D i
        //  h E F G H e
        //  d I J K L a
        //  l a b c d i
        
        for (int x = gl; x <= gr; x++) {
            // join top and bottom edges with no twist
            int state = curralgo->getcell(x, gt);
            if (state > 0) curralgo->setcell(x, bb, state);
            state = curralgo->getcell(x, gb);
            if (state > 0) curralgo->setcell(x, bt, state);
        }
        
        for (int y = gt; y <= gb; y++) {
            int twistedy = gb - y + gt;
            int state = curralgo->getcell(gl, twistedy);
            if (state > 0) curralgo->setcell(br, y, state);
            state = curralgo->getcell(gr, twistedy);
            if (state > 0) curralgo->setcell(bl, y, state);
        }
        
        // do corner cells
        curralgo->setcell(bl, bt, curralgo->getcell(gr, gt));
        curralgo->setcell(br, bt, curralgo->getcell(gl, gt));
        curralgo->setcell(bl, bb, curralgo->getcell(gr, gb));
        curralgo->setcell(br, bb, curralgo->getcell(gl, gb));
    }
}

// -----------------------------------------------------------------------------

static void JoinTwistedAndShiftedEdges(lifealgo* curralgo)
{
    // set grid edges
    int gl = curralgo->gridleft.toint();
    int gt = curralgo->gridtop.toint();
    int gr = curralgo->gridright.toint();
    int gb = curralgo->gridbottom.toint();
    
    // border edges are 1 cell outside grid edges
    int bl = gl - 1;
    int bt = gt - 1;
    int br = gr + 1;
    int bb = gb + 1;
    
    if (curralgo->hshift != 0) {
        // Klein bottle with shift by 1 on twisted horizontal edge (with even number of cells)
        //  eg. :K4*+1,3
        //  j i l k j i
        //  d A B C D a
        //  h E F G H e
        //  l I J K L i
        //  b a d c b a
        
        int state, twistedx, shiftedx;
        
        for (int x = gl; x <= gr; x++) {
            // join top and bottom edges with a twist and then shift by 1
            twistedx = gr - x + gl;
            shiftedx = twistedx - 1; if (shiftedx < gl) shiftedx = gr;
            state = curralgo->getcell(shiftedx, gb);
            if (state > 0) curralgo->setcell(x, bt, state);
            
            state = curralgo->getcell(shiftedx, gt);
            if (state > 0) curralgo->setcell(x, bb, state);
        }
        
        for (int y = gt; y <= gb; y++) {
            // join left and right edges with no twist or shift
            state = curralgo->getcell(gl, y);
            if (state > 0) curralgo->setcell(br, y, state);
            state = curralgo->getcell(gr, y);
            if (state > 0) curralgo->setcell(bl, y, state);
        }
        
        // do corner cells
        shiftedx = gl - 1; if (shiftedx < gl) shiftedx = gr;
        curralgo->setcell(bl, bt, curralgo->getcell(shiftedx, gb));
        curralgo->setcell(bl, bb, curralgo->getcell(shiftedx, gt));
        shiftedx = gr - 1; if (shiftedx < gl) shiftedx = gr;
        curralgo->setcell(br, bt, curralgo->getcell(shiftedx, gb));
        curralgo->setcell(br, bb, curralgo->getcell(shiftedx, gt));
        
    } else { // curralgo->vshift != 0
        // Klein bottle with shift by 1 on twisted vertical edge (with even number of cells)
        //  eg. :K3,4*+1
        //  f j k l d
        //  c A B C a
        //  l D E F j
        //  i G H I g
        //  f J K L d
        //  c a b c a
        
        int state, twistedy, shiftedy;
        
        for (int x = gl; x <= gr; x++) {
            // join top and bottom edges with no twist or shift
            state = curralgo->getcell(x, gt);
            if (state > 0) curralgo->setcell(x, bb, state);
            state = curralgo->getcell(x, gb);
            if (state > 0) curralgo->setcell(x, bt, state);
        }
        
        for (int y = gt; y <= gb; y++) {
            // join left and right edges with a twist and then shift by 1
            twistedy = gb - y + gt;
            shiftedy = twistedy - 1; if (shiftedy < gt) shiftedy = gb;
            state = curralgo->getcell(gr, shiftedy);
            if (state > 0) curralgo->setcell(bl, y, state);
            
            state = curralgo->getcell(gl, shiftedy);
            if (state > 0) curralgo->setcell(br, y, state);
        }
        
        // do corner cells
        shiftedy = gt - 1; if (shiftedy < gt) shiftedy = gb;
        curralgo->setcell(bl, bt, curralgo->getcell(gr, shiftedy));
        curralgo->setcell(br, bt, curralgo->getcell(gl, shiftedy));
        shiftedy = gb - 1; if (shiftedy < gt) shiftedy = gb;
        curralgo->setcell(bl, bb, curralgo->getcell(gr, shiftedy));
        curralgo->setcell(br, bb, curralgo->getcell(gl, shiftedy));
    }
}

// -----------------------------------------------------------------------------

static void JoinShiftedEdges(lifealgo* curralgo,
                             int gwd, int ght,        // grid wd and ht
                             int hshift, int vshift)  // horizontal and vertical shifts
{
    // set grid edges
    int gl = curralgo->gridleft.toint();
    int gt = curralgo->gridtop.toint();
    int gr = curralgo->gridright.toint();
    int gb = curralgo->gridbottom.toint();
    
    // border edges are 1 cell outside grid edges
    int bl = gl - 1;
    int bt = gt - 1;
    int br = gr + 1;
    int bb = gb + 1;
    
    if (hshift != 0) {
        // torus with horizontal shift
        //  eg. :T4+1,3
        //  k l i j k l
        //  d A B C D a
        //  h E F G H e
        //  l I J K L i
        //  a b c d a b
        
        int state, shiftedx;
        
        for (int x = gl; x <= gr; x++) {
            // join top and bottom edges with a horizontal shift
            shiftedx = x - hshift;
            if (shiftedx < gl) shiftedx += gwd; else if (shiftedx > gr) shiftedx -= gwd;
            state = curralgo->getcell(shiftedx, gb);
            if (state > 0) curralgo->setcell(x, bt, state);
            
            shiftedx = x + hshift;
            if (shiftedx < gl) shiftedx += gwd; else if (shiftedx > gr) shiftedx -= gwd;
            state = curralgo->getcell(shiftedx, gt);
            if (state > 0) curralgo->setcell(x, bb, state);
        }
        
        for (int y = gt; y <= gb; y++) {
            // join left and right edges with no shift
            state = curralgo->getcell(gl, y);
            if (state > 0) curralgo->setcell(br, y, state);
            
            state = curralgo->getcell(gr, y);
            if (state > 0) curralgo->setcell(bl, y, state);
        }
        
        // do corner cells
        shiftedx = gr - hshift;
        if (shiftedx < gl) shiftedx += gwd; else if (shiftedx > gr) shiftedx -= gwd;
        curralgo->setcell(bl, bt, curralgo->getcell(shiftedx, gb));
        shiftedx = gl - hshift;
        if (shiftedx < gl) shiftedx += gwd; else if (shiftedx > gr) shiftedx -= gwd;
        curralgo->setcell(br, bt, curralgo->getcell(shiftedx, gb));
        shiftedx = gr + hshift;
        if (shiftedx < gl) shiftedx += gwd; else if (shiftedx > gr) shiftedx -= gwd;
        curralgo->setcell(bl, bb, curralgo->getcell(shiftedx, gt));
        shiftedx = gl + hshift;
        if (shiftedx < gl) shiftedx += gwd; else if (shiftedx > gr) shiftedx -= gwd;
        curralgo->setcell(br, bb, curralgo->getcell(shiftedx, gt));
        
    } else { // vshift != 0
        // torus with vertical shift
        //  eg. :T4,3+1
        //  h i j k l a
        //  l A B C D e
        //  d E F G H i
        //  h I J K L a
        //  l a b c d e
        
        int state, shiftedy;
        
        for (int x = gl; x <= gr; x++) {
            // join top and bottom edges with no shift
            state = curralgo->getcell(x, gt);
            if (state > 0) curralgo->setcell(x, bb, state);
            
            state = curralgo->getcell(x, gb);
            if (state > 0) curralgo->setcell(x, bt, state);
        }
        
        for (int y = gt; y <= gb; y++) {
            // join left and right edges with a vertical shift
            shiftedy = y - vshift;
            if (shiftedy < gt) shiftedy += ght; else if (shiftedy > gb) shiftedy -= ght;
            state = curralgo->getcell(gr, shiftedy);
            if (state > 0) curralgo->setcell(bl, y, state);
            
            shiftedy = y + vshift;
            if (shiftedy < gt) shiftedy += ght; else if (shiftedy > gb) shiftedy -= ght;
            state = curralgo->getcell(gl, shiftedy);
            if (state > 0) curralgo->setcell(br, y, state);
        }
        
        // do corner cells
        shiftedy = gb - vshift;
        if (shiftedy < gt) shiftedy += ght; else if (shiftedy > gb) shiftedy -= ght;
        curralgo->setcell(bl, bt, curralgo->getcell(gr, shiftedy));
        shiftedy = gb + vshift;
        if (shiftedy < gt) shiftedy += ght; else if (shiftedy > gb) shiftedy -= ght;
        curralgo->setcell(br, bt, curralgo->getcell(gl, shiftedy));
        shiftedy = gt - vshift;
        if (shiftedy < gt) shiftedy += ght; else if (shiftedy > gb) shiftedy -= ght;
        curralgo->setcell(bl, bb, curralgo->getcell(gr, shiftedy));
        shiftedy = gt + vshift;
        if (shiftedy < gt) shiftedy += ght; else if (shiftedy > gb) shiftedy -= ght;
        curralgo->setcell(br, bb, curralgo->getcell(gl, shiftedy));
    }
}

// -----------------------------------------------------------------------------

static void JoinAdjacentEdges(lifealgo* curralgo,
                              int pt, int pl, int pb, int pr)    // pattern edges
{
    // set grid edges
    int gl = curralgo->gridleft.toint();
    int gt = curralgo->gridtop.toint();
    int gr = curralgo->gridright.toint();
    int gb = curralgo->gridbottom.toint();
    
    // border edges are 1 cell outside grid edges
    int bl = gl - 1;
    int bt = gt - 1;
    int br = gr + 1;
    int bb = gb + 1;
    
    // sphere
    //  eg. :S3
    //  a a d g c
    //  a A B C g
    //  b D E F h
    //  c G H I i
    //  g c f i i
    
    // copy live cells in top edge to left border
    for (int x = pl; x <= pr; x++) {
        int state;
        int skip = curralgo->nextcell(x, gt, state);
        if (skip < 0) break;
        x += skip;
        if (state > 0) curralgo->setcell(bl, gt + (x - gl), state);
    }
    
    // copy live cells in left edge to top border
    for (int y = pt; y <= pb; y++) {
        // no point using nextcell() here -- edge is only 1 cell wide
        int state = curralgo->getcell(gl, y);
        if (state > 0) curralgo->setcell(gl + (y - gt), bt, state);
    }
    
    // copy live cells in bottom edge to right border
    for (int x = pl; x <= pr; x++) {
        int state;
        int skip = curralgo->nextcell(x, gb, state);
        if (skip < 0) break;
        x += skip;
        if (state > 0) curralgo->setcell(br, gt + (x - gl), state);
    }
    
    // copy live cells in right edge to bottom border
    for (int y = pt; y <= pb; y++) {
        // no point using nextcell() here -- edge is only 1 cell wide
        int state = curralgo->getcell(gr, y);
        if (state > 0) curralgo->setcell(gl + (y - gt), bb, state);
    }
    
    // copy grid's corner cells to SAME corners in border
    curralgo->setcell(bl, bt, curralgo->getcell(gl, gt));
    curralgo->setcell(br, bt, curralgo->getcell(gr, gt));
    curralgo->setcell(br, bb, curralgo->getcell(gr, gb));
    curralgo->setcell(bl, bb, curralgo->getcell(gl, gb));
}

// -----------------------------------------------------------------------------

static void JoinEdges(lifealgo* curralgo,
                      int gwd, int ght,                  // grid wd and ht
                      int pt, int pl, int pb, int pr)    // pattern edges
{
    // set grid edges
    int gl = curralgo->gridleft.toint();
    int gt = curralgo->gridtop.toint();
    int gr = curralgo->gridright.toint();
    int gb = curralgo->gridbottom.toint();
    
    // border edges are 1 cell outside grid edges
    int bl = gl - 1;
    int bt = gt - 1;
    int br = gr + 1;
    int bb = gb + 1;
    
    if (ght > 0) {
        // copy live cells in top edge to bottom border
        for (int x = pl; x <= pr; x++) {
            int state;
            int skip = curralgo->nextcell(x, gt, state);
            if (skip < 0) break;
            x += skip;
            if (state > 0) curralgo->setcell(x, bb, state);
        }
        // copy live cells in bottom edge to top border
        for (int x = pl; x <= pr; x++) {
            int state;
            int skip = curralgo->nextcell(x, gb, state);
            if (skip < 0) break;
            x += skip;
            if (state > 0) curralgo->setcell(x, bt, state);
        }
    }
    
    if (gwd > 0) {
        // copy live cells in left edge to right border
        for (int y = pt; y <= pb; y++) {
            // no point using nextcell() here -- edge is only 1 cell wide
            int state = curralgo->getcell(gl, y);
            if (state > 0) curralgo->setcell(br, y, state);
        }
        // copy live cells in right edge to left border
        for (int y = pt; y <= pb; y++) {
            // no point using nextcell() here -- edge is only 1 cell wide
            int state = curralgo->getcell(gr, y);
            if (state > 0) curralgo->setcell(bl, y, state);
        }
    }
    
    if (gwd > 0 && ght > 0) {
        // copy grid's corner cells to opposite corners in border
        curralgo->setcell(bl, bt, curralgo->getcell(gr, gb));
        curralgo->setcell(br, bt, curralgo->getcell(gl, gb));
        curralgo->setcell(br, bb, curralgo->getcell(gl, gt));
        curralgo->setcell(bl, bb, curralgo->getcell(gr, gt));
    }
}

// -----------------------------------------------------------------------------

bool CreateBorderCells(lifealgo* curralgo)
{
    // no need to do anything if there is no pattern or if the grid is a bounded plane
    if (curralgo->isEmpty() || curralgo->boundedplane) return true;
    
    int gwd = curralgo->gridwd;
    int ght = curralgo->gridht;
    
    bigint top, left, bottom, right;
    curralgo->findedges(&top, &left, &bottom, &right);
    
    // no need to do anything if pattern is completely inside grid edges
    if ( (gwd == 0 || (curralgo->gridleft < left && curralgo->gridright > right)) &&
        (ght == 0 || (curralgo->gridtop < top && curralgo->gridbottom > bottom)) ) {
        return true;
    }
    
    // if grid has infinite width or height then pattern might be too big to use setcell/getcell
    if ( (gwd == 0 || ght == 0) && OutsideLimits(top, left, bottom, right) ) {
        ErrorMessage("Pattern is too big!");
        // return false so caller can exit step() loop
        return false;
    }
    
    if (curralgo->sphere) {
        // to get a sphere we join top edge with left edge, and right edge with bottom edge;
        // note that grid must be square (gwd == ght)
        int pl = left.toint();
        int pt = top.toint();
        int pr = right.toint();      
        int pb = bottom.toint();
        JoinAdjacentEdges(curralgo, pt, pl, pb, pr);
        
    } else if (curralgo->htwist || curralgo->vtwist) {
        // Klein bottle or cross-surface
        if ( (curralgo->htwist && curralgo->hshift != 0 && (gwd & 1) == 0) ||
            (curralgo->vtwist && curralgo->vshift != 0 && (ght & 1) == 0) ) {
            // Klein bottle with shift is only possible if the shift is on the
            // twisted edge and that edge has an even number of cells
            JoinTwistedAndShiftedEdges(curralgo);
        } else {
            JoinTwistedEdges(curralgo);
        }
        
    } else if (curralgo->hshift != 0 || curralgo->vshift != 0) {
        // torus with horizontal or vertical shift
        JoinShiftedEdges(curralgo, gwd, ght, curralgo->hshift, curralgo->vshift);
        
    } else {
        // unshifted torus or infinite tube
        int pl = left.toint();
        int pt = top.toint();
        int pr = right.toint();      
        int pb = bottom.toint();
        JoinEdges(curralgo, gwd, ght, pt, pl, pb, pr);
    }
    
    curralgo->endofpattern();
    return true;
}

// -----------------------------------------------------------------------------

void ClearRect(lifealgo* curralgo, int top, int left, int bottom, int right)
{
    int cx, cy, v;
    for ( cy = top; cy <= bottom; cy++ ) {
        for ( cx = left; cx <= right; cx++ ) {
            int skip = curralgo->nextcell(cx, cy, v);
            if (skip + cx > right)
                skip = -1;           // pretend we found no more live cells
            if (skip >= 0) {
                // found next live cell so delete it
                cx += skip;
                curralgo->setcell(cx, cy, 0);
            } else {
                cx = right + 1;     // done this row
            }
        }
    }
}

// -----------------------------------------------------------------------------

bool DeleteBorderCells(lifealgo* curralgo)
{
    // no need to do anything if there is no pattern
    if (curralgo->isEmpty()) return true;
    
    int gwd = curralgo->gridwd;
    int ght = curralgo->gridht;
    
    // need to find pattern edges because pattern may have expanded beyond grid
    // (typically by 2 cells, but could be more if rule allows births in empty space)
    bigint top, left, bottom, right;
    curralgo->findedges(&top, &left, &bottom, &right);
    
    // no need to do anything if grid encloses entire pattern
    if ( (gwd == 0 || (curralgo->gridleft <= left && curralgo->gridright >= right)) &&
        (ght == 0 || (curralgo->gridtop <= top && curralgo->gridbottom >= bottom)) ) {
        return true;
    }
    
    if ( OutsideLimits(top, left, bottom, right) ) {
        ErrorMessage("Pattern is too big!");
        // return false so caller can exit step() loop
        return false;
    }
    
    // set pattern edges
    int pl = left.toint();
    int pt = top.toint();
    int pr = right.toint();      
    int pb = bottom.toint();
    
    // set grid edges
    int gl = curralgo->gridleft.toint();
    int gt = curralgo->gridtop.toint();
    int gr = curralgo->gridright.toint();
    int gb = curralgo->gridbottom.toint();
    
    if (ght > 0 && pt < gt) {
        // delete live cells above grid
        ClearRect(curralgo, pt, pl, gt-1, pr);
        pt = gt; // reduce size of rect below
    }
    
    if (ght > 0 && pb > gb) {
        // delete live cells below grid
        ClearRect(curralgo, gb+1, pl, pb, pr);
        pb = gb; // reduce size of rect below
    }
    
    if (gwd > 0 && pl < gl) {
        // delete live cells left of grid
        ClearRect(curralgo, pt, pl, pb, gl-1);
    }
    
    if (gwd > 0 && pr > gr) {
        // delete live cells right of grid
        ClearRect(curralgo, pt, gr+1, pb, pr);
    }
    
    curralgo->endofpattern();
    return true;
}

// -----------------------------------------------------------------------------

void DisplayTimingInfo()
{
    endtime = TimeInSeconds();
    if (endtime <= begintime) endtime = begintime + 0.000001;
    endgen = currlayer->algo->getGeneration().todouble();
    
    double secs = endtime - begintime;
    double gens = endgen - begingen;
    char s[128];
    sprintf(s, "%g gens in %g secs (%g gens/sec).", gens, secs, gens/secs);
    DisplayMessage(s);
}

// -----------------------------------------------------------------------------

bool StartGenerating()
{
    if (generating) Warning("Bug detected in StartGenerating!");
    
    lifealgo* curralgo = currlayer->algo;
    if (curralgo->isEmpty()) {
        ErrorMessage(empty_pattern);
        return false;
    }
    
    if (!SaveStartingPattern()) {
        return false;
    }
    
    if (allowundo) currlayer->undoredo->RememberGenStart();
    
    // only show hashing info while generating
    lifealgo::setVerbose(currlayer->showhashinfo);
    
    // for DisplayTimingInfo
    begintime = TimeInSeconds();
    begingen = curralgo->getGeneration().todouble();
    
    generating = true;
    
    PollerReset();
    
    // caller will start a repeating timer
    return true;
}

// -----------------------------------------------------------------------------

void StopGenerating()
{
    if (!generating) Warning("Bug detected in StopGenerating!");
    
    generating = false;
    
    PollerInterrupt();
    
    if (showtiming) DisplayTimingInfo();
    lifealgo::setVerbose(0);
    
    if (event_checker > 0) {
        // we're currently in the event poller somewhere inside step(), so we must let
        // step() complete and only call RememberGenFinish at the end of NextGeneration
    } else {
        if (allowundo) currlayer->undoredo->RememberGenFinish();
    }
    
    // caller will stop the timer
}

// -----------------------------------------------------------------------------

void NextGeneration(bool useinc)
{
    lifealgo* curralgo = currlayer->algo;
    bool boundedgrid = (curralgo->gridwd > 0 || curralgo->gridht > 0);
    
    if (generating) {
        // we were called via timer so StartGenerating has already checked
        // if the pattern is empty, etc (note that useinc is also true)

    } else {
        // we were called via Next/Step button
        if (curralgo->isEmpty()) {
            ErrorMessage(empty_pattern);
            return;
        }

        if (!SaveStartingPattern()) {
            return;
        }
        
        if (allowundo) currlayer->undoredo->RememberGenStart();
        
        // only show hashing info while generating
        lifealgo::setVerbose(currlayer->showhashinfo);
        
        if (useinc && curralgo->getIncrement() > bigint::one) {
            // for DisplayTimingInfo
            begintime = TimeInSeconds();
            begingen = curralgo->getGeneration().todouble();
        }
        
        PollerReset();
    }
    
    if (useinc) {
        // step by current increment
        if (boundedgrid) {
            // temporarily set the increment to 1 so we can call CreateBorderCells()
            // and DeleteBorderCells() around each step()
            int savebase = currlayer->currbase;
            int saveexpo = currlayer->currexpo;
            bigint inc = curralgo->getIncrement();
            curralgo->setIncrement(1);
            while (inc > 0) {
                if (Poller()->checkevents()) break;
                if (savebase != currlayer->currbase || saveexpo != currlayer->currexpo) {
                    // user changed step base/exponent, so reset increment to 1
                    inc = curralgo->getIncrement();
                    curralgo->setIncrement(1);
                }
                if (!CreateBorderCells(curralgo)) break;
                curralgo->step();
                if (!DeleteBorderCells(curralgo)) break;
                inc -= 1;
            }
            // safe way to restore correct increment in case user altered base/expo in above loop
            SetGenIncrement();
        } else {
            curralgo->step();
        }
    } else {
        // step by 1 gen
        bigint saveinc = curralgo->getIncrement();
        curralgo->setIncrement(1);
        if (boundedgrid) CreateBorderCells(curralgo);
        curralgo->step();
        if (boundedgrid) DeleteBorderCells(curralgo);
        curralgo->setIncrement(saveinc);
    }
    
    if (!generating) {
        if (showtiming && useinc && curralgo->getIncrement() > bigint::one) DisplayTimingInfo();
        lifealgo::setVerbose(0);
        if (allowundo) currlayer->undoredo->RememberGenFinish();
    }
    
    // autofit is only used when doing many gens
    if (currlayer->autofit && (generating || useinc)) {
        //!!! FitInView(0);
    }

    if (draw_pending) {
        draw_pending = false;
        TouchBegan(pendingx, pendingy);
    }
}

// -----------------------------------------------------------------------------

void ClearOutsideGrid()
{
    // check current pattern and clear any live cells outside bounded grid
    bool patternchanged = false;
    bool savechanges = allowundo && !currlayer->stayclean;
    
    // might also need to truncate selection
    currlayer->currsel.CheckGridEdges();
    
    if (currlayer->algo->isEmpty()) return;
    
    // check if current pattern is too big to use nextcell/setcell
    bigint top, left, bottom, right;
    currlayer->algo->findedges(&top, &left, &bottom, &right);
    if ( OutsideLimits(top, left, bottom, right) ) {
        ErrorMessage("Pattern too big to check (outside +/- 10^9 boundary).");
        return;
    }
    
    int itop = top.toint();
    int ileft = left.toint();
    int ibottom = bottom.toint();
    int iright = right.toint();
    
    // no need to do anything if pattern is entirely within grid
    int gtop = currlayer->algo->gridtop.toint();
    int gleft = currlayer->algo->gridleft.toint();
    int gbottom = currlayer->algo->gridbottom.toint();
    int gright = currlayer->algo->gridright.toint();
    if (currlayer->algo->gridwd == 0) {
        // grid has infinite width
        gleft = INT_MIN;
        gright = INT_MAX;
    }
    if (currlayer->algo->gridht == 0) {
        // grid has infinite height
        gtop = INT_MIN;
        gbottom = INT_MAX;
    }
    if (itop >= gtop && ileft >= gleft && ibottom <= gbottom && iright <= gright) {
        return;
    }
    
    int ht = ibottom - itop + 1;
    int cx, cy;
    
    // for showing accurate progress we need to add pattern height to pop count
    // in case this is a huge pattern with many blank rows
    double maxcount = currlayer->algo->getPopulation().todouble() + ht;
    double accumcount = 0;
    int currcount = 0;
    bool abort = false;
    int v = 0;
    BeginProgress("Checking cells outside grid");
    
    lifealgo* curralgo = currlayer->algo;
    for ( cy=itop; cy<=ibottom; cy++ ) {
        currcount++;
        for ( cx=ileft; cx<=iright; cx++ ) {
            int skip = curralgo->nextcell(cx, cy, v);
            if (skip >= 0) {
                // found next live cell in this row
                cx += skip;
                if (cx < gleft || cx > gright || cy < gtop || cy > gbottom) {
                    // clear cell outside grid
                    if (savechanges) currlayer->undoredo->SaveCellChange(cx, cy, v, 0);
                    curralgo->setcell(cx, cy, 0);
                    patternchanged = true;
                }
                currcount++;
            } else {
                cx = iright;  // done this row
            }
            if (currcount > 1024) {
                accumcount += currcount;
                currcount = 0;
                abort = AbortProgress(accumcount / maxcount, "");
                if (abort) break;
            }
        }
        if (abort) break;
    }
    
    curralgo->endofpattern();
    EndProgress();
    
    if (patternchanged) {
        ErrorMessage("Pattern was truncated (live cells were outside grid).");
    }
}

// -----------------------------------------------------------------------------

void ReduceCellStates(int newmaxstate)
{
    // check current pattern and reduce any cell states > newmaxstate
    bool patternchanged = false;
    bool savechanges = allowundo && !currlayer->stayclean;
    
    // check if current pattern is too big to use nextcell/setcell
    bigint top, left, bottom, right;
    currlayer->algo->findedges(&top, &left, &bottom, &right);
    if ( OutsideLimits(top, left, bottom, right) ) {
        ErrorMessage("Pattern too big to check (outside +/- 10^9 boundary).");
        return;
    }
    
    int itop = top.toint();
    int ileft = left.toint();
    int ibottom = bottom.toint();
    int iright = right.toint();
    int ht = ibottom - itop + 1;
    int cx, cy;
    
    // for showing accurate progress we need to add pattern height to pop count
    // in case this is a huge pattern with many blank rows
    double maxcount = currlayer->algo->getPopulation().todouble() + ht;
    double accumcount = 0;
    int currcount = 0;
    bool abort = false;
    int v = 0;
    BeginProgress("Checking cell states");
    
    lifealgo* curralgo = currlayer->algo;
    for ( cy=itop; cy<=ibottom; cy++ ) {
        currcount++;
        for ( cx=ileft; cx<=iright; cx++ ) {
            int skip = curralgo->nextcell(cx, cy, v);
            if (skip >= 0) {
                // found next live cell in this row
                cx += skip;
                if (v > newmaxstate) {
                    // reduce cell's current state to largest state
                    if (savechanges) currlayer->undoredo->SaveCellChange(cx, cy, v, newmaxstate);
                    curralgo->setcell(cx, cy, newmaxstate);
                    patternchanged = true;
                }
                currcount++;
            } else {
                cx = iright;  // done this row
            }
            if (currcount > 1024) {
                accumcount += currcount;
                currcount = 0;
                abort = AbortProgress(accumcount / maxcount, "");
                if (abort) break;
            }
        }
        if (abort) break;
    }
    
    curralgo->endofpattern();
    EndProgress();
    
    if (patternchanged) {
        ErrorMessage("Pattern has changed (new rule has fewer states).");
    }
}

// -----------------------------------------------------------------------------

void ChangeRule(const std::string& rulestring)
{
    // load recently installed .rule/table/tree/colors/icons file
    std::string oldrule = currlayer->algo->getrule();
    int oldmaxstate = currlayer->algo->NumCellStates() - 1;
    
    // selection might change if grid becomes smaller,
    // so save current selection for RememberRuleChange/RememberAlgoChange
    SaveCurrentSelection();
    
    const char* err = currlayer->algo->setrule( rulestring.c_str() );
    if (err) {
        // try to find another algorithm that supports the given rule
        for (int i = 0; i < NumAlgos(); i++) {
            if (i != currlayer->algtype) {
                lifealgo* tempalgo = CreateNewUniverse(i);
                err = tempalgo->setrule( rulestring.c_str() );
                delete tempalgo;
                if (!err) {
                    // change the current algorithm and switch to the new rule
                    ChangeAlgorithm(i, rulestring.c_str());
                    if (i != currlayer->algtype) {
                        RestoreRule(oldrule.c_str());
                        Warning("Algorithm could not be changed (pattern is too big to convert).");
                        return;
                    } else {
                        UpdateEverything();
                        return;
                    }
                }
            }
        }
        // should only get here if .rule/table/tree file contains some sort of error
        RestoreRule(oldrule.c_str());
        Warning("New rule is not valid in any algorithm!");
        return;
    }
    
    std::string newrule = currlayer->algo->getrule();
    if (oldrule != newrule) {
        UpdateStatus();
        
        // if grid is bounded then remove any live cells outside grid edges
        if (currlayer->algo->gridwd > 0 || currlayer->algo->gridht > 0) {
            ClearOutsideGrid();
        }
    }
    
    // new table/tree might have changed the number of cell states;
    // if there are fewer states then pattern might change
    int newmaxstate = currlayer->algo->NumCellStates() - 1;
    if (newmaxstate < oldmaxstate && !currlayer->algo->isEmpty()) {
        ReduceCellStates(newmaxstate);
    }
    
    // set colors/icons for new rule
    UpdateLayerColors();
    
    // pattern might have changed or colors/icons might have changed
    UpdateEverything();
    
    if (oldrule != newrule) {
        if (allowundo && !currlayer->stayclean) {
            currlayer->undoredo->RememberRuleChange(oldrule.c_str());
        }
    }
}

// -----------------------------------------------------------------------------

void ChangeAlgorithm(algo_type newalgotype, const char* newrule, bool inundoredo)
{
    if (newalgotype == currlayer->algtype) return;
    
    // check if current pattern is too big to use nextcell/setcell
    bigint top, left, bottom, right;
    if ( !currlayer->algo->isEmpty() ) {
        currlayer->algo->findedges(&top, &left, &bottom, &right);
        if ( OutsideLimits(top, left, bottom, right) ) {
            ErrorMessage("Pattern cannot be converted (outside +/- 10^9 boundary).");
            return;
        }
    }
    
    // save changes if undo/redo is enabled and script isn't constructing a pattern
    // and we're not undoing/redoing an earlier algo change
    bool savechanges = allowundo && !currlayer->stayclean && !inundoredo;
    if (savechanges && inscript) {
        // note that we must save pending gen changes BEFORE changing algo type
        // otherwise temporary files won't be the correct type (mc or rle)
        //!!! SavePendingChanges();
    }
    
    // selection might change if grid becomes smaller,
    // so save current selection for RememberAlgoChange
    if (savechanges) SaveCurrentSelection();
    
    bool rulechanged = false;
    std::string oldrule = currlayer->algo->getrule();
    
    // change algorithm type, reset step size, and update status bar
    algo_type oldalgo = currlayer->algtype;
    currlayer->algtype = newalgotype;
    currlayer->currbase = algoinfo[newalgotype]->defbase;
    currlayer->currexpo = 0;
    UpdateStatus();
    
    // create a new universe of the requested flavor
    lifealgo* newalgo = CreateNewUniverse(newalgotype);
    
    if (inundoredo) {
        // switch to given newrule
        const char* err = newalgo->setrule(newrule);
        if (err) newalgo->setrule(newalgo->DefaultRule());
    } else {
        const char* err;
        if (newrule[0] == 0) {
            // try to use same rule
            err = newalgo->setrule(currlayer->algo->getrule());
        } else {
            // switch to newrule
            err = newalgo->setrule(newrule);
            rulechanged = true;
        }
        if (err) {
            std::string defrule = newalgo->DefaultRule();
            size_t oldpos = oldrule.find(':');
            if (newrule[0] == 0 && oldpos != std::string::npos) {
                // switch to new algo's default rule, but preserve the topology in oldrule
                // so we can do things like switch from "LifeHistory:T30,20" in RuleLoader
                // to "B3/S23:T30,20" in QuickLife
                size_t defpos = defrule.find(':');
                if (defpos != std::string::npos) {
                    // default rule shouldn't have a suffix but play safe and remove it
                    defrule = defrule.substr(0, defpos);
                }
                defrule += ":";
                defrule += oldrule.substr(oldpos+1);
            }
            err = newalgo->setrule(defrule.c_str());
            // shouldn't ever fail but play safe
            if (err) newalgo->setrule( newalgo->DefaultRule() );
            rulechanged = true;
        }
    }
    
    // set same gen count
    newalgo->setGeneration( currlayer->algo->getGeneration() );
    
    bool patternchanged = false;
    if ( !currlayer->algo->isEmpty() ) {
        // copy pattern in current universe to new universe
        int itop = top.toint();
        int ileft = left.toint();
        int ibottom = bottom.toint();
        int iright = right.toint();
        int ht = ibottom - itop + 1;
        int cx, cy;
        
        // for showing accurate progress we need to add pattern height to pop count
        // in case this is a huge pattern with many blank rows
        double maxcount = currlayer->algo->getPopulation().todouble() + ht;
        double accumcount = 0;
        int currcount = 0;
        bool abort = false;
        int v = 0;
        BeginProgress("Converting pattern");
        
        // set newalgo's grid edges so we can save cells that are outside grid
        int gtop = newalgo->gridtop.toint();
        int gleft = newalgo->gridleft.toint();
        int gbottom = newalgo->gridbottom.toint();
        int gright = newalgo->gridright.toint();
        if (newalgo->gridwd == 0) {
            // grid has infinite width
            gleft = INT_MIN;
            gright = INT_MAX;
        }
        if (newalgo->gridht == 0) {
            // grid has infinite height
            gtop = INT_MIN;
            gbottom = INT_MAX;
        }
        
        // need to check for state change if new algo has fewer states than old algo
        int newmaxstate = newalgo->NumCellStates() - 1;
        
        lifealgo* curralgo = currlayer->algo;
        for ( cy=itop; cy<=ibottom; cy++ ) {
            currcount++;
            for ( cx=ileft; cx<=iright; cx++ ) {
                int skip = curralgo->nextcell(cx, cy, v);
                if (skip >= 0) {
                    // found next live cell in this row
                    cx += skip;
                    if (cx < gleft || cx > gright || cy < gtop || cy > gbottom) {
                        // cx,cy is outside grid
                        if (savechanges) currlayer->undoredo->SaveCellChange(cx, cy, v, 0);
                        // no need to clear cell from curralgo (that universe will soon be deleted)
                        patternchanged = true;
                    } else {
                        if (v > newmaxstate) {
                            // reduce v to largest state in new algo
                            if (savechanges) currlayer->undoredo->SaveCellChange(cx, cy, v, newmaxstate);
                            v = newmaxstate;
                            patternchanged = true;
                        }
                        newalgo->setcell(cx, cy, v);
                    }
                    currcount++;
                } else {
                    cx = iright;  // done this row
                }
                if (currcount > 1024) {
                    accumcount += currcount;
                    currcount = 0;
                    abort = AbortProgress(accumcount / maxcount, "");
                    if (abort) break;
                }
            }
            if (abort) break;
        }
        
        newalgo->endofpattern();
        EndProgress();
    }
    
    // delete old universe and point current universe to new universe
    delete currlayer->algo;
    currlayer->algo = newalgo;   
    SetGenIncrement();
    
    // if new grid is bounded then we might need to truncate the selection
    if (currlayer->algo->gridwd > 0 || currlayer->algo->gridht > 0) {
        currlayer->currsel.CheckGridEdges();
    }
    
    // switch to default colors for new algo+rule
    UpdateLayerColors();

    if (!inundoredo) {
        if (rulechanged) {
            // if pattern exists and is at starting gen then set savestart true
            // so that SaveStartingPattern will save pattern to suitable file
            // (and thus ResetPattern will work correctly)
            if ( currlayer->algo->getGeneration() == currlayer->startgen && !currlayer->algo->isEmpty() ) {
                currlayer->savestart = true;
            }
            
            if (newrule[0] == 0) {
                if (patternchanged) {
                    ErrorMessage("Rule has changed and pattern has changed.");
                } else {
                    // don't beep
                    DisplayMessage("Rule has changed.");
                }
            } else {
                if (patternchanged) {
                    ErrorMessage("Algorithm has changed and pattern has changed.");
                } else {
                    // don't beep
                    DisplayMessage("Algorithm has changed.");
                }
            }
        } else if (patternchanged) {
            ErrorMessage("Pattern has changed.");
        }
    }
    
    if (savechanges) {
        currlayer->undoredo->RememberAlgoChange(oldalgo, oldrule.c_str());
    }
    
    if (!inundoredo && !inscript) {
        // do this AFTER RememberAlgoChange so Undo button becomes enabled
        UpdateEverything();
    }
}

// -----------------------------------------------------------------------------

static std::string CreateTABLE(const std::string& tablepath)
{
    std::string contents = "\n@TABLE\n\n";
    // append contents of .table file
    FILE* f = fopen(tablepath.c_str(), "r");
    if (f) {
        const int MAXLINELEN = 4095;
        char linebuf[MAXLINELEN + 1];
        linereader reader(f);
        while (true) {
            if (reader.fgets(linebuf, MAXLINELEN) == 0) break;
            contents += linebuf;
            contents += "\n";
        }
        reader.close();
    } else {
        std::ostringstream oss;
        oss << "Could not read .table file:\n" << tablepath.c_str();
        throw std::runtime_error(oss.str().c_str());
    }
    return contents;
}

// -----------------------------------------------------------------------------

static std::string CreateTREE(const std::string& treepath)
{
    std::string contents = "\n@TREE\n\n";
    // append contents of .tree file
    FILE* f = fopen(treepath.c_str(), "r");
    if (f) {
        const int MAXLINELEN = 4095;
        char linebuf[MAXLINELEN + 1];
        linereader reader(f);
        while (true) {
            if (reader.fgets(linebuf, MAXLINELEN) == 0) break;
            contents += linebuf;
            contents += "\n";
        }
        reader.close();
    } else {
        std::ostringstream oss;
        oss << "Could not read .tree file:\n" << treepath.c_str();
        throw std::runtime_error(oss.str().c_str());
    }
    return contents;
}

// -----------------------------------------------------------------------------

static std::string CreateCOLORS(const std::string& colorspath)
{
    std::string contents = "\n@COLORS\n\n";
    FILE* f = fopen(colorspath.c_str(), "r");
    if (f) {
        const int MAXLINELEN = 4095;
        char linebuf[MAXLINELEN + 1];
        linereader reader(f);
        while (true) {
            if (reader.fgets(linebuf, MAXLINELEN) == 0) break;
            int skip = 0;
            if (strncmp(linebuf, "color", 5) == 0 ||
                strncmp(linebuf, "gradient", 8) == 0) {
                // strip off everything before 1st digit
                while (linebuf[skip] && (linebuf[skip] < '0' || linebuf[skip] > '9')) {
                    skip++;
                }
            }
            contents += linebuf + skip;
            contents += "\n";
        }
        reader.close();
    } else {
        std::ostringstream oss;
        oss << "Could not read .colors file:\n" << colorspath.c_str();
        throw std::runtime_error(oss.str().c_str());
    }
    return contents;
}

// -----------------------------------------------------------------------------

static unsigned char* GetRGBAPixels(CGImageRef image)
{
    int wd = CGImageGetWidth(image);
    int ht = CGImageGetHeight(image);
    int bytesPerPixel = 4;
    int bytesPerRow = bytesPerPixel * wd;
    int bitsPerComponent = 8;

    // allocate memory to store image's RGBA bitmap data
    unsigned char* pxldata = (unsigned char*) calloc(wd * ht * 4, 1);
    if (pxldata == NULL) return NULL;

    CGColorSpaceRef colorspace = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(pxldata, wd, ht,
        bitsPerComponent, bytesPerRow, colorspace,
        kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
    CGContextDrawImage(ctx, CGRectMake(0, 0, wd, ht), image);
    CGContextRelease(ctx);
    CGColorSpaceRelease(colorspace);

    // pxldata now contains the image bitmap in RGBA pixel format
    return pxldata;
}

// -----------------------------------------------------------------------------

static bool OneColor(CGImageRef image, unsigned char* R, unsigned char* G, unsigned char* B)
{
    unsigned char* pxldata = GetRGBAPixels(image);
    if (pxldata == NULL) return false;

    // pxldata contains the image bitmap in RGBA pixel format
    int color = (pxldata[0] << 16) + (pxldata[1] << 8) + pxldata[2];
    int byte = 0;
    int wd = CGImageGetWidth(image);
    int ht = CGImageGetHeight(image);
    for (int i = 0; i < wd*ht; i++) {
        if (color != (pxldata[byte] << 16) + (pxldata[byte+1] << 8) + pxldata[byte+2]) {
            // there is more than one color
            free(pxldata);
            return false;
        }
        byte += 4;
    }

    // return the single color
    *R = pxldata[0];
    *G = pxldata[1];
    *B = pxldata[2];
    
    free(pxldata);
    return true;
}

// -----------------------------------------------------------------------------

static std::string CreateStateColors(CGImageRef image, int numicons)
{
    std::string contents = "\n@COLORS\n\n";
    
    // if the last icon has only 1 color then assume it is the extra 15x15 icon
    // supplied to set the color of state 0
    char s[128];
    if (numicons > 1) {
        CGImageRef icon = CGImageCreateWithImageInRect(image,CGRectMake((numicons-1)*15, 0, 15, 15));
        unsigned char R, G, B;
        if (OneColor(icon, &R, &G, &B)) {
            sprintf(s, "0 %d %d %d\n", R, G, B);
            contents += s;
            numicons--;
        }
    }
    
    // set non-icon colors for each live state to the average of the non-black pixels
    // in each 15x15 icon (note we've skipped the extra icon detected above)
    for (int i = 0; i < numicons; i++) {
        CGImageRef icon = CGImageCreateWithImageInRect(image,CGRectMake(i*15, 0, 15, 15));
        int nbcount = 0;   // non-black pixels
        int totalR = 0;
        int totalG = 0;
        int totalB = 0;
        unsigned char* idata = GetRGBAPixels(icon);
        if (idata) {
            for (int y = 0; y < 15; y++) {
                for (int x = 0; x < 15; x++) {
                    int pos = (y * 15 + x) * 4;
                    unsigned char R = idata[pos];
                    unsigned char G = idata[pos+1];
                    unsigned char B = idata[pos+2];
                    if (R > 0 || G > 0 || B > 0) {
                        // non-black pixel
                        nbcount++;
                        totalR += R;
                        totalG += G;
                        totalB += B;
                    }
                }
            }
            free(idata);
        }
        if (nbcount > 0) {
            sprintf(s, "%d %d %d %d\n", i+1, totalR/nbcount, totalG/nbcount, totalB/nbcount);
            contents += s;
        } else {
            // unlikely, but avoid div by zero
            sprintf(s, "%d 0 0 0\n", i+1);
            contents += s;
        }
    }
    
    return contents;
}

// -----------------------------------------------------------------------------

static std::string hex2(int i)
{
    // convert given number from 0..255 into 2 hex digits
    std::string result = "xx";
    const char* hexdigit = "0123456789ABCDEF";
    result[0] = hexdigit[i / 16];
    result[1] = hexdigit[i % 16];
    return result;
}

// -----------------------------------------------------------------------------

static std::list<int> GetColors(CGImageRef image)
{
    // return the list of colors used in the given image
    std::list<int> result;
    unsigned char* pxldata = GetRGBAPixels(image);
    if (pxldata) {
        // pxldata contains the image bitmap in RGBA pixel format
        int byte = 0;
        int wd = CGImageGetWidth(image);
        int ht = CGImageGetHeight(image);
        for (int i = 0; i < wd*ht; i++) {
            int color = (pxldata[byte] << 16) + (pxldata[byte+1] << 8) + pxldata[byte+2];
            if (std::find(result.begin(),result.end(),color) == result.end()) {
                // first time we've seen this color
                result.push_back(color);
            }
            byte += 4;
        }
        free(pxldata);
    }
    return result;
}

// -----------------------------------------------------------------------------

static std::string CreateXPM(const std::string& iconspath, CGImageRef image, int size, int numicons)
{
    // create XPM data for given set of icons
    std::string contents = "\nXPM\n";
    
    char s[128];
    int charsperpixel = 1;
    const char* cindex = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    
    std::list<int> colors = GetColors(image);
    
    int numcolors = colors.size();
    if (numcolors > 256) {
        std::ostringstream oss;
        oss << "Image in " << iconspath.c_str() << " has more than 256 colors.";
        throw std::runtime_error(oss.str().c_str());
    }
    if (numcolors > 26) charsperpixel = 2;   // AABA..PA, ABBB..PB, ... , APBP..PP
    
    contents += "/* width height num_colors chars_per_pixel */\n";
    sprintf(s, "\"%d %d %d %d\"\n", size, size*numicons, numcolors, charsperpixel);
    contents += s;
    
    contents += "/* colors */\n";
    int n = 0;
    for (std::list<int>::iterator it = colors.begin(); it != colors.end(); ++it) {
        int RGB = *it;
        unsigned char R = (RGB & 0xFF0000) >> 16;
        unsigned char G = (RGB & 0x00FF00) >> 8;
        unsigned char B = (RGB & 0x0000FF);
        if (R == 0 && G == 0 && B == 0) {
            // nicer to show . or .. for black pixels
            contents += "\".";
            if (charsperpixel == 2) contents += ".";
            contents += " c #000000\"\n";
        } else {
            std::string hexcolor = "#";
            hexcolor += hex2(R);
            hexcolor += hex2(G);
            hexcolor += hex2(B);
            contents += "\"";
            if (charsperpixel == 1) {
                contents += cindex[n];
            } else {
                contents += cindex[n % 16];
                contents += cindex[n / 16];
            }
            contents += " c ";
            contents += hexcolor;
            contents += "\"\n";
        }
        n++;
    }
    
    for (int i = 0; i < numicons; i++) {
        sprintf(s, "/* icon for state %d */\n", i+1);
        contents += s;
        CGImageRef icon = CGImageCreateWithImageInRect(image,CGRectMake(i*15, 0, size, size));
        unsigned char* idata = GetRGBAPixels(icon);
        if (idata) {
            for (int y = 0; y < size; y++) {
                contents += "\"";
                for (int x = 0; x < size; x++) {
                    long pos = (y * size + x) * 4;
                    unsigned char R = idata[pos];
                    unsigned char G = idata[pos+1];
                    unsigned char B = idata[pos+2];
                    if (R == 0 && G == 0 && B == 0) {
                        // nicer to show . or .. for black pixels
                        contents += ".";
                        if (charsperpixel == 2) contents += ".";
                    } else {
                        n = 0;
                        int thisRGB = (R << 16) + (G << 8) + B;
                        for (std::list<int>::iterator it = colors.begin(); it != colors.end(); ++it) {
                            if (thisRGB == *it) break;
                            n++;
                        }
                        if (charsperpixel == 1) {
                            contents += cindex[n];
                        } else {
                            contents += cindex[n % 16];
                            contents += cindex[n / 16];
                        }
                    }
                }
                contents += "\"\n";
            }
            free(idata);
        }
    }
    
    return contents;
}

// -----------------------------------------------------------------------------

static std::string CreateICONS(const std::string& iconspath, bool nocolors)
{
    std::string contents = "\n@ICONS\n";
    CGImageRef image = [UIImage imageWithContentsOfFile:
        [NSString stringWithCString:iconspath.c_str() encoding:NSUTF8StringEncoding]].CGImage;
    if (image == NULL) {
        std::ostringstream oss;
        oss << "Could not load image from .icons file:\n" << iconspath.c_str();
        throw std::runtime_error(oss.str().c_str());
    } else {
        int wd = CGImageGetWidth(image);
        int ht = CGImageGetHeight(image);
         if (ht != 15 && ht != 22) {
            std::ostringstream oss;
            oss << "Image in " << iconspath.c_str() << " has incorrect height (should be 15 or 22).";
            throw std::runtime_error(oss.str().c_str());
        }
        if (wd % 15 > 0) {
            std::ostringstream oss;
            oss << "Image in " << iconspath.c_str() << " has incorrect width (should be multiple of 15).";
            throw std::runtime_error(oss.str().c_str());
        }
        int numicons = wd / 15;
        
        if (nocolors && MultiColorImage(image)) {
            // there was no .colors file and .icons file is multi-color,
            // so prepend a @COLORS section that sets non-icon colors
            contents = CreateStateColors(CGImageCreateWithImageInRect(image,CGRectMake(0,0,wd,15)), numicons) + contents;
        }
        
        if (ht == 15) {
            contents += CreateXPM(iconspath, image, 15, numicons);
        } else {
            contents += CreateXPM(iconspath, CGImageCreateWithImageInRect(image,CGRectMake(0,0,wd,15)), 15, numicons);
            contents += CreateXPM(iconspath, CGImageCreateWithImageInRect(image,CGRectMake(0,15,wd,7)), 7, numicons);
        }
    }
    return contents;
}

// -----------------------------------------------------------------------------

static void CreateOneRule(const std::string& rulefile, const std::string& folder,
                          std::list<std::string>& deprecated, std::string& htmlinfo)
{
    std::string rulename = rulefile.substr(0,rulefile.rfind('.'));
    std::string tablefile = rulename + ".table";
    std::string treefile = rulename + ".tree";
    std::string colorsfile = rulename + ".colors";
    std::string iconsfile = rulename + ".icons";
    std::string tabledata, treedata, colordata, icondata;
    
    std::string prefix = "";
    size_t hyphenpos = rulename.rfind('-');
    if (hyphenpos != std::string::npos)
        prefix = rulename.substr(0,hyphenpos);
    
    if (FOUND(deprecated,tablefile))
        tabledata = CreateTABLE(folder + tablefile);
    
    if (FOUND(deprecated,treefile))
        treedata = CreateTREE(folder + treefile);
    
    if (FOUND(deprecated,colorsfile)) {
        colordata = CreateCOLORS(folder + colorsfile);
    } else if (prefix.length() > 0) {
        // check for shared .colors file
        std::string sharedcolors = prefix + ".colors";
        if (FOUND(deprecated,sharedcolors))
            colordata = CreateCOLORS(folder + sharedcolors);
    }
    
    if (FOUND(deprecated,iconsfile)) {
        icondata = CreateICONS(folder + iconsfile, colordata.length() == 0);
    } else if (prefix.length() > 0) {
        // check for shared .icons file
        std::string sharedicons = prefix + ".icons";
        if (FOUND(deprecated,sharedicons))
            icondata = CreateICONS(folder + sharedicons, colordata.length() == 0);
    }
    
    std::string contents = "@RULE " + rulename + "\n";
    contents += tabledata;
    contents += treedata;
    contents += colordata;
    contents += icondata;
    
    // write contents to .rule file
    std::string rulepath = folder + rulefile;
    FILE* outfile = fopen(rulepath.c_str(), "w");
    if (outfile) {
        if (fputs(contents.c_str(), outfile) == EOF) {
            fclose(outfile);
            std::ostringstream oss;
            oss << "Could not write data to rule file:\n" << rulepath.c_str();
            throw std::runtime_error(oss.str().c_str());
        }
        fclose(outfile);
    } else {
        std::ostringstream oss;
        oss << "Could not create rule file:\n" << rulepath.c_str();
        throw std::runtime_error(oss.str().c_str());
    }
    
    // append created file to htmlinfo
    htmlinfo += "<a href=\"open:";
    htmlinfo += folder;
    htmlinfo += rulefile;
    htmlinfo += "\">";
    htmlinfo += rulefile;
    htmlinfo += "</a><br>\n";
}

// -----------------------------------------------------------------------------

std::string CreateRuleFiles(std::list<std::string>& deprecated,
                            std::list<std::string>& ziprules)
{
    // use the given list of deprecated .table/tree/colors/icons files
    // (recently extracted from a .zip file and installed in userrules)
    // to create new .rule files, except those in ziprules (they were in
    // .zip file and have already been installed)
    std::string htmlinfo;
    bool aborted = false;
    try {
        // create a list of candidate .rule files
        std::string rulefile;
        std::list<std::string> candidates;
        std::list<std::string>::iterator it;
        for (it=deprecated.begin(); it!=deprecated.end(); ++it) {
            // add .rule file to candidates if it hasn't been added yet
            rulefile = *it;
            rulefile = rulefile.substr(0,rulefile.rfind('.')) + ".rule";
            if (NOT_FOUND(candidates,rulefile)) {
                candidates.push_back(rulefile);
            }
        }
        
        // look for .rule files of the form foo-*.rule and ignore any foo.rule
        // entries in candidates if foo.table and foo.tree don't exist
        // (ie. foo.colors and/or foo.icons is shared by foo-*.table/tree)
        std::list<std::string> ignore;
        for (it=candidates.begin(); it!=candidates.end(); ++it) {
            rulefile = *it;
            size_t hyphenpos = rulefile.rfind('-');
            if (hyphenpos != std::string::npos && hyphenpos > 0) {
                std::string prefix = rulefile.substr(0,hyphenpos);
                std::string tablefile = prefix + ".table";
                std::string treefile = prefix + ".tree";
                if ( NOT_FOUND(deprecated,tablefile) && NOT_FOUND(deprecated,treefile) ) {
                    std::string sharedfile = prefix + ".rule";
                    if (FOUND(candidates,sharedfile)) {
                        ignore.push_back(sharedfile);
                    }
                }
            }
            // also ignore any .rule files included in zip file (and thus installed)
            if (FOUND(ziprules,rulefile)) {
                ignore.push_back(rulefile);
            }
            // note that we will overwrite any existing .rule files
            // (not in ziprules) in case the zip file's contents have changed
        }
        
        // candidates not in ignore list are the .rule files that need to be created
        for (it=candidates.begin(); it!=candidates.end(); ++it) {
            rulefile = *it;
            if (NOT_FOUND(ignore,rulefile)) {
                CreateOneRule(rulefile, userrules, deprecated, htmlinfo);
            }
        }
    }
    catch(const std::exception& e) {
        // display message set by throw std::runtime_error(...)
        Warning(e.what());
        aborted = true;
        // nice to also show error message in help window
        htmlinfo += "\n<p>*** CONVERSION ABORTED DUE TO ERROR ***\n<p>";
        htmlinfo += std::string(e.what());
    }
    
    if (!aborted) {
        // delete all the deprecated files
        std::list<std::string>::iterator it;
        for (it=deprecated.begin(); it!=deprecated.end(); ++it) {
            RemoveFile(userrules + *it);
        }
    }
    return htmlinfo;
}
