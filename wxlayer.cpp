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
#include "wxprefs.h"       // for drawlayers, etc
#include "wxlayer.h"

// -----------------------------------------------------------------------------

int currlayer = -1;        // index of current layer
int numlayers = 0;         // number of existing layers

Layer* layer[maxlayers];   // array of layers

// -----------------------------------------------------------------------------

void SaveLayerGlobals()
{
   // save curralgo, currview, etc in layer[currlayer]

   layer[currlayer]->lalgo = curralgo;
   layer[currlayer]->lhash = hashing;
   layer[currlayer]->lrule = wxString(curralgo->getrule(),wxConvLocal);

   layer[currlayer]->lview = currview;

   layer[currlayer]->ltop = viewptr->seltop;
   layer[currlayer]->lbottom = viewptr->selbottom;
   layer[currlayer]->lleft = viewptr->selleft;
   layer[currlayer]->lright = viewptr->selright;
   
   layer[currlayer]->lcurs = currcurs;

   layer[currlayer]->lwarp = mainptr->GetWarp();
   layer[currlayer]->ltitle = mainptr->GetWindowTitle();
}

// -----------------------------------------------------------------------------

void ChangeLayerGlobals()
{
   // set curralgo, currview, etc using layer[currlayer]

   // curralgo does not exist if called from DeleteLayer so use global_liferules
   wxString oldrule = wxString(global_liferules.getrule(),wxConvLocal);
   
   curralgo = layer[currlayer]->lalgo;

   // need to update global rule table if the universe type has changed
   // or the current rule has changed
   if ( hashing != layer[currlayer]->lhash ||
        !oldrule.IsSameAs(layer[currlayer]->lrule, false) ) {
      curralgo->setrule(layer[currlayer]->lrule.mb_str(wxConvLocal));
   }
   hashing = layer[currlayer]->lhash;

   currview = layer[currlayer]->lview;

   viewptr->seltop = layer[currlayer]->ltop;
   viewptr->selbottom = layer[currlayer]->lbottom;
   viewptr->selleft = layer[currlayer]->lleft;
   viewptr->selright = layer[currlayer]->lright;
   
   currcurs = layer[currlayer]->lcurs;

   mainptr->SetWarp(layer[currlayer]->lwarp);
   mainptr->SetWindowTitle(layer[currlayer]->ltitle);
      
   mainptr->UpdateUserInterface(mainptr->IsActive());
   mainptr->UpdatePatternAndStatus();
}

// -----------------------------------------------------------------------------

void AddLayer()
{
   if (numlayers >= maxlayers) return;
   
   if (numlayers == 0) {
      // create the very first layer
      currlayer = 0;
   } else {
      // save curralgo, currview, etc
      SaveLayerGlobals();
      
      // insert new layer after currlayer
      currlayer++;
      if (currlayer < numlayers) {
         // shift right one or more layers
         int i;
         for (i = numlayers; i > currlayer; i--)
            layer[i] = layer[i-1];
      }
   }

   Layer* templayer = new Layer();
   layer[currlayer] = templayer;
   
   numlayers++;

   if (numlayers > 1) {
      // add new item at end of Layer menu
      mainptr->AppendLayerItem();

      // adjust titles in any items after currlayer
      int i;
      for (i = currlayer + 1; i < numlayers; i++)
         mainptr->UpdateLayerItem(i, layer[i]->ltitle);
      
      ChangeLayerGlobals();
   }
}

// -----------------------------------------------------------------------------

void DeleteLayer()
{
   if (numlayers <= 1) return;
   
   SaveLayerGlobals();
   
   delete layer[currlayer];
   numlayers--;
   if (currlayer < numlayers) {
      // shift left one or more layers
      int i;
      for (i = currlayer; i < numlayers; i++)
         layer[i] = layer[i+1];
   }
   if (currlayer > 0) currlayer--;

   // remove item from end of Layer menu
   mainptr->RemoveLayerItem();

   // adjust titles in any items after currlayer
   int i;
   for (i = currlayer + 1; i < numlayers; i++)
      mainptr->UpdateLayerItem(i, layer[i]->ltitle);
   
   ChangeLayerGlobals();
}

// -----------------------------------------------------------------------------

void DeleteOtherLayers()
{
   if (numlayers <= 1) return;
   
   // delete all layers except currlayer
   int i;
   for (i = 0; i < numlayers; i++)
      if (i != currlayer) delete layer[i];

   layer[0] = layer[currlayer];
   currlayer = 0;

   // remove all items except layer 0 from end of Layer menu
   while (numlayers > 1) {
      numlayers--;
      mainptr->RemoveLayerItem();
   }

   // update title in currlayer item
   mainptr->UpdateLayerItem(currlayer, mainptr->GetWindowTitle());

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
   if (currlayer == index) return;
   
   SaveLayerGlobals();
   currlayer = index;
   ChangeLayerGlobals();
}

// -----------------------------------------------------------------------------

void ToggleDrawLayers()
{
   drawlayers = !drawlayers;
   if (drawlayers) {
      // synchronize all layers to use same scale and position as currlayer
      //!!! or do temporarily in DrawView???
   }
   mainptr->UpdatePatternAndStatus();
}

// -----------------------------------------------------------------------------

void ToggleGenLayers()
{
   genlayers = !genlayers;
   if (genlayers) {
      // synchronize all layers to use same step increment as currlayer
      //!!! or do temporarily in GenPattern & NextGen???
   }
}

// -----------------------------------------------------------------------------

void ResizeLayers(int wd, int ht)
{
   // resize viewport in each layer
   int i;
   for (i = 0; i < numlayers; i++) {
      if (i == currlayer)
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
      return layer[index];
   }
}

// -----------------------------------------------------------------------------

Layer::Layer()
{
   if (numlayers == 0) {
      // creating very first layer; note that lalgo, lview, etc will be
      // set by SaveLayerGlobals in next AddLayer call, but play safe
      // with the variables deleted in ~Layer
      lalgo = NULL;
      lview = NULL;
   } else {
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
      
      ltitle = _("untitled");
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
}
