                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2006 Andrew Trevorrow and Tomas Rokicki.

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
#include "qlifealgo.h"
#include "hlifealgo.h"
#include "viewport.h"

#include "wxgolly.h"       // for wxGetApp, mainptr, viewptr
#include "wxmain.h"        // for mainptr->...
#include "wxview.h"        // for viewptr->...
#include "wxutils.h"       // for Warning
#include "wxprefs.h"       // for gollydir, inithash, initrule, etc
#include "wxlayer.h"

// -----------------------------------------------------------------------------

int numlayers = 0;            // number of existing layers
int currindex = -1;           // index of current layer

Layer* currlayer;             // pointer to current layer
Layer* layer[maxlayers];      // array of layers

bool available[maxlayers];    // for setting tempstart suffix

bool oldhash;                 // hash setting in old layer
wxString oldrule;             // rule string in old layer
int oldmag;                   // scale in old layer
bigint oldx;                  // X position in old layer
bigint oldy;                  // Y position in old layer
wxCursor* oldcurs;            // cursor mode in old layer

// -----------------------------------------------------------------------------

void SaveLayerSettings()
{
   // set oldhash and oldrule for use in CurrentLayerChanged
   oldhash = currlayer->hash;
   oldrule = wxString(global_liferules.getrule(), wxConvLocal);
   
   if (syncviews) {
      // save scale and location for use in CurrentLayerChanged
      oldmag = currlayer->view->getmag();
      oldx = currlayer->view->x;
      oldy = currlayer->view->y;
   }
   
   if (synccursors) {
      // save cursor mode for use in CurrentLayerChanged
      oldcurs = currlayer->curs;
   }
   
   // we're about to change layer so remember current rule
   currlayer->rule = oldrule;
}

// -----------------------------------------------------------------------------

void CurrentLayerChanged()
{
   // need to update global rule table if the hash setting has changed
   // or if the current rule has changed
   wxString currentrule = wxString(global_liferules.getrule(), wxConvLocal);
   if ( oldhash != currlayer->hash || !oldrule.IsSameAs(currentrule,false) ) {
      currlayer->algo->setrule( currentrule.mb_str(wxConvLocal) );
   }
   
   if (syncviews) currlayer->view->setpositionmag(oldx, oldy, oldmag);
   if (synccursors) currlayer->curs = oldcurs;

   mainptr->SetWarp(currlayer->warp);
   mainptr->SetWindowTitle(currlayer->currname);
      
   mainptr->UpdateUserInterface(mainptr->IsActive());
   mainptr->UpdatePatternAndStatus();
}

// -----------------------------------------------------------------------------

void AddLayer()
{
   if (numlayers >= maxlayers) return;
   
   if (numlayers == 0) {
      // creating the very first layer
      currindex = 0;
   } else {
      SaveLayerSettings();
      
      // insert new layer after currindex
      currindex++;
      if (currindex < numlayers) {
         // shift right one or more layers
         for (int i = numlayers; i > currindex; i--)
            layer[i] = layer[i-1];
      }
   }

   currlayer = new Layer();
   layer[currindex] = currlayer;
   
   numlayers++;

   if (numlayers > 1) {
      // add another item at end of Layer menu
      mainptr->AppendLayerItem();

      // update names in any items after currindex
      for (int i = currindex + 1; i < numlayers; i++)
         mainptr->UpdateLayerItem(i);
      
      CurrentLayerChanged();
   }
}

// -----------------------------------------------------------------------------

void DeleteLayer()
{
   if (numlayers <= 1) return;
   
   SaveLayerSettings();
   
   delete currlayer;
   numlayers--;
   if (currindex < numlayers) {
      // shift left one or more layers
      for (int i = currindex; i < numlayers; i++)
         layer[i] = layer[i+1];
   }
   if (currindex > 0) currindex--;
   currlayer = layer[currindex];

   // remove item from end of Layer menu
   mainptr->RemoveLayerItem();

   // update names in any items after currindex
   for (int i = currindex + 1; i < numlayers; i++)
      mainptr->UpdateLayerItem(i);
   
   CurrentLayerChanged();
}

// -----------------------------------------------------------------------------

void DeleteOtherLayers()
{
   if (numlayers <= 1) return;
   
   // delete all layers except current layer
   for (int i = 0; i < numlayers; i++)
      if (i != currindex) delete layer[i];

   layer[0] = layer[currindex];
   currindex = 0;
   // currlayer doesn't change

   // remove all items except layer 0 from end of Layer menu
   while (numlayers > 1) {
      numlayers--;
      mainptr->RemoveLayerItem();
   }

   // update name in currindex item
   mainptr->UpdateLayerItem(currindex);

   mainptr->UpdateMenuItems(mainptr->IsActive());
   mainptr->UpdatePatternAndStatus();
}

// -----------------------------------------------------------------------------

void MoveLayerDialog()
{
   Warning(_("Not yet implemented."));    //!!!
}

// -----------------------------------------------------------------------------

void SetLayer(int index)
{
   if (index < 0 || index >= numlayers) return;
   if (currindex == index) return;
   
   SaveLayerSettings();
   currindex = index;
   currlayer = layer[currindex];
   CurrentLayerChanged();
}

// -----------------------------------------------------------------------------

