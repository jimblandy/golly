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

#include "wxgolly.h"       // for wxGetApp, mainptr, viewptr, statusptr
#include "wxmain.h"        // for mainptr->...
#include "wxview.h"        // for viewptr->...
#include "wxstatus.h"      // for statusptr->...
#include "wxutils.h"       // for Warning, Fatal
#include "wxalgos.h"

// -----------------------------------------------------------------------------

wxMenu* algomenu;                   // menu of algorithm names
algo_type initalgo = QLIFE_ALGO;    // initial layer's algorithm
int algomem[MAX_ALGOS];             // maximum memory (in MB) for each algorithm
int algobase[MAX_ALGOS];            // base step for each algorithm
wxColor* algorgb[MAX_ALGOS];        // status bar color for each algorithm
wxBrush* algobrush[MAX_ALGOS];      // corresponding brushes

wxBitmap** icons7x7[MAX_ALGOS] = {NULL};    // icon bitmaps for scale 1:8
wxBitmap** icons15x15[MAX_ALGOS] = {NULL};  // icon bitmaps for scale 1:16

// -----------------------------------------------------------------------------

//!!! get this static icon data from jvnalgo.h???
// maybe add char** GetIconData7x7() { return NULL; } in lifealgo.h???

// XPM data for the 28 7x7 icons used in JvN algo
static char* jvn7x7[] = {
// width height ncolors chars_per_pixel
"7 196 9 1",
// colors
"A c #FFFFFFFFFFFF",
"B c #FFFFFFFF0000",
"C c #FFFFA5A54242",
"D c #FFFF0000FFFF",
"E c #FFFF00000000",
"F c #A5A5A5A58484",
"G c #0000FFFFFFFF",
"H c #00009494FFFF",
"I c None",             // transparent
// pixels
"IIEEEII",
"IEEEEEI",
"EEEEEEE",
"EEEEEEE",
"EEEEEEE",
"IEEEEEI",
"IIEEEII",
"IIBBBII",
"IBBBBBI",
"BBBBBBB",
"IIIIIII",
"EEEEEEE",
"IEEEEEI",
"IIEEEII",
"IIEEEII",
"IEEEEEI",
"EEEEEEE",
"IIIIIII",
"BBBBBBB",
"IBBBBBI",
"IIBBBII",
"IIBIBII",
"IBBIBBI",
"BBBIBBB",
"IIIIIII",
"EEEIEEE",
"IEEIEEI",
"IIEIEII",
"IIBIEII",
"IBBIEEI",
"BBBIEEE",
"IIIIIII",
"EEEIBBB",
"IEEIBBI",
"IIEIBII",
"IIEIBII",
"IEEIBBI",
"EEEIBBB",
"IIIIIII",
"BBBIEEE",
"IBBIEEI",
"IIBIEII",
"IIEIEII",
"IEEIEEI",
"EEEIEEE",
"IIIIIII",
"BBBIBBB",
"IBBIBBI",
"IIBIBII",
"IIBIBII",
"IBBIBBI",
"BBBIBBB",
"IIIIIII",
"BBBIBBB",
"IBBIBBI",
"IIBIBII",
"IIIIIII",
"IIIIHII",
"IIIIHHI",
"HHHHHHH",
"IIIIHHI",
"IIIIHII",
"IIIIIII",
"IIIHIII",
"IIHHHII",
"IHHHHHI",
"IIIHIII",
"IIIHIII",
"IIIHIII",
"IIIHIII",
"IIIIIII",
"IIHIIII",
"IHHIIII",
"HHHHHHH",
"IHHIIII",
"IIHIIII",
"IIIIIII",
"IIIHIII",
"IIIHIII",
"IIIHIII",
"IIIHIII",
"IHHHHHI",
"IIHHHII",
"IIIHIII",
"IIIIIII",
"IIIIGII",
"IIIIGGI",
"GGGGGGG",
"IIIIGGI",
"IIIIGII",
"IIIIIII",
"IIIGIII",
"IIGGGII",
"IGGGGGI",
"IIIGIII",
"IIIGIII",
"IIIGIII",
"IIIGIII",
"IIIIIII",
"IIGIIII",
"IGGIIII",
"GGGGGGG",
"IGGIIII",
"IIGIIII",
"IIIIIII",
"IIIGIII",
"IIIGIII",
"IIIGIII",
"IIIGIII",
"IGGGGGI",
"IIGGGII",
"IIIGIII",
"IIIIIII",
"IIIIEII",
"IIIIEEI",
"EEEEEEE",
"IIIIEEI",
"IIIIEII",
"IIIIIII",
"IIIEIII",
"IIEEEII",
"IEEEEEI",
"IIIEIII",
"IIIEIII",
"IIIEIII",
"IIIEIII",
"IIIIIII",
"IIEIIII",
"IEEIIII",
"EEEEEEE",
"IEEIIII",
"IIEIIII",
"IIIIIII",
"IIIEIII",
"IIIEIII",
"IIIEIII",
"IIIEIII",
"IEEEEEI",
"IIEEEII",
"IIIEIII",
"IIIIIII",
"IIIIDII",
"IIIIDDI",
"DDDDDDD",
"IIIIDDI",
"IIIIDII",
"IIIIIII",
"IIIDIII",
"IIDDDII",
"IDDDDDI",
"IIIDIII",
"IIIDIII",
"IIIDIII",
"IIIDIII",
"IIIIIII",
"IIDIIII",
"IDDIIII",
"DDDDDDD",
"IDDIIII",
"IIDIIII",
"IIIIIII",
"IIIDIII",
"IIIDIII",
"IIIDIII",
"IIIDIII",
"IDDDDDI",
"IIDDDII",
"IIIDIII",
"IIIFIII",
"IIFFFII",
"IFFIFFI",
"FFIIIFF",
"IFFIFFI",
"IIFFFII",
"IIIFIII",
"IIICIII",
"IICCCII",
"ICCICCI",
"CCIIICC",
"ICCICCI",
"IICCCII",
"IIICIII",
"IIIBIII",
"IIBBBII",
"IBBBBBI",
"BBBIBBB",
"IBBBBBI",
"IIBBBII",
"IIIBIII",
"IIIAIII",
"IIAAAII",
"IAAAAAI",
"AAAAAAA",
"IAAAAAI",
"IIAAAII",
"IIIAIII"
};

