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

#include "wx/filename.h"   // for wxFileName

#include "bigint.h"
#include "lifealgo.h"
#include "writepattern.h"  // for MC_format, XRLE_format

#include "wxgolly.h"       // for mainptr, viewptr
#include "wxmain.h"        // for mainptr->...
#include "wxview.h"        // for viewptr->...
#include "wxutils.h"       // for Warning, Fatal
#include "wxscript.h"      // for inscript
#include "wxlayer.h"       // for currlayer, MarkLayerDirty, etc
#include "wxprefs.h"       // for allowundo
#include "wxundo.h"

// -----------------------------------------------------------------------------

const wxString lack_of_memory = _("Due to lack of memory, some changes can't be undone!");
const wxString temp_prefix = _("golly_undo_");
const wxString to_gen = _("to Gen ");

// -----------------------------------------------------------------------------

// encapsulate change info stored in undo/redo lists

typedef enum {
   cellstates,          // one or more cell states were changed
   fliptb,              // selection was flipped top-bottom
   fliplr,              // selection was flipped left-right
   rotatecw,            // selection was rotated clockwise
   rotateacw,           // selection was rotated anticlockwise
   rotatepattcw,        // pattern was rotated clockwise
   rotatepattacw,       // pattern was rotated anticlockwise
   
   // WARNING: code in UndoChange/RedoChange assumes only changes < selchange
   // can alter the layer's dirty state; ie. the wasdirty flag is not used
   // for all the following changes
   
   selchange,           // selection was changed
   genchange,           // pattern was generated
   setgen,              // generation count was changed
   rulechange,          // rule was changed
   algochange,          // algorithm was changed (ie. hashing was toggled)
   scriptstart,         // later changes were made by script
   scriptfinish         // earlier changes were made by script
} change_type;

class ChangeNode: public wxObject {
public:
   ChangeNode(change_type id);
   ~ChangeNode();

   bool DoChange(bool undo);
   // do the undo/redo; if it returns false (eg. user has aborted a lengthy
   // rotate/flip operation) then cancel the undo/redo

   change_type changeid;                  // specifies the type of change
   wxString suffix;                       // action string for Undo/Redo item
   bool wasdirty;                         // layer's dirty state BEFORE change was made
   
   // cellstates info
   int* cellcoords;                       // array of x,y coordinates (2 ints per cell)
   unsigned int cellcount;                // number of cells in array
   
   // rotatecw/rotateacw info
   int oldt, oldl, oldb, oldr;            // selection edges before rotation
   int newt, newl, newb, newr;            // selection edges after rotation
   
   // selchange/genchange info
   bigint prevt, prevl, prevb, prevr;     // old selection edges
   bigint nextt, nextl, nextb, nextr;     // new selection edges
   
   // genchange info
   wxString oldfile, newfile;             // old and new pattern files
   bigint oldgen, newgen;                 // old and new generation counts
   bigint oldx, oldy, newx, newy;         // old and new positions
   int oldmag, newmag;                    // old and new scales
   int oldwarp, newwarp;                  // old and new speeds
   bool oldhash, newhash;                 // old and new hash states
   bool scriptgen;                        // gen change was done by script?
   // also uses oldrule, newrule

   // rulechange info
   wxString oldrule, newrule;             // old and new rules
   
   // setgen info
   bigint oldstart, newstart;             // old and new startgen values
   bool oldsave, newsave;                 // old and new savestart states
   // also uses oldgen, newgen
};

// -----------------------------------------------------------------------------

ChangeNode::ChangeNode(change_type id)
{
   changeid = id;
   cellcoords = NULL;
   oldfile = wxEmptyString;
   newfile = wxEmptyString;
}

// -----------------------------------------------------------------------------

ChangeNode::~ChangeNode()
{
   if (cellcoords) free(cellcoords);
   
   if (!oldfile.IsEmpty() && wxFileExists(oldfile)) wxRemoveFile(oldfile);
   if (!newfile.IsEmpty() && wxFileExists(newfile)) wxRemoveFile(newfile);
}

// -----------------------------------------------------------------------------

