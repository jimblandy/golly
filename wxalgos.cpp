                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2008 Andrew Trevorrow and Tomas Rokicki.

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
#include "slifealgo.h"
#include "jvnalgo.h"
#include "wwalgo.h"
#include "generationsalgo.h"

#include "wxgolly.h"       // for wxGetApp, mainptr, viewptr, statusptr
#include "wxmain.h"        // for mainptr->...
#include "wxview.h"        // for viewptr->...
#include "wxstatus.h"      // for statusptr->...
#include "wxutils.h"       // for Warning, Fatal
#include "wxalgos.h"
#include "wxdefaultcolors.h" // pick up the default colors here only

// -----------------------------------------------------------------------------

wxMenu* algomenu;                   // menu of algorithm names
algoData *algoDatas[MAX_NUM_ALGOS] ;

algoData::algoData() {
  algomem = algobase = 0 ;
  algorgb = 0 ;
  algobrush = 0 ;
  icons7x7 = icons15x15 = 0 ;
  memset(statusrgb, 0, sizeof(statusrgb)) ;
  memset(cellr, 0, sizeof(cellr)) ;
  memset(cellg, 0, sizeof(cellg)) ;
  memset(cellb, 0, sizeof(cellb)) ;
}

// rgb colors for each cell state in each algorithm

// -----------------------------------------------------------------------------

void algoData::setStatusRGB(int r, int g, int b) {
   statusrgb[0] = r ;
   statusrgb[1] = g ;
   statusrgb[2] = b ;
}

#include <cstdio>
#include "util.h"
using namespace std ;

algoData &algoData::tick() {
   algoData *r = new algoData() ;
   algoDatas[r->id] = r ;
   return *r ;
}

void algoData::initCellColors(int numcolors, unsigned char *rgbptr) {
   if (rgbptr) {
      if (numcolors > 256) numcolors = 256;     // play safe
      for (int i = 0; i < numcolors; i++) {
         cellr[i] = *rgbptr++;
         cellg[i] = *rgbptr++;
         cellb[i] = *rgbptr++;
      }
   }
}

// -----------------------------------------------------------------------------

static wxBitmap** CreateIconBitmaps(char** xpmdata)
{
   if (xpmdata == NULL) return NULL;
   
   wxImage image(xpmdata);
   image.SetMaskColour(0, 0, 0);    // make black transparent
   wxBitmap allicons(image);

   int wd = allicons.GetWidth();
   int numicons = allicons.GetHeight() / wd;
   
   wxBitmap** iconptr = (wxBitmap**) malloc(256 * sizeof(wxBitmap*));
   if (iconptr) {
      for (int i = 0; i < 256; i++) iconptr[i] = NULL;
      
      if (numicons > 255) numicons = 255;    // play safe
      for (int i = 0; i < numicons; i++) {
         wxRect rect(0, i*wd, wd, wd);
         // add 1 because iconptr[0] must be NULL (ie. dead state)
         iconptr[i+1] = new wxBitmap(allicons.GetSubBitmap(rect));
      }
   }
   return iconptr;
}

// -----------------------------------------------------------------------------

static wxBitmap** ScaleIconBitmaps(wxBitmap** srcicons, int size)
{
   if (srcicons == NULL) return NULL;
   
   wxBitmap** iconptr = (wxBitmap**) malloc(256 * sizeof(wxBitmap*));
   if (iconptr) {
      for (int i = 0; i < 256; i++) {
         if (srcicons[i] == NULL) {
            iconptr[i] = NULL;
         } else {
            wxImage image = srcicons[i]->ConvertToImage();
            iconptr[i] = new wxBitmap(image.Scale(size, size));
         }
      }
   }
   return iconptr;
}

void algoData::createIconBitmaps(int size, char **xpmdata) {
   wxBitmap **bm = CreateIconBitmaps(xpmdata) ;
   if (size == 7)
      icons7x7 = bm ;
   else if (size == 15)
      icons15x15 = bm ;
}

// -----------------------------------------------------------------------------

