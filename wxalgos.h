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
   QLIFE_ALGO,       // QuickLife
   HLIFE_ALGO,       // HashLife
} ;
typedef int algo_type; // it's now open-ended

extern wxMenu* algomenu;                  // menu of algorithm names
extern algo_type initalgo;                // initial layer's algorithm

void InitAlgorithms();
// Initialize above data -- must be called very early (before reading prefs file).

lifealgo* CreateNewUniverse(algo_type algotype, bool allowcheck = true);
// Create a new universe of given type.  If allowcheck is true then
// event checking is allowed; ie. poller is set to wxGetApp().Poller().

const char* GetAlgoName(algo_type algotype);
// Return name of given algorithm.  This name appears in various menus
// and is also stored in the prefs file.

int getNumberAlgorithms() ;

const int MAX_NUM_ALGOS = 256 ;     // no more than this number of algos

/**
 *   A class for all the info that wx needs about a particular algorithm.
 */
class algoData : public staticAlgoInfo {
public:
   algoData() ;
   virtual void initCellColors(int, unsigned char *) ;
   virtual void createIconBitmaps(int /* size */, char ** /* xpmdata */ ) ;
   virtual void setDefaultBaseStep(int v) { algobase = v ; }
   virtual void setDefaultMaxMem(int v) { algomem = v ; }
   virtual void setStatusRGB(int /* r */, int /* g */, int /* b */) ;
   /* additional data used by wx */
   int algomem, algobase ;
   unsigned char statusrgb[3] ;
   wxColor *algorgb ;
   wxBrush *algobrush ;
   wxBitmap **icons7x7, **icons15x15 ;
   unsigned char cellr[256], cellg[256], cellb[256] ;
   /* static allocator */
   static algoData &tick() ;
} ;

extern algoData *algoDatas[MAX_NUM_ALGOS] ;
#endif