bool ChangeNode::DoChange(bool undo)
{
   switch (changeid) {
      case cellstates:
         // change state of cell(s) stored in cellcoords array
         {
            unsigned int i = 0;
            while (i < cellcount * 2) {
               int x = cellcoords[i++];
               int y = cellcoords[i++];
               int state = currlayer->algo->getcell(x, y);
               currlayer->algo->setcell(x, y, 1 - state);
            }
            currlayer->algo->endofpattern();
            mainptr->UpdatePatternAndStatus();
         }
         break;

      case fliptb:
      case fliplr:
         // safer not to call FlipSelection (it calls MarkLayerDirty)
         {
            bool done = true;
            int itop = currlayer->seltop.toint();
            int ileft = currlayer->selleft.toint();
            int ibottom = currlayer->selbottom.toint();
            int iright = currlayer->selright.toint();
            if (ibottom <= itop || iright <= ileft) {
               // should never happen
               Warning(_("Flip bug detected in DoChange!"));
               return false;
            } else if (changeid == fliptb) {
               done = viewptr->FlipTopBottom(itop, ileft, ibottom, iright);
            } else {
               done = viewptr->FlipLeftRight(itop, ileft, ibottom, iright);
            }
            mainptr->UpdatePatternAndStatus();
            if (!done) return false;
         }
         break;

      case rotatecw:
      case rotateacw:
         // change state of cell(s) stored in cellcoords array
         {
            unsigned int i = 0;
            while (i < cellcount * 2) {
               int x = cellcoords[i++];
               int y = cellcoords[i++];
               int state = currlayer->algo->getcell(x, y);
               currlayer->algo->setcell(x, y, 1 - state);
            }
            currlayer->algo->endofpattern();
         }
         // rotate selection edges
         if (undo) {
            currlayer->seltop = oldt;
            currlayer->selleft = oldl;
            currlayer->selbottom = oldb;
            currlayer->selright = oldr;
         } else {
            currlayer->seltop = newt;
            currlayer->selleft = newl;
            currlayer->selbottom = newb;
            currlayer->selright = newr;
         }
         viewptr->DisplaySelectionSize();
         mainptr->UpdatePatternAndStatus();
         break;

      case rotatepattcw:
      case rotatepattacw:
         // pass in true so RotateSelection won't save changes or call MarkLayerDirty
         if (!viewptr->RotateSelection(changeid == rotatepattcw ? !undo : undo, true))
            return false;
         break;
      
      case selchange:
         if (undo) {
            currlayer->seltop = prevt;
            currlayer->selleft = prevl;
            currlayer->selbottom = prevb;
            currlayer->selright = prevr;
         } else {
            currlayer->seltop = nextt;
            currlayer->selleft = nextl;
            currlayer->selbottom = nextb;
            currlayer->selright = nextr;
         }
         if (viewptr->SelectionExists()) viewptr->DisplaySelectionSize();
         mainptr->UpdatePatternAndStatus();
         break;
      
      case genchange:
         if (undo) {
            currlayer->seltop = prevt;
            currlayer->selleft = prevl;
            currlayer->selbottom = prevb;
            currlayer->selright = prevr;
            mainptr->RestorePattern(oldgen, oldfile, oldrule,
                                    oldx, oldy, oldmag, oldwarp, oldhash);
         } else {
            currlayer->seltop = nextt;
            currlayer->selleft = nextl;
            currlayer->selbottom = nextb;
            currlayer->selright = nextr;
            mainptr->RestorePattern(newgen, newfile, newrule,
                                    newx, newy, newmag, newwarp, newhash);
         }
         break;
      
      case setgen:
         if (undo) {
            mainptr->ChangeGenCount(oldgen.tostring(), true);
            currlayer->startgen = oldstart;
            currlayer->savestart = oldsave;
         } else {
            mainptr->ChangeGenCount(newgen.tostring(), true);
            currlayer->startgen = newstart;
            currlayer->savestart = newsave;
         }
         // Reset item may become enabled/disabled
         if (!inscript) mainptr->UpdateMenuItems(mainptr->IsActive());
         break;

      case rulechange:
         if (undo) {
            currlayer->algo->setrule( oldrule.mb_str(wxConvLocal) );
         } else {
            currlayer->algo->setrule( newrule.mb_str(wxConvLocal) );
         }
         // show new rule in window title (file name doesn't change)
         if (!inscript) mainptr->SetWindowTitle(wxEmptyString);
         break;

      case algochange:
         // temporarily reset allowundo so ToggleHashing won't call RememberAlgoChange
         allowundo = false;
         mainptr->ToggleHashing();
         allowundo = true;
         break;
      
      case scriptstart:
      case scriptfinish:
         // should never happen
         Warning(_("Bug detected in DoChange!"));
         break;
   }
   return true;
}

// -----------------------------------------------------------------------------

