                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2007 Andrew Trevorrow and Tomas Rokicki.

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

#include "wx/wxprec.h"     // for compilers that support precompilation
#ifndef WX_PRECOMP
   #include "wx/wx.h"      // for all others include the necessary headers
#endif

#include "wx/file.h"       // for wxFile

#include "bigint.h"
#include "lifealgo.h"
#include "qlifealgo.h"
#include "hlifealgo.h"
#include "viewport.h"

#include "wxgolly.h"       // for wxGetApp, mainptr, statusptr
#include "wxutils.h"       // for Warning
#include "wxprefs.h"       // for randomfill, etc
#include "wxmain.h"        // for mainptr->...
#include "wxstatus.h"      // for statusptr->...
#include "wxrender.h"      // for CreatePasteImage
#include "wxscript.h"      // for inscript
#include "wxview.h"        // for PatternView
#include "wxundo.h"        // for currlayer->undoredo->...
#include "wxlayer.h"       // for currlayer, MarkLayerDirty, etc

#ifdef __WXMAC__
   #include <Carbon/Carbon.h>    // for Button
#endif

// Edit menu functions:

// -----------------------------------------------------------------------------

// most editing and saving operations are limited to abs coords <= 10^9
// because getcell/setcell take int parameters (the limits must be smaller
// than INT_MIN and INT_MAX to avoid boundary conditions)
static bigint min_coord = -1000000000;
static bigint max_coord = +1000000000;

bool PatternView::OutsideLimits(bigint& t, bigint& l, bigint& b, bigint& r)
{
   return ( t < min_coord || l < min_coord || b > max_coord || r > max_coord );
}

// -----------------------------------------------------------------------------

void PatternView::NoSelection()
{
   // set seltop > selbottom
   currlayer->seltop = 1;
   currlayer->selbottom = 0;
}

// -----------------------------------------------------------------------------

bool PatternView::SelectionExists()
{
   return (currlayer->seltop <= currlayer->selbottom) == 1;     // avoid VC++ warning
}

// -----------------------------------------------------------------------------

bool PatternView::SelectionVisible(wxRect* visrect)
{
   if (!SelectionExists()) return false;

   pair<int,int> lt = currlayer->view->screenPosOf(
                        currlayer->selleft, currlayer->seltop, currlayer->algo);
   pair<int,int> rb = currlayer->view->screenPosOf(
                        currlayer->selright, currlayer->selbottom, currlayer->algo);

   if (lt.first > currlayer->view->getxmax() || rb.first < 0 ||
       lt.second > currlayer->view->getymax() || rb.second < 0) {
      // no part of selection is visible
      return false;
   }

   // all or some of selection is visible in viewport;
   // only set visible rectangle if requested
   if (visrect) {
      if (lt.first < 0) lt.first = 0;
      if (lt.second < 0) lt.second = 0;
      if (currlayer->view->getmag() > 0) {
         // move rb to pixel at bottom right corner of cell
         rb.first += (1 << currlayer->view->getmag()) - 1;
         rb.second += (1 << currlayer->view->getmag()) - 1;
         if (currlayer->view->getmag() > 1) {
            // avoid covering gaps at scale 1:4 and above
            rb.first--;
            rb.second--;
         }
      }
      if (rb.first > currlayer->view->getxmax()) rb.first = currlayer->view->getxmax();
      if (rb.second > currlayer->view->getymax()) rb.second = currlayer->view->getymax();
      visrect->SetX(lt.first);
      visrect->SetY(lt.second);
      visrect->SetWidth(rb.first - lt.first + 1);
      visrect->SetHeight(rb.second - lt.second + 1);
   }
   return true;
}

// -----------------------------------------------------------------------------

bool PatternView::GridVisible()
{
   return ( showgridlines && currlayer->view->getmag() >= mingridmag );
}

// -----------------------------------------------------------------------------

void PatternView::EmptyUniverse()
{
   // kill all live cells in current universe
   int savewarp = currlayer->warp;
   int savemag = currlayer->view->getmag();
   bigint savex = currlayer->view->x;
   bigint savey = currlayer->view->y;
   bigint savegen = currlayer->algo->getGeneration();
   mainptr->CreateUniverse();
   // restore various settings
   mainptr->SetWarp(savewarp);
   mainptr->SetGenIncrement();
   currlayer->view->setpositionmag(savex, savey, savemag);
   currlayer->algo->setGeneration(savegen);
   mainptr->UpdatePatternAndStatus();
}

// -----------------------------------------------------------------------------

bool PatternView::SaveDifferences(lifealgo* oldalgo, lifealgo* newalgo,
                                  int itop, int ileft, int ibottom, int iright)
{
   int wd = iright - ileft + 1;
   int ht = ibottom - itop + 1;
   int cx, cy;
   double maxcount = (double)wd * (double)ht;
   int cntr = 0;
   bool abort = false;
   
   // compare patterns in given algos and call SaveCellChange for each different cell
   BeginProgress(_("Saving cell changes"));
   for ( cy=itop; cy<=ibottom; cy++ ) {
      for ( cx=ileft; cx<=iright; cx++ ) {
         if ( oldalgo->getcell(cx, cy) != newalgo->getcell(cx, cy) ) {
            // assume this is only called if allowundo && !currlayer->stayclean
            currlayer->undoredo->SaveCellChange(cx, cy);
         }
         cntr++;
         if ((cntr % 4096) == 0) {
            abort = AbortProgress((double)cntr / maxcount, wxEmptyString);
            if (abort) break;
         }
      }
      if (abort) break;
   }
   EndProgress();
   
   return !abort;
}

// -----------------------------------------------------------------------------

bool PatternView::CopyRect(int itop, int ileft, int ibottom, int iright,
                           lifealgo* srcalgo, lifealgo* destalgo,
                           bool erasesrc, const wxString& progmsg)
{
   int wd = iright - ileft + 1;
   int ht = ibottom - itop + 1;
   int cx, cy;
   double maxcount = (double)wd * (double)ht;
   int cntr = 0;
   bool abort = false;
   
   // copy (and erase if requested) live cells from given rect
   // in source universe to same rect in destination universe
   BeginProgress(progmsg);
   for ( cy=itop; cy<=ibottom; cy++ ) {
      for ( cx=ileft; cx<=iright; cx++ ) {
         int skip = srcalgo->nextcell(cx, cy);
         if (skip + cx > iright)
            skip = -1;           // pretend we found no more live cells
         if (skip >= 0) {
            // found next live cell
            cx += skip;
            destalgo->setcell(cx, cy, 1);
            if (erasesrc) srcalgo->setcell(cx, cy, 0);
         } else {
            cx = iright + 1;     // done this row
         }
         cntr++;
         if ((cntr % 4096) == 0) {
            double prog = ((cy - itop) * (double)(iright - ileft + 1) +
                           (cx - ileft)) / maxcount;
            abort = AbortProgress(prog, wxEmptyString);
            if (abort) break;
         }
      }
      if (abort) break;
   }
   if (erasesrc) srcalgo->endofpattern();
   destalgo->endofpattern();
   EndProgress();
   
   return !abort;
}

// -----------------------------------------------------------------------------

void PatternView::CopyAllRect(int itop, int ileft, int ibottom, int iright,
                              lifealgo* srcalgo, lifealgo* destalgo,
                              const wxString& progmsg)
{
   int wd = iright - ileft + 1;
   int ht = ibottom - itop + 1;
   int cx, cy;
   double maxcount = (double)wd * (double)ht;
   int cntr = 0;
   bool abort = false;
   
   // copy all cells from given rect in srcalgo to same rect in destalgo
   BeginProgress(progmsg);
   for ( cy=itop; cy<=ibottom; cy++ ) {
      for ( cx=ileft; cx<=iright; cx++ ) {
         destalgo->setcell(cx, cy, srcalgo->getcell(cx, cy));
         cntr++;
         if ((cntr % 4096) == 0) {
            abort = AbortProgress((double)cntr / maxcount, wxEmptyString);
            if (abort) break;
         }
      }
      if (abort) break;
   }
   destalgo->endofpattern();
   EndProgress();
}

// -----------------------------------------------------------------------------

