/*** /
 
 This file is part of Golly, a Game of Life Simulator.
 Copyright (C) 2013 Andrew Trevorrow and Tomas Rokicki.
 
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

#ifndef _ALGOS_H_
#define _ALGOS_H_

#include "lifealgo.h"

#include "utils.h"      // for gColor

// Golly supports multiple algorithms.  The first algorithm
// registered must *always* be qlifealgo.  The second must
// *always* be hlifealgo.  The order of the rest does not matter.

enum {
    QLIFE_ALGO,         // QuickLife
    HLIFE_ALGO          // HashLife
};

const int MAX_ALGOS = 50;         // maximum number of algorithms
typedef int algo_type;            // 0..MAX_ALGOS-1

// A class for all the static info we need about a particular algorithm:

class AlgoData : public staticAlgoInfo {
public:
    AlgoData();
    virtual void setDefaultBaseStep(int v) { defbase = v; }
    
    // all hashing algos use maxhashmem and QuickLife uses 0 (unlimited)
    // virtual void setDefaultMaxMem(int v) { }
    
    static AlgoData& tick();      // static allocator
    
    // additional data
    bool canhash;                 // algo uses hashing?
    int defbase;                  // default base step
    
    gColor statusrgb;             // status bar color

    CGImageRef* icons7x7;         // icon bitmaps for scale 1:8
    CGImageRef* icons15x15;       // icon bitmaps for scale 1:16
    CGImageRef* icons31x31;       // icon bitmaps for scale 1:32
    
    // default color scheme
    bool gradient;                // use color gradient?
    gColor fromrgb;               // color at start of gradient
    gColor torgb;                 // color at end of gradient
    
    // if gradient is false then use these colors for each cell state
    unsigned char algor[256];
    unsigned char algog[256];
    unsigned char algob[256];
};

extern AlgoData* algoinfo[MAX_ALGOS];     // static info for each algorithm
extern algo_type initalgo;                // initial algorithm

// the following bitmaps are grayscale icons that can be used with any rules
// with any number of states

extern CGImageRef* circles7x7;           // circular icons for scale 1:8
extern CGImageRef* circles15x15;         // circular icons for scale 1:16
extern CGImageRef* circles31x31;         // circular icons for scale 1:32

extern CGImageRef* diamonds7x7;          // diamond icons for scale 1:8
extern CGImageRef* diamonds15x15;        // diamond icons for scale 1:16
extern CGImageRef* diamonds31x31;        // diamond icons for scale 1:32

extern CGImageRef* hexagons7x7;          // hexagonal icons for scale 1:8
extern CGImageRef* hexagons15x15;        // hexagonal icons for scale 1:16
extern CGImageRef* hexagons31x31;        // hexagonal icons for scale 1:32

// NOTE: the triangular icons are only suitable for a 4-state rule that
// is emulating a triangular neighborhood with 2 triangles per cell

extern CGImageRef* triangles7x7;         // triangular icons for scale 1:8
extern CGImageRef* triangles15x15;       // triangular icons for scale 1:16
extern CGImageRef* triangles31x31;       // triangular icons for scale 1:32

void InitAlgorithms();
// Initialize above data.  Must be called before reading the prefs file.

lifealgo* CreateNewUniverse(algo_type algotype, bool allowcheck = true);
// Create a new universe of given type.  If allowcheck is true then
// event checking is allowed.

const char* GetAlgoName(algo_type algotype);
// Return name of given algorithm.  This name appears in various places
// and is also stored in the prefs file.

int NumAlgos();
// Return current number of algorithms.

bool MultiColorImage(CGImageRef image);
// Return true if image contains at least one color that isn't a shade of gray.

bool LoadIconFile(const std::string& path, int maxstate,
                  CGImageRef** out7x7, CGImageRef** out15x15, CGImageRef** out31x31);
// Return true if we can successfully load icon bitmaps from given file.

CGImageRef* CreateIconBitmaps(const char** xpmdata, int maxstates);
// Create icon bitmaps using the given XPM data.

CGImageRef* ScaleIconBitmaps(CGImageRef* srcicons, int size);
// Return icon bitmaps scaled to given size.

#endif
