// This file is part of Golly.
// See docs/License.html for the copyright notice.

#ifndef _WXALGOS_H_
#define _WXALGOS_H_

#include "lifealgo.h"

// Golly supports multiple algorithms.  The first algorithm
// registered must *always* be qlifealgo.  The second must
// *always* be hlifealgo.  (These are to support old scripts.)

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

// the following bitmaps are grayscale icons that can be used with any rules
// with any number of states

extern wxBitmap** circles7x7;           // circular icons for scale 1:8
extern wxBitmap** circles15x15;         // circular icons for scale 1:16
extern wxBitmap** circles31x31;         // circular icons for scale 1:32

extern wxBitmap** diamonds7x7;          // diamond icons for scale 1:8
extern wxBitmap** diamonds15x15;        // diamond icons for scale 1:16
extern wxBitmap** diamonds31x31;        // diamond icons for scale 1:32

extern wxBitmap** hexagons7x7;          // hexagonal icons for scale 1:8
extern wxBitmap** hexagons15x15;        // hexagonal icons for scale 1:16
extern wxBitmap** hexagons31x31;        // hexagonal icons for scale 1:32

// NOTE: the triangular icons are only suitable for a 4-state rule that
// is emulating a triangular neighborhood with 2 triangles per cell

extern wxBitmap** triangles7x7;         // triangular icons for scale 1:8
extern wxBitmap** triangles15x15;       // triangular icons for scale 1:16
extern wxBitmap** triangles31x31;       // triangular icons for scale 1:32

void InitAlgorithms();
// Initialize above data.  Must be called before reading the prefs file.

void DeleteAlgorithms();
// Deallocate memory allocated in InitAlgorithms().

lifealgo* CreateNewUniverse(algo_type algotype, bool allowcheck = true);
// Create a new universe of given type.  If allowcheck is true then
// event checking is allowed; ie. poller is set to wxGetApp().Poller().

const char* GetAlgoName(algo_type algotype);
// Return name of given algorithm.  This name appears in various menus
// and is also stored in the prefs file.

int NumAlgos();
// Return current number of algorithms.

bool MultiColorImage(wxImage& image);
// Return true if image contains at least one color that isn't a shade of gray.

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