void PatternView::ClearSelection()
{
   if (mainptr->generating || !SelectionExists()) return;
   
   // no need to do anything if there is no pattern
   if (currlayer->algo->isEmpty()) return;
   
   // save cell changes if undo/redo is enabled and script isn't constructing a pattern
   bool savecells = allowundo && !currlayer->stayclean;
   if (savecells && inscript) SavePendingChanges();
   
   bigint top, left, bottom, right;
   currlayer->algo->findedges(&top, &left, &bottom, &right);

   if ( !savecells &&
        currlayer->seltop <= top && currlayer->selbottom >= bottom &&
        currlayer->selleft <= left && currlayer->selright >= right ) {
      // selection encloses entire pattern so just create empty universe
      EmptyUniverse();
      MarkLayerDirty();
      return;
   }

   // no need to do anything if selection is completely outside pattern edges
   if ( currlayer->seltop > bottom || currlayer->selbottom < top ||
        currlayer->selleft > right || currlayer->selright < left ) {
      return;
   }
   
   // find intersection of selection and pattern to minimize work
   if (currlayer->seltop > top) top = currlayer->seltop;
   if (currlayer->selleft > left) left = currlayer->selleft;
   if (currlayer->selbottom < bottom) bottom = currlayer->selbottom;
   if (currlayer->selright < right) right = currlayer->selright;

   // can only use setcell in limited domain
   if ( OutsideLimits(top, left, bottom, right) ) {
      statusptr->ErrorMessage(selection_too_big);
      return;
   }

   // clear all live cells in selection
   int itop = top.toint();
   int ileft = left.toint();
   int ibottom = bottom.toint();
   int iright = right.toint();
   int wd = iright - ileft + 1;
   int ht = ibottom - itop + 1;
   int cx, cy;
   double maxcount = (double)wd * (double)ht;
   int cntr = 0;
   bool abort = false;
   bool selchanged = false;
   BeginProgress(_("Clearing selection"));
   lifealgo* curralgo = currlayer->algo;
   for ( cy=itop; cy<=ibottom; cy++ ) {
      for ( cx=ileft; cx<=iright; cx++ ) {
         int skip = curralgo->nextcell(cx, cy);
         if (skip + cx > iright)
            skip = -1;           // pretend we found no more live cells
         if (skip >= 0) {
            // found next live cell
            cx += skip;
            curralgo->setcell(cx, cy, 0);
            selchanged = true;
            if (savecells) currlayer->undoredo->SaveCellChange(cx, cy);
         } else {
            cx = iright + 1;     // done this row
         }
         cntr++;
         if ((cntr % 4096) == 0) {
            double prog = ((cy - itop) * (double)(iright - ileft + 1) +
                           (cx - ileft)) / maxcount;
            abort = AbortProgress(prog, wxEmptyString);
            if (abort) break;
         }
      }
      if (abort) break;
   }
   if (selchanged) curralgo->endofpattern();
   EndProgress();
   
   if (selchanged) {
      if (savecells) currlayer->undoredo->RememberCellChanges(_("Clear"), currlayer->dirty);
      MarkLayerDirty();
      mainptr->UpdatePatternAndStatus();
   }
}

// -----------------------------------------------------------------------------

bool PatternView::SaveOutsideSelection(bigint& t, bigint& l, bigint& b, bigint& r)
{
   if ( OutsideLimits(t, l, b, r) ) {
      statusptr->ErrorMessage(pattern_too_big);
      return false;
   }
   
   int itop = t.toint();
   int ileft = l.toint();
   int ibottom = b.toint();
   int iright = r.toint();
   
   // save ALL cells if selection is completely outside pattern edges
   bool saveall = currlayer->seltop > b || currlayer->selbottom < t ||
                  currlayer->selleft > r || currlayer->selright < l;

   // integer selection edges must not be outside pattern edges
   int stop = itop;
   int sleft = ileft;
   int sbottom = ibottom;
   int sright = iright;
   if (!saveall) {
      if (currlayer->seltop > t)    stop = currlayer->seltop.toint();
      if (currlayer->selleft > l)   sleft = currlayer->selleft.toint();
      if (currlayer->selbottom < b) sbottom = currlayer->selbottom.toint();
      if (currlayer->selright < r)  sright = currlayer->selright.toint();
   }
   
   int wd = iright - ileft + 1;
   int ht = ibottom - itop + 1;
   int cx, cy;
   double maxcount = (double)wd * (double)ht;
   int cntr = 0;
   bool abort = false;
   BeginProgress(_("Saving outside selection"));
   lifealgo* curralgo = currlayer->algo;
   for ( cy=itop; cy<=ibottom; cy++ ) {
      for ( cx=ileft; cx<=iright; cx++ ) {
         int skip = curralgo->nextcell(cx, cy);
         if (skip + cx > iright)
            skip = -1;           // pretend we found no more live cells
         if (skip >= 0) {
            // found next live cell
            cx += skip;
            if (saveall || cx < sleft || cx > sright || cy < stop || cy > sbottom) {
               // cell is outside selection edges
               currlayer->undoredo->SaveCellChange(cx, cy);
            }
         } else {
            cx = iright + 1;     // done this row
         }
         cntr++;
         if ((cntr % 4096) == 0) {
            double prog = ((cy - itop) * (double)(iright - ileft + 1) +
                           (cx - ileft)) / maxcount;
            abort = AbortProgress(prog, wxEmptyString);
            if (abort) break;
         }
      }
      if (abort) break;
   }
   EndProgress();
   
   if (abort) currlayer->undoredo->ForgetCellChanges();
   return !abort;
}

// -----------------------------------------------------------------------------

void PatternView::ClearOutsideSelection()
{
   if (mainptr->generating || !SelectionExists()) return;
   
   // no need to do anything if there is no pattern
   if (currlayer->algo->isEmpty()) return;
   
   // no need to do anything if selection encloses entire pattern
   bigint top, left, bottom, right;
   currlayer->algo->findedges(&top, &left, &bottom, &right);
   if ( currlayer->seltop <= top && currlayer->selbottom >= bottom &&
        currlayer->selleft <= left && currlayer->selright >= right ) {
      return;
   }
   
   // save cell changes if undo/redo is enabled and script isn't constructing a pattern
   bool savecells = allowundo && !currlayer->stayclean;
   if (savecells && inscript) SavePendingChanges();

   if (savecells) {
      // save live cells outside selection
      if ( !SaveOutsideSelection(top, left, bottom, right) ) {
         return;
      }
   } else {
      // create empty universe if selection is completely outside pattern edges
      if ( currlayer->seltop > bottom || currlayer->selbottom < top ||
           currlayer->selleft > right || currlayer->selright < left ) {
         EmptyUniverse();
         MarkLayerDirty();
         return;
      }
   }
   
   // find intersection of selection and pattern to minimize work
   if (currlayer->seltop > top) top = currlayer->seltop;
   if (currlayer->selleft > left) left = currlayer->selleft;
   if (currlayer->selbottom < bottom) bottom = currlayer->selbottom;
   if (currlayer->selright < right) right = currlayer->selright;

   // check that selection is small enough to save
   if ( OutsideLimits(top, left, bottom, right) ) {
      statusptr->ErrorMessage(selection_too_big);
      return;
   }
   
   // create a new universe of same type
   lifealgo* newalgo = CreateNewUniverse(currlayer->hash);

   // set same gen count
   newalgo->setGeneration( currlayer->algo->getGeneration() );
   
   // copy live cells in selection to new universe
   if ( CopyRect(top.toint(), left.toint(), bottom.toint(), right.toint(),
                 currlayer->algo, newalgo, false, _("Saving selection")) ) {
      // delete old universe and point currlayer->algo at new universe
      delete currlayer->algo;
      currlayer->algo = newalgo;
      mainptr->SetGenIncrement();
      if (savecells) currlayer->undoredo->RememberCellChanges(_("Clear Outside"), currlayer->dirty);
      MarkLayerDirty();
      mainptr->UpdatePatternAndStatus();
   } else {
      // CopyRect was aborted, so don't change current universe
      delete newalgo;
      if (savecells) currlayer->undoredo->ForgetCellChanges();
   }
}