UndoRedo::UndoRedo()
{
   intcount = 0;                 // for 1st SaveCellChange
   maxcount = 0;                 // ditto
   badalloc = false;             // true if malloc/realloc fails
   cellarray = NULL;             // play safe
   savecellchanges = false;      // no script cell changes are pending
   savegenchanges = false;       // no script gen changes are pending
   doingscriptchanges = false;   // not undoing/redoing script changes
   prevfile = wxEmptyString;     // play safe for ClearUndoRedo
   startcount = 0;               // unfinished RememberGenStart calls
   
   // need to remember if script has created a new layer
   if (inscript) RememberScriptStart();
}

// -----------------------------------------------------------------------------

UndoRedo::~UndoRedo()
{
   ClearUndoRedo();
}

// -----------------------------------------------------------------------------

void UndoRedo::SaveCellChange(int x, int y)
{
   if (intcount == maxcount) {
      if (intcount == 0) {
         // initially allocate room for 1 cell (x and y coords)
         maxcount = 2;
         cellarray = (int*) malloc(maxcount * sizeof(int));
         if (cellarray == NULL) {
            badalloc = true;
            return;
         }
         // ChangeNode::~ChangeNode or UndoRedo::ForgetChanges will free cellarray
      } else {
         // double size of cellarray
         int* newptr = (int*) realloc(cellarray, maxcount * 2 * sizeof(int));
         if (newptr == NULL) {
            badalloc = true;
            return;
         }
         cellarray = newptr;
         maxcount *= 2;
      }
   }
   
   cellarray[intcount++] = x;
   cellarray[intcount++] = y;
}

// -----------------------------------------------------------------------------

void UndoRedo::ForgetChanges()
{
   if (intcount > 0) {
      if (cellarray) {
         free(cellarray);
      } else {
         Warning(_("Bug detected in ForgetChanges!"));
      }
      intcount = 0;        // reset for next SaveCellChange
      maxcount = 0;        // ditto
      badalloc = false;
   }
}

// -----------------------------------------------------------------------------

bool UndoRedo::RememberChanges(const wxString& action, bool wasdirty)
{
   if (intcount > 0) {
      if (intcount < maxcount) {
         // reduce size of cellarray
         int* newptr = (int*) realloc(cellarray, intcount * sizeof(int));
         if (newptr != NULL) cellarray = newptr;
         // in the unlikely event that newptr is NULL, cellarray should
         // still point to valid data
      }
      
      // clear the redo history
      WX_CLEAR_LIST(wxList, redolist);
      UpdateRedoItem(wxEmptyString);
      
      // add cellstates node to head of undo list
      ChangeNode* change = new ChangeNode(cellstates);
      if (change == NULL) Fatal(_("Failed to create cellstates node!"));
      
      change->cellcoords = cellarray;
      change->cellcount = intcount / 2;
      change->suffix = action;
      change->wasdirty = wasdirty;
      
      undolist.Insert(change);
      
      // update Undo item in Edit menu
      UpdateUndoItem(change->suffix);
      
      intcount = 0;        // reset for next SaveCellChange
      maxcount = 0;        // ditto

      if (badalloc) {
         Warning(lack_of_memory);
         badalloc = false;
      }
      return true;   // at least one cell changed state
   }
   return false;     // no cells changed state (SaveCellChange wasn't called)
}

// -----------------------------------------------------------------------------

void UndoRedo::RememberFlip(bool topbot, bool wasdirty)
{
   // clear the redo history
   WX_CLEAR_LIST(wxList, redolist);
   UpdateRedoItem(wxEmptyString);
   
   // add fliptb/fliplr node to head of undo list
   ChangeNode* change = new ChangeNode(topbot ? fliptb : fliplr);
   if (change == NULL) Fatal(_("Failed to create flip node!"));

   change->suffix = _("Flip");
   change->wasdirty = wasdirty;

   undolist.Insert(change);
   
   // update Undo item in Edit menu
   UpdateUndoItem(change->suffix);
}

// -----------------------------------------------------------------------------

void UndoRedo::RememberRotation(bool clockwise, bool wasdirty)
{
   // clear the redo history
   WX_CLEAR_LIST(wxList, redolist);
   UpdateRedoItem(wxEmptyString);
   
   // add rotatepattcw/rotatepattacw node to head of undo list
   ChangeNode* change = new ChangeNode(clockwise ? rotatepattcw : rotatepattacw);
   if (change == NULL) Fatal(_("Failed to create simple rotation node!"));

   change->suffix = _("Rotation");
   change->wasdirty = wasdirty;

   undolist.Insert(change);
   
   // update Undo item in Edit menu
   UpdateUndoItem(change->suffix);
}

