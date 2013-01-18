/*** /
 
 This file is part of Golly, a Game of Life Simulator.
 Copyright (C) 2012 Andrew Trevorrow and Tomas Rokicki.
 
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

enum {
    QLIFE_ALGO,    // QuickLife
    HLIFE_ALGO     // HashLife
};

const int MAX_ALGOS = 50;   // maximum number of algorithms

typedef int algo_type;      // 0..MAX_ALGOS-1

// A class for all the static info we need about a particular algorithm.
class AlgoData : public staticAlgoInfo {
public:
    AlgoData();
    ~AlgoData();
    virtual void setDefaultBaseStep(int v) { defbase = v; }
    virtual void setDefaultMaxMem(int v) { algomem = v; }
    
    static AlgoData& tick();      // static allocator
    
    // additional data
    bool canhash;                 // algo uses hashing?
    int algomem;                  // maximum memory (in MB)
    int defbase;                  // default base step
    wxColor statusrgb;            // status bar color
    wxBrush* statusbrush;         // corresponding brush
    wxBitmap** icons7x7;          // icon bitmaps for scale 1:8
    wxBitmap** icons15x15;        // icon bitmaps for scale 1:16
    wxBitmap** icons31x31;        // icon bitmaps for scale 1:32
    
    // default color scheme
    bool gradient;                // use color gradient?
    wxColor fromrgb;              // color at start of gradient
    wxColor torgb;                // color at end of gradient
    // if gradient is false then use these colors for each cell state
    unsigned char algor[256];
    unsigned char algog[256];
    unsigned char algob[256];
};

extern AlgoData* algoinfo[MAX_ALGOS];   // static info for each algorithm
extern wxMenu* algomenu;                // menu of algorithm names
extern wxMenu* algomenupop;             // copy of algomenu for PopupMenu calls
extern algo_type initalgo;              // initial algorithm

extern wxBitmap** hexicons7x7;          // hexagonal icon bitmaps for scale 1:8
extern wxBitmap** hexicons15x15;        // hexagonal icon bitmaps for scale 1:16
extern wxBitmap** hexicons31x31;        // hexagonal icon bitmaps for scale 1:32

extern wxBitmap** vnicons7x7;           // diamond icon bitmaps for scale 1:8
extern wxBitmap** vnicons15x15;         // diamond icon bitmaps for scale 1:16
extern wxBitmap** vnicons31x31;         // diamond icon bitmaps for scale 1:32

void InitAlgorithms();
// Initialize above data.  Must be called before reading the prefs file.

void DeleteAlgorithms();
// Deallocate memory allocated in InitAlgorithms()

lifealgo* CreateNewUniverse(algo_type algotype, bool allowcheck = true);
// Create a new universe of given type.  If allowcheck is true then
// event checking is allowed; ie. poller is set to wxGetApp().Poller().

const char* GetAlgoName(algo_type algotype);
// Return name of given algorithm.  This name appears in various menus
// and is also stored in the prefs file.

int NumAlgos();
// Return current number of algorithms.

bool LoadIconFile(const wxString& path, int maxstate,
                  wxBitmap*** out7x7, wxBitmap*** out15x15, wxBitmap*** out31x31);
// Return true if we can successfully load icon bitmaps from given file.

wxBitmap** CreateIconBitmaps(const char** xpmdata, int maxstates);
// Create icon bitmaps using the given XPM data.

wxBitmap** ScaleIconBitmaps(wxBitmap** srcicons, int size);
// Create scaled versions of the given icon bitmaps.

void FreeIconBitmaps(wxBitmap** icons);
// Delete the given icon bitmaps.

#endif