void InitAlgorithms()
{
   qlifealgo::doInitializeAlgoInfo(algoData::tick()) ;
   hlifealgo::doInitializeAlgoInfo(algoData::tick()) ;
   slifealgo::doInitializeAlgoInfo(algoData::tick()) ;
   jvnalgo::doInitializeAlgoInfo(algoData::tick()) ;
   wwalgo::doInitializeAlgoInfo(algoData::tick()) ;
   generationsalgo::doInitializeAlgoInfo(algoData::tick()) ;

   // algomenu is used when algo button is pressed and for Set Algo submenu
   algomenu = new wxMenu();
   for (int i = 0; i < getNumberAlgorithms(); i++ ) {
      algoData *ad = algoDatas[i] ;
      if (ad->algoName == 0 || ad->creator == 0)
	 Fatal(_("Algorithm did not set name and/or creator")) ;
      wxString name = wxString(ad->algoName, wxConvLocal);
      algomenu->AppendCheckItem(ID_ALGO0 + i, name);
      if (ad->statusrgb[0] == 0 && ad->statusrgb[1] == 0 &&
	  ad->statusrgb[2] == 0) {
	 // make a new pale status color as far from the other
	 // colors as we can.
	 int bd = -1 ;
	 for (int j=0; j<64; j++) {
	     int tr = 191 + ((j & 1) << 5) + ((j & 8) << 1) ;
	     int tg = 191 + ((j & 2) << 4) + (j & 16) ;
	     int tb = 191 + ((j & 4) << 3) + ((j & 32) >> 1) ;
	     int md = 3 * 256 * 256 ;
	     for (int k=0; k<getNumberAlgorithms(); k++) {
	        if (k == i)
		   continue ;
	        int oldr = algoDatas[k]->statusrgb[0] ;
	        int oldg = algoDatas[k]->statusrgb[1] ;
	        int oldb = algoDatas[k]->statusrgb[2] ;
		int td = (oldr - tr) * (oldr - tr) + (oldg - tg) * (oldg - tg) +
		  (oldb - tb) * (oldb - tb) ;
		if (td < md)
		  md = td ;
	     }
	     if (md > bd) {
	       ad->statusrgb[0] = tr ;
	       ad->statusrgb[1] = tg ;
	       ad->statusrgb[2] = tb ;
	       bd = md ;
	     }
	 }
      }
      ad->algorgb = new wxColor(ad->statusrgb[0], ad->statusrgb[1],
			        ad->statusrgb[2]) ;
      ad->algobrush = new wxBrush(*ad->algorgb) ;
      if (!ad->icons15x15)
	ad->icons15x15 = ScaleIconBitmaps(ad->icons7x7, 15) ;
      if (!ad->icons7x7)
	ad->icons7x7 = ScaleIconBitmaps(ad->icons15x15, 15) ;
      if (ad->cellr[0] == ad->cellr[1] && ad->cellg[0] == ad->cellg[1] &&
	  ad->cellb[0] == ad->cellb[1]) {
	// colors are nonsensical, probably unset; fold in defaults
	unsigned char *rgbptr = default_colors ;
	for (int c = 0; c < 256; c++) {
	  ad->cellr[c] = *rgbptr++;
	  ad->cellg[c] = *rgbptr++;
	  ad->cellb[c] = *rgbptr++;
	}
      }
   }
}

// -----------------------------------------------------------------------------
lifealgo* CreateNewUniverse(algo_type algotype, bool allowcheck)
{
   lifealgo* newalgo = NULL;
   newalgo = algoDatas[algotype]->creator() ;

   if (newalgo == NULL) Fatal(_("Failed to create new universe!"));

   if (algoDatas[algotype]->algomem >= 0)
      newalgo->setMaxMemory(algoDatas[algotype]->algomem);

   if (allowcheck) newalgo->setpoll(wxGetApp().Poller());

   return newalgo;
}

// -----------------------------------------------------------------------------

const char* GetAlgoName(algo_type algotype) {
   return algoDatas[algotype]->algoName ;
}
int getNumberAlgorithms() {
   return staticAlgoInfo::getNumAlgos() ;
}
algo_type initalgo ;