// -----------------------------------------------------------------------------

void UndoRedo::RememberRotation(bool clockwise,
                                int oldt, int oldl, int oldb, int oldr,
                                int newt, int newl, int newb, int newr,
                                bool wasdirty)
{
   // clear the redo history
   WX_CLEAR_LIST(wxList, redolist);
   UpdateRedoItem(wxEmptyString);
   
   // add rotatecw/rotateacw node to head of undo list
   ChangeNode* change = new ChangeNode(clockwise ? rotatecw : rotateacw);
   if (change == NULL) Fatal(_("Failed to create rotation node!"));

   change->oldt = oldt;
   change->oldl = oldl;
   change->oldb = oldb;
   change->oldr = oldr;
   change->newt = newt;
   change->newl = newl;
   change->newb = newb;
   change->newr = newr;
   change->suffix = _("Rotation");
   change->wasdirty = wasdirty;

   // if intcount == 0 we still need to rotate selection edges
   if (intcount > 0) {
      if (intcount < maxcount) {
         // reduce size of cellarray
         int* newptr = (int*) realloc(cellarray, intcount * sizeof(int));
         if (newptr != NULL) cellarray = newptr;
      }

      change->cellcoords = cellarray;
      change->cellcount = intcount / 2;
      
      intcount = 0;        // reset for next SaveCellChange
      maxcount = 0;        // ditto
      if (badalloc) {
         Warning(lack_of_memory);
         badalloc = false;
      }
   }

   undolist.Insert(change);
   
   // update Undo item in Edit menu
   UpdateUndoItem(change->suffix);
}

// -----------------------------------------------------------------------------

void UndoRedo::RememberSelection(const wxString& action)
{
   if (currlayer->savetop == currlayer->seltop &&
       currlayer->saveleft == currlayer->selleft &&
       currlayer->savebottom == currlayer->selbottom &&
       currlayer->saveright == currlayer->selright) {
      // selection has not changed
      return;
   }
   
   if (mainptr->generating) {
      // don't record selection changes while a pattern is generating;
      // RememberGenStart and RememberGenFinish will remember the overall change
      return;
   }
   
   // clear the redo history
   WX_CLEAR_LIST(wxList, redolist);
   UpdateRedoItem(wxEmptyString);
   
   // add selchange node to head of undo list
   ChangeNode* change = new ChangeNode(selchange);
   if (change == NULL) Fatal(_("Failed to create selchange node!"));

   change->prevt = currlayer->savetop;
   change->prevl = currlayer->saveleft;
   change->prevb = currlayer->savebottom;
   change->prevr = currlayer->saveright;
   change->nextt = currlayer->seltop;
   change->nextl = currlayer->selleft;
   change->nextb = currlayer->selbottom;
   change->nextr = currlayer->selright;
   if (currlayer->seltop <= currlayer->selbottom) {
      change->suffix = action;
   } else {
      change->suffix = _("Deselection");
   }
   // change->wasdirty is not used for selection changes

   undolist.Insert(change);
   
   // update Undo item in Edit menu
   UpdateUndoItem(change->suffix);
}

// -----------------------------------------------------------------------------

void UndoRedo::SaveCurrentPattern(const wxString& tempfile)
{
   const char* err = NULL;
   if ( currlayer->hash ) {
      // save hlife pattern in a macrocell file
      err = mainptr->WritePattern(tempfile, MC_format, 0, 0, 0, 0);
   } else {
      // can only save RLE file if edges are within getcell/setcell limits
      bigint top, left, bottom, right;
      currlayer->algo->findedges(&top, &left, &bottom, &right);      
      if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
         err = "Pattern is too big to save.";
      } else {
         // use XRLE format so the pattern's top left location and the current
         // generation count are stored in the file
         err = mainptr->WritePattern(tempfile, XRLE_format,
                                     top.toint(), left.toint(),
                                     bottom.toint(), right.toint());
      }
   }
   if (err) Warning(wxString(err,wxConvLocal));
}

// -----------------------------------------------------------------------------

