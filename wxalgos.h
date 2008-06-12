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

// Golly supports multiple algorithms.

typedef enum {
   QLIFE_ALGO,       // QuickLife
   HLIFE_ALGO,       // HashLife
   SLIFE_ALGO,       // Not-so-fast Life
   JVN_ALGO,         // John von Neumann's 29-state CA
   WW_ALGO,          // WireWorld!
   NUM_ALGOS
} algo_type;

extern wxMenu* algomenu;                  // menu of algorithm names
extern algo_type initalgo;                // initial layer's algorithm
extern int algomem[NUM_ALGOS];            // maximum memory (in MB) for each algorithm
extern int algobase[NUM_ALGOS];           // base step for each algorithm
extern wxColor* algorgb[NUM_ALGOS];       // status bar color for each algorithm
extern wxBrush* algobrush[NUM_ALGOS];     // corresponding brush
extern wxBitmap** icons7x7[NUM_ALGOS];    // icon bitmaps for scale 1:8
extern wxBitmap** icons15x15[NUM_ALGOS];  // icon bitmaps for scale 1:16

extern unsigned char cellr[NUM_ALGOS][256];
extern unsigned char cellg[NUM_ALGOS][256];
extern unsigned char cellb[NUM_ALGOS][256];
// rgb colors for each cell state in each algorithm

void InitAlgorithms();
// Initialize above data -- must be called very early (before reading prefs file).

lifealgo* CreateNewUniverse(algo_type algotype, bool allowcheck = true);
// Create a new universe of given type.  If allowcheck is true then
// event checking is allowed; ie. poller is set to wxGetApp().Poller().

const char* GetAlgoName(algo_type algotype);
// Return name of given algorithm.  This name appears in various menus
// and is also stored in the prefs file.

#endif
