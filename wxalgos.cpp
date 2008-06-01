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
int algobase[MAX_ALGOS];            // base step for each algorithm
wxColor* algorgb[MAX_ALGOS];        // status bar color for each algorithm
wxBrush* algobrush[MAX_ALGOS];      // corresponding brushes

// -----------------------------------------------------------------------------

void InitAlgorithms()
{
   // algomenu is used when algo button is pressed
   algomenu = new wxMenu();
   for ( int i = 0; i < MAX_ALGOS; i++ ) {
      wxString name = wxString(GetAlgoName((algo_type)i), wxConvLocal);
      algomenu->AppendCheckItem(ID_ALGO0 + i, name);
   }

   //!!! perhaps use static *algo::GetBestBaseStep()???
   algobase[QLIFE_ALGO] = 10;
   algobase[HLIFE_ALGO] = 8;        // best if power of 2
   algobase[SLIFE_ALGO] = 8 ;
   algobase[JVN_ALGO] = 8;          //!!!??? best if power of 2

   algorgb[QLIFE_ALGO] = new wxColor(255, 255, 206);  // pale yellow
   algorgb[HLIFE_ALGO] = new wxColor(226, 250, 248);  // pale blue
   algorgb[SLIFE_ALGO] = new wxColor(255, 225, 225);  // pale red
   algorgb[JVN_ALGO]   = new wxColor(225, 255, 225);  // pale green

   for (int i = 0; i < MAX_ALGOS; i++)
      algobrush[i] = new wxBrush(*algorgb[i]);
}

// -----------------------------------------------------------------------------

lifealgo* CreateNewUniverse(algo_type algotype, bool allowcheck)
{
   lifealgo* newalgo = NULL;

   switch (algotype) {
      case QLIFE_ALGO:
         newalgo = new qlifealgo();
         if (newalgo == NULL) Fatal(_("Failed to create new qlifealgo!"));
         break;
      case HLIFE_ALGO:
         newalgo = new hlifealgo();
         if (newalgo == NULL) Fatal(_("Failed to create new hlifealgo!"));
         break;
      case SLIFE_ALGO:
         newalgo = new slifealgo();
         if (newalgo == NULL) Fatal(_("Failed to create new slifealgo!"));
         break;
      case JVN_ALGO:
         newalgo = new jvnalgo();
         if (newalgo == NULL) Fatal(_("Failed to create new jvnalgo!"));
         break;
      default:
         Fatal(_("Bug detected in CreateNewUniverse!"));
   }

   //!!! or call setMaxMemory for all algos???
   if (newalgo->hyperCapable()) newalgo->setMaxMemory(maxhashmem);

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