void UndoRedo::RememberGenStart()
{
   startcount++;
   if (startcount > 1) {
      // return immediately and ignore next RememberGenFinish call;
      // this can happen in wxGTK app if user holds down space bar
      return;
   }

   if (inscript) {
      if (savegenchanges) return;   // ignore consecutive run/step command
      savegenchanges = true;
      // we're about to do first run/step command of a (possibly long)
      // sequence, so save starting info
   }

   // save current generation, selection, position, scale, speed, etc
   prevgen = currlayer->algo->getGeneration();
   prevrule = wxString(currlayer->algo->getrule(), wxConvLocal);
   prevt = currlayer->seltop;
   prevl = currlayer->selleft;
   prevb = currlayer->selbottom;
   prevr = currlayer->selright;
   viewptr->GetPos(prevx, prevy);
   prevmag = viewptr->GetMag();
   prevwarp = currlayer->warp;
   prevhash = currlayer->hash;

   if (prevgen == currlayer->startgen) {
      // we can just reset to starting pattern
      prevfile = wxEmptyString;
   } else {
      // save starting pattern in a unique temporary file;
      // on my Mac the file is in /private/var/tmp/folders.502/TemporaryItems,
      // on WinXP the file is in C:\Documents and Settings\Andy\Local Settings\Temp
      // (or shorter equivalent C:\DOCUME~1\Andy\LOCALS~1\Temp) but the file name
      // is gol*.tmp (ie. temp_prefix is ignored),
      // on Linux the file is in /tmp
      prevfile = wxFileName::CreateTempFileName(temp_prefix);

      // if head of undo list is a genchange node with same rule and hash state
      // then we can copy that change node's newfile to prevfile; this makes
      // consecutive generating runs faster (setting prevfile to newfile would
      // be even faster but it's difficult to avoid the file being deleted
      // if the redo list is cleared)
      if (!undolist.IsEmpty()) {
         wxList::compatibility_iterator node = undolist.GetFirst();
         ChangeNode* change = (ChangeNode*) node->GetData();
         if (change->changeid == genchange &&
             change->newrule == prevrule && change->newhash == prevhash) {
            if (wxCopyFile(change->newfile, prevfile, true)) {
               return;
            } else {
               Warning(_("Failed to copy temporary file!"));
               // continue and call SaveCurrentPattern
            }
         }
      }
      
      SaveCurrentPattern(prevfile);
   }
}

// -----------------------------------------------------------------------------

void UndoRedo::RememberGenFinish()
{
   startcount--;
   if (startcount > 0) return;

   if (inscript && savegenchanges) return;   // ignore consecutive run/step command

   // generation count might not have changed (can happen in wxGTK)
   if (prevgen == currlayer->algo->getGeneration()) {
      // delete prevfile created by RememberGenStart
      if (!prevfile.IsEmpty() && wxFileExists(prevfile)) wxRemoveFile(prevfile);
      prevfile = wxEmptyString;
      return;
   }
   
   wxString fpath;
   if (currlayer->algo->getGeneration() == currlayer->startgen) {
      // this can happen if script called reset() so just use starting pattern
      fpath = wxEmptyString;
   } else {
      // save finishing pattern in a unique temporary file
      fpath = wxFileName::CreateTempFileName(temp_prefix);
      SaveCurrentPattern(fpath);
   }
   
   // clear the redo history
   WX_CLEAR_LIST(wxList, redolist);
   UpdateRedoItem(wxEmptyString);
   
   // add genchange node to head of undo list
   ChangeNode* change = new ChangeNode(genchange);
   if (change == NULL) Fatal(_("Failed to create genchange node!"));

   change->scriptgen = inscript;
   change->oldgen = prevgen;
   change->newgen = currlayer->algo->getGeneration();
   change->oldfile = prevfile;
   change->newfile = fpath;
   change->oldrule = prevrule;
   change->newrule = wxString(currlayer->algo->getrule(), wxConvLocal);
   change->oldx = prevx;
   change->oldy = prevy;
   viewptr->GetPos(change->newx, change->newy);
   change->oldmag = prevmag;
   change->newmag = viewptr->GetMag();
   change->oldwarp = prevwarp;
   change->newwarp = currlayer->warp;
   change->oldhash = prevhash;
   change->newhash = currlayer->hash;
   change->prevt = prevt;
   change->prevl = prevl;
   change->prevb = prevb;
   change->prevr = prevr;
   change->nextt = currlayer->seltop;
   change->nextl = currlayer->selleft;
   change->nextb = currlayer->selbottom;
   change->nextr = currlayer->selright;
   change->suffix = to_gen + wxString(prevgen.tostring(), wxConvLocal);
   // change->wasdirty is not used for generation changes

   // prevfile has been saved in change->oldfile (~ChangeNode will delete it)
   prevfile = wxEmptyString;

   undolist.Insert(change);
   
   // update Undo item in Edit menu
   UpdateUndoItem(change->suffix);
}

// -----------------------------------------------------------------------------

