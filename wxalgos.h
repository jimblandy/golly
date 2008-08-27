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
#ifndef _WXALGOS_H_
#define _WXALGOS_H_

#include "lifealgo.h"

// Golly supports multiple algorithms.  The first algorithm
// registered must *always* be qlifealgo.  The second must
// *always* be hlifealgo.  (These are to support old scripts.)
// The order of the rest do not matter and indeed should soon
// be capable of being dynamic.

typedef enum {
   QLIFE_ALGO,    // QuickLife
   HLIFE_ALGO     // HashLife
};

const int MAX_ALGOS = 200;       // maximum number of algorithms

typedef int algo_type;           // 0..MAX_ALGOS-1

// A class for all the static info we need about a particular algorithm.
class AlgoData : public staticAlgoInfo {
public:
   AlgoData();
   virtual void initCellColors(int, unsigned char*);
   virtual void createIconBitmaps(int, char**);
   virtual void setDefaultBaseStep(int v) { algobase = v; }
   virtual void setDefaultMaxMem(int v) { algomem = v; }
   virtual void setStatusRGB(int, int, int);

   // additional data
   bool canhash;                 // algo uses hashing?
   int algomem;                  // maximum memory (in MB)
   int algobase;                 // base step
   unsigned char statusrgb[3];
   wxColor* algorgb;             // status bar color
   wxBrush* algobrush;           // corresponding brush
   wxBitmap** icons7x7;          // icon bitmaps for scale 1:8
   wxBitmap** icons15x15;        // icon bitmaps for scale 1:16
   
   // rgb colors for each cell state
   unsigned char cellr[256], cellg[256], cellb[256];
   
   // static allocator
   static AlgoData& tick();
};

extern AlgoData* algoinfo[MAX_ALGOS];     // static info for each algorithm
extern wxMenu* algomenu;                  // menu of algorithm names
extern algo_type initalgo;                // initial algorithm

void InitAlgorithms();
// Initialize above data -- must be called very early (before reading prefs file).

lifealgo* CreateNewUniverse(algo_type algotype, bool allowcheck = true);
// Create a new universe of given type.  If allowcheck is true then
// event checking is allowed; ie. poller is set to wxGetApp().Poller().

const char* GetAlgoName(algo_type algotype);
// Return name of given algorithm.  This name appears in various menus
// and is also stored in the prefs file.

int NumAlgos();
// Return current number of algorithms.

#endif
