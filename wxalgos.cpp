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

#include "wxgolly.h"       // for wxGetApp, mainptr, viewptr, statusptr
#include "wxmain.h"        // for mainptr->...
#include "wxview.h"        // for viewptr->...
#include "wxstatus.h"      // for statusptr->...
#include "wxutils.h"       // for Warning, Fatal
#include "wxalgos.h"

// -----------------------------------------------------------------------------

wxMenu* algomenu;                   // menu of algorithm names
algo_type initalgo = QLIFE_ALGO;    // initial layer's algorithm
int algomem[NUM_ALGOS];             // maximum memory (in MB) for each algorithm
int algobase[NUM_ALGOS];            // base step for each algorithm
wxColor* algorgb[NUM_ALGOS];        // status bar color for each algorithm
wxBrush* algobrush[NUM_ALGOS];      // corresponding brushes

wxBitmap** icons7x7[NUM_ALGOS] = {NULL};    // icon bitmaps for scale 1:8
wxBitmap** icons15x15[NUM_ALGOS] = {NULL};  // icon bitmaps for scale 1:16

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

// -----------------------------------------------------------------------------

void InitAlgorithms()
{
   qlifealgo temp_qlife;
   hlifealgo temp_hlife;
   slifealgo temp_slife;
   jvnalgo   temp_jvn;
   wwalgo    temp_ww;

   // algomenu is used when algo button is pressed and for Set Algo submenu
   algomenu = new wxMenu();
   for ( int i = 0; i < NUM_ALGOS; i++ ) {
      wxString name = wxString(GetAlgoName((algo_type)i), wxConvLocal);
      algomenu->AppendCheckItem(ID_ALGO0 + i, name);
   }

   //!!! perhaps use temp_*.GetInitialMaxMem()???
   //!!! use -1 to indicate the algo ignores setMaxMemory???
   algomem[QLIFE_ALGO] = 0;         // no limit
   algomem[HLIFE_ALGO] = 300;       // in megabytes
   algomem[SLIFE_ALGO] = 300;       // ditto
   algomem[JVN_ALGO]   = 300;       // ditto
   algomem[WW_ALGO]    = 300;

   //!!! perhaps use temp_*.GetInitialBaseStep()???
   algobase[QLIFE_ALGO] = 10;
   algobase[HLIFE_ALGO] = 8;        // best if power of 2
   algobase[SLIFE_ALGO] = 8 ;
   algobase[JVN_ALGO]   = 8;        // best if power of 2
   algobase[WW_ALGO]    = 8;

   // set status bar background for each algo
   algorgb[QLIFE_ALGO] = new wxColor(255, 255, 206);  // pale yellow
   algorgb[HLIFE_ALGO] = new wxColor(226, 250, 248);  // pale blue
   algorgb[SLIFE_ALGO] = new wxColor(255, 225, 225);  // pale red
   algorgb[JVN_ALGO]   = new wxColor(225, 255, 225);  // pale green
   algorgb[WW_ALGO]    = new wxColor(225, 225, 255);  // not sure

   for (int i = 0; i < NUM_ALGOS; i++)
      algobrush[i] = new wxBrush(*algorgb[i]);
   
   // build icon bitmaps for each algo (if icon data is present)
   icons7x7  [QLIFE_ALGO]  = CreateIconBitmaps( temp_qlife.GetIconData(7) );
   icons15x15[QLIFE_ALGO]  = CreateIconBitmaps( temp_qlife.GetIconData(15) );
   icons7x7  [HLIFE_ALGO]  = CreateIconBitmaps( temp_hlife.GetIconData(7) );
   icons15x15[HLIFE_ALGO]  = CreateIconBitmaps( temp_hlife.GetIconData(15) );
   icons7x7  [SLIFE_ALGO]  = CreateIconBitmaps( temp_slife.GetIconData(7) );
   icons15x15[SLIFE_ALGO]  = CreateIconBitmaps( temp_slife.GetIconData(15) );
   icons7x7  [JVN_ALGO]    = CreateIconBitmaps( temp_jvn.GetIconData(7) );
   icons15x15[JVN_ALGO]    = CreateIconBitmaps( temp_jvn.GetIconData(15) );
   icons7x7  [WW_ALGO]     = CreateIconBitmaps( temp_ww.GetIconData(7) );
   icons15x15[WW_ALGO]     = CreateIconBitmaps( temp_ww.GetIconData(15) );
   
   // create scaled bitmaps if only one size is supplied
   for (int i = 0; i < NUM_ALGOS; i++) {
      if (!icons15x15[i]) {
         // scale up 7x7 bitmaps (looks ugly)
         icons15x15[i] = ScaleIconBitmaps(icons7x7[i], 15);
      }
      if (!icons7x7[i]) {
         // scale down 15x15 bitmaps (not too bad)
         icons7x7[i] = ScaleIconBitmaps(icons15x15[i], 7);
      }
   }
}

// -----------------------------------------------------------------------------

lifealgo* CreateNewUniverse(algo_type algotype, bool allowcheck)
{
   lifealgo* newalgo = NULL;

   switch (algotype) {
      case QLIFE_ALGO:  newalgo = new qlifealgo(); break;
      case HLIFE_ALGO:  newalgo = new hlifealgo(); break;
      case SLIFE_ALGO:  newalgo = new slifealgo(); break;
      case JVN_ALGO:    newalgo = new jvnalgo(); break;
      case WW_ALGO:     newalgo = new wwalgo(); break;
      default:          Fatal(_("Bug detected in CreateNewUniverse!"));
   }

   if (newalgo == NULL) Fatal(_("Failed to create new universe!"));

   if (algomem[algotype] >= 0) newalgo->setMaxMemory(algomem[algotype]);

   // allow event checking?
   if (allowcheck) newalgo->setpoll(wxGetApp().Poller());

   return newalgo;
}

// -----------------------------------------------------------------------------

const char* GetAlgoName(algo_type algotype)
{
   switch (algotype) {
      case QLIFE_ALGO:  return "QuickLife";
      case HLIFE_ALGO:  return "HashLife";
      case SLIFE_ALGO:  return "SlowLife";
      case JVN_ALGO:    return "JvN 29-state CA";
      case WW_ALGO:     return "WireWorld";
      default:          Fatal(_("Bug detected in GetAlgoName!"));
   }
   return "BUG";        // avoid gcc warning
}