// -----------------------------------------------------------------------------

static wxBitmap** CreateIconBitmaps(char** xpmdata)
{
   #ifdef __WXMSW__
      wxImage image(xpmdata);
      wxBitmap allicons(image);
   #else
      wxBitmap allicons(xpmdata, wxBITMAP_TYPE_XPM);
   #endif

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
   // algomenu is used when algo button is pressed and for Set Algo submenu
   algomenu = new wxMenu();
   for ( int i = 0; i < MAX_ALGOS; i++ ) {
      wxString name = wxString(GetAlgoName((algo_type)i), wxConvLocal);
      algomenu->AppendCheckItem(ID_ALGO0 + i, name);
   }

   //!!! perhaps use static *algo::GetBestMaxMem()???
   //!!! use -1 to indicate the algo ignores setMaxMemory???
   algomem[QLIFE_ALGO] = 0;         // no limit
   algomem[HLIFE_ALGO] = 300;       // in megabytes
   algomem[SLIFE_ALGO] = 300;       // ditto
   algomem[JVN_ALGO]   = 300;       // ditto

   //!!! perhaps use static *algo::GetBestBaseStep()???
   algobase[QLIFE_ALGO] = 10;
   algobase[HLIFE_ALGO] = 8;        // best if power of 2
   algobase[SLIFE_ALGO] = 8 ;
   algobase[JVN_ALGO]   = 8;        // best if power of 2

   algorgb[QLIFE_ALGO] = new wxColor(255, 255, 206);  // pale yellow
   algorgb[HLIFE_ALGO] = new wxColor(226, 250, 248);  // pale blue
   algorgb[SLIFE_ALGO] = new wxColor(255, 225, 225);  // pale red
   algorgb[JVN_ALGO]   = new wxColor(225, 255, 225);  // pale green

   for (int i = 0; i < MAX_ALGOS; i++)
      algobrush[i] = new wxBrush(*algorgb[i]);
   
   //!!! for now we assume only JVN_ALGO uses icons
   icons7x7[JVN_ALGO] = CreateIconBitmaps(jvn7x7);
   icons15x15[JVN_ALGO] = ScaleIconBitmaps(icons7x7[JVN_ALGO], 15);
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
      default:
         Fatal(_("Bug detected in CreateNewUniverse!"));
   }

   if (newalgo == NULL) Fatal(_("Failed to create new universe!"));

   //!!! safe to call setMaxMemory for all algos???
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
      default:          Fatal(_("Bug detected in GetAlgoName!"));
   }
   return "BUG";        // avoid gcc warning
}