void UndoRedo::AddGenChange()
{
   // add a genchange node to empty undo list
   if (!undolist.IsEmpty())
      Warning(_("AddGenChange bug: undo list NOT empty!"));
   
   // use starting pattern info for previous state
   prevgen = currlayer->startgen;
   prevrule = currlayer->startrule;
   prevt = currlayer->starttop;
   prevl = currlayer->startleft;
   prevb = currlayer->startbottom;
   prevr = currlayer->startright;
   prevx = currlayer->startx;
   prevy = currlayer->starty;
   prevmag = currlayer->startmag;
   prevwarp = currlayer->startwarp;
   prevhash = currlayer->starthash;
   prevfile = wxEmptyString;
   
   // avoid RememberGenFinish returning early if inscript is true
   savegenchanges = false;
   RememberGenFinish();

   if (undolist.IsEmpty())
      Warning(_("AddGenChange bug: undo list is empty!"));
}

// -----------------------------------------------------------------------------

void UndoRedo::SyncUndoHistory()
{
   // synchronize undo history due to a ResetPattern call;
   // wind back the undo list to just past the oldest genchange node
   wxList::compatibility_iterator node;
   ChangeNode* change;
   while (!undolist.IsEmpty()) {
      node = undolist.GetFirst();
      change = (ChangeNode*) node->GetData();

      // remove node from head of undo list and append it to redo list
      undolist.Erase(node);
      redolist.Insert(change);

      if (change->changeid == genchange && change->oldgen == currlayer->startgen) {
         if (change->scriptgen) {
            // gen change was done by a script so keep winding back the undo list
            // to just past the scriptstart node, or until the list is empty
            while (!undolist.IsEmpty()) {
               node = undolist.GetFirst();
               change = (ChangeNode*) node->GetData();
               undolist.Erase(node);
               redolist.Insert(change);
               if (change->changeid == scriptstart) break;
            }
         }
         // update Undo/Redo items so they show the correct suffix
         UpdateUndoRedoItems();
         return;
      }
   }
   // should never get here
   Warning(_("Bug detected in SyncUndoHistory!"));
}

// -----------------------------------------------------------------------------

void UndoRedo::RememberSetGen(bigint& oldgen, bigint& newgen,
                              bigint& oldstart, bool oldsave)
{
   // clear the redo history
   WX_CLEAR_LIST(wxList, redolist);
   UpdateRedoItem(wxEmptyString);
   
   // add setgen node to head of undo list
   ChangeNode* change = new ChangeNode(setgen);
   if (change == NULL) Fatal(_("Failed to create setgen node!"));

   change->oldgen = oldgen;
   change->newgen = newgen;
   change->oldstart = oldstart;
   change->newstart = currlayer->startgen;
   change->oldsave = oldsave;
   change->newsave = currlayer->savestart;
   change->suffix = _("Set Generation");
   // change->wasdirty is not used
   
   undolist.Insert(change);
   
   // update Undo item in Edit menu
   UpdateUndoItem(change->suffix);
}

// -----------------------------------------------------------------------------

void UndoRedo::RememberRuleChange(const wxString& oldrule)
{
   if (inscript) SavePendingChanges();
   
   wxString newrule = wxString(currlayer->algo->getrule(), wxConvLocal);
   if (oldrule == newrule) return;
   
   // clear the redo history
   WX_CLEAR_LIST(wxList, redolist);
   UpdateRedoItem(wxEmptyString);
   
   // add rulechange node to head of undo list
   ChangeNode* change = new ChangeNode(rulechange);
   if (change == NULL) Fatal(_("Failed to create rulechange node!"));

   change->oldrule = oldrule;
   change->newrule = newrule;
   change->suffix = _("Rule Change");
   // change->wasdirty is not used
   
   undolist.Insert(change);
   
   // update Undo item in Edit menu
   UpdateUndoItem(change->suffix);
}

// -----------------------------------------------------------------------------

void UndoRedo::RememberAlgoChange()
{
   if (inscript) SavePendingChanges();
   
   // clear the redo history
   WX_CLEAR_LIST(wxList, redolist);
   UpdateRedoItem(wxEmptyString);
   
   // add algochange node to head of undo list
   ChangeNode* change = new ChangeNode(algochange);
   if (change == NULL) Fatal(_("Failed to create algochange node!"));

   change->suffix = _("Hashing Change");
   // change->wasdirty is not used
   
   undolist.Insert(change);
   
   // update Undo item in Edit menu
   UpdateUndoItem(change->suffix);
}

