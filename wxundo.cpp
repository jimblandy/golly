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

#include "bigint.h"
#include "lifealgo.h"

#include "wxgolly.h"       // for mainptr, viewptr
#include "wxmain.h"        // for mainptr->...
#include "wxview.h"        // for viewptr->...
#include "wxutils.h"       // for Warning, Fatal
#include "wxlayer.h"       // for currlayer, MarkLayerDirty, etc
#include "wxundo.h"

// -----------------------------------------------------------------------------

// encapsulate change info stored in undo/redo lists

typedef enum {
   cellstates,       // toggle cell states
   fliptb,           // flip rect top-bottom
   fliplr,           // flip rect left-right
   selchange         // selection change
} change_type;

class ChangeNode: public wxObject {
public:
   ChangeNode(int* cells, unsigned int numcells, const wxString& action);
   ChangeNode(bool topbot, int t, int l, int b, int r);
   ChangeNode(bigint& oldt, bigint& oldl, bigint& oldb, bigint& oldr,
              bigint& newt, bigint& newl, bigint& newb, bigint& newr);
   ~ChangeNode();

   void DoChange(bool undo);     // do the undo/redo

   change_type changeid;         // specifies the type of change
   wxString suffix;              // string to append to Undo/Redo item
   bool wasdirty;                // dirty state BEFORE change was made

private:
   int* cellcoords;                       // array of x,y coordinates (2 ints per cell)
   unsigned int cellcount;                // number of cells in array
   int top, left, bottom, right;          // rectangle edges for flip/rotate/etc
   bigint prevt, prevl, prevb, prevr;     // old selection edges
   bigint nextt, nextl, nextb, nextr;     // new selection edges
};

// -----------------------------------------------------------------------------

ChangeNode::ChangeNode(int* cells, unsigned int numcells, const wxString& action)
{
   changeid = cellstates;
   cellcoords = cells;
   if (cellcoords == NULL) Warning(_("Bug in ChangeNode!"));
   cellcount = numcells;
   suffix = action;
}

// -----------------------------------------------------------------------------

ChangeNode::ChangeNode(bool topbot, int t, int l, int b, int r)
{
   changeid = topbot ? fliptb : fliplr;
   top = t;
   left = l;
   bottom = b;
   right = r;
   suffix = _("Flip");
}

// -----------------------------------------------------------------------------

ChangeNode::ChangeNode(bigint& oldt, bigint& oldl, bigint& oldb, bigint& oldr,
                       bigint& newt, bigint& newl, bigint& newb, bigint& newr)
{
   changeid = selchange;
   prevt = oldt;
   prevl = oldl;
   prevb = oldb;
   prevr = oldr;
   nextt = newt;
   nextl = newl;
   nextb = newb;
   nextr = newr;
   if (newt <= newb)
      suffix = _("Selection");
   else
      suffix = _("Deselection");
}

// -----------------------------------------------------------------------------

ChangeNode::~ChangeNode()
{
   if (changeid == cellstates) free(cellcoords);
}

// -----------------------------------------------------------------------------

void ChangeNode::DoChange(bool undo)
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
         viewptr->FlipTopBottom(top, left, bottom, right);
         mainptr->UpdatePatternAndStatus();
         break;

      case fliplr:
         viewptr->FlipLeftRight(top, left, bottom, right);
         mainptr->UpdatePatternAndStatus();
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
         mainptr->UpdatePatternAndStatus();
         break;
   }
}

// -----------------------------------------------------------------------------

UndoRedo::UndoRedo()
{
   intcount = 0;        // for 1st SaveCellChange
   maxcount = 0;        // ditto
   badalloc = false;    // true if malloc/realloc fails
   cellarray = NULL;    // play safe
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
         // note that either ~ChangeNode or ForgetChanges will free cellarray
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
      free(cellarray);
      intcount = 0;        // reset for next SaveCellChange
      maxcount = 0;        // ditto
      badalloc = false;
   }
}

// -----------------------------------------------------------------------------

void UndoRedo::RememberChanges(const wxString& action, bool wasdirty)
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
      
      // add new change node to head of undo list
      ChangeNode* change = new ChangeNode(cellarray, intcount/2, action);
      if (change == NULL) Fatal(_("Failed to create new change node!"));
      change->wasdirty = wasdirty;
      undolist.Insert(change);
      
      // update Undo item in Edit menu
      UpdateUndoItem(change->suffix);
      
      intcount = 0;        // reset for next SaveCellChange
      maxcount = 0;        // ditto

      if (badalloc) {
         Warning(_("Due to lack of memory, some changes can't be undone!"));
         badalloc = false;
      }
   }
}

// -----------------------------------------------------------------------------