// -----------------------------------------------------------------------------

void PatternView::AddEOL(char** chptr)
{
   #ifdef __WXMAC__
      **chptr = '\r';      // nicer for stupid apps like LifeLab :)
      *chptr += 1;
   #elif defined(__WXMSW__)
      **chptr = '\r';
      *chptr += 1;
      **chptr = '\n';
      *chptr += 1;
   #else // assume Unix
      **chptr = '\n';
      *chptr += 1;
   #endif
}

// -----------------------------------------------------------------------------

const unsigned int maxrleline = 70;    // max line length for RLE data

void PatternView::AddRun(char ch,
                         unsigned int* run,        // in and out
                         unsigned int* linelen,    // ditto
                         char** chptr)             // ditto
{
   // output of RLE pattern data is channelled thru here to make it easier to
   // ensure all lines have <= maxrleline characters
   unsigned int i, numlen;
   char numstr[32];
   
   if ( *run > 1 ) {
      sprintf(numstr, "%u", *run);
      numlen = strlen(numstr);
   } else {
      numlen = 0;                      // no run count shown if 1
   }
   // keep linelen <= maxrleline
   if ( *linelen + numlen + 1 > maxrleline ) {
      AddEOL(chptr);
      *linelen = 0;
   }
   i = 0;
   while (i < numlen) {
      **chptr = numstr[i];
      *chptr += 1;
      i++;
   }
   **chptr = ch;
   *chptr += 1;
   *linelen += numlen + 1;
   *run = 0;                           // reset run count
}

// -----------------------------------------------------------------------------

void PatternView::CopySelectionToClipboard(bool cut)
{
   // can only use getcell/setcell in limited domain
   if ( OutsideLimits(currlayer->seltop, currlayer->selbottom,
                      currlayer->selleft, currlayer->selright) ) {
      statusptr->ErrorMessage(selection_too_big);
      return;
   }
   
   int itop = currlayer->seltop.toint();
   int ileft = currlayer->selleft.toint();
   int ibottom = currlayer->selbottom.toint();
   int iright = currlayer->selright.toint();
   unsigned int wd = iright - ileft + 1;
   unsigned int ht = ibottom - itop + 1;

   // convert cells in selection to RLE data in textptr
   char* textptr;
   char* etextptr;
   int cursize = 4096;
   
   textptr = (char*)malloc(cursize);
   if (textptr == NULL) {
      statusptr->ErrorMessage(_("Not enough memory for clipboard data!"));
      return;
   }
   etextptr = textptr + cursize;

   // add RLE header line
   sprintf(textptr, "x = %u, y = %u, rule = %s", wd, ht, currlayer->algo->getrule());
   char* chptr = textptr;
   chptr += strlen(textptr);
   AddEOL(&chptr);
   // save start of data in case livecount is zero
   int datastart = chptr - textptr;
   
   // add RLE pattern data
   unsigned int livecount = 0;
   unsigned int linelen = 0;
   unsigned int brun = 0;
   unsigned int orun = 0;
   unsigned int dollrun = 0;
   char lastchar;
   int cx, cy;

   // save cell changes if undo/redo is enabled and script isn't constructing a pattern
   bool savecells = allowundo && !currlayer->stayclean;
   if (savecells && inscript) SavePendingChanges();
   
   double maxcount = (double)wd * (double)ht;
   int cntr = 0;
   bool abort = false;
   if (cut)
      BeginProgress(_("Cutting selection"));
   else
      BeginProgress(_("Copying selection"));

   lifealgo* curralgo = currlayer->algo;
   for ( cy=itop; cy<=ibottom; cy++ ) {
      // set lastchar to anything except 'o' or 'b'
      lastchar = 0;
      for ( cx=ileft; cx<=iright; cx++ ) {
         int skip = curralgo->nextcell(cx, cy);
         if (skip + cx > iright)
            skip = -1;           // pretend we found no more live cells
         if (skip > 0) {
            // have exactly "skip" empty cells here
            if (lastchar == 'b') {
               brun += skip;
            } else {
               if (orun > 0) {
                  // output current run of live cells
                  AddRun('o', &orun, &linelen, &chptr);
               }
               lastchar = 'b';
               brun = skip;
            }
         }
         if (skip >= 0) {
            // found next live cell
            cx += skip;
            livecount++;
            if (cut) {
               curralgo->setcell(cx, cy, 0);
               if (savecells) currlayer->undoredo->SaveCellChange(cx, cy);
            }
            if (lastchar == 'o') {
               orun++;
            } else {
               if (dollrun > 0) {
                  // output current run of $ chars
                  AddRun('$', &dollrun, &linelen, &chptr);
               }
               if (brun > 0) {
                  // output current run of dead cells
                  AddRun('b', &brun, &linelen, &chptr);
               }
               lastchar = 'o';
               orun = 1;
            }
         } else {
            cx = iright + 1;     // done this row
         }
         cntr++;
         if ((cntr % 4096) == 0) {
            double prog = ((cy - itop) * (double)(iright - ileft + 1) +
                           (cx - ileft)) / maxcount;
            abort = AbortProgress(prog, wxEmptyString);
            if (abort) break;
         }
         if (chptr + 60 >= etextptr) {
            // nearly out of space; try to increase allocation
            char* ntxtptr = (char*) realloc(textptr, 2*cursize);
            if (ntxtptr == 0) {
               statusptr->ErrorMessage(_("No more memory for clipboard data!"));
               // don't return here -- best to set abort flag and break so that
               // partially cut/copied portion gets saved to clipboard
               abort = true;
               break;
            }
            chptr = ntxtptr + (chptr - textptr);
            cursize *= 2;
            etextptr = ntxtptr + cursize;
            textptr = ntxtptr;
         }
      }
      if (abort) break;
      // end of current row
      if (lastchar == 'b') {
         // forget dead cells at end of row
         brun = 0;
      } else if (lastchar == 'o') {
         // output current run of live cells
         AddRun('o', &orun, &linelen, &chptr);
      }
      dollrun++;
   }
   
   if (livecount == 0) {
      // no live cells in selection so simplify RLE data to "!"
      chptr = textptr + datastart;
      *chptr = '!';
      chptr++;
   } else {
      // terminate RLE data
      dollrun = 1;
      AddRun('!', &dollrun, &linelen, &chptr);
      if (cut) currlayer->algo->endofpattern();
   }
   AddEOL(&chptr);
   *chptr = 0;
   
   EndProgress();
   
   if (cut && livecount > 0) {
      if (savecells) currlayer->undoredo->RememberCellChanges(_("Cut"), currlayer->dirty);
      // update dirty flag AFTER RememberCellChanges
      MarkLayerDirty();
      mainptr->UpdatePatternAndStatus();
   }
   
   wxString text = wxString(textptr,wxConvLocal);
   mainptr->CopyTextToClipboard(text);
   free(textptr);
}

// -----------------------------------------------------------------------------

void PatternView::CutSelection()
{
   if (mainptr->generating || !SelectionExists()) return;
   CopySelectionToClipboard(true);
}

// -----------------------------------------------------------------------------

void PatternView::CopySelection()
{
   if (mainptr->generating || !SelectionExists()) return;
   CopySelectionToClipboard(false);
}

// -----------------------------------------------------------------------------