// -----------------------------------------------------------------------------

void UndoRedo::RememberScriptStart()
{
   // add scriptstart node to head of undo list
   ChangeNode* change = new ChangeNode(scriptstart);
   if (change == NULL) Fatal(_("Failed to create scriptstart node!"));

   change->suffix = _("Script Changes");
   // change->wasdirty is not used

   undolist.Insert(change);
   
   // don't alter Undo item in Edit menu
   // UpdateUndoItem(change->suffix);
}

// -----------------------------------------------------------------------------

void UndoRedo::RememberScriptFinish()
{
   if (undolist.IsEmpty()) {
      // there should be at least a scriptstart node (see ClearUndoRedo)
      Warning(_("Bug detected in RememberScriptFinish!"));
      return;
   }
   
   // if head of undo list is a scriptstart node then simply remove it
   // and return (ie. the script didn't make any changes)
   wxList::compatibility_iterator node = undolist.GetFirst();
   ChangeNode* change = (ChangeNode*) node->GetData();
   if (change->changeid == scriptstart) {
      undolist.Erase(node);
      delete change;
      return;
   }

   // add scriptfinish node to head of undo list
   change = new ChangeNode(scriptfinish);
   if (change == NULL) Fatal(_("Failed to create scriptfinish node!"));

   change->suffix = _("Script Changes");
   // change->wasdirty is not used

   undolist.Insert(change);
   
   // update Undo item in Edit menu
   UpdateUndoItem(change->suffix);
}

// -----------------------------------------------------------------------------

bool UndoRedo::CanUndo()
{
   return !undolist.IsEmpty() && !inscript && !mainptr->generating &&
          !viewptr->waitingforclick && !viewptr->drawingcells &&
          !viewptr->selectingcells;
}

// -----------------------------------------------------------------------------

bool UndoRedo::CanRedo()
{
   return !redolist.IsEmpty() && !inscript && !mainptr->generating &&
          !viewptr->waitingforclick && !viewptr->drawingcells &&
          !viewptr->selectingcells;
}

// -----------------------------------------------------------------------------

void UndoRedo::UndoChange()
{
   if (!CanUndo()) return;
   
   // get change info from head of undo list and do the change
   wxList::compatibility_iterator node = undolist.GetFirst();
   ChangeNode* change = (ChangeNode*) node->GetData();

   if (change->changeid == scriptfinish) {
      // undo all changes between scriptfinish and scriptstart nodes;
      // first remove scriptfinish node from undo list and add it to redo list
      undolist.Erase(node);
      redolist.Insert(change);

      while (change->changeid != scriptstart) {
         // call UndoChange recursively; temporarily set doingscriptchanges so
         // 1) UndoChange won't return if DoChange is aborted
         // 2) user won't see any intermediate pattern/status updates
         // 3) Undo/Redo items won't be updated
         doingscriptchanges = true;
         UndoChange();
         doingscriptchanges = false;
         node = undolist.GetFirst();
         if (node == NULL) Fatal(_("Bug in UndoChange!"));
         change = (ChangeNode*) node->GetData();
      }
      mainptr->UpdatePatternAndStatus();
      // continue below so that scriptstart node is removed from undo list
      // and added to redo list

   } else {
      // user might abort the undo (eg. a lengthy rotate/flip)
      if (!change->DoChange(true) && !doingscriptchanges) return;
   }
   
   // remove node from head of undo list (doesn't delete node's data)
   undolist.Erase(node);
   
   if (change->changeid < selchange && change->wasdirty != currlayer->dirty) {
      if (currlayer->dirty) {
         // clear dirty flag, update window title and Layer menu items
         MarkLayerClean(currlayer->currname);
         if (currlayer->algo->getGeneration() == currlayer->startgen) {
            // restore savestart flag to correct state
            if (currlayer->algo->isEmpty())
               currlayer->savestart = false;
            else if (!currlayer->currfile.IsEmpty())
               currlayer->savestart = false;
            else
               currlayer->savestart = true;     // pattern created by script
         }
      } else {
         // this can happen if user creates a new pattern, does some editing,
         // saves pattern (which calls MarkLayerClean), then does Undo
         MarkLayerDirty();
      }
   }

   // add change to head of redo list
   redolist.Insert(change);
   
   // update Undo/Redo items in Edit menu
   UpdateUndoRedoItems();
}

// -----------------------------------------------------------------------------