void UndoRedo::RememberFlip(bool topbot, int t, int l, int b, int r, bool wasdirty)
{
   // clear the redo history
   WX_CLEAR_LIST(wxList, redolist);
   UpdateRedoItem(wxEmptyString);
      
   // add new change node to head of undo list
   ChangeNode* change = new ChangeNode(topbot, t, l, b, r);
   if (change == NULL) Fatal(_("Failed to create new flip node!"));
   change->wasdirty = wasdirty;
   undolist.Insert(change);
   
   // update Undo item in Edit menu
   UpdateUndoItem(change->suffix);
}

// -----------------------------------------------------------------------------

void UndoRedo::RememberSelection(bigint& oldt, bigint& oldl, bigint& oldb, bigint& oldr,
                                 bigint& newt, bigint& newl, bigint& newb, bigint& newr)
{
   if (oldt == newt && oldl == newl && oldb == newb && oldr == newr) {
      // selection has not changed
      return;
   }
   
   // clear the redo history
   WX_CLEAR_LIST(wxList, redolist);
   UpdateRedoItem(wxEmptyString);
      
   // add new change node to head of undo list
   ChangeNode* change = new ChangeNode(oldt, oldl, oldb, oldr,
                                       newt, newl, newb, newr);
   if (change == NULL) Fatal(_("Failed to create new selection node!"));
   // change->wasdirty is not used for selection changes
   undolist.Insert(change);
   
   // update Undo item in Edit menu
   UpdateUndoItem(change->suffix);
}

// -----------------------------------------------------------------------------

bool UndoRedo::CanUndo()
{
   return !undolist.IsEmpty();
}

// -----------------------------------------------------------------------------

bool UndoRedo::CanRedo()
{
   return !redolist.IsEmpty();
}

// -----------------------------------------------------------------------------

void UndoRedo::UndoChange()
{
   if (undolist.IsEmpty()) return;
   
   // get change info from head of undo list and do the change
   wxList::compatibility_iterator node = undolist.GetFirst();
   ChangeNode* change = (ChangeNode*) node->GetData();
   change->DoChange(true);
   
   // remove node from head of undo list (doesn't delete node's data)
   undolist.Erase(node);
   
   if (change->changeid != selchange && change->wasdirty != currlayer->dirty) {
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
         // should never happen???
         Warning(_("Bug? UndoChange is setting dirty flag!"));
         MarkLayerDirty();
      }
   }

   // add change to head of redo list
   redolist.Insert(change);
   
   // update Undo/Redo items in Edit menu
   UpdateRedoItem(change->suffix);
   if (undolist.IsEmpty()) {
      UpdateUndoItem(wxEmptyString);
   } else {
      node = undolist.GetFirst();
      change = (ChangeNode*) node->GetData();
      UpdateUndoItem(change->suffix);
   }
}

// -----------------------------------------------------------------------------

void UndoRedo::RedoChange()
{
   if (redolist.IsEmpty()) return;
   
   // get change info from head of redo list and do the change
   wxList::compatibility_iterator node = redolist.GetFirst();
   ChangeNode* change = (ChangeNode*) node->GetData();
   change->DoChange(false);
   
   // remove node from head of redo list (doesn't delete node's data)
   redolist.Erase(node);
   
   if (change->changeid != selchange && !change->wasdirty) {
      // set dirty flag, update window title and Layer menu items
      MarkLayerDirty();
   }

   // add change to head of undo list
   undolist.Insert(change);
   
   // update Undo/Redo items in Edit menu
   UpdateUndoItem(change->suffix);
   if (redolist.IsEmpty()) {
      UpdateRedoItem(wxEmptyString);
   } else {
      node = redolist.GetFirst();
      change = (ChangeNode*) node->GetData();
      UpdateRedoItem(change->suffix);
   }
}

// -----------------------------------------------------------------------------

void UndoRedo::ClearUndoRedo()
{
   // free cellarray in case there were SaveCellChange calls not followed
   // by ForgetChanges or RememberChanges
   ForgetChanges();

   // clear the undo/redo lists (and delete each node's data)
   WX_CLEAR_LIST(wxList, undolist);
   WX_CLEAR_LIST(wxList, redolist);
   
   UpdateUndoItem(wxEmptyString);
   UpdateRedoItem(wxEmptyString);
}

// -----------------------------------------------------------------------------

void UndoRedo::UpdateUndoItem(const wxString& action)
{
   wxMenuBar* mbar = mainptr->GetMenuBar();
   if (mbar) mbar->SetLabel(wxID_UNDO, _("Undo ") + action);
}

// -----------------------------------------------------------------------------

void UndoRedo::UpdateRedoItem(const wxString& action)
{
   wxMenuBar* mbar = mainptr->GetMenuBar();
   if (mbar) mbar->SetLabel(wxID_REDO, _("Redo ") + action);
}
