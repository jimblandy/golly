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
#include "wxview.h"        // for currview, viewptr->...
#include "wxutils.h"       // for Warning
#include "wxprefs.h"       // for gollydir, drawlayers, etc
#include "wxlayer.h"

// -----------------------------------------------------------------------------

int numlayers = 0;            // number of existing layers
int currindex = -1;           // index of current layer

Layer* currlayer;             // pointer to current layer
Layer* layer[maxlayers];      // array of layers

bool available[maxlayers];    // for setting tempstart suffix

// -----------------------------------------------------------------------------

void SaveLayerGlobals()
{
   // save curralgo, currview, etc in current layer

   currlayer->lalgo = curralgo;
   currlayer->lhash = hashing;
   currlayer->lrule = wxString(curralgo->getrule(),wxConvLocal);

   currlayer->lview = currview;

   currlayer->ltop = viewptr->seltop;
   currlayer->lbottom = viewptr->selbottom;
   currlayer->lleft = viewptr->selleft;
   currlayer->lright = viewptr->selright;
   
   currlayer->lcurs = currcurs;
   currlayer->lwarp = mainptr->GetWarp();
}

// -----------------------------------------------------------------------------

void ChangeLayerGlobals()
{
   // set curralgo, currview, etc using currlayer info

   // curralgo does not exist if called from DeleteLayer so use global_liferules
   wxString oldrule = wxString(global_liferules.getrule(),wxConvLocal);
   
   curralgo = currlayer->lalgo;

   // need to update global rule table if the universe type has changed
   // or the current rule has changed
   if ( hashing != currlayer->lhash || !oldrule.IsSameAs(currlayer->lrule,false) ) {
      curralgo->setrule(currlayer->lrule.mb_str(wxConvLocal));
   }
   hashing = currlayer->lhash;

   currview = currlayer->lview;

   viewptr->seltop = currlayer->ltop;
   viewptr->selbottom = currlayer->lbottom;
   viewptr->selleft = currlayer->lleft;
   viewptr->selright = currlayer->lright;
   
   currcurs = currlayer->lcurs;

   mainptr->SetWarp(currlayer->lwarp);
   mainptr->SetWindowTitle(currlayer->currname);
      
   mainptr->UpdateUserInterface(mainptr->IsActive());
   mainptr->UpdatePatternAndStatus();
}

// -----------------------------------------------------------------------------

void AddLayer()
{
   if (numlayers >= maxlayers) return;
   
   if (numlayers == 0) {
      // create the very first layer
      currindex = 0;
   } else {
      // save curralgo, currview, etc
      SaveLayerGlobals();
      
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
      // add new item at end of Layer menu
      mainptr->AppendLayerItem();

      // update names in any items after currindex
      for (int i = currindex + 1; i < numlayers; i++)
         mainptr->UpdateLayerItem(i);
      
      ChangeLayerGlobals();
   }
}

// -----------------------------------------------------------------------------

void DeleteLayer()
{
   if (numlayers <= 1) return;
   
   SaveLayerGlobals();
   
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
   
   ChangeLayerGlobals();
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
   
   SaveLayerGlobals();
   currindex = index;
   currlayer = layer[currindex];
   ChangeLayerGlobals();
}

// -----------------------------------------------------------------------------

void ToggleDrawLayers()
{
   drawlayers = !drawlayers;
   // if drawlayers is true then rendering routine will temporarily synchronize
   // all layers to use same scale and position as current layer
   mainptr->UpdatePatternAndStatus();
}

// -----------------------------------------------------------------------------

void ToggleGenLayers()
{
   genlayers = !genlayers;
   // if genlayers is true then generating routines will temporarily synchronize
   // all layers to use same step increment as current layer
}

// -----------------------------------------------------------------------------

void ResizeLayers(int wd, int ht)
{
   // resize viewport in each layer
   for (int i = 0; i < numlayers; i++) {
      if (i == currindex)
         currview->resize(wd, ht);
      else
         layer[i]->lview->resize(wd, ht);
   }
}

// -----------------------------------------------------------------------------

Layer* GetLayer(int index)
{
   if (index < 0 || index >= numlayers) {
      Warning(_("Bad index in GetLayer!"));
      return NULL;
   } else {
      if (index == currindex) {
         // update currlayer info with current settings
         SaveLayerGlobals();
      }
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
   // set tempstart prefix; ~Layer() assumes it ends with '_'
   tempstart = gollydir + wxT(".golly_start_");

   savestart = false;         // no need to save starting pattern just yet
   startfile.Clear();         // no starting pattern
   startgen = 0;              // initial starting generation
   currname = _("untitled");
   currfile = wxEmptyString;

   if (numlayers == 0) {
      // creating very first layer; note that lalgo, lview, etc will be
      // set by SaveLayerGlobals in next AddLayer call, but play safe
      // with the variables deleted in ~Layer
      lalgo = NULL;
      lview = NULL;
      
      // complete tempstart and initialize available array
      tempstart += wxT("0");
      available[0] = false;
      for (int i = 1; i < maxlayers; i++) available[i] = true;

   } else {
      // add unique suffix to tempstart
      tempstart += wxString::Format("%d", FindAvailableSuffix());

      lhash = hashing;
      if (hashing) {
         lalgo = new hlifealgo();
         lalgo->setMaxMemory(maxhashmem);
      } else {
         lalgo = new qlifealgo();
      }
      lalgo->setpoll(wxGetApp().Poller());
      
      // inherit current rule
      lrule = wxString(curralgo->getrule(),wxConvLocal);
      
      // inherit current viewport's size, scale and location
      lview = new viewport(100,100);
      lview->resize( currview->getwidth(), currview->getheight() );
      lview->setpositionmag( currview->x, currview->y, currview->getmag() );
      
      // inherit current cursor
      lcurs = currcurs;
      
      // reset speed
      lwarp = 0;
      
      // no selection
      ltop = 1;
      lbottom = 0;
      lleft = 0;
      lright = 0;
   }
}

// -----------------------------------------------------------------------------

Layer::~Layer()
{
   if (lalgo) delete lalgo;
   if (lview) delete lview;
   
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