void UndoRedo::RedoChange()
{
   if (!CanRedo()) return;
   
   // get change info from head of redo list and do the change
   wxList::compatibility_iterator node = redolist.GetFirst();
   ChangeNode* change = (ChangeNode*) node->GetData();
   
   if (change->changeid == scriptstart) {
      // redo all changes between scriptstart and scriptfinish nodes;
      // first remove scriptstart node from redo list and add it to undo list
      redolist.Erase(node);
      undolist.Insert(change);

      while (change->changeid != scriptfinish) {
         // call RedoChange recursively; temporarily set doingscriptchanges so
         // 1) RedoChange won't return if DoChange is aborted
         // 2) user won't see any intermediate pattern/status updates
         // 3) Undo/Redo items won't be updated
         doingscriptchanges = true;
         RedoChange();
         doingscriptchanges = false;
         node = redolist.GetFirst();
         if (node == NULL) Fatal(_("Bug in RedoChange!"));
         change = (ChangeNode*) node->GetData();
      }
      mainptr->UpdatePatternAndStatus();
      // continue below so that scriptfinish node is removed from redo list
      // and added to undo list

   } else {
      // user might abort the redo (eg. a lengthy rotate/flip)
      if (!change->DoChange(false) && !doingscriptchanges) return;
   }
   
   // remove node from head of redo list (doesn't delete node's data)
   redolist.Erase(node);
   
   if (change->changeid < selchange && !change->wasdirty) {
      // set dirty flag, update window title and Layer menu items
      MarkLayerDirty();
   }

   // add change to head of undo list
   undolist.Insert(change);
   
   // update Undo/Redo items in Edit menu
   UpdateUndoRedoItems();
}

// -----------------------------------------------------------------------------

void UndoRedo::UpdateUndoRedoItems()
{
   if (inscript) return;   // update Undo/Redo items at end of script
   
   if (doingscriptchanges) return;

   if (undolist.IsEmpty()) {
      UpdateUndoItem(wxEmptyString);
   } else {
      wxList::compatibility_iterator node = undolist.GetFirst();
      ChangeNode* change = (ChangeNode*) node->GetData();
      if (change->changeid == genchange) {
         change->suffix = to_gen + wxString(change->oldgen.tostring(), wxConvLocal);
      }
      UpdateUndoItem(change->suffix);
   }

   if (redolist.IsEmpty()) {
      UpdateRedoItem(wxEmptyString);
   } else {
      wxList::compatibility_iterator node = redolist.GetFirst();
      ChangeNode* change = (ChangeNode*) node->GetData();
      if (change->changeid == genchange) {
         change->suffix = to_gen + wxString(change->newgen.tostring(), wxConvLocal);
      }
      UpdateRedoItem(change->suffix);
   }
}

// -----------------------------------------------------------------------------

void UndoRedo::UpdateUndoItem(const wxString& action)
{
   if (inscript) return;   // update Undo/Redo items at end of script

   wxMenuBar* mbar = mainptr->GetMenuBar();
   if (mbar) {
      mbar->SetLabel(wxID_UNDO,
                     wxString::Format(_("Undo %s\tCtrl+Z"), action.c_str()));
   }
}

// -----------------------------------------------------------------------------

void UndoRedo::UpdateRedoItem(const wxString& action)
{
   if (inscript) return;   // update Undo/Redo items at end of script

   wxMenuBar* mbar = mainptr->GetMenuBar();
   if (mbar) {
      mbar->SetLabel(wxID_REDO,
                     wxString::Format(_("Redo %s\tShift+Ctrl+Z"), action.c_str()));
   }
}

// -----------------------------------------------------------------------------

void UndoRedo::ClearUndoRedo()
{
   // free cellarray in case there were SaveCellChange calls not followed
   // by ForgetChanges or RememberChanges
   ForgetChanges();
   
   if (startcount > 0) {
      // RememberGenStart was not followed by RememberGenFinish
      if (!prevfile.IsEmpty() && wxFileExists(prevfile)) wxRemoveFile(prevfile);
      prevfile = wxEmptyString;
      startcount = 0;
   }

   // clear the undo/redo lists (and delete each node's data)
   WX_CLEAR_LIST(wxList, undolist);
   WX_CLEAR_LIST(wxList, redolist);
   
   if (inscript) {
      // script has called a command like new() so add a scriptstart node
      // to the undo list to match the final scriptfinish node
      RememberScriptStart();
      // reset flags to indicate no pending cell/gen changes
      savecellchanges = false;
      savegenchanges = false;
   } else {
      UpdateUndoItem(wxEmptyString);
      UpdateRedoItem(wxEmptyString);
   }
}