void PatternView::SetPasteRect(wxRect& rect, bigint& wd, bigint& ht)
{
   int x, y, pastewd, pasteht;
   int mag = currlayer->view->getmag();
   
   // find cell coord of current paste cursor position
   pair<bigint, bigint> pcell = currlayer->view->at(pastex, pastey);

   // determine bottom right cell
   bigint right = pcell.first;     right += wd;    right -= 1;
   bigint bottom = pcell.second;   bottom += ht;   bottom -= 1;
   
   // best to use same method as in SelectionVisible
   pair<int,int> lt =
      currlayer->view->screenPosOf(pcell.first, pcell.second, currlayer->algo);
   pair<int,int> rb =
      currlayer->view->screenPosOf(right, bottom, currlayer->algo);

   if (mag > 0) {
      // move rb to pixel at bottom right corner of cell
      rb.first += (1 << mag) - 1;
      rb.second += (1 << mag) - 1;
      if (mag > 1) {
         // avoid covering gaps at scale 1:4 and above
         rb.first--;
         rb.second--;
      }
   }

   x = lt.first;
   y = lt.second;
   pastewd = rb.first - lt.first + 1;
   pasteht = rb.second - lt.second + 1;

   // this should never happen but play safe
   if (pastewd <= 0) pastewd = 1;
   if (pasteht <= 0) pasteht = 1;
   
   rect = wxRect(x, y, pastewd, pasteht);
   int xoffset, yoffset;
   int cellsize = 1 << mag;      // only used if mag > 0
   int gap = 1;                  // ditto
   if (mag == 1) gap = 0;        // but no gap between cells at scale 1:2
   switch (plocation) {
      case TopLeft:
         break;
      case TopRight:
         xoffset = mag > 0 ? -(pastewd - cellsize + gap) : -pastewd + 1;
         rect.Offset(xoffset, 0);
         break;
      case BottomRight:
         xoffset = mag > 0 ? -(pastewd - cellsize + gap) : -pastewd + 1;
         yoffset = mag > 0 ? -(pasteht - cellsize + gap) : -pasteht + 1;
         rect.Offset(xoffset, yoffset);
         break;
      case BottomLeft:
         yoffset = mag > 0 ? -(pasteht - cellsize + gap) : -pasteht + 1;
         rect.Offset(0, yoffset);
         break;
      case Middle:
         xoffset = mag > 0 ? -(pastewd / cellsize / 2) * cellsize : -pastewd / 2;
         yoffset = mag > 0 ? -(pasteht / cellsize / 2) * cellsize : -pasteht / 2;
         rect.Offset(xoffset, yoffset);
         break;
   }
}

// -----------------------------------------------------------------------------

void PatternView::PasteTemporaryToCurrent(lifealgo* tempalgo, bool toselection,
                                          bigint top, bigint left, bigint bottom, bigint right)
{
   // make sure given edges are within getcell/setcell limits
   if ( OutsideLimits(top, left, bottom, right) ) {
      statusptr->ErrorMessage(_("Clipboard pattern is too big."));
      return;
   }
   int itop = top.toint();
   int ileft = left.toint();
   int ibottom = bottom.toint();
   int iright = right.toint();
   bigint wd = iright - ileft + 1;
   bigint ht = ibottom - itop + 1;
   
   if ( toselection ) {
      bigint selht = currlayer->selbottom;  selht -= currlayer->seltop;   selht += 1;
      bigint selwd = currlayer->selright;   selwd -= currlayer->selleft;  selwd += 1;
      if ( ht > selht || wd > selwd ) {
         statusptr->ErrorMessage(_("Clipboard pattern is bigger than selection."));
         return;
      }

      // set paste rectangle's top left cell coord
      top = currlayer->seltop;
      left = currlayer->selleft;

   } else {

      // ask user where to paste the clipboard pattern
      statusptr->DisplayMessage(_("Click where you want to paste..."));

      // temporarily change cursor to cross
      wxCursor* savecurs = currlayer->curs;
      currlayer->curs = curs_cross;
      // CheckCursor(true);            // probs on Mac if Paste menu item selected
      #ifdef __WXMAC__
         wxSetCursor(*currlayer->curs);
      #endif
      SetCursor(*currlayer->curs);

      // create image for drawing pattern to be pasted; note that given box
      // is not necessarily the minimal bounding box because clipboard pattern
      // might have blank borders (in fact it could be empty)
      wxRect bbox = wxRect(ileft, itop, wd.toint(), ht.toint());
      CreatePasteImage(tempalgo, bbox);

      waitingforclick = true;
      mainptr->EnableAllMenus(false);  // disable all menu items
      mainptr->UpdateToolBar(false);   // disable all tool bar buttons
      UpdateLayerBar(false);           // disable all layer bar buttons
      CaptureMouse();                  // get mouse down event even if outside view
      pasterect = wxRect(-1,-1,0,0);

      while (waitingforclick) {
         wxPoint pt = ScreenToClient( wxGetMousePosition() );
         pastex = pt.x;
         pastey = pt.y;
         if (PointInView(pt.x, pt.y)) {
            // determine new paste rectangle
            wxRect newrect;
            SetPasteRect(newrect, wd, ht);
            if (newrect != pasterect) {
               // draw new pasterect
               pasterect = newrect;
               Refresh(false);
               // don't update immediately
            }
         } else {
            // mouse outside viewport so erase old pasterect if necessary
            if ( pasterect.width > 0 ) {
               pasterect = wxRect(-1,-1,0,0);
               Refresh(false);
               // don't update immediately
            }
         }
         wxMilliSleep(10);             // don't hog CPU
         wxGetApp().Yield(true);
         // make sure viewport retains focus so we can use keyboard shortcuts
         SetFocus();
         // waitingforclick becomes false if OnMouseDown is called
         #ifdef __WXMAC__
            // need to check for click here because OnMouseDown does not
            // get called if click is in menu bar or in another window;
            // is this a CaptureMouse bug in wxMac???
            if ( waitingforclick && Button() ) {
               pt = ScreenToClient( wxGetMousePosition() );
               pastex = pt.x;
               pastey = pt.y;
               waitingforclick = false;
               FlushEvents(mDownMask + mUpMask, 0);   // avoid wx seeing click
            }
         #endif
      }

      if ( HasCapture() ) ReleaseMouse();
      mainptr->EnableAllMenus(true);
      DestroyPasteImage();
   
      // restore cursor
      currlayer->curs = savecurs;
      CheckCursor(mainptr->IsActive());
      
      if ( pasterect.width > 0 ) {
         // erase old pasterect
         Refresh(false);
         // no need to update immediately
         // Update();
      }
      
      if ( pastex < 0 || pastex > currlayer->view->getxmax() ||
           pastey < 0 || pastey > currlayer->view->getymax() ) {
         statusptr->DisplayMessage(_("Paste aborted."));
         return;
      }
      
      // set paste rectangle's top left cell coord
      pair<bigint, bigint> clickpos = currlayer->view->at(pastex, pastey);
      top = clickpos.second;
      left = clickpos.first;
      bigint halfht = ht;
      bigint halfwd = wd;
      halfht.div2();
      halfwd.div2();
      if (currlayer->view->getmag() > 1) {
         if (ht.even()) halfht -= 1;
         if (wd.even()) halfwd -= 1;
      }
      switch (plocation) {
         case TopLeft:     /* no change*/ break;
         case TopRight:    left -= wd; left += 1; break;
         case BottomRight: left -= wd; left += 1; top -= ht; top += 1; break;
         case BottomLeft:  top -= ht; top += 1; break;
         case Middle:      left -= halfwd; top -= halfht; break;
      }
   }

   // check that paste rectangle is within edit limits
   bottom = top;   bottom += ht;   bottom -= 1;
   right = left;   right += wd;    right -= 1;
   if ( OutsideLimits(top, left, bottom, right) ) {
      statusptr->ErrorMessage(_("Pasting is not allowed outside +/- 10^9 boundary."));
      return;
   }

   // set pastex,pastey to top left cell of paste rectangle
   pastex = left.toint();
   pastey = top.toint();

   // save cell changes if undo/redo is enabled and script isn't constructing a pattern
   bool savecells = allowundo && !currlayer->stayclean;
   if (savecells && inscript) SavePendingChanges();

   // copy pattern from temporary universe to current universe
   int tx, ty, cx, cy;
   double maxcount = wd.todouble() * ht.todouble();
   int cntr = 0;
   bool abort = false;
   bool pattchanged = false;
   BeginProgress(_("Pasting pattern"));
   
   // we can speed up pasting sparse patterns by using nextcell in these cases:
   // - if using Or mode
   // - if current universe is empty
   // - if paste rect is outside current pattern edges
   bool usenextcell;
   if ( pmode == Or || currlayer->algo->isEmpty() ) {
      usenextcell = true;
   } else {
      bigint ctop, cleft, cbottom, cright;
      currlayer->algo->findedges(&ctop, &cleft, &cbottom, &cright);
      usenextcell = top > cbottom || bottom < ctop || left > cright || right < cleft;
   }
   
   lifealgo* curralgo = currlayer->algo;
   if ( usenextcell ) {
      cy = pastey;
      for ( ty=itop; ty<=ibottom; ty++ ) {
         cx = pastex;
         for ( tx=ileft; tx<=iright; tx++ ) {
            int skip = tempalgo->nextcell(tx, ty);
            if (skip + tx > iright)
               skip = -1;           // pretend we found no more live cells
            if (skip >= 0) {
               // found next live cell so paste it into current universe
               tx += skip;
               cx += skip;
               if (curralgo->getcell(cx, cy) != 1) {
                  curralgo->setcell(cx, cy, 1);
                  pattchanged = true;
                  if (savecells) currlayer->undoredo->SaveCellChange(cx, cy);
               }
               cx++;
            } else {
               tx = iright + 1;     // done this row
            }
            cntr++;
            if ((cntr % 4096) == 0) {
               double prog = ((ty - itop) * (double)(iright - ileft + 1) +
                              (tx - ileft)) / maxcount;
               abort = AbortProgress(prog, wxEmptyString);
               if (abort) break;
            }
         }
         if (abort) break;
         cy++;
      }
   } else {
      // have to use slower getcell/setcell calls
      int tempstate, currstate;
      cy = pastey;
      for ( ty=itop; ty<=ibottom; ty++ ) {
         cx = pastex;
         for ( tx=ileft; tx<=iright; tx++ ) {
            tempstate = tempalgo->getcell(tx, ty);
            currstate = curralgo->getcell(cx, cy);
            switch (pmode) {
               case Copy:
                  if (tempstate != currstate) {
                     curralgo->setcell(cx, cy, tempstate);
                     pattchanged = true;
                     if (savecells) currlayer->undoredo->SaveCellChange(cx, cy);
                  }
                  break;
               case Or:
                  // Or mode is done using above nextcell loop;
                  // we only include this case to avoid compiler warning
                  break;
               case Xor:
                  if (tempstate == currstate) {
                     if (currstate != 0) {
                        curralgo->setcell(cx, cy, 0);
                        pattchanged = true;
                        if (savecells) currlayer->undoredo->SaveCellChange(cx, cy);
                     }
                  } else {
                     if (currstate != 1) {
                        curralgo->setcell(cx, cy, 1);
                        pattchanged = true;
                        if (savecells) currlayer->undoredo->SaveCellChange(cx, cy);
                     }
                  }
                  break;
            }
            cx++;
            cntr++;
            if ( (cntr % 4096) == 0 ) {
               abort = AbortProgress((double)cntr / maxcount, wxEmptyString);
               if (abort) break;
            }
         }
         if (abort) break;
         cy++;
      }
   }

   if (pattchanged) currlayer->algo->endofpattern();
   EndProgress();
   
   // tidy up and display result
   statusptr->ClearMessage();
   if (pattchanged) {
      if (savecells) currlayer->undoredo->RememberCellChanges(_("Paste"), currlayer->dirty);
      MarkLayerDirty();
      mainptr->UpdatePatternAndStatus();
   }
}

