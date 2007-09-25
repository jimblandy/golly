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
#ifndef _WXUNDO_H_
#define _WXUNDO_H_

// This class implements unlimited undo/redo:

class UndoRedo {
public:
   UndoRedo();
   ~UndoRedo();
   
   void SaveCellChange(int x, int y);
   // cell at x,y has changed state
   
   void ForgetChanges();
   // ignore changes made by previous SaveCellChange calls
   
   bool RememberChanges(const wxString& action, bool wasdirty);
   // remember changes made by previous SaveCellChange calls
   // and the state of the layer's dirty flag BEFORE the change;
   // the given action string will be appended to the Undo/Redo items;
   // return true if one or more cells changed state, false otherwise
   
   void RememberFlip(bool topbot, bool wasdirty);
   // remember flip's direction

   void RememberRotation(bool clockwise, bool wasdirty);
   // remember simple rotation (selection includes entire pattern)
   
   void RememberRotation(bool clockwise,
                         int oldt, int oldl, int oldb, int oldr,
                         int newt, int newl, int newb, int newr,
                         bool wasdirty);
   // remember rotation's direction and old and new selection edges;
   // this variant assumes SaveCellChange may have been called
   
   void RememberSelection(const wxString& action);
   // remember change in selection (no-op if selection hasn't changed)

   void RememberScriptStart();
   // remember that script is about to start; this allows us to undo/redo
   // any script changes all at once

   void RememberScriptFinish();
   // remember that script has ended
   
   bool savechanges;             // script cell changes need to be remembered?
   bool doingscriptchanges;      // are script changes being undone/redone?

   bool CanUndo();               // can a change be undone?
   bool CanRedo();               // can an undone change be redone?
   
   void UndoChange();            // undo a change
   void RedoChange();            // redo an undone change

   void UpdateUndoRedoItems();   // update Undo/Redo items in Edit menu
   void ClearUndoRedo();         // clear all undo/redo history

private:
   wxList undolist;              // list of undoable changes
   wxList redolist;              // list of redoable changes

   int* cellarray;               // x,y coordinates of changed cells
   unsigned int intcount;        // number of elements (2 * number of cells)
   unsigned int maxcount;        // number of elements allocated
   bool badalloc;                // malloc/realloc failed?
   
   void UpdateUndoItem(const wxString& action);
   void UpdateRedoItem(const wxString& action);
   // update the Undo/Redo items in the Edit menu
};

#endif
