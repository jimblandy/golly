// This file is part of Golly.
// See docs/License.html for the copyright notice.

#ifndef _ALGOS_H_
#define _ALGOS_H_

#include "lifealgo.h"

#include "utils.h"      // for gColor

// for storing icon info:
typedef struct {
    int wd;
    int ht;
    unsigned char* pxldata;     // RGBA data (size = wd * ht * 4)
} gBitmap;
typedef gBitmap* gBitmapPtr;

// Golly supports multiple algorithms.  The first algorithm
// registered must *always* be qlifealgo.  The second must
// *always* be hlifealgo.  The order of the rest does not matter.

enum {
    QLIFE_ALGO,     // QuickLife
    HLIFE_ALGO      // HashLife
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

    gBitmapPtr* icons7x7;         // icon bitmaps for scale 1:8
    gBitmapPtr* icons15x15;       // icon bitmaps for scale 1:16
    gBitmapPtr* icons31x31;       // icon bitmaps for scale 1:32

    // default color scheme
    bool gradient;                // use color gradient?
    gColor fromrgb;               // color at start of gradient
    gColor torgb;                 // color at end of gradient

    // if gradient is false then use these colors for each cell state
    unsigned char algor[256];
    unsigned char algog[256];
    unsigned char algob[256];
};

extern AlgoData* algoinfo[MAX_ALGOS];   // static info for each algorithm
extern algo_type initalgo;              // initial algorithm

// the following bitmaps are grayscale icons that can be used with any rules
// with any number of states

extern gBitmapPtr* circles7x7;          // circular icons for scale 1:8
extern gBitmapPtr* circles15x15;        // circular icons for scale 1:16
extern gBitmapPtr* circles31x31;        // circular icons for scale 1:32

extern gBitmapPtr* diamonds7x7;         // diamond icons for scale 1:8
extern gBitmapPtr* diamonds15x15;       // diamond icons for scale 1:16
extern gBitmapPtr* diamonds31x31;       // diamond icons for scale 1:32

extern gBitmapPtr* hexagons7x7;         // hexagonal icons for scale 1:8
extern gBitmapPtr* hexagons15x15;       // hexagonal icons for scale 1:16
extern gBitmapPtr* hexagons31x31;       // hexagonal icons for scale 1:32

// NOTE: the triangular icons are only suitable for a 4-state rule that
// is emulating a triangular neighborhood with 2 triangles per cell

extern gBitmapPtr* triangles7x7;        // triangular icons for scale 1:8
extern gBitmapPtr* triangles15x15;      // triangular icons for scale 1:16
extern gBitmapPtr* triangles31x31;      // triangular icons for scale 1:32

void InitAlgorithms();
// Initialize above data.  Must be called before reading the prefs file.

void DeleteAlgorithms();
// Deallocate memory allocated in InitAlgorithms().

lifealgo* CreateNewUniverse(algo_type algotype, bool allowcheck = true);
// Create a new universe of given type.  If allowcheck is true then
// event checking is allowed.

const char* GetAlgoName(algo_type algotype);
// Return name of given algorithm.  This name appears in various places
// and is also stored in the prefs file.

int NumAlgos();
// Return current number of algorithms.

gBitmapPtr* CreateIconBitmaps(const char** xpmdata, int maxstates);
// Create icon bitmaps using the given XPM data.

gBitmapPtr* ScaleIconBitmaps(gBitmapPtr* srcicons, int size);
// Return icon bitmaps scaled to given size.

void FreeIconBitmaps(gBitmapPtr* icons);
// Free all the memory used by the given set of icons.

bool MultiColorImage(gBitmapPtr image);
// Return true if image contains at least one color that isn't a shade of gray.

#endif