// -----------------------------------------------------------------------------

bool PatternView::GetClipboardPattern(lifealgo** tempalgo,
                                      bigint* t, bigint* l, bigint* b, bigint* r)
{
   #ifdef __WXX11__
      if ( !wxFileExists(mainptr->clipfile) ) return false;
   #else
      wxTextDataObject data;
      if ( !mainptr->GetTextFromClipboard(&data) ) return false;
   
      // copy clipboard data to temporary file so we can handle all formats
      // supported by readclipboard
      wxFile tmpfile(mainptr->clipfile, wxFile::write);
      if ( !tmpfile.IsOpened() ) {
         Warning(_("Could not create temporary file for clipboard data!"));
         return false;
      }
      if ( !tmpfile.Write(data.GetText()) ) {
         Warning(_("Could not write clipboard data to temporary file!  Maybe disk is full?"));
         tmpfile.Close();
         return false;
      }
      tmpfile.Close();
   #endif         

   const char* err = readclipboard(mainptr->clipfile.mb_str(wxConvLocal),
                                   **tempalgo, t, l, b, r);
   if (err && strcmp(err,cannotreadhash) == 0) {
      // clipboard contains macrocell data so we have to use hlife
      delete *tempalgo;
      *tempalgo = CreateNewUniverse(true);
      err = readclipboard(mainptr->clipfile.mb_str(wxConvLocal),
                          **tempalgo, t, l, b, r);
   }
   #ifdef __WXX11__
      // don't delete clipboard file
   #else
      wxRemoveFile(mainptr->clipfile);
   #endif

   if (err) {
      Warning(wxString(err,wxConvLocal));
      return false;
   }

   return true;
}

// -----------------------------------------------------------------------------

void PatternView::PasteClipboard(bool toselection)
{
   if (mainptr->generating || waitingforclick || !mainptr->ClipboardHasText()) return;
   if (toselection && !SelectionExists()) return;

   // create a temporary universe for storing clipboard pattern;
   // use qlife because its setcell/getcell calls are faster
   lifealgo* tempalgo = CreateNewUniverse(false);

   // read clipboard pattern into temporary universe;
   // note that tempalgo will be deleted and re-created as a hlifealgo
   // if clipboard contains macrocell data
   bigint top, left, bottom, right;
   if ( GetClipboardPattern(&tempalgo, &top, &left, &bottom, &right) ) {
      PasteTemporaryToCurrent(tempalgo, toselection, top, left, bottom, right);
   }

   delete tempalgo;
}

// -----------------------------------------------------------------------------

void PatternView::CyclePasteLocation()
{
   if (plocation == TopLeft) {
      plocation = TopRight;
      if (!waitingforclick) statusptr->DisplayMessage(_("Paste location is Top Right."));
   } else if (plocation == TopRight) {
      plocation = BottomRight;
      if (!waitingforclick) statusptr->DisplayMessage(_("Paste location is Bottom Right."));
   } else if (plocation == BottomRight) {
      plocation = BottomLeft;
      if (!waitingforclick) statusptr->DisplayMessage(_("Paste location is Bottom Left."));
   } else if (plocation == BottomLeft) {
      plocation = Middle;
      if (!waitingforclick) statusptr->DisplayMessage(_("Paste location is Middle."));
   } else {
      plocation = TopLeft;
      if (!waitingforclick) statusptr->DisplayMessage(_("Paste location is Top Left."));
   }
   if (waitingforclick) {
      // force redraw of paste rectangle if mouse is inside viewport
      pasterect = wxRect(-1,-1,0,0);
   }
}

// -----------------------------------------------------------------------------

void PatternView::CyclePasteMode()
{
   if (pmode == Copy) {
      pmode = Or;
      if (!waitingforclick) statusptr->DisplayMessage(_("Paste mode is Or."));
   } else if (pmode == Or) {
      pmode = Xor;
      if (!waitingforclick) statusptr->DisplayMessage(_("Paste mode is Xor."));
   } else {
      pmode = Copy;
      if (!waitingforclick) statusptr->DisplayMessage(_("Paste mode is Copy."));
   }
   if (waitingforclick) {
      // force redraw of paste rectangle if mouse is inside viewport
      pasterect = wxRect(-1,-1,0,0);
   }
}

// -----------------------------------------------------------------------------

void PatternView::DisplaySelectionSize()
{
   if (waitingforclick || inscript || currlayer->undoredo->doingscriptchanges)
      return;
   
   bigint wd = currlayer->selright;    wd -= currlayer->selleft;   wd += bigint::one;
   bigint ht = currlayer->selbottom;   ht -= currlayer->seltop;    ht += bigint::one;
   wxString msg = _("Selection wd x ht = ");
   msg += statusptr->Stringify(wd);
   msg += _(" x ");
   msg += statusptr->Stringify(ht);
   statusptr->SetMessage(msg);
}

// -----------------------------------------------------------------------------

void PatternView::SaveCurrentSelection()
{
   if (allowundo && !currlayer->stayclean) {
      currlayer->savetop = currlayer->seltop;
      currlayer->saveleft = currlayer->selleft;
      currlayer->savebottom = currlayer->selbottom;
      currlayer->saveright = currlayer->selright;
   }
}

