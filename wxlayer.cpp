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
#include "wxprefs.h"       // for drawlayers, etc
#include "wxlayer.h"

// -----------------------------------------------------------------------------

int currlayer = -1;        // index of current layer
int numlayers = 0;         // number of existing layers

Layer* layer[maxlayers];   // array of layers

// -----------------------------------------------------------------------------

void ReadLayerGlobals()
{
   // save curralgo, rule, currview, etc in layer[currlayer]

   layer[currlayer]->lalgo = curralgo;
   layer[currlayer]->lrule = wxString(curralgo->getrule(),wxConvLocal);

/*!!!
   layer[currlayer]->lview = viewptr->currview;
*/

   //!!!
}

// -----------------------------------------------------------------------------

void WriteLayerGlobals()
{
   // set curralgo, rule, currview, etc using layer[currlayer]

   curralgo = layer[currlayer]->lalgo;

   // no need to set poller here???
   //!!! curralgo->setpoll(wxGetApp().Poller());

   // need to update global rule table
   curralgo->setrule(layer[currlayer]->lrule.mb_str(wxConvLocal));

/*!!!
   viewptr->currview = layer[currlayer]->lview;
*/

   //!!!
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
      ReadLayerGlobals();
      
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
   
      // prevent CreateUniverse() deleting curralgo (saved above)
      curralgo = NULL;
      mainptr->NewPattern(layer[currlayer]->ltitle);
   }
}

// -----------------------------------------------------------------------------

void DeleteLayer()
{
   if (numlayers <= 1) return;
   
   delete layer[currlayer];
   numlayers--;
   if (currlayer < numlayers) {
      // shift left one or more layers
      int i;
      for (i = currlayer; i < numlayers; i++)
         layer[i] = layer[i+1];
   }
   if (currlayer > 0) currlayer--;
   
   WriteLayerGlobals();

   // remove item from end of Layer menu
   mainptr->RemoveLayerItem();
   mainptr->UpdateMenuItems(mainptr->IsActive());
   mainptr->UpdatePatternAndStatus();
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
   
   WriteLayerGlobals();

   // remove all items except layer 0 from end of Layer menu
   while (numlayers > 1) {
      numlayers--;
      mainptr->RemoveLayerItem();
   }
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
   
   ReadLayerGlobals();
   currlayer = index;
   WriteLayerGlobals();

   mainptr->UpdateMenuItems(mainptr->IsActive());
   mainptr->UpdatePatternAndStatus();
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

Layer::Layer()
{
   lalgo = NULL;
   
   if (numlayers == 0) {
      lview = viewptr->currview;
   } else {
      lview = new viewport(100,100);
      // inherit currview's size, scale and location
      //!!!
   }
   
   lrule = wxT("B3/S23");
   
   ltitle = _("untitled");
   
   // no selection
   ltop = 1;
   lbottom = 0;
   lleft = 0;
   lright = 0;
}

// -----------------------------------------------------------------------------

Layer::~Layer()
{
   if (lalgo) delete lalgo;
   if (lview != viewptr->currview) delete lview;  //!!!???
}