void ToggleLayerBar()
{
   showlayer = !showlayer;
   wxRect r = viewptr->GetRect();
   if (showlayer) {
      // show layer bar at top of viewport window
      r.y = layerbarht;
      r.height -= layerbarht;
   } else {
      // hide layer bar
      r.y = 0;
      r.height += layerbarht;
   }
   viewptr->SetSize(r);
}

// -----------------------------------------------------------------------------

void ToggleSyncViews()
{
   syncviews = !syncviews;
}

// -----------------------------------------------------------------------------

void ToggleSyncCursors()
{
   synccursors = !synccursors;
}

// -----------------------------------------------------------------------------

void ToggleStackLayers()
{
   stacklayers = !stacklayers;
   if (stacklayers && tilelayers) tilelayers = false;
   mainptr->UpdatePatternAndStatus();
}

// -----------------------------------------------------------------------------

void ToggleTileLayers()
{
   tilelayers = !tilelayers;
   if (tilelayers && stacklayers) stacklayers = false;
   mainptr->UpdatePatternAndStatus();
}

// -----------------------------------------------------------------------------

void ResizeLayers(int wd, int ht)
{
   // resize viewport in each layer
   for (int i = 0; i < numlayers; i++)
      layer[i]->view->resize(wd, ht);
}

// -----------------------------------------------------------------------------

Layer* GetLayer(int index)
{
   if (index < 0 || index >= numlayers) {
      Warning(_("Bad index in GetLayer!"));
      return NULL;
   } else {
      return layer[index];
   }
}

// -----------------------------------------------------------------------------

int FindAvailableSuffix()
{
   // find first available index to use as tempstart suffix
   for (int i = 0; i < maxlayers; i++) {
      if (available[i]) {
         available[i] = false;
         return i;
      }
   }
   // bug if we get here
   Warning(_("Bug in FindAvailableSuffix!"));
   return 0;
}

// -----------------------------------------------------------------------------

Layer::Layer()
{
   // set tempstart prefix (unique suffix will be added below);
   // WARNING: ~Layer() assumes prefix ends with '_'
   tempstart = gollydir + wxT(".golly_start_");

   savestart = false;            // no need to save starting pattern just yet
   startfile.Clear();            // no starting pattern
   startgen = 0;                 // initial starting generation
   currname = _("untitled");
   currfile = wxEmptyString;
   warp = 0;                     // initial speed setting
   originx = 0;                  // no X origin offset
   originy = 0;                  // no Y origin offset
   
   // no selection
   seltop = 1;
   selbottom = 0;
   selleft = 0;
   selright = 0;

   // rule is only set later in SaveLayerSettings, so here we just assign
   // a string to help catch caller accessing rule when it shouldn't
   rule = _("bug!");

   if (numlayers == 0) {
      // creating very first layer
      
      // set hash using inithash stored in prefs file
      hash = inithash;
      
      // create empty universe
      if (hash) {
         algo = new hlifealgo();
         algo->setMaxMemory(maxhashmem);
      } else {
         algo = new qlifealgo();
      }
      algo->setpoll(wxGetApp().Poller());

      // set rule using initrule stored in prefs file;
      // errors can only occur if someone has edited the prefs file
      const char *err = algo->setrule(initrule);
      if (err) {
         Warning(wxString(err,wxConvLocal));
         // user will see offending rule string in window title
      } else if (global_liferules.hasB0notS8 && hash) {
         // silently turn off hashing
         hash = false;
         delete algo;
         algo = new qlifealgo();
         algo->setpoll(wxGetApp().Poller());
         algo->setrule(initrule);
      }
      
      // create viewport; the initial size is not important because
      // ResizeLayers will soon be called
      view = new viewport(100,100);
      
      // set cursor in case newcurs/opencurs are set to "No Change"
      curs = curs_pencil;
      
      // add suffix to tempstart and initialize available array
      tempstart += wxT("0");
      available[0] = false;
      for (int i = 1; i < maxlayers; i++) available[i] = true;

   } else {
      // adding a new layer after currlayer (see AddLayer)

      // inherit current universe type and create empty universe
      hash = currlayer->hash;
      if (hash) {
         algo = new hlifealgo();
         algo->setMaxMemory(maxhashmem);
      } else {
         algo = new qlifealgo();
      }
      algo->setpoll(wxGetApp().Poller());
      
      // inherit current rule (already in global_liferules)
      
      // inherit current viewport's size, scale and location
      view = new viewport(100,100);
      view->resize( currlayer->view->getwidth(),
                    currlayer->view->getheight() );
      view->setpositionmag( currlayer->view->x, currlayer->view->y,
                            currlayer->view->getmag() );
      
      // inherit current cursor
      curs = currlayer->curs;

      // add unique suffix to tempstart
      tempstart += wxString::Format("%d", FindAvailableSuffix());
   }
}

// -----------------------------------------------------------------------------

Layer::~Layer()
{
   if (algo) delete algo;
   if (view) delete view;
   
   // delete tempstart file if it exists
   if (wxFileExists(tempstart)) wxRemoveFile(tempstart);
   
   // make tempstart suffix available for new layers
   wxString suffix = tempstart.AfterLast('_');
   long val;
   if (suffix.ToLong(&val) && val >= 0 && val < maxlayers) {
      available[val] = true;
   } else {
      Warning(_("Problem in tempstart: ") + tempstart);
   }
}