// -----------------------------------------------------------------------------

void PatternView::RememberNewSelection(const wxString& action)
{
   if (allowundo && !currlayer->stayclean) {
      if (inscript) SavePendingChanges();
      currlayer->undoredo->RememberSelection(action);
   }
}

// -----------------------------------------------------------------------------

void PatternView::SelectAll()
{
   SaveCurrentSelection();
   if (SelectionExists()) {
      NoSelection();
      mainptr->UpdatePatternAndStatus();
   }

   if (currlayer->algo->isEmpty()) {
      statusptr->ErrorMessage(empty_pattern);
      RememberNewSelection(_("Deselection"));
      return;
   }
   
   currlayer->algo->findedges(&currlayer->seltop, &currlayer->selleft,
                              &currlayer->selbottom, &currlayer->selright);

   RememberNewSelection(_("Select All"));
   DisplaySelectionSize();
   mainptr->UpdatePatternAndStatus();
}

// -----------------------------------------------------------------------------

void PatternView::RemoveSelection()
{
   if (SelectionExists()) {
      SaveCurrentSelection();
      NoSelection();
      RememberNewSelection(_("Deselection"));
      mainptr->UpdatePatternAndStatus();
   }
}

// -----------------------------------------------------------------------------

void PatternView::ShrinkSelection(bool fit)
{
   if (!SelectionExists()) return;
   
   // check if there is no pattern
   if (currlayer->algo->isEmpty()) {
      statusptr->ErrorMessage(empty_selection);
      if (fit) FitSelection();
      return;
   }
   
   // check if selection encloses entire pattern
   bigint top, left, bottom, right;
   currlayer->algo->findedges(&top, &left, &bottom, &right);
   if ( currlayer->seltop <= top && currlayer->selbottom >= bottom &&
        currlayer->selleft <= left && currlayer->selright >= right ) {
      // shrink edges
      SaveCurrentSelection();
      currlayer->seltop = top;
      currlayer->selleft = left;
      currlayer->selbottom = bottom;
      currlayer->selright = right;
      RememberNewSelection(_("Shrink Selection"));
      DisplaySelectionSize();
      if (fit)
         FitSelection();   // calls UpdateEverything
      else
         mainptr->UpdatePatternAndStatus();
      return;
   }

   // check if selection is completely outside pattern edges
   if ( currlayer->seltop > bottom || currlayer->selbottom < top ||
        currlayer->selleft > right || currlayer->selright < left ) {
      statusptr->ErrorMessage(empty_selection);
      if (fit) FitSelection();
      return;
   }
   
   // find intersection of selection and pattern to minimize work
   if (currlayer->seltop > top) top = currlayer->seltop;
   if (currlayer->selleft > left) left = currlayer->selleft;
   if (currlayer->selbottom < bottom) bottom = currlayer->selbottom;
   if (currlayer->selright < right) right = currlayer->selright;

   // check that selection is small enough to save
   if ( OutsideLimits(top, left, bottom, right) ) {
      statusptr->ErrorMessage(selection_too_big);
      if (fit) FitSelection();
      return;
   }
   
   // the easy way to shrink selection is to create a new temporary universe,
   // copy selection into new universe and then call findedges;
   // use qlife because its findedges call is faster
   lifealgo* tempalgo = CreateNewUniverse(false);
   
   // copy live cells in selection to temporary universe
   if ( CopyRect(top.toint(), left.toint(), bottom.toint(), right.toint(),
                 currlayer->algo, tempalgo, false, _("Saving selection")) ) {
      if ( tempalgo->isEmpty() ) {
         statusptr->ErrorMessage(empty_selection);
      } else {
         SaveCurrentSelection();
         tempalgo->findedges(&currlayer->seltop, &currlayer->selleft,
                             &currlayer->selbottom, &currlayer->selright);
         RememberNewSelection(_("Shrink Selection"));
         DisplaySelectionSize();
         if (!fit) mainptr->UpdatePatternAndStatus();
      }
   }
   
   delete tempalgo;
   if (fit) FitSelection();
}

// -----------------------------------------------------------------------------

void PatternView::RandomFill()
{
   if (mainptr->generating || !SelectionExists()) return;

   // can only use getcell/setcell in limited domain
   if ( OutsideLimits(currlayer->seltop, currlayer->selbottom,
                      currlayer->selleft, currlayer->selright) ) {
      statusptr->ErrorMessage(selection_too_big);
      return;
   }

   // save cell changes if undo/redo is enabled and script isn't constructing a pattern
   bool savecells = allowundo && !currlayer->stayclean;
   if (savecells && inscript) SavePendingChanges();

   // no need to kill cells if selection is empty
   bool killcells = !currlayer->algo->isEmpty();
   if ( killcells ) {
      // find pattern edges and compare with selection edges
      bigint top, left, bottom, right;
      currlayer->algo->findedges(&top, &left, &bottom, &right);
      if ( currlayer->seltop <= top && currlayer->selbottom >= bottom &&
           currlayer->selleft <= left && currlayer->selright >= right ) {
         // selection encloses entire pattern so create empty universe
         if (savecells) {
            // don't kill pattern otherwise we can't use SaveCellChange below
         } else {
            EmptyUniverse();
            killcells = false;
         }
      } else if ( currlayer->seltop > bottom || currlayer->selbottom < top ||
                  currlayer->selleft > right || currlayer->selright < left ) {
         // selection is completely outside pattern edges
         killcells = false;
      }
   }
   
   int itop = currlayer->seltop.toint();
   int ileft = currlayer->selleft.toint();
   int ibottom = currlayer->selbottom.toint();
   int iright = currlayer->selright.toint();
   int wd = iright - ileft + 1;
   int ht = ibottom - itop + 1;
   double maxcount = (double)wd * (double)ht;
   int cntr = 0;
   bool abort = false;
   BeginProgress(_("Randomly filling selection"));
   int cx, cy;
   lifealgo* curralgo = currlayer->algo;
   for ( cy=itop; cy<=ibottom; cy++ ) {
      for ( cx=ileft; cx<=iright; cx++ ) {
         // randomfill is from 1..100
         if (savecells) {
            // remember cell coords if state changes
            if ((rand() % 100) < randomfill) {
               if (!killcells || curralgo->getcell(cx, cy) == 0) {
                  curralgo->setcell(cx, cy, 1);
                  currlayer->undoredo->SaveCellChange(cx, cy);
               }
            } else if (killcells && curralgo->getcell(cx, cy) > 0) {
               curralgo->setcell(cx, cy, 0);
               currlayer->undoredo->SaveCellChange(cx, cy);
            }
         } else {
            if ((rand() % 100) < randomfill) {
               curralgo->setcell(cx, cy, 1);
            } else if (killcells) {
               curralgo->setcell(cx, cy, 0);
            }
         }
         cntr++;
         if ((cntr % 4096) == 0) {
            abort = AbortProgress((double)cntr / maxcount, wxEmptyString);
            if (abort) break;
         }
      }
      if (abort) break;
   }
   currlayer->algo->endofpattern();
   EndProgress();

   if (savecells) currlayer->undoredo->RememberCellChanges(_("Random Fill"), currlayer->dirty);

   // update dirty flag AFTER RememberCellChanges
   MarkLayerDirty();
   mainptr->UpdatePatternAndStatus();
}

// -----------------------------------------------------------------------------

bool PatternView::FlipTopBottom(int itop, int ileft, int ibottom, int iright)
{
   // following code can be optimized by using faster nextcell calls!!!
   // ie. if entire pattern is selected then flip into new universe and
   // switch to that universe
   
   int wd = iright - ileft + 1;
   int ht = ibottom - itop + 1;
   double maxcount = (double)wd * (double)ht / 2.0;
   int cntr = 0;
   bool abort = false;
   
   BeginProgress(_("Flipping top-bottom"));
   int cx, cy;
   int mirrory = ibottom;
   int halfway = (itop - 1) + ht / 2;
   lifealgo* curralgo = currlayer->algo;
   for ( cy=itop; cy<=halfway; cy++ ) {
      for ( cx=ileft; cx<=iright; cx++ ) {
         int currstate = curralgo->getcell(cx, cy);
         int mirrstate = curralgo->getcell(cx, mirrory);
         if ( currstate != mirrstate ) {
            curralgo->setcell(cx, cy, mirrstate);
            curralgo->setcell(cx, mirrory, currstate);
         }
         cntr++;
         if ((cntr % 4096) == 0) {
            abort = AbortProgress((double)cntr / maxcount, wxEmptyString);
            if (abort) break;
         }
      }
      if (abort) break;
      mirrory--;
   }
   currlayer->algo->endofpattern();
   EndProgress();
   
   return !abort;
}

// -----------------------------------------------------------------------------

bool PatternView::FlipLeftRight(int itop, int ileft, int ibottom, int iright)
{
   // following code can be optimized by using faster nextcell calls!!!
   // ie. if entire pattern is selected then flip into new universe and
   // switch to that universe
   
   int wd = iright - ileft + 1;
   int ht = ibottom - itop + 1;
   double maxcount = (double)wd * (double)ht / 2.0;
   int cntr = 0;
   bool abort = false;
   
   BeginProgress(_("Flipping left-right"));
   int cx, cy;
   int mirrorx = iright;
   int halfway = (ileft - 1) + wd / 2;
   lifealgo* curralgo = currlayer->algo;
   for ( cx=ileft; cx<=halfway; cx++ ) {
      for ( cy=itop; cy<=ibottom; cy++ ) {
         int currstate = curralgo->getcell(cx, cy);
         int mirrstate = curralgo->getcell(mirrorx, cy);
         if ( currstate != mirrstate ) {
            curralgo->setcell(cx, cy, mirrstate);
            curralgo->setcell(mirrorx, cy, currstate);
         }
         cntr++;
         if ((cntr % 4096) == 0) {
            abort = AbortProgress((double)cntr / maxcount, wxEmptyString);
            if (abort) break;
         }
      }
      if (abort) break;
      mirrorx--;
   }
   currlayer->algo->endofpattern();
   EndProgress();
   
   return !abort;
}

// -----------------------------------------------------------------------------

bool PatternView::FlipSelection(bool topbottom)
{
   if (mainptr->generating || !SelectionExists()) return false;

   // can only use getcell/setcell in limited domain
   if ( OutsideLimits(currlayer->seltop, currlayer->selbottom,
                      currlayer->selleft, currlayer->selright) ) {
      statusptr->ErrorMessage(selection_too_big);
      return false;
   }
   
   int itop = currlayer->seltop.toint();
   int ileft = currlayer->selleft.toint();
   int ibottom = currlayer->selbottom.toint();
   int iright = currlayer->selright.toint();
   
   if (topbottom) {
      if (ibottom == itop) return false;
      if (!FlipTopBottom(itop, ileft, ibottom, iright)) return false;
   } else {
      if (iright == ileft) return false;
      if (!FlipLeftRight(itop, ileft, ibottom, iright)) return false;
   }

   // flips are always reversible so no need to use SaveCellChange and RememberCellChanges
   if (allowundo && !currlayer->stayclean) {
      if (inscript) SavePendingChanges();
      currlayer->undoredo->RememberFlip(topbottom, currlayer->dirty);
   }
   
   // update dirty flag AFTER RememberFlip
   MarkLayerDirty();
   mainptr->UpdatePatternAndStatus();
   
   return true;
}

// -----------------------------------------------------------------------------

const wxString rotate_clockwise       = _("Rotating selection +90 degrees");
const wxString rotate_anticlockwise   = _("Rotating selection -90 degrees");

bool PatternView::RotateRect(bool clockwise,
                             lifealgo* srcalgo, lifealgo* destalgo, bool erasesrc,
                             int itop, int ileft, int ibottom, int iright,
                             int ntop, int nleft, int nbottom, int nright)
{
   int wd = iright - ileft + 1;
   int ht = ibottom - itop + 1;
   double maxcount = (double)wd * (double)ht;
   int cntr = 0;
   bool abort = false;
   int cx, cy, newx, newy, newxinc, newyinc, firstnewy;
   
   if (clockwise) {
      BeginProgress(rotate_clockwise);
      firstnewy = ntop;
      newx = nright;
      newyinc = 1;
      newxinc = -1;
   } else {
      BeginProgress(rotate_anticlockwise);
      firstnewy = nbottom;
      newx = nleft;
      newyinc = -1;
      newxinc = 1;
   }

   for ( cy=itop; cy<=ibottom; cy++ ) {
      newy = firstnewy;
      for ( cx=ileft; cx<=iright; cx++ ) {
         int skip = srcalgo->nextcell(cx, cy);
         if (skip + cx > iright)
            skip = -1;           // pretend we found no more live cells
         if (skip >= 0) {
            // found next live cell
            cx += skip;
            if (erasesrc) srcalgo->setcell(cx, cy, 0);
            newy += newyinc * skip;
            destalgo->setcell(newx, newy, 1);
         } else {
            cx = iright + 1;     // done this row
         }
         cntr++;
         if ((cntr % 4096) == 0) {
            double prog = ((cy - itop) * (double)(iright - ileft + 1) +
                           (cx - ileft)) / maxcount;
            abort = AbortProgress(prog, wxEmptyString);
            if (abort) break;
         }
         newy += newyinc;
      }
      if (abort) break;
      newx += newxinc;
   }
   
   destalgo->endofpattern();
   srcalgo->endofpattern();
   EndProgress();

   return !abort;
}

// -----------------------------------------------------------------------------

bool PatternView::RotatePattern(bool clockwise,
                                bigint& newtop, bigint& newbottom,
                                bigint& newleft, bigint& newright,
                                bool inundoredo)
{
   // create new universe of same type as current universe
   lifealgo* newalgo = CreateNewUniverse(currlayer->hash);

   // set same gen count
   newalgo->setGeneration( currlayer->algo->getGeneration() );
   
   // copy all live cells to new universe, rotating the coords by +/- 90 degrees
   int itop    = currlayer->seltop.toint();
   int ileft   = currlayer->selleft.toint();
   int ibottom = currlayer->selbottom.toint();
   int iright  = currlayer->selright.toint();
   int wd = iright - ileft + 1;
   int ht = ibottom - itop + 1;
   double maxcount = (double)wd * (double)ht;
   int cntr = 0;
   bool abort = false;
   int cx, cy, newx, newy, newxinc, newyinc, firstnewy;
   
   if (clockwise) {
      BeginProgress(rotate_clockwise);
      firstnewy = newtop.toint();
      newx = newright.toint();
      newyinc = 1;
      newxinc = -1;
   } else {
      BeginProgress(rotate_anticlockwise);
      firstnewy = newbottom.toint();
      newx = newleft.toint();
      newyinc = -1;
      newxinc = 1;
   }

   lifealgo* curralgo = currlayer->algo;
   for ( cy=itop; cy<=ibottom; cy++ ) {
      newy = firstnewy;
      for ( cx=ileft; cx<=iright; cx++ ) {
         int skip = curralgo->nextcell(cx, cy);
         if (skip + cx > iright)
            skip = -1;           // pretend we found no more live cells
         if (skip >= 0) {
            // found next live cell
            cx += skip;
            newy += newyinc * skip;
            newalgo->setcell(newx, newy, 1);
         } else {
            cx = iright + 1;     // done this row
         }
         cntr++;
         if ((cntr % 4096) == 0) {
            double prog = ((cy - itop) * (double)(iright - ileft + 1) +
                           (cx - ileft)) / maxcount;
            abort = AbortProgress(prog, wxEmptyString);
            if (abort) break;
         }
         newy += newyinc;
      }
      if (abort) break;
      newx += newxinc;
   }

   newalgo->endofpattern();
   EndProgress();
   
   if (abort) {
      delete newalgo;
   } else {
      // rotate the selection edges
      currlayer->seltop    = newtop;
      currlayer->selbottom = newbottom;
      currlayer->selleft   = newleft;
      currlayer->selright  = newright;
      
      // switch to new universe and display results
      delete currlayer->algo;
      currlayer->algo = newalgo;
      mainptr->SetGenIncrement();
      DisplaySelectionSize();
      
      // rotating entire pattern is easily reversible so no need to use
      // SaveCellChange and RememberCellChanges in this case
      if (allowundo && !currlayer->stayclean && !inundoredo) {
         if (inscript) SavePendingChanges();
         currlayer->undoredo->RememberRotation(clockwise, currlayer->dirty);
      }
      
      // update dirty flag AFTER RememberRotation
      if (!inundoredo) MarkLayerDirty();
      mainptr->UpdatePatternAndStatus();
   }

   return !abort;
}

// -----------------------------------------------------------------------------

bool PatternView::RotateSelection(bool clockwise, bool inundoredo)
{
   if (mainptr->generating || !SelectionExists()) return false;
   
   // determine rotated selection edges
   bigint halfht = currlayer->selbottom;   halfht -= currlayer->seltop;    halfht.div2();
   bigint halfwd = currlayer->selright;    halfwd -= currlayer->selleft;   halfwd.div2();
   bigint midy = currlayer->seltop;    midy += halfht;
   bigint midx = currlayer->selleft;   midx += halfwd;
   bigint newtop    = midy;   newtop    += currlayer->selleft;     newtop    -= midx;
   bigint newbottom = midy;   newbottom += currlayer->selright;    newbottom -= midx;
   bigint newleft   = midx;   newleft   += currlayer->seltop;      newleft   -= midy;
   bigint newright  = midx;   newright  += currlayer->selbottom;   newright  -= midy;
   
   // if there is no pattern then just rotate the selection edges
   if (currlayer->algo->isEmpty()) {
      SaveCurrentSelection();
      currlayer->seltop    = newtop;
      currlayer->selbottom = newbottom;
      currlayer->selleft   = newleft;
      currlayer->selright  = newright;
      RememberNewSelection(_("Rotation"));
      DisplaySelectionSize();
      mainptr->UpdatePatternAndStatus();
      return true;
   }
   
   // if the current selection and the rotated selection are both outside the
   // pattern edges (ie. both are empty) then just rotate the selection edges
   bigint top, left, bottom, right;
   currlayer->algo->findedges(&top, &left, &bottom, &right);
   if ( (currlayer->seltop > bottom || currlayer->selbottom < top ||
         currlayer->selleft > right || currlayer->selright < left) &&
        (newtop > bottom || newbottom < top || newleft > right || newright < left) ) {
      SaveCurrentSelection();
      currlayer->seltop    = newtop;
      currlayer->selbottom = newbottom;
      currlayer->selleft   = newleft;
      currlayer->selright  = newright;
      RememberNewSelection(_("Rotation"));
      DisplaySelectionSize();
      mainptr->UpdatePatternAndStatus();
      return true;
   }

   // can only use nextcell/getcell/setcell in limited domain
   if ( OutsideLimits(currlayer->seltop, currlayer->selbottom,
                      currlayer->selleft, currlayer->selright) ) {
      statusptr->ErrorMessage(selection_too_big);
      return false;
   }

   // make sure rotated selection edges are also within limits
   if ( OutsideLimits(newtop, newbottom, newleft, newright) ) {
      statusptr->ErrorMessage(_("New selection would be outside +/- 10^9 boundary."));
      return false;
   }
   
   // use faster method if selection encloses entire pattern
   if ( currlayer->seltop <= top && currlayer->selbottom >= bottom &&
        currlayer->selleft <= left && currlayer->selright >= right ) {
      return RotatePattern(clockwise, newtop, newbottom, newleft, newright, inundoredo);
   }

   int itop    = currlayer->seltop.toint();
   int ileft   = currlayer->selleft.toint();
   int ibottom = currlayer->selbottom.toint();
   int iright  = currlayer->selright.toint();

   int ntop    = newtop.toint();
   int nleft   = newleft.toint();
   int nbottom = newbottom.toint();
   int nright  = newright.toint();
   
   // save cell changes if undo/redo is enabled and script isn't constructing a pattern
   // and we're not undoing/redoing an earlier rotation
   bool savecells = allowundo && !currlayer->stayclean && !inundoredo;
   if (savecells && inscript) SavePendingChanges();
   
   lifealgo* oldalgo = NULL;
   int otop = itop;
   int oleft = ileft;
   int obottom = ibottom;
   int oright = iright;

   if (savecells) {
      // copy current pattern to oldalgo using union of old and new selection rects
      if (otop > ntop) otop = ntop;
      if (oleft > nleft) oleft = nleft;
      if (obottom < nbottom) obottom = nbottom;
      if (oright < nright) oright = nright;
      oldalgo = CreateNewUniverse(false);
      if ( !CopyRect(otop, oleft, obottom, oright,
                     currlayer->algo, oldalgo, false, _("Saving part of pattern")) ) {
         delete oldalgo;
         return false;
      }
   }

   // create temporary universe; doesn't need to match current universe so
   // use qlife because its setcell/getcell calls are faster
   lifealgo* tempalgo = CreateNewUniverse(false);
   
   // copy (and kill) live cells in selection to temporary universe,
   // rotating the new coords by +/- 90 degrees
   if ( !RotateRect(clockwise, currlayer->algo, tempalgo, true,
                    itop, ileft, ibottom, iright,
                    ntop, nleft, nbottom, nright) ) {
      // user aborted rotation
      if (savecells) {
         // use oldalgo to restore erased selection
         CopyRect(itop, ileft, ibottom, iright,
                  oldalgo, currlayer->algo, false, _("Restoring selection"));
         delete oldalgo;
      } else {
         // restore erased selection by rotating tempalgo in opposite direction
         // back into the current universe
         RotateRect(!clockwise, tempalgo, currlayer->algo, false,
                    ntop, nleft, nbottom, nright,
                    itop, ileft, ibottom, iright);
      }
      delete tempalgo;
      mainptr->UpdatePatternAndStatus();
      return false;
   }
   
   // copy rotated selection from temporary universe to current universe;
   // check if new selection rect is outside modified pattern edges
   currlayer->algo->findedges(&top, &left, &bottom, &right);
   if ( newtop > bottom || newbottom < top || newleft > right || newright < left ) {
      // safe to use fast nextcell calls
      CopyRect(ntop, nleft, nbottom, nright,
               tempalgo, currlayer->algo, false, _("Adding rotated selection"));
   } else {
      // have to use slow getcell calls
      CopyAllRect(ntop, nleft, nbottom, nright,
                  tempalgo, currlayer->algo, _("Pasting rotated selection"));
   }
   // don't need temporary universe any more
   delete tempalgo;
   
   // rotate the selection edges
   currlayer->seltop    = newtop;
   currlayer->selbottom = newbottom;
   currlayer->selleft   = newleft;
   currlayer->selright  = newright;
   
   if (savecells) {
      // compare patterns in oldalgo and currlayer->algo and call SaveCellChange
      // for each cell that has a different state
      if ( SaveDifferences(oldalgo, currlayer->algo, otop, oleft, obottom, oright) ) {
         currlayer->undoredo->RememberRotation(clockwise,
                                               itop, ileft, ibottom, iright,
                                               ntop, nleft, nbottom, nright,
                                               currlayer->dirty);
      } else {
         currlayer->undoredo->ForgetCellChanges();
         Warning(_("You can't undo this change!"));
      }
      delete oldalgo;
   }

   // display results
   DisplaySelectionSize();
   if (!inundoredo) MarkLayerDirty();
   mainptr->UpdatePatternAndStatus();
   
   return true;
}

// -----------------------------------------------------------------------------

void PatternView::SetCursorMode(wxCursor* cursor)
{
   currlayer->curs = cursor;
}

// -----------------------------------------------------------------------------

void PatternView::CycleCursorMode()
{
   if (drawingcells || selectingcells || movingview || waitingforclick)
      return;

   if (currlayer->curs == curs_pencil)
      currlayer->curs = curs_cross;
   else if (currlayer->curs == curs_cross)
      currlayer->curs = curs_hand;
   else if (currlayer->curs == curs_hand)
      currlayer->curs = curs_zoomin;
   else if (currlayer->curs == curs_zoomin)
      currlayer->curs = curs_zoomout;
   else
      currlayer->curs = curs_pencil;
}
