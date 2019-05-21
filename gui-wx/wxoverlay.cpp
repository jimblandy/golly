// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "wx/wxprec.h"      // for compilers that support precompilation
#ifndef WX_PRECOMP
    #include "wx/wx.h"      // for all others include the necessary headers
#endif

#include "wx/rawbmp.h"      // for wxAlphaPixelData

#include "wxgolly.h"        // for mainptr, viewptr
#include "wxmain.h"         // for mainptr->...
#include "wxview.h"         // for viewptr->...
#include "wxlayer.h"        // for currlayer->...
#include "wxprefs.h"        // for showoverlay
#include "wxutils.h"        // for Warning

#include "wxoverlay.h"

#include <vector>           // for std::vector
#include <cstdio>           // for FILE*, etc
#include <math.h>           // for sin, cos, log, sqrt and atn2
#include <stdlib.h>         // for malloc, free
#include <string.h>         // for strchr, strtok
#include <ctype.h>          // for isspace, isdigit

// Endian definitions
// Note: for big endian platforms you must compile with BIGENDIAN defined
#ifdef BIGENDIAN

// big endian 32bit pixel component order is RGBA

// masks to isolate components in pixel
#define RMASK   0xff000000
#define GMASK   0x00ff0000
#define BMASK   0x0000ff00
#define AMASK   0x000000ff
#define RBMASK  0xff00ff00
#define RBGMASK 0xffffff00

// shift RB components right and left to avoid overflow
// R.B. becomes .R.B
#define RBRIGHT(x) ((x)>>8)
// .R.B becomes R.B.
#define RBLEFT(x)  ((x)<<8)

// get components as byte from pixel
#define RED2BYTE(x)   ((x)>>24)
#define GREEN2BYTE(x) (((x)&GMASK)>>16)
#define BLUE2BYTE(x)  (((x)&BMASK)>>8)
#define ALPHA2BYTE(x) ((x)&AMASK)

// set components from bytes in pixel
#define BYTE2RED(x)   ((x)<<24)
#define BYTE2GREEN(x) ((x)<<16)
#define BYTE2BLUE(x)  ((x)<<8)
#define BYTE2ALPHA(x) (x)

#else

// little endian 32bit pixel component order is ABGR

// masks to isolate components in pixel
#define RMASK   0x000000ff
#define GMASK   0x0000ff00
#define BMASK   0x00ff0000
#define AMASK   0xff000000
#define RBMASK  0x00ff00ff
#define RGBMASK 0x00ffffff

// shift RB components right and left to avoid overflow
// not required for little endian
#define RBRIGHT(x) (x)
#define RBLEFT(x)  (x)

// get components as byte from pixel
#define RED2BYTE(x)   ((x)&RMASK)
#define GREEN2BYTE(x) (((x)&GMASK)>>8)
#define BLUE2BYTE(x)  (((x)&BMASK)>>16)
#define ALPHA2BYTE(x) ((x)>>24)

// set components from bytes in pixel
#define BYTE2RED(x)   (x)
#define BYTE2GREEN(x) ((x)<<8)
#define BYTE2BLUE(x)  ((x)<<16)
#define BYTE2ALPHA(x) ((x)<<24)

#endif

// alpha blend source with opaque destination
#define ALPHABLENDOPAQUEDEST(source, dest, resultptr, alpha, invalpha) \
    { \
        const unsigned int _newrb = (alpha * RBRIGHT(source & RBMASK) + invalpha * RBRIGHT(dest & RBMASK)) >> 8; \
        const unsigned int _newg  = (alpha *        (source & GMASK)  + invalpha *        (dest & GMASK))  >> 8; \
        *resultptr = (RBLEFT(_newrb) & RBMASK) | (_newg & GMASK) | AMASK; \
    }

// alpha blend source with translucent destination
#define ALPHABLENDTRANSDEST(source, dest, resultptr, alpha, invalpha) \
    { \
        const unsigned int _destinva = (ALPHA2BYTE(dest) * invalpha) >> 8; \
        const unsigned int _outa = alpha + _destinva; \
        const unsigned int _newr = (alpha * RED2BYTE(source)   + _destinva * RED2BYTE(dest))   / _outa; \
        const unsigned int _newg = (alpha * GREEN2BYTE(source) + _destinva * GREEN2BYTE(dest)) / _outa; \
        const unsigned int _newb = (alpha * BLUE2BYTE(source)  + _destinva * BLUE2BYTE(dest))  / _outa; \
        *resultptr = BYTE2RED(_newr) | BYTE2GREEN(_newg) | BYTE2BLUE(_newb) | BYTE2ALPHA(_outa - 1); \
    }

// alpha blend source with destination
#define ALPHABLEND(source, dest, resultptr, alpha, invalpha) \
    if ((dest & AMASK) == AMASK) { \
        ALPHABLENDOPAQUEDEST(source, dest, resultptr, alpha, invalpha); \
    } else { \
        ALPHABLENDTRANSDEST(source, dest, resultptr, alpha, invalpha); \
    }

// alpha blend premultiplied source with opaque destination
#define ALPHABLENDPREOPAQUEDEST(sourcearb, sourceag, dest, resultptr, invalpha) \
    { \
        const unsigned int _newrb = (sourcearb + invalpha * RBRIGHT(dest & RBMASK)) >> 8; \
        const unsigned int _newg  = (sourceag  + invalpha *        (dest & GMASK))  >> 8; \
        *resultptr = (RBLEFT(_newrb) & RBMASK) | (_newg & GMASK) | AMASK; \
    }

// alpha blend premultiplied source with destination
#define ALPHABLENDPRE(source, sourcearb, sourceag, dest, resultptr, alpha, invalpha) \
    if ((dest & AMASK) == AMASK) { \
        ALPHABLENDPREOPAQUEDEST(sourcearb, sourceag, dest, resultptr, invalpha); \
    } else { \
        ALPHABLENDTRANSDEST(source, dest, resultptr, alpha, invalpha); \
    }

// -----------------------------------------------------------------------------

Clip::Clip(int w, int h, bool use_calloc) {
    cwd = w;
    cht = h;
    if (use_calloc) {
        cdata = (unsigned char*) calloc(cwd * cht * 4, sizeof(*cdata));
    } else {
        cdata = (unsigned char*) malloc(cwd * cht * 4 * sizeof(*cdata));
    }
    rowindex = NULL;
    // set bounding box to clip extent
    xbb = 0;
    ybb = 0;
    wbb = w;
    hbb = h;
    cdatabb = cdata;
}

Clip::~Clip() {
    if (cdata) {
        free(cdata);
        cdata = NULL;
    }
    RemoveIndex();
}

// compute non-transparent pixel bounding box
void Clip::ComputeBoundingBox() {
    unsigned int *clipdata = (unsigned int*)cdata;

    // discard transparent top rows
    int x, y;
    for (y = 0; y < cht; y++) {
        // use row index if available
        if (rowindex) {
            if (rowindex[y] != alpha0) break;
        } else {
            // otherwise look along the row for non-zero alpha
            x = 0;
            while (x < cwd && (!(*clipdata++ & AMASK))) {
                x++;
            }
            if (x < cwd) break;
        }
    }
    ybb = y;
    hbb = cht - y;

    // discard transparent bottom rows
    if (hbb > 0) {
        clipdata = ((unsigned int*)cdata) + cwd * cht;
        for (y = cht - 1; y > ybb; y--) {
            // use row index if available
            if (rowindex) {
                if (rowindex[y] != alpha0) break;
            } else {
                // otherwise look along the row for non-zero alpha
                x = 0;
                while (x < cwd && (!(*--clipdata & AMASK))) {
                    x++;
                }
                if (x < cwd) break;
            }
        }
        y = cht - 1 - y;
        hbb -= y;

        // discard transparent left columns
        clipdata = (unsigned int*)cdata;
        for (x = 0; x < cwd; x++) {
            y = 0;
            unsigned int *rowdata = clipdata;
            while (y < cht && (!(*rowdata & AMASK))) {
                y++;
                rowdata += cwd;
            }
            if (y < cht) break;
            clipdata++;
        }
        xbb = x;
        wbb = cwd - x;

        // discard transparent right columns
        if (wbb > 0) {
            clipdata = ((unsigned int*)cdata) + cwd;
            for (x = cwd - 1; x > xbb; x--) {
                y = 0;
                clipdata--;
                unsigned int *rowdata = clipdata;
                while (y < cht && (!(*rowdata & AMASK))) {
                    y++;
                    rowdata += cwd;
                }
                if (y < cht) break;
            }
            x = cwd - 1 - x;
            wbb -= x;
        }
    }

    // compute top left pixel in bounding box
    cdatabb = cdata + (ybb * cwd + xbb) * 4;
}

// add row index to the clip
// if there are no rows that can be optimized the index will not be created
void Clip::AddIndex() {
    if (!rowindex) {
        // allocate the index
        rowindex = (rowtype*)malloc(cht * sizeof(*rowindex));
    }
    unsigned int *lp = (unsigned int*)cdata;
    unsigned int alpha;
    unsigned int first;
    bool bothrow = false;
    int j;

    // check each row
    int numopt = 0;
    for (int i = 0; i < cht; i++) {
        // check what type of pixels the row contains
        first = *lp & AMASK;
        alpha = first;
        lp++;
        j = 1;
        bothrow = false;
        // check for all transparent or all opaque
        if (first == 0 || first == AMASK) {
            while (j < cwd && alpha == first) {
                alpha = *lp++ & AMASK;
                j++;
            }
            if (j < cwd) {
                // that failed so check for a mix of transparent and opaque
                while (j < cwd && (alpha == 0 || alpha == AMASK)) {
                    alpha = *lp++ & AMASK;
                    j++;
                }
                if (j == cwd) bothrow = true;
            }
        }

        // set this row's flag
       if (bothrow) {
            numopt++;
            rowindex[i] = both;
        } else if (alpha == 0 && first == 0) {
            numopt++;
            rowindex[i] = alpha0;
        } else if (alpha == AMASK && first == AMASK) {
            numopt++;
            rowindex[i] = opaque;
        } else {
            rowindex[i] = mixed;
        }
        lp += cwd - j;
    }

    // compute non-zero alpha bounding box
    ComputeBoundingBox();

    // remove the index if there were no optimized rows
    if (numopt == 0) RemoveIndex();
}

// remove row index from the clip
void Clip::RemoveIndex() {
    if (rowindex) {
        free(rowindex);
        rowindex = NULL;
    }
};

// -----------------------------------------------------------------------------

const int clipbatch = 16;    // clip batch size for allocation

ClipManager::ClipManager() {
    Clear();
    lsize = clipbatch;
    esize = clipbatch;
    osize = clipbatch;
    hsize = clipbatch;
    lcliplist = (const Clip**)malloc(lsize * sizeof(*lcliplist));
    ecliplist = (const Clip**)malloc(esize * sizeof(*ecliplist));
    ocliplist = (const Clip**)malloc(osize * sizeof(*ocliplist));
    hcliplist = (const Clip**)malloc(hsize * sizeof(*hcliplist));
}

ClipManager::~ClipManager() {
    if (lcliplist) {
        free(lcliplist);
        lcliplist = NULL;
    }
    if (ecliplist) {
        free(ecliplist);
        ecliplist = NULL;
    }
    if (ocliplist) {
        free(ocliplist);
        ocliplist = NULL;
    }
    if (hcliplist) {
        free(hcliplist);
        hcliplist = NULL;
    }
}

void ClipManager::Clear() {
    lclips = 0;
    eclips = 0;
    oclips = 0;
    hclips = 0;
    lclip = NULL;
    eclip = NULL;
    oclip = NULL;
    sclip = NULL;
    pclip = NULL;
    aclip = NULL;
    lnaclip = NULL;
    snaclip = NULL;
    elnaclip = NULL;
    olnaclip = NULL;
    hclip = NULL;
    hnaclip = NULL;
}

void ClipManager::AddLiveClip(const Clip *liveclip) {
    if (lclips == lsize) {
        // allocate more memory
        lsize += clipbatch;
        lcliplist = (const Clip**)realloc(lcliplist, lsize * sizeof(*lcliplist));
    }
    lcliplist[lclips++] = liveclip;
}

void ClipManager::AddEvenClip(const Clip *clip) {
    if (eclips == esize) {
        // allocate more memory
        esize += clipbatch;
        ecliplist = (const Clip**)realloc(ecliplist, esize * sizeof(*ecliplist));
    }
    ecliplist[eclips++] = clip;
}

void ClipManager::AddOddClip(const Clip *clip) {
    if (oclips == osize) {
        // allocate more memory
        osize += clipbatch;
        ocliplist = (const Clip**)realloc(ocliplist, osize * sizeof(*ocliplist));
    }
    ocliplist[oclips++] = clip;
}

void ClipManager::AddHistoryClip(const Clip *clip) {
    if (hclips == hsize) {
        // allocate more memory
        hsize += clipbatch;
        hcliplist = (const Clip**)realloc(hcliplist, hsize * sizeof(*hcliplist));
    }
    hcliplist[hclips++] = clip;
}

const Clip **ClipManager::GetLiveClips(int *numclips) {
    *numclips = lclips;
    return lcliplist;
}

const Clip **ClipManager::GetEvenClips(int *numclips) {
    *numclips = eclips;
    return ecliplist;
}

const Clip **ClipManager::GetOddClips(int *numclips) {
    *numclips = oclips;
    return ocliplist;
}

const Clip **ClipManager::GetHistoryClips(int *numclips) {
    *numclips = hclips;
    return hcliplist;
}

void ClipManager::SetLiveClip(const Clip *liveclip) {
    lclip = liveclip;
}

void ClipManager::SetOddClip(const Clip *oddclip) {
    oclip = oddclip;
}

void ClipManager::SetEvenClip(const Clip *evenclip) {
    eclip = evenclip;
}

void ClipManager::SetSelectClip(const Clip *selectclip) {
    sclip = selectclip;
}

void ClipManager::SetPasteClip(const Clip *pasteclip) {
    pclip = pasteclip;
}

void ClipManager::SetLiveNotActiveClip(const Clip *livenaclip) {
    lnaclip = livenaclip;
}

void ClipManager::SetSelectNotActiveClip(const Clip *selectnaclip) {
    snaclip = selectnaclip;
}

void ClipManager::SetEvenLiveNotActiveClip(const Clip *evennaclip) {
    elnaclip = evennaclip;
}

void ClipManager::SetOddLiveNotActiveClip(const Clip *oddnaclip) {
    olnaclip = oddnaclip;
}

void ClipManager::SetActiveClip(const Clip *activeclip) {
    aclip = activeclip;
}

void ClipManager::SetHistoryClip(const Clip *historyclip) {
    hclip = historyclip;
}

void ClipManager::SetHistoryNotActiveClip(const Clip *historynaclip) {
    hnaclip = historynaclip;
}

const Clip *ClipManager::GetLiveClip(int *clipwd) {
    if (lclip && clipwd) *clipwd = lclip->cwd;
    return lclip;
}

const Clip *ClipManager::GetOddClip(int *clipwd) {
    if (oclip && clipwd) *clipwd = oclip->cwd;
    return oclip;
}

const Clip *ClipManager::GetEvenClip(int *clipwd) {
    if (eclip && clipwd) *clipwd = eclip->cwd;
    return eclip;
}

const Clip *ClipManager::GetSelectClip(int *clipwd) {
    if (sclip && clipwd) *clipwd = sclip->cwd;
    return sclip;
}

const Clip *ClipManager::GetPasteClip(int *clipwd) {
    if (pclip && clipwd) *clipwd = pclip->cwd;
    return pclip;
}

const Clip *ClipManager::GetActiveClip(int *clipwd) {
    if (aclip && clipwd) *clipwd = aclip->cwd;
    return aclip;
}

const Clip *ClipManager::GetLiveNotActiveClip(int *clipwd) {
    if (lnaclip && clipwd) *clipwd = lnaclip->cwd;
    return lnaclip;
}

const Clip *ClipManager::GetSelectNotActiveClip(int *clipwd) {
    if (snaclip && clipwd) *clipwd = snaclip->cwd;
    return snaclip;
}

const Clip *ClipManager::GetEvenLiveNotActiveClip(int *clipwd) {
    if (elnaclip && clipwd) *clipwd = elnaclip->cwd;
    return elnaclip;
}

const Clip *ClipManager::GetOddLiveNotActiveClip(int *clipwd) {
    if (olnaclip && clipwd) *clipwd = olnaclip->cwd;
    return olnaclip;
}

const Clip *ClipManager::GetHistoryClip(int *clipwd) {
    if (hclip && clipwd) *clipwd = hclip->cwd;
    return hclip;
}

const Clip *ClipManager::GetHistoryNotActiveClip(int *clipwd) {
    if (hnaclip && clipwd) *clipwd = hnaclip->cwd;
    return hnaclip;
}

// -----------------------------------------------------------------------------

Table::Table() {
    nkeys = 0;
    keys = NULL;
    values = NULL;
    exists = NULL;
}

Table::~Table() {
    FreeMemory();
}

bool Table::SetSize(int sz) {
    // reallocate memory
    FreeMemory();
    size = sz;
    nkeys = 0;
    return AllocateMemory();
}

void Table::Clear() {
    // clear the table
    if (nkeys > 0) {
        ClearKeys();
        memset(values, 0, size * sizeof(*values));
    }
}

void Table::ClearKeys() {
    // zero the number of keys and exists flags
    nkeys = 0;
    memset(exists, 0, size * sizeof(*exists));
}

const int *Table::GetKeys(int *numkeys) {
    // return the number of keys
    if (numkeys) *numkeys = nkeys;

    // return the list of keys
    return keys;
}

const int Table::GetNumKeys() {
    // return the number of keys
    return nkeys;
}

const unsigned char *Table::GetValues() {
    // return the list of values
    return values;
}

void Table::SetValue(const int key, const unsigned char value) {
    // check if the key exists
    if (!exists[key]) {
        // create a new key
        keys[nkeys++] = key;

        // mark the key as exists
        exists[key] = true;
    }

    // set the value at the key
    values[key] = value;
}

void Table::SetTo1(const int key) {
    // check if the key exists
    if (!exists[key]) {
        // create a new key
        keys[nkeys++] = key;

        // mark the key as exists
        exists[key] = true;
    }

    // set the value at the key
    values[key] = 1;
}

void Table::AddToValue(const int key, const unsigned char amount) {
    // check if the key exists
    if (!exists[key]) {
        // create a new key
        keys[nkeys++] = key;

        // set the value to the amount
        values[key] = amount;

        // mark the key as exists
        exists[key] = true;
    } else {
        // add the amount to the value at the key
        values[key] += amount;
    }
}

void Table::DecrementTo1(const int key) {
    // decrement the value if above 1
    if (values[key] > 1) values[key]--;
}

void Table::SortKeys() {
    int *key = keys;
    char *exist = exists;
    int *lastkey = key + nkeys;
    // sort keys into ascending order
    while (key < lastkey) {
        if (*exist) {
            *key++ = exist - exists;
        }
        exist++;
    }
}

void Table::Copy(const Table &from) {
    if (from.size != size) {
        SetSize(from.size);
    }
    nkeys = from.nkeys;
    memcpy(values, from.values, size * sizeof(*values));
    memcpy(exists, from.exists, size * sizeof(*exists));
    // create keys in ascending order from exists flags
    SortKeys();
}

void Table::FreeMemory() {
    if (values) {
        free(values);
        values = NULL;
    }
    if (keys) {
        free(keys);
        keys = NULL;
    }
    if (exists) {
        free(exists);
        exists = NULL;
    }
}

bool Table::AllocateMemory() {
    // allocate keys
    keys = (int*)malloc(size * sizeof(*keys));

    // allocate and clear values and key exists
    values = (unsigned char*)calloc(size, sizeof(*values));
    exists = (char*)calloc(size, sizeof(*exists));

    // check allocation succeeded
    if (keys == NULL || values == NULL || exists == NULL) {
        FreeMemory();
        return false;
    }
    return true;
}

// -----------------------------------------------------------------------------

// some useful macros:

#if !wxCHECK_VERSION(2,9,0)
    #define wxImageResizeQuality int
#endif

#ifdef __WXMSW__
    #define round(x) int( (x) < 0 ? (x)-0.5 : (x)+0.5 )
    #define remainder(n,d) ( (n) - round((n)/(d)) * (d) )
#endif

#define PixelInOverlay(x,y) \
    (((unsigned int)(x)) < (unsigned int)ovwd && ((unsigned int)(y)) < (unsigned int)ovht)

#define PixelInTarget(x,y) \
    (((unsigned int)(x)) < (unsigned int)wd && ((unsigned int)(y)) < (unsigned int)ht)

#define RectOutsideTarget(x,y,w,h) \
    (x >= wd || x + w <= 0 || \
     y >= ht || y + h <= 0)

#define RectInsideTarget(x,y,w,h) \
    (x >= 0 && x + w <= wd && \
     y >= 0 && y + h <= ht)

// -----------------------------------------------------------------------------

Overlay *curroverlay = NULL;        // pointer to current overlay

const char *no_overlay = "overlay has not been created";
const char *no_cellview = "overlay does not have a cell view";

const int cellviewmaxsize = 4096;   // maximum dimension for cell view
const int cellviewmultiple = 16;    // cellview dimensions must be a multiple of this value

// for camera
const double camminzoom = 0.0625;   // minimum zoom
const double cammaxzoom = 32.0;     // maximum zoom

// for theme
const int aliveStart = 64;          // new cell color index
const int aliveEnd = 127;           // cell alive longest color index
const int deadStart = 63;           // cell just died color index
const int deadEnd = 1;              // cell dead longest color index

// for stars
const int numStars = 10000;         // number of stars in starfield
const int starMaxX = 8192;
const int starMaxY = 8192;
const int starMaxZ = 1024;
const double degToRad = M_PI / 180;
const double radToDeg = 180 / M_PI;

// for replace
const int matchany = -1;            // match any component value

#ifdef __WXMAC__
    // on Mac we'll need to increase the line height of text by 1 or 2 pixels to avoid
    // a GetTextExtent bug that clips the bottom pixels of descenders like "gjpqy"
    static int extraht;
#endif

#ifdef ENABLE_SOUND
    static ISoundEngine *engine = NULL;
#endif

// -----------------------------------------------------------------------------

Overlay::Overlay()
{
    pixmap = NULL;
    ovpixmap = NULL;
    cellview = NULL;
    cellview1 = NULL;
    zoomview = NULL;
    starx = NULL;
    stary = NULL;
    starz = NULL;
    renderclip = NULL;
    #ifdef ENABLE_SOUND
    // initialize sound engine once (avoids "Could not add IO Proc for CoreAudio" warnings in Mac console)
    if (engine == NULL) {
        engine = createIrrKlangDevice(ESOD_AUTO_DETECT, ESEO_MULTI_THREADED | ESEO_LOAD_PLUGINS | ESEO_USE_3D_BUFFERS);
        if (!engine) Warning(_("Unable to initialize sound!"));
    }
    #endif

    // 3D
    stepsize = 1;
    depthshading = false;
    celltype = cube;
    gridsize = 0;
    showhistory = 0;
    fadehistory = false;
    modN = NULL;
    modNN = NULL;
    xyz = NULL;
    xaxis = NULL;
    yaxis = NULL;
    zaxis = NULL;
}

// -----------------------------------------------------------------------------

Overlay::~Overlay()
{
    DeleteOverlay();
}

// -----------------------------------------------------------------------------

void Overlay::DeleteOverlay()
{
    if (ovpixmap) {
        free(ovpixmap);
        ovpixmap = NULL;
    }
    pixmap = NULL;

    // delete clips
    std::map<std::string,Clip*>::iterator it;
    for (it = clips.begin(); it != clips.end(); ++it) {
        delete it->second;
    }
    clips.clear();

    #ifdef ENABLE_SOUND
    // stop any sound playback and delete cached sounds
    if (engine) {
        // delete sounds
        std::map<std::string,ISound*>::iterator itsnd;
        for (itsnd = sounds.begin(); itsnd != sounds.end(); ++itsnd) {
            itsnd->second->drop();
        }
        sounds.clear();
    }
    #endif

    // delete cellview
    DeleteCellView();

    // free div table
    FreeDivTable();

    // clear axis flags
    FreeAxisFlags();
}

// -----------------------------------------------------------------------------

void Overlay::DeleteStars()
{
    if (starx) {
        free(starx);
        starx = NULL;
    }
    if (stary) {
        free(stary);
        stary = NULL;
    }
    if (starz) {
        free(starz);
        starz = NULL;
    }
}

// -----------------------------------------------------------------------------

void Overlay::DeleteCellView()
{
    if (cellview) {
        free(cellview);
        cellview = NULL;
    }
    if (cellview1) {
        free(cellview1);
        cellview1 = NULL;
    }
    if (zoomview) {
        free(zoomview);
        zoomview = NULL;
    }
    DeleteStars();
}

// -----------------------------------------------------------------------------

void Overlay::SetRGBA(unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha, unsigned int *rgba)
{
    unsigned char *rgbaptr = (unsigned char *)rgba;
    *rgbaptr++ = red;
    *rgbaptr++ = green;
    *rgbaptr++ = blue;
    *rgbaptr   = alpha;
}

// -----------------------------------------------------------------------------

void Overlay::GetRGBA(unsigned char *red, unsigned char *green, unsigned char *blue, unsigned char *alpha, unsigned int rgba)
{
    unsigned char *rgbaptr = (unsigned char *)&rgba;
    *red   = *rgbaptr++;
    *green = *rgbaptr++;
    *blue  = *rgbaptr++;
    *alpha = *rgbaptr;
}

// -----------------------------------------------------------------------------

void Overlay::RefreshCellViewWithTheme()
{
    // refresh the cellview for a 2 state pattern using LifeViewer theme
    unsigned char *cellviewptr = cellview;
    unsigned char *cellviewptr1 = cellview1;
    const unsigned char *end = cellview + (cellwd * cellht);

    // get the cells in the cell view
    lifealgo *algo = currlayer->algo;
    algo->getcells(cellviewptr1, cellx, celly, cellwd, cellht);

    // update based on the theme
    while (cellviewptr < end) {
        const unsigned char state = *cellviewptr;
        if (*cellviewptr1++) {
            // new cell is alive
            if (state >= aliveStart) {
                // cell was already alive
                if (state < aliveEnd) *cellviewptr = state + 1;
            } else {
                // cell just born
                *cellviewptr = aliveStart;
            }
        } else {
            // new cell is dead
            if (state >= aliveStart) {
                // cell just died
                *cellviewptr = deadStart;
            } else {
                // cell is decaying
                if (state > deadEnd) *cellviewptr = state - 1;
            }
        }
        cellviewptr++;
    }
}

// -----------------------------------------------------------------------------

void Overlay::RefreshCellView()
{
    lifealgo *algo = currlayer->algo;

    // read cells into the buffer
    algo->getcells(cellview, cellx, celly, cellwd, cellht);
}

// -----------------------------------------------------------------------------

void Overlay::GetPatternColors()
{
    unsigned long *rgba = (unsigned long *)cellRGBA;

    // read pattern colors
    const int numicons = currlayer->numicons;
    const unsigned char *cellr = currlayer->cellr;
    const unsigned char *cellg = currlayer->cellg;
    const unsigned char *cellb = currlayer->cellb;

    for (int i = 0; i <= numicons; i++) {
        *rgba++ = BYTE2RED(cellr[i]) | BYTE2GREEN(cellg[i]) | BYTE2BLUE(cellb[i]) | AMASK;
    }

    // read border color from View Settings
    unsigned char borderr = borderrgb->Red();
    unsigned char borderg = borderrgb->Green();
    unsigned char borderb = borderrgb->Blue();
    unsigned char alpha = 255; // opaque
    SetRGBA(borderr, borderg, borderb, alpha, &borderRGBA);
}

// -----------------------------------------------------------------------------

void Overlay::GetThemeColors(double brightness)
{
    unsigned char *rgb = (unsigned char *)cellRGBA;

    // cell born color
    unsigned char aliveStartR, aliveStartG, aliveStartB, aliveStartA;

    // cell alive long time color
    unsigned char aliveEndR, aliveEndG, aliveEndB;

    // cell just died color
    unsigned char deadStartR, deadStartG, deadStartB, deadStartA;

    // cell dead long time color
    unsigned char deadEndR, deadEndG, deadEndB;

    // cell never occupied color
    unsigned char unoccupiedR, unoccupiedG, unoccupiedB, unoccupiedA;

    // get the color rgb components
    GetRGBA(&aliveStartR, &aliveStartG, &aliveStartB, &aliveStartA, aliveStartRGBA);
    GetRGBA(&aliveEndR, &aliveEndG, &aliveEndB, &aliveStartA, aliveEndRGBA);
    GetRGBA(&deadStartR, &deadStartG, &deadStartB, &deadStartA, deadStartRGBA);
    GetRGBA(&deadEndR, &deadEndG, &deadEndB, &deadStartA, deadEndRGBA);
    GetRGBA(&unoccupiedR, &unoccupiedG, &unoccupiedB, &unoccupiedA, unoccupiedRGBA);

    // set never occupied cell color
    *rgb++ = unoccupiedR;
    *rgb++ = unoccupiedG;
    *rgb++ = unoccupiedB;
    *rgb++ = unoccupiedA;

    // set decaying colors
    for (int i = deadEnd; i <= deadStart; i++) {
        const double weight = 1 - ((double)(i - deadEnd) / (deadStart - deadEnd));
        *rgb++ = deadStartR * (1 - weight) + deadEndR * weight;
        *rgb++ = deadStartG * (1 - weight) + deadEndG * weight;
        *rgb++ = deadStartB * (1 - weight) + deadEndB * weight;
        *rgb++ = deadStartA;
    }

    // set living colors
    for (int i = aliveStart; i <= aliveEnd; i++) {
        const double weight = 1 - ((double)(i - aliveStart) / (aliveEnd - aliveStart));
        *rgb++ = (aliveStartR * weight + aliveEndR * (1 - weight)) * brightness;
        *rgb++ = (aliveStartG * weight + aliveEndG * (1 - weight)) * brightness;
        *rgb++ = (aliveStartB * weight + aliveEndB * (1 - weight)) * brightness;
        *rgb++ = aliveStartA;
    }

    // read border color from View Settings
    unsigned char borderr = borderrgb->Red();
    unsigned char borderg = borderrgb->Green();
    unsigned char borderb = borderrgb->Blue();
    SetRGBA(borderr, borderg, borderb, bordera, &borderRGBA);
}

// -----------------------------------------------------------------------------

void Overlay::UpdateZoomView(unsigned char *source, unsigned char *dest, const unsigned int step)
{
    unsigned char state;
    unsigned char max;
    const unsigned int halfstep = step >> 1;
    const unsigned int ystep = step * cellwd;
    unsigned char *row1 = source;
    unsigned char *row2 = source + halfstep * cellwd;

    for (unsigned int h = 0; h < cellht; h += step) {
        for (unsigned int w = 0; w < cellwd; w += step) {
            // find the maximum state value in each 2x2 block
            max = row1[w];
            state = row1[w + halfstep];
            if (state > max) max = state;
            state = row2[w];
            if (state > max) max = state;
            state = row2[w + halfstep];
            if (state > max) max = state;
            dest[w] = max;
        }

        // update row pointers
        row1 += ystep;
        row2 += ystep;
        dest += ystep;
    }
}

// -----------------------------------------------------------------------------

const char *Overlay::DoDrawCells()
{
    if (cellview == NULL) return OverlayError(no_cellview);

    int mask = 0;
    unsigned char *cells = cellview;
    unsigned char *source = cellview;
    unsigned char *dest = zoomview;

    // check for zoom < 1
    if (camzoom < 1) {
        int negzoom = (1 / camzoom) - 0.001;
        int step = 2;
        do {
            UpdateZoomView(source, dest, step);

            // next zoom level
            step <<= 1;
            negzoom >>= 1;
            mask = (mask << 1) | 1;

            // update source and destination
            source = dest;
            dest = zoomview + (step >> 1) - 1;
        } while (negzoom >= 1);

        // source is the zoom view
        cells = source;
    }

    // check for hex
    double angle = camangle;
    if (ishex) {
        angle = 0;
    }

    // pick renderer based on whether camera is rotated
    if (angle == 0) {
        DrawCellsNoRotate(cells, ~mask);
    } else {
        DrawCellsRotate(cells, ~mask, angle);
    }

    // draw stars if enabled
    if (stars) {
        DrawStars(angle);
    }

    // mark target clip as changed
    DisableTargetClipIndex();

    return NULL;
}

// -----------------------------------------------------------------------------

void Overlay::DrawCellsRotate(unsigned char *cells, int mask, double angle)
{
    // convert depth to actual depth
    const double depth = camlayerdepth / 2 + 1;

    // check pixel brightness depending on layers
    double brightness = 1;
    double brightinc = 0;

    // check whether to draw layers
    int layertarget = 0;
    if (theme && camlayers > 1 && depth > 1) {
        brightness = 0.6;
        brightinc = 0.4 / (camlayers - 1);
        layertarget = camlayers;
    }

    // refresh the cell view
    if (theme) {
        // using theme colors
        GetThemeColors(brightness);
    } else {
        // using standard pattern colors
        GetPatternColors();
    }

    // compute deltas in horizontal and vertical direction based on rotation
    double dxy = sin(angle / 180 * M_PI) / camzoom;
    double dyy = cos(angle / 180 * M_PI) / camzoom;

    double sy = -((wd / 2) * (-dxy) + (ht / 2) * dyy) + camy;
    double sx = -((wd / 2) * dyy + (ht / 2) * dxy) + camx;

    unsigned char state;
    unsigned int *overlayptr = (unsigned int *)pixmap;
    double x, y;

    // draw each pixel
    y = sy;
    const int height = ht;
    const int width = wd;
    for (int h = 0; h < height; h++) {
        x = sx;

        // offset if hex rule
        if (ishex) {
            x += 0.5 * (int)y;
        }

        // check if entire row in on the grid
        unsigned int ix = (((int)x) & mask);
        unsigned int iy = (((int)y) & mask);
        unsigned int tx = (((int)(x + dyy * wd)) & mask);
        unsigned int ty = (((int)(y - dxy * wd)) & mask);
        if (ix < cellwd && iy < cellht && tx < cellwd && ty < cellht) {
            for (int w = 0; w < width; w++) {
                ix = (((int)x) & mask);
                iy = (((int)y) & mask);
                state = cells[cellwd * iy + ix];
                *overlayptr++ = cellRGBA[state];
                x += dyy;
                y -= dxy;
            }
        } else {
            for (int w = 0; w < width; w++) {
                ix = (((int)x) & mask);
                iy = (((int)y) & mask);

                // check if pixel is in the cell view
                if (ix < cellwd && iy < cellht) {
                    state = cells[cellwd * iy + ix];
                    *overlayptr++ = cellRGBA[state];
                } else {
                    *overlayptr++ = borderRGBA;
                }

                // update row position
                x += dyy;
                y -= dxy;
            }
        }
        // update column position
        sx += dxy;
        sy += dyy;
        y = sy;
    }

    // draw grid lines if enabled
    if (grid && angle == 0 && camzoom >= 4) {
        DrawGridLines();
    }

    // draw any layers
    if (theme) {
        double layerzoom = camzoom;
        int zoomlevel;

        for (int i = 1; i < layertarget; i++) {
            unsigned char transparenttarget = (i * ((aliveEnd + 1) / camlayers));

            // update brightness
            brightness += brightinc;
            GetThemeColors(brightness);

            // adjust zoom for next level
            dxy /= depth;
            dyy /= depth;
            layerzoom *= depth;
            cells = cellview;
            zoomlevel = 0;
            mask = ~0;

            // compute which zoomview level to use for this layer
            if (layerzoom < 0.125) {
                zoomlevel = 8;
            } else {
                if (layerzoom < 0.25) {
                    zoomlevel = 4;
                } else {
                    if (layerzoom < 0.5) {
                        zoomlevel = 2;
                    } else {
                        if (layerzoom < 1) {
                            zoomlevel = 1;
                        }
                    }
                }
            }

            // setup the mask for the zoom level
            if (zoomlevel > 0) {
                mask = ~((zoomlevel << 1) - 1);
                cells = zoomview + zoomlevel - 1;
            }

            sy = -((wd / 2) * (-dxy) + (ht / 2) * dyy) + camy;
            sx = -((wd / 2) * dyy + (ht / 2) * dxy) + camx;

            overlayptr = (unsigned int *)pixmap;

            // draw each pixel
            y = sy;
            for (int h = 0; h < ht; h++) {
                x = sx;

                // offset if hex rule
                if (ishex) {
                    x += 0.5 * (int)y;
                }

                // check if entire row in on the grid
                unsigned int ix = (((int)x) & mask);
                unsigned int iy = (((int)y) & mask);
                unsigned int tx = (((int)(x + dyy * wd)) & mask);
                unsigned int ty = (((int)(y - dxy * wd)) & mask);
                if (ix < cellwd && iy < cellht && tx < cellwd && ty < cellht) {
                    for (int w = 0; w < wd; w++) {
                        ix = (((int)x) & mask);
                        iy = (((int)y) & mask);
                        state = cells[cellwd * iy + ix];
                        if (state >= transparenttarget) {
                            *overlayptr = cellRGBA[state];
                        }
                        overlayptr++;
                        x += dyy;
                        y -= dxy;
                    }
                } else {
                    for (int w = 0; w < wd; w++) {
                        ix = (((int)x) & mask);
                        iy = (((int)y) & mask);

                        // check if pixel is on the grid
                        if (ix < cellwd && iy < cellht) {
                            state = cells[cellwd * iy + ix];

                            // check if it is transparent
                            if (state >= transparenttarget) {
                                // draw the pixel
                                *overlayptr = cellRGBA[state];
                            }
                        }
                        overlayptr++;

                        // update row position
                        x += dyy;
                        y -= dxy;
                    }
                }

                // update column position
                sx += dxy;
                sy += dyy;
                y = sy;
            }
        }
    }
}

// -----------------------------------------------------------------------------

void Overlay::DrawCellsNoRotate(unsigned char *cells, int mask)
{
    // convert depth to actual depth
    const double depth = camlayerdepth / 2 + 1;

    // check pixel brightness depending on layers
    double brightness = 1;
    double brightinc = 0;

    // check whether to draw layers
    int layertarget = 0;
    if (theme && camlayers > 1 && depth > 1) {
        brightness = 0.6;
        brightinc = 0.4 / (camlayers - 1);
        layertarget = camlayers;
    }

    // refresh the cell view
    if (theme) {
        // using theme colors
        GetThemeColors(brightness);
    } else {
        // using standard pattern colors
        GetPatternColors();
    }

    // compute deltas in horizontal and vertical direction
    double dyy = 1 / camzoom;

    double sy = -((ht / 2) * dyy) + camy;
    double sx = -((wd / 2) * dyy) + camx;

    unsigned char state;
    unsigned int  *overlayptr = (unsigned int *)pixmap;
    unsigned char *rowptr;
    double x, y;

    // draw each pixel
    y = sy;
    for (int h = 0; h < ht; h++) {
        unsigned int iy = (((int)y) & mask);

        // clip to the grid
        if (iy < cellht) {
            // get the row
            rowptr = cells + cellwd * iy;
            x = sx;

            // offset if hex rule
            if (ishex) {
                x += 0.5 * (int)y;
            }

            // check if the whole row is on the grid
            unsigned int ix = (((int)x) & mask);
            unsigned int tx = (((int)(x + dyy * wd)) & mask);
            if (ix < cellwd && tx < cellwd) {
                for (int w = 0; w < wd; w++) {
                    ix = (((int)x) & mask);
                    state = rowptr[ix];
                    *overlayptr++ = cellRGBA[state];
                    x += dyy;
                }
            } else {
                for (int w = 0; w < wd; w++) {
                    // check if pixel is in the cell view
                    ix = (((int)x) & mask);
                    if (ix < cellwd) {
                        state = rowptr[ix];
                        *overlayptr++ = cellRGBA[state];
                    } else {
                        *overlayptr++ = borderRGBA;
                    }

                    // update row position
                    x += dyy;
                }
            }
        } else {
            // draw off grid row
            for (int w = 0; w < wd; w++) {
                // draw pixel
                *overlayptr++ = borderRGBA;
            }
        }

        // update column position
        sy += dyy;
        y = sy;
    }

    // draw grid lines if enabled
    if (grid && camzoom >= 4) {
        DrawGridLines();
    }

    // draw any layers
    if (theme) {
        double layerzoom = camzoom;
        int zoomlevel;

        for (int i = 1; i < layertarget; i++) {
            unsigned char transparenttarget = (i * ((aliveEnd + 1) / camlayers));

            // update brightness
            brightness += brightinc;
            GetThemeColors(brightness);

            // adjust zoom for next level
            dyy /= depth;
            layerzoom *= depth;
            cells = cellview;
            zoomlevel = 0;
            mask = ~0;

            // compute which zoomview level to use for this layer
            if (layerzoom < 0.125) {
                zoomlevel = 8;
            } else {
                if (layerzoom < 0.25) {
                    zoomlevel = 4;
                } else {
                    if (layerzoom < 0.5) {
                        zoomlevel = 2;
                    } else {
                        if (layerzoom < 1) {
                            zoomlevel = 1;
                        }
                    }
                }
            }

            // setup the mask for the zoom level
            if (zoomlevel > 0) {
                mask = ~((zoomlevel << 1) - 1);
                cells = zoomview + zoomlevel - 1;
            }

            sy = -((ht / 2) * dyy) + camy;
            sx = -((wd / 2) * dyy) + camx;

            overlayptr = (unsigned int *)pixmap;

            // draw each pixel
            y = sy;
            for (int h = 0; h < ht; h++) {
                unsigned int iy = (((int)y) & mask);

                // clip to the grid
                if (iy < cellht) {
                    // get the row
                    rowptr = cells + cellwd * iy;
                    x = sx;

                    // offset if hex rule
                    if (ishex) {
                        x += 0.5 * (int)y;
                    }

                    // check if the whole row is on the grid
                    unsigned int ix = (((int)x) & mask);
                    unsigned int tx = (((int)(x + dyy * wd)) & mask);
                    if (ix < cellwd && tx < cellwd) {
                        for (int w = 0; w < wd; w++) {
                            ix = (((int)x) & mask);
                            state = rowptr[ix];
                            if (state >= transparenttarget) {
                                *overlayptr = cellRGBA[state];
                            }
                            overlayptr++;
                            x += dyy;
                        }
                    } else {
                        for (int w = 0; w < wd; w++) {
                            // check if pixel is on the grid
                            ix = (((int)x) & mask);
                            if (ix < cellwd) {
                                state = rowptr[ix];
                                // check if it is transparent
                                if (state >= transparenttarget) {
                                    // draw the pixel
                                    *overlayptr = cellRGBA[state];
                                }
                            }
                            overlayptr++;
                            // update row position
                            x += dyy;
                        }
                    }
                } else {
                    // draw off grid row
                    for (int w = 0; w < wd; w++) {
                        // draw pixel
                        *overlayptr++ = borderRGBA;
                    }
                }

                // update column position
                sy += dyy;
                y = sy;
            }
        }
    }
}

// -----------------------------------------------------------------------------

const char *Overlay::DoUpdateCells()
{
    if (cellview == NULL) return OverlayError(no_cellview);

    // check if themes are used
    if (theme) {
        RefreshCellViewWithTheme();
    } else {
        RefreshCellView();
    }

    return NULL;
}

// -----------------------------------------------------------------------------

void Overlay::DrawVLine(int x, int y1, int y2, unsigned int color)
{
    // check the line is on the display
    if (x < 0 || x >= wd) {
        return;
    }

    // clip the line to the display
    if (y1 < 0) {
        y1 = 0;
    } else {
        if (y1 >= ht) {
            y1 = ht - 1;
        }
    }
    if (y2 < 0) {
        y2 = 0;
    } else {
        if (y2 >= ht) {
            y2 = ht - 1;
        }
    }

    // ensure the y coordinates are in ascending order
    if (y1 > y2) {
        int temp = y1;
        y1 = y2;
        y2 = temp;
    }

    // get the starting pixel
    unsigned int *pix = ((unsigned int*)pixmap) + y1 * wd + x;

    // draw down the column
    while (y1 <= y2) {
        *pix = color;
        pix += wd;
        y1++;
    }
}

// -----------------------------------------------------------------------------

void Overlay::DrawHLine(int x1, int x2, int y, unsigned int color)
{
    // check the line is on the display
    if (y < 0 || y >= ht) {
        return;
    }

    // clip the line to the display
    if (x1 < 0) {
        x1 = 0;
    } else {
        if (x1 >= wd) {
            x1 = wd - 1;
        }
    }
    if (x2 < 0) {
        x2 = 0;
    } else {
        if (x2 >= wd) {
            x2 = wd - 1;
        }
    }

    // ensure the x coordinates are in ascending order
    if (x1 > x2) {
        int temp = x1;
        x1 = x2;
        x2 = temp;
    }

    // get the starting pixel
    unsigned int *pix = ((unsigned int*)pixmap) + y * wd + x1;

    // draw along the row
    while (x1 <= x2) {
        *pix++ = color;
        x1++;
    }
}

// -----------------------------------------------------------------------------

void Overlay::DrawGridLines()
{
    double x, y;
    unsigned char shade;
    bool light = false;

    // check if background is light or dark
    unsigned char red, green, blue, alpha;
    GetRGBA(&red, &green, &blue, &alpha, cellRGBA[0]);
    if ((red + green + blue) / 3 >= 128) {
        light = true;
    }

    // check if custom grid line color is defined
    if (!customgridcolor) {
        // no custom grid color defined to base it on background color
        shade = light ? 229 : 80;
        SetRGBA(shade, shade, shade, 255, &gridRGBA);
    }

    // check if custom major grid line color is defined
    if (!customgridmajorcolor) {
        // no custom grid color defined to base it on background color
        shade = light ? 209 : 112;
        SetRGBA(shade, shade, shade, 255, &gridmajorRGBA);
    }

    // compute single cell offset
    double xoff = remainder(((cellwd / 2 - camx + 0.5) * camzoom) + (wd / 2), camzoom);
    double yoff = remainder(((cellht / 2 - camy + 0.5) * camzoom) + (ht / 2), camzoom);

    // draw twice if major grid lines enabled
    int loop = 1;
    if (gridmajor > 0) {
        loop = 2;
    }

    // start drawing the grid lines
    unsigned int targetRGBA = gridRGBA;
    unsigned int drawRGBA = gridRGBA;
    int gridlineNum;
    int vlineNum;

    while (loop) {
        // compute grid line vertical offset
        gridlineNum = floor(-(wd / 2 / camzoom) - (cellwd / 2 - camx));

        // draw vertical lines
        for (x = 0; x <= wd * camzoom; x += camzoom) {
            if (gridmajor > 0) {
                // choose whether to use major or minor color
                if (gridlineNum % gridmajor == 0) {
                    drawRGBA = gridmajorRGBA;
                } else {
                    drawRGBA = gridRGBA;
                }
            }
            gridlineNum++;

            // check whether to draw the line
            if (drawRGBA == targetRGBA) {
                // check for hex display
                if (ishex) {
                    vlineNum = (int)(-(ht / 2 / camzoom) - (cellht / 2 - camy));

                    // draw staggered vertical line
                    for (y = yoff - camzoom; y <= ht + camzoom; y += camzoom) {
                        if ((vlineNum & 1) != 0) {
                            DrawVLine(round(x + xoff + camzoom / 2), round(y + camzoom / 2), round(y + camzoom / 2 +  camzoom - 1), drawRGBA);
                        } else {
                            DrawVLine(round(x + xoff + camzoom), round(y + camzoom / 2), round(y + camzoom / 2 + camzoom - 1), drawRGBA);
                        }
                        vlineNum++;
                    }
                } else {
                    DrawVLine(round(x + xoff + camzoom / 2), 0, ht - 1, drawRGBA);
                }
            }
        }

        // compute grid line horizontal offset
        gridlineNum = (int)(-(ht / 2 / camzoom) - (cellht / 2 - camy));

        // draw horizontal lines
        for (y = 0; y <= ht + camzoom; y += camzoom) {
            if (gridmajor > 0) {
                // choose whether to use major or minor color
                if (gridlineNum % gridmajor == 0) {
                    drawRGBA = gridmajorRGBA;
                } else {
                    drawRGBA = gridRGBA;
                }
            }
            gridlineNum++;

            // check whether to draw the line
            if (drawRGBA == targetRGBA) {
                DrawHLine(0, wd - 1, round(y + yoff + camzoom / 2), drawRGBA);
            }
        }

        // next iteration so switch to major color
        loop--;
        targetRGBA = gridmajorRGBA;
    }
}

// -----------------------------------------------------------------------------

void Overlay::CreateStars()
{
    int i;
    double curx, cury, curz;

    // allocate the stars
    if (starx == NULL) {
        starx = (double *)malloc(numStars * sizeof(*starx));
    }
    if (stary == NULL) {
        stary = (double *)malloc(numStars * sizeof(*stary));
    }
    if (starz == NULL) {
        starz = (double *)malloc(numStars * sizeof(*starz));
    }

    // compute radius^2 of the starfield
    int radius2 = (starMaxX * starMaxX) + (starMaxY * starMaxY);
    double id;

    // create random stars using fixed seed
    srand(52315);

    for (i = 0; i < numStars; i++) {
        // get the next z coordinate based on the star number
        // (more stars nearer the camera)
        id = (double)i;
        curz = ((id / numStars) * (id / numStars) * (id / numStars) * (id / numStars) * starMaxZ) + 1;

        // pick a random 2d position and ensure it is within the radius
        do {
            curx = 3 * ((((double)rand()) / RAND_MAX * starMaxX) - (starMaxX / 2));
            cury = 3 * ((((double)rand()) / RAND_MAX * starMaxY) - (starMaxY / 2));
        } while (((curx * curx) + (cury * cury)) > radius2);

        // save the star position
        starx[i] = curx;
        stary[i] = cury;
        starz[i] = curz;
    }

    // create random seed
    srand(time(0));
}

// -----------------------------------------------------------------------------

void Overlay::DrawStars(double angle)
{
    int offset;
    unsigned int *pixmapRGBA = (unsigned int*)pixmap;

    // get the unoccupied cell pixel color
    unsigned int blankRGBA = cellRGBA[0];
    unsigned char *blankCol = (unsigned char*)&blankRGBA;
    unsigned char blankR = *blankCol++;
    unsigned char blankG = *blankCol++;
    unsigned char blankB = *blankCol++;

    // get the star color components
    unsigned char *starCol = (unsigned char*)&starRGBA;
    unsigned char starR = *starCol++;
    unsigned char starG = *starCol++;
    unsigned char starB = *starCol++;

    unsigned int pixelRGBA;
    unsigned char *pixelCol = (unsigned char*)&pixelRGBA;
    pixelCol[3] = 255;
    unsigned char red, green, blue;

    // check if stars have been allocated
    if (starx == NULL) {
        CreateStars();
    }

    // update each star
    for (int i = 0; i < numStars; i++) {
        // get the 2d part of 3d position
        double x = starx[i] - camx;
        double y = stary[i] - camy;

        // check if angle is non zero
        if (angle != 0) {
            // compute radius
            double radius = sqrt((x * x) + (y * y));

            // get angle
            double theta = atan2(y, x) * radToDeg;

            // add current rotation
            theta += angle;
            if (theta < 0) {
                theta += 360;
            } else {
                if (theta >= 360) {
                    theta -= 360;
                }
            }

            // compute rotated position
            x = radius * cos(theta * degToRad);
            y = radius * sin(theta * degToRad);
        }

        // create the 2d position
        double z = (starz[i] / camzoom) * 2;
        int ix = (int)(x / z) + wd / 2;
        int iy = (int)(y / z) + ht / 2;

        // check the star and halo are on the display
        if (ix > 0 && ix < (wd - 1) && iy > 0 && iy < (ht - 1)) {
            // compute the star brightness
            z = 1536 / z;
            if (z > 255) {
                z = 255;
            }
            z = z / 255;

            // check if pixel is blank
            offset = ix + iy * wd;
            if (pixmapRGBA[offset] == blankRGBA) {
                // compute the color components
                red = blankR + (starR - blankR) * z;
                green = blankG + (starG - blankG) * z;
                blue = blankB + (starB - blankB) * z;
                pixelCol[0] = red;
                pixelCol[1] = green;
                pixelCol[2] = blue;
                pixmapRGBA[offset] = pixelRGBA;
            }

            // use a dimmer color for the halo
            red = blankR + (starR - blankR) * z;
            green = blankG + (starG - blankG) * z;
            blue = blankB + (starB - blankB) * z;
            pixelCol[0] = red;
            pixelCol[1] = green;
            pixelCol[2] = blue;

            // left halo
            offset -= 1;
            if (pixmapRGBA[offset] == blankRGBA) {
                pixmapRGBA[offset] = pixelRGBA;
            }

            // right halo
            offset += 2;
            if (pixmapRGBA[offset] == blankRGBA) {
                pixmapRGBA[offset] = pixelRGBA;
            }

            // top halo
            offset -= (1 + wd);
            if (pixmapRGBA[offset] == blankRGBA) {
                pixmapRGBA[offset] = pixelRGBA;
            }

            // bottom halo
            offset += (wd + wd);
            if (pixmapRGBA[offset] == blankRGBA) {
                pixmapRGBA[offset] = pixelRGBA;
            }
        }
    }
}

// -----------------------------------------------------------------------------

const char *Overlay::DoCellView(const char *args)
{
    // check the arguments are valid
    int x, y, w, h;
    if (sscanf(args, " %d %d %d %d", &x, &y, &w, &h) != 4) {
        return OverlayError("cellview command requires 4 arguments");
    }

    if (w < cellviewmultiple) return OverlayError("width of cellview must be >= 16");
    if (h < cellviewmultiple) return OverlayError("height of cellview must be >= 16");

    if (w > cellviewmaxsize) return OverlayError("width of cellview too big");
    if (h > cellviewmaxsize) return OverlayError("height of cellview too big");

    if ((w & (cellviewmultiple - 1)) != 0) return OverlayError("width of cellview must be a multiple of 16");
    if ((h & (cellviewmultiple - 1)) != 0) return OverlayError("height of cellview  must be a multiple of 16");

    // delete any existing cellview
    DeleteCellView();

    // use calloc so all cells will be in state 0
    cellview = (unsigned char*) calloc(w * h, sizeof(*cellview));
    if (cellview == NULL) return OverlayError("not enough memory to create cellview");
    cellview1 = (unsigned char*) calloc(w * h, sizeof(*cellview1));
    if (cellview1 == NULL) return OverlayError("not enough memory to create cellview");

    // allocate the zoom view
    zoomview = (unsigned char*) calloc(w * h, sizeof(*zoomview));
    if (zoomview == NULL) return OverlayError("not enough memory to create cellview");

    // save the arguments
    cellwd = w;
    cellht = h;
    cellx = x;
    celly = y;

    // set the default camera position to the center
    camx = w / 2;
    camy = h / 2;

    // set default angle
    camangle = 0;

    // set default zoom
    camzoom = 1;

    // set default layers
    camlayers = 1;
    camlayerdepth = 0.05;

    // initialize ishex to false and let scripts use the hexrule() function
    // from the oplus package to determine if the current rule uses a
    // hexagonal neighborhood
    ishex = false;

    // use standard pattern colors
    theme = false;

    // disable grid and set default grid major interval
    grid = false;
    gridmajor = 10;
    customgridcolor = false;
    customgridmajorcolor = false;

    // disable stars and set star color to opaque whie
    stars = false;
    SetRGBA(255, 255, 255, 255, &starRGBA);

    // populate cellview
    DoUpdateCells();

    return NULL;
}

// -----------------------------------------------------------------------------

const char *Overlay::CamZoom(const char *args)
{
    // check the argument is valid
    double zoom;
    if (sscanf(args, " %lf", &zoom) != 1) {
        return OverlayError("camera zoom command requires 1 argument");
    }

    if (zoom < camminzoom) return OverlayError("camera zoom too small");
    if (zoom > cammaxzoom) return OverlayError("camera zoom too big");

    // save the new value
    camzoom = zoom;

    return NULL;
}

// -----------------------------------------------------------------------------

const char *Overlay::CamAngle(const char *args)
{
    // check the argument is valid
    double angle;
    if (sscanf(args, " %lf", &angle) != 1) {
        return OverlayError("camera angle command requires 1 argument");
    }

    if (angle < 0) return OverlayError("camera angle too small");
    if (angle > 360) return OverlayError("camera angle too big");

    // save the new value
    camangle = angle;

    return NULL;
}

// -----------------------------------------------------------------------------

const char *Overlay::CamXY(const char *args)
{
    // check the arguments are valid
    double x;
    double y;
    if (sscanf(args, " %lf %lf", &x, &y) != 2) {
        return OverlayError("camera xy command requires 2 arguments");
    }

    // save the new values
    camx = x;
    camy = y;

    return NULL;
}

// -----------------------------------------------------------------------------

const char *Overlay::DoCamera(const char *args)
{
    if (cellview == NULL) return OverlayError(no_cellview);

    if (strncmp(args, "xy ", 3) == 0)    return CamXY(args+3);
    if (strncmp(args, "angle ", 6) == 0) return CamAngle(args+6);
    if (strncmp(args, "zoom ", 5) == 0)  return CamZoom(args+5);

    return OverlayError("unknown camera command");
}

// -----------------------------------------------------------------------------

const char *Overlay::CellOptionLayers(const char *args)
{
    // check the argument is valid
    int howmany;

    if (sscanf(args, " %d", &howmany) != 1) {
        return OverlayError("celloption layers command requires 1 argument");
    }

    if (howmany < 1) return OverlayError("celloption layers must be at least 1");
    if (howmany > 10) return OverlayError("celloption layers is too big");

    // save the new values
    camlayers = howmany;

    return NULL;
}

// -----------------------------------------------------------------------------

const char *Overlay::CellOptionDepth(const char *args)
{
    // check the argument is valid
    double depth;
    if (sscanf(args, " %lf", &depth) != 1) {
        return OverlayError("celloption depth command requires 1 argument");
    }

    if (depth < 0 || depth > 1) return OverlayError("celloption depth is out of range");

    // save the new values
    camlayerdepth = depth;

    return NULL;
}

// -----------------------------------------------------------------------------

const char *Overlay::CellOptionHex(const char *args)
{
    int mode;
    if (sscanf(args, "%d", &mode) != 1) {
        return OverlayError("celloption hex command requires 1 argument");
    }

    ishex = mode == 1;

    return NULL;
}

// -----------------------------------------------------------------------------

const char *Overlay::CellOptionGrid(const char *args)
{
    int mode;
    if (sscanf(args, "%d", &mode) != 1) {
        return OverlayError("celloption grid command requires 1 argument");
    }

    grid = mode == 1;

    return NULL;
}

// -----------------------------------------------------------------------------

const char *Overlay::CellOptionGridMajor(const char *args)
{
    int major;
    if (sscanf(args, "%d", &major) != 1) {
        return OverlayError("celloption grid command requires 1 argument");
    }

    if (major < 0 || major > 16) return OverlayError("celloption major is out of range");

    gridmajor = major;

    return NULL;
}

// -----------------------------------------------------------------------------

const char *Overlay::CellOptionStars(const char *args)
{
    int mode;
    if (sscanf(args, "%d", &mode) != 1) {
        return OverlayError("celloption stars command requires 1 argument");
    }

    stars = mode == 1;

    return NULL;
}

// -----------------------------------------------------------------------------

const char *Overlay::DoCellOption(const char *args)
{
    if (cellview == NULL) return OverlayError(no_cellview);

    if (strncmp(args, "hex ", 3) == 0)        return CellOptionHex(args+3);
    if (strncmp(args, "depth ", 6) == 0)      return CellOptionDepth(args+6);
    if (strncmp(args, "layers ", 7) == 0)     return CellOptionLayers(args+7);
    if (strncmp(args, "grid ", 5) == 0)       return CellOptionGrid(args+5);
    if (strncmp(args, "gridmajor ", 10) == 0) return CellOptionGridMajor(args+10);
    if (strncmp(args, "stars ", 6) == 0)      return CellOptionStars(args+6);

    return OverlayError("unknown celloption command");
}

// -----------------------------------------------------------------------------

const char *Overlay::DoTheme(const char *args)
{
    if (cellview == NULL) return OverlayError(no_cellview);

    // check the arguments are valid
    int asr, asg, asb, aer, aeg, aeb, dsr, dsg, dsb, der, deg, deb, ur, ug, ub;

    // default alpha values to opaque
    int aa = 255;
    int da = 255;
    int ua = 255;
    int ba = 255;

    // whether theme is disabled
    int disable = 0;

    // argument count
    int count = 0;

    // check for 19 argument version
    count = sscanf(args, " %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
        &asr, &asg, &asb, &aer, &aeg, &aeb, &dsr, &dsg, &dsb, &der, &deg, &deb, &ur, &ug, &ub, &aa, &da, &ua, &ba);
    if (count != 19) {
        // check for 15 argument version
        if (count != 15) {
            // check for single argument version
            if (count == 1) {
                disable = asr;
                if (disable != -1) {
                    return OverlayError("theme command single argument must be -1");
                }
            } else {
                return OverlayError("theme command requires single argument -1, or 15 or 19 rgb components");
            }
        }
    }

    if (disable != -1) {
        if (asr < 0 || asr > 255 ||
            asg < 0 || asg > 255 ||
            asb < 0 || asb > 255 ) {
            return OverlayError("theme alivestart values must be from 0 to 255");
        }
        if (aer < 0 || aer > 255 ||
            aeg < 0 || aeg > 255 ||
            aeb < 0 || aeb > 255 ) {
            return OverlayError("theme aliveend values must be from 0 to 255");
        }
        if (dsr < 0 || dsr > 255 ||
            dsg < 0 || dsg > 255 ||
            dsb < 0 || dsb > 255 ) {
            return OverlayError("theme deadstart values must be from 0 to 255");
        }
        if (der < 0 || der > 255 ||
            deg < 0 || deg > 255 ||
            deb < 0 || deb > 255 ) {
            return OverlayError("theme deadend values must be from 0 to 255");
        }
        if (ur < 0 || ur > 255 ||
            ug < 0 || ug > 255 ||
            ub < 0 || ub > 255 ) {
            return OverlayError("theme unnocupied values must be from 0 to 255");
        }
        if (aa < 0 || aa > 255) {
            return OverlayError("theme alive alpha must be from 0 to 255");
        }
        if (da < 0 || da > 255) {
            return OverlayError("theme dead alpha must be from 0 to 255");
        }
        if (ua < 0 || ua > 255) {
            return OverlayError("theme unoccupied alpha must be from 0 to 255");
        }
        if (ba < 0 || ba > 255) {
            return OverlayError("theme border alpha must be from 0 to 255");
        }
    }

    // save the new values
    if (disable == -1) {
        theme = false;
    } else {
        theme = true;
        SetRGBA(asr, asg, asb, aa, &aliveStartRGBA);
        SetRGBA(aer, aeg, aeb, aa, &aliveEndRGBA);
        SetRGBA(dsr, dsg, dsb, da, &deadStartRGBA);
        SetRGBA(der, deg, deb, da, &deadEndRGBA);
        SetRGBA(ur, ug, ub, ua, &unoccupiedRGBA);
        bordera = ba;
    }

    return NULL;
}

// -----------------------------------------------------------------------------

void Overlay::SetRenderTarget(unsigned char *pix, int pwd, int pht, Clip *clip)
{
    pixmap = pix;
    wd = pwd;
    ht = pht;
    renderclip = clip;
}

// -----------------------------------------------------------------------------

const char *Overlay::DoResize(const char *args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    // don't set wd and ht until we've checked the args are valid
    int w, h, oldw, oldh;
    bool isclip = false;
    int namepos;
    char dummy;
    if (sscanf(args, " %d %d %n%c", &w, &h, &namepos, &dummy) != 3) {
        if (sscanf(args, " %d %d", &w, &h) != 2) {
            return OverlayError("create command requires 2 or 3 arguments");
        }
    } else {
        isclip = true;
    }

    // check whether resizing clip or overlay
    if (isclip) {
        // resize clip
        if (w <= 0) return OverlayError("width of clip must be > 0");
        if (h <= 0) return OverlayError("height of clip must be > 0");

        // get clip name
        std::string name = args + namepos;

        // check if the clip exists
        std::map<std::string,Clip*>::iterator it;
        it = clips.find(name);
        if (it == clips.end()) {
            static std::string msg;
            msg = "unknown resize clip (";
            msg += name;
            msg += ")";
            return OverlayError(msg.c_str());
        }

        // get the clip dimensions
        oldw = it->second->cwd;
        oldh = it->second->cht;

        // delete the clip
        delete it->second;
        clips.erase(it);

        // allocate the resized clip with calloc
        Clip *newclip = new Clip(w, h, true);
        if (newclip == NULL || newclip->cdata == NULL) {
            delete newclip;
            return OverlayError("not enough memory to resize clip");
        }

        // save named clip
        clips[name] = newclip;

        // check if the clip is the render target
        if (targetname == name) {
            SetRenderTarget(newclip->cdata, newclip->cwd, newclip->cht, newclip);
        }
    } else {
        // resize overlay
        if (w <= 0) return OverlayError("width of overlay must be > 0");
        if (h <= 0) return OverlayError("height of overlay must be > 0");

        // given width and height are ok
        oldw = ovwd;
        oldh = ovht;
        ovwd = w;
        ovht = h;

        // free the previous pixmap
        free(ovpixmap);

        // create the new pixmap
        ovpixmap = (unsigned char*) calloc(ovwd * ovht * 4, sizeof(*ovpixmap));
        if (ovpixmap == NULL) return OverlayError("not enough memory to resize overlay");

        // check if overlay is the render target
        if (targetname == "") {
            SetRenderTarget(ovpixmap, ovwd, ovht, NULL);
        }
    }

    // return old dimensions
    static char result[32];
    sprintf(result, "%d %d", oldw, oldh);
    return result;
}

// -----------------------------------------------------------------------------

const char *Overlay::DoCreate(const char *args)
{
    // don't set wd and ht until we've checked the args are valid
    int w, h;
    bool isclip = false;
    int namepos;
    char dummy;
    if (sscanf(args, " %d %d %n%c", &w, &h, &namepos, &dummy) != 3) {
        if (sscanf(args, " %d %d", &w, &h) != 2) {
            return OverlayError("create command requires 2 or 3 arguments");
        }
    } else {
        isclip = true;
    }

    // check whether creating clip or overlay
    if (isclip) {
        // create clip
        if (w <= 0) return OverlayError("width of clip must be > 0");
        if (h <= 0) return OverlayError("height of clip must be > 0");

        std::string name = args + namepos;

        // delete any existing clip data with the given name
        std::map<std::string,Clip*>::iterator it;
        it = clips.find(name);
        if (it != clips.end()) {
            delete it->second;
            clips.erase(it);
        }

        // allocate the clip with calloc
        Clip *newclip = new Clip(w, h, true);
        if (newclip == NULL || newclip->cdata == NULL) {
            delete newclip;
            return OverlayError("not enough memory to create clip");
        }

        // create named clip
        clips[name] = newclip;
    } else {
        // creating overlay
        if (w <= 0) return OverlayError("width of overlay must be > 0");
        if (h <= 0) return OverlayError("height of overlay must be > 0");

        // given width and height are ok
        ovwd = w;
        ovht = h;

        // delete any existing pixmap
        DeleteOverlay();

        // use calloc so all pixels will be 100% transparent (alpha = 0)
        ovpixmap = (unsigned char*) calloc(ovwd * ovht * 4, sizeof(*ovpixmap));
        if (ovpixmap == NULL) return OverlayError("not enough memory to create overlay");

        // initialize RGBA values to opaque white
        r = g = b = a = 255;
        SetRGBA(r, g, b, a, &rgbadraw);

        // don't do alpha blending initially
        alphablend = 0;

        only_draw_overlay = false;

        // initial position of overlay is in top left corner of current layer
        pos = topleft;

        ovcursor = wxSTANDARD_CURSOR;
        cursname = "arrow";

        // identity transform
        axx = 1;
        axy = 0;
        ayx = 0;
        ayy = 1;
        identity = true;

        // initialize current font used by text command
        currfont = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
        fontname = "default";
        fontsize = 10;
        #ifdef __WXMAC__
            // need to increase Mac font size by 25% to match text size on Win/Linux
            currfont.SetPointSize(int(fontsize * 1.25 + 0.5));
            extraht = 1;
        #else
            currfont.SetPointSize(fontsize);
        #endif

        // default text alignment
        align = left;

        // default text background
        textbgRGBA = 0;

        // default width for lines and ellipses
        linewidth = 1;

        // make sure the Show Overlay option is ticked
        if (!showoverlay) {
            mainptr->ToggleOverlay();
        } else {
            // enable Save Overlay
            mainptr->UpdateMenuItems();
        }

        // set overlay as render target
        SetRenderTarget(ovpixmap, ovwd, ovht, NULL);
        targetname = "";
    }

    return NULL;
}

// -----------------------------------------------------------------------------

bool Overlay::PointInOverlay(int vx, int vy, int *ox, int *oy)
{
    if (ovpixmap == NULL) return false;

    int viewwd, viewht;
    viewptr->GetClientSize(&viewwd, &viewht);
    if (viewwd <= 0 || viewht <= 0) return false;

    int x = 0;
    int y = 0;
    switch (pos) {
        case topleft:
            break;
        case topright:
            x = viewwd - ovwd;
            break;
        case bottomright:
            x = viewwd - ovwd;
            y = viewht - ovht;
            break;
        case bottomleft:
            y = viewht - ovht;
            break;
        case middle:
            x = (viewwd - ovwd) / 2;
            y = (viewht - ovht) / 2;
            break;
    }

    if (vx < x) return false;
    if (vy < y) return false;
    if (vx >= x + ovwd) return false;
    if (vy >= y + ovht) return false;

    *ox = vx - x;
    *oy = vy - y;

    return true;
}

// -----------------------------------------------------------------------------

const char *Overlay::DoPosition(const char *args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    if (strncmp(args+1, "topleft", 7) == 0) {
        pos = topleft;

    } else if (strncmp(args+1, "topright", 8) == 0) {
        pos = topright;

    } else if (strncmp(args+1, "bottomright", 11) == 0) {
        pos = bottomright;

    } else if (strncmp(args+1, "bottomleft", 10) == 0) {
        pos = bottomleft;

    } else if (strncmp(args+1, "middle", 6) == 0) {
        pos = middle;

    } else {
        return OverlayError("unknown position");
    }

    return NULL;
}

// -----------------------------------------------------------------------------

const char *Overlay::DecodeReplaceArg(const char *arg, int *find, bool *negfind, int *replace, int *invreplace, int *delta, int component) {
    // argument is a string defining the find component value, optional replacement specification and
    // optional postfix value adjustment
    // find part is one of:
    //     *       (match any value)
    //     0..255  (match specific value)
    //     !0..255 (match values other than this value)
    // optional replacement part is one of:
    //     r       (replace with red component)
    //     g       (replace with green component)
    //     b       (replace with blue component)
    //     a       (replace with alpha component)
    //     #       (leave this component unchanged)
    // optional postfix is one of:
    //     -       (value should be inverted: v -> 255-v)
    //     --      (value should be decremented)
    //     ++      (value should be incremented)
    //     -0..255 (value should have constant subtracted from it)
    //     +0..255 (value should have constant added to it)
    char *p     = (char*)arg;
    *find       = 0;
    *negfind    = false;
    *replace    = 0;
    *invreplace = 0;

    // decode find
    if (*p == '*') {
        // match any
        *find = matchany;
        p++;
    } else {
        if (*p == '!') {
            // invert match
            *negfind = true;
            p++;
        }
        // read value
        while (*p >= '0' && *p <= '9') {
            *find = 10 * (*find) + *p - '0';
            p++;
        }
        if (*find < 0 || *find > 255) {
            return "replace argument is out of range";
        }
    }

    // decode optional replacement
    if (*p != '\0') {
        const char *valid = "rgba#";
        char *match = strchr((char*)valid, *p);
        if (match) {
            *replace = match - valid + 1;
            if (*replace == 5) *replace = component;
            p++;
        } else {
            if (*p != '-') return "replace argument postfix is invalid";
        }
        // check for invert, increment or decrement
        if (*p == '-') {
            p++;
            if (*p == '-') {
                *delta = -1;
                p++;
            } else {
                if (*p >= '0' && *p <= '9') {
                    while (*p >= '0' && *p <= '9') {
                        *delta = 10 * (*delta) + *p - '0';
                        p++;
                    }
                    if (*delta < 0 || *delta > 255) {
                        return "replace delta is out of range";
                    }
                    *delta = -*delta;
                } else {
                    *invreplace = 255;
                }
            }
        } else {
            if (*p == '+') {
                p++;
                if (*p == '+') {
                    *delta = 1;
                    p++;
                } else {
                    if (*p >= '0' && *p <= '9') {
                        while (*p >= '0' && *p <= '9') {
                            *delta = 10 * (*delta) + *p - '0';
                            p++;
                        }
                        if (*delta < 0 || *delta > 255) {
                            return "replace delta is out of range";
                        }
                    } else {
                        p--;
                    }
                }
            }
        }
    }

    // any trailing characters are invalid
    if (*p != '\0') {
        return "replace argument postix is invalid";
    }
    return NULL;
}

// -----------------------------------------------------------------------------

const char *Overlay::DoReplace(const char *args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    // allocate memory for the arguments
    char *buffer = (char*)malloc(strlen(args) + 1);
    strcpy(buffer, args);

    // check the arguments exist
    const char *delim = " ";
    const char *arg1 = strtok(buffer, delim);
    const char *arg2 = strtok(NULL, delim);
    const char *arg3 = strtok(NULL, delim);
    const char *arg4 = strtok(NULL, delim);
    if (arg1 == NULL || arg2 == NULL || arg3 == NULL || arg4 == NULL) {
        free(buffer);
        return OverlayError("replace command requires 4 arguments");
    }

    // decode the r g b a arguments
    int findr = 0;
    int findg = 0;
    int findb = 0;
    int finda = 0;
    int replacer = 0;
    int replaceg = 0;
    int replaceb = 0;
    int replacea = 0;
    bool negr = false;
    bool negg = false;
    bool negb = false;
    bool nega = false;
    int invr = 0;
    int invg = 0;
    int invb = 0;
    int inva = 0;
    int deltar = 0;
    int deltag = 0;
    int deltab = 0;
    int deltaa = 0;
    const char *error = DecodeReplaceArg(arg1, &findr, &negr, &replacer, &invr, &deltar, 1);
    if (error) { free(buffer); return OverlayError(error); }
    error = DecodeReplaceArg(arg2, &findg, &negg, &replaceg, &invg, &deltag, 2);
    if (error) { free(buffer); return OverlayError(error); }
    error = DecodeReplaceArg(arg3, &findb, &negb, &replaceb, &invb, &deltab, 3);
    if (error) { free(buffer); return OverlayError(error); }
    error = DecodeReplaceArg(arg4, &finda, &nega, &replacea, &inva, &deltaa, 4);
    if (error) { free(buffer); return OverlayError(error); }
    free(buffer);

    // get the current render target
    unsigned char *clipdata = pixmap;
    const int w = wd;
    const int h = ht;
    const int allpixels = w * h;

    // check that negation is correctly used
    if (negg || negb || (nega && negr)) {
        return OverlayError("replace ! may only be at start or before alpha");
    }

    // mark target clip as changed
    DisableTargetClipIndex();

    unsigned char clipr, clipg, clipb, clipa;
    const int bytebits = 8;
    const int remainbits = 32 - bytebits;

    // count how many pixels are replaced
    int numchanged = 0;
    static char result[16];

    const bool allwild = (findr == matchany && findg == matchany && findb == matchany && finda == matchany);
    const bool zerodelta = (deltar == 0 && deltag == 0 && deltab == 0 && deltaa == 0);
    const bool zeroinv = (invr == 0 && invg == 0 && invb == 0 && inva == 0);
    const bool fixedreplace = (replacer == 0 && replaceg == 0 && replaceb == 0 && replacea == 0);
    const bool destreplace = (replacer == 1 && replaceg == 2 && replaceb == 3 && replacea == 4);

    // some specific common use cases are optimized for performance followed by the general purpose routine
    // the optimized versions are typically an order of magnitude faster

    // optimization case 1: fixed find and replace
    if ((findr != matchany && findg != matchany && findb != matchany && finda != matchany) &&
        fixedreplace && !nega && zeroinv && zerodelta) {
        // use 32 bit colors
        unsigned int *cdata = (unsigned int*)clipdata;
        unsigned int findcol = 0;
        const unsigned int replacecol = rgbadraw;
        SetRGBA(findr, findg, findb, finda, &findcol);

        // check for not equals case
        if (negr) {
            for (int i = 0; i < allpixels; i++) {
                if (*cdata != findcol) {
                    *cdata = replacecol;
                    numchanged++;
                }
                cdata++;
            }
        } else {
            for (int i = 0; i < allpixels; i++) {
                if (*cdata == findcol) {
                    *cdata = replacecol;
                    numchanged++;
                }
                cdata++;
            }
        }

        // return number of pixels replaced
        sprintf(result, "%d", numchanged);
        return result;
    }

    // optimization case 2: match pixels with different alpha and fixed replace
    if (zerodelta && fixedreplace && zeroinv && nega &&
        ((findr != matchany && findg != matchany && findb != matchany) || (findr == matchany && findg == matchany && findb == matchany))) {
        unsigned int *cdata = (unsigned int*)clipdata;
        const unsigned int replacecol = rgbadraw;
        if (findr != matchany) {
            // fixed match
            for (int i = 0; i < allpixels; i++) {
                if (clipdata[0] == findr && clipdata[1] == findg && clipdata[2] == findb && clipdata[3] != finda) {
                    *cdata = replacecol;
                    numchanged++;
                }
                cdata++;
                clipdata += 4;
            }
        } else {
            // r g b wildcard match
            clipdata += 3;
            for (int i = 0; i < allpixels; i++) {
                if (*clipdata != finda) {
                    *cdata = replacecol;
                    numchanged++;
                }
                cdata++;
                clipdata += 4;
            }
        }

        // return number of pixels replaced
        sprintf(result, "%d", numchanged);
        return result;
    }

    // optimization case 3: fill
    if (allwild && zerodelta && zeroinv && fixedreplace) {
        // fill clip with current RGBA
        unsigned int *cdata = (unsigned int*)clipdata;
        const unsigned int replacecol = rgbadraw;
        for (int i = 0; i < allpixels; i++) {
            if (*cdata != replacecol) {
                *cdata = replacecol;
                numchanged++;
            }
            cdata++;
        }

        // return number of pixels replaced
        sprintf(result, "%d", numchanged);
        return result;
    }

    // optimization case 4: no-op
    if (allwild && zerodelta && zeroinv && destreplace) {
        // return number of pixels replaced
        sprintf(result, "%d", numchanged);
        return result;
    }

    // optimization case 5: set constant alpha value on every pixel
    if (allwild && zerodelta && zeroinv &&
        (replacer == 1 && replaceg == 2 && replaceb == 3 && replacea == 0)) {
        // set alpha
        clipdata += 3;
        for (int i = 0; i < allpixels; i++) {
            if (*clipdata != a) {
                *clipdata = a;
                numchanged++;
            }
            clipdata += 4;
        }

        // return number of pixels replaced
        sprintf(result, "%d", numchanged);
        return result;
    }

    // optimization case 6: invert one or more rgba components
    if (allwild && zerodelta && !zeroinv && destreplace) {
        // invert specified components of every pixel
        unsigned int *cdata = (unsigned int*)clipdata;
        unsigned int invmask = 0;
        SetRGBA(invr, invg, invb, inva, &invmask);

        for (int i = 0; i < allpixels; i++) {
            *cdata = *cdata ^ invmask;
            cdata++;
        }

        // return number of pixels replaced
        numchanged = allpixels;
        sprintf(result, "%d", numchanged);
        return result;
    }

    // optimization case 7: offset only alpha value
    if (allwild && zeroinv && destreplace && deltar == 0 && deltag == 0 && deltab == 0 && deltaa != 0) {
        // offset alpha value of every pixel
        bool changed;
        int value, orig;
        unsigned int clamp;

        clipdata += 3;
        for (int i = 0; i < allpixels; i++) {
            changed = false;
            orig = *clipdata;
            value = orig + deltaa;
            clamp = value >> bytebits;
            if (clamp) { value = ~clamp >> remainbits; }
            changed = value != orig;
            if (changed) {
                *clipdata = value;
                numchanged++;
            }
            clipdata += 4;
        }

        // return number of pixels replaced
        sprintf(result, "%d", numchanged);
        return result;
    }

    // optimization case 8: offset one or more rgba components
    if (allwild && zeroinv && destreplace && !zerodelta) {
        // offset rgba values of every pixel
        bool changed;
        int value, orig;
        unsigned int clamp;

        for (int i = 0; i < allpixels; i++) {
            changed = false;

            // change r if required
            if (deltar) {
                orig = clipdata[0];
                value = orig + deltar;
                clamp = value >> bytebits;
                if (clamp) { value = ~clamp >> remainbits; }
                changed = value != orig;
                if (changed) {
                    clipdata[0] = value;
                }
            }
            // change g if required
            if (deltag) {
                orig = clipdata[1];
                value = orig + deltag;
                clamp = value >> bytebits;
                if (clamp) { value = ~clamp >> remainbits; }
                changed = value != orig;
                if (changed) {
                    clipdata[1] = value;
                }
            }
            // change b if required
            if (deltab) {
                orig = clipdata[2];
                value = orig + deltab;
                clamp = value >> bytebits;
                if (clamp) { value = ~clamp >> remainbits; }
                changed = value != orig;
                if (changed) {
                    clipdata[2] = value;
                }
            }
            // change a if required
            if (deltaa) {
                orig = clipdata[3];
                value = orig + deltaa;
                clamp = value >> bytebits;
                if (clamp) { value = ~clamp >> remainbits; }
                changed = value != orig;
                if (changed) {
                    clipdata[3] = value;
                }
            }
            // count number changed
            if (changed) {
                numchanged++;
            }
            clipdata += 4;
        }

        // return number of pixels replaced
        sprintf(result, "%d", numchanged);
        return result;
    }

    // optimization case 9: convert RGBA to ABGR
    if (allwild && zeroinv && zerodelta && replacer == 4 && replaceg == 3 && replaceb == 2 && replacea == 1) {
        unsigned int *cdata = (unsigned int*)clipdata;
        unsigned int c;
        for (int i = 0; i < allpixels; i++) {
            c = *cdata;
            *cdata++ = BYTE2RED(ALPHA2BYTE(c)) | BYTE2GREEN(BLUE2BYTE(c)) | BYTE2BLUE(GREEN2BYTE(c)) | BYTE2ALPHA(RED2BYTE(c));
        }
    
        // return number changed
        numchanged = allpixels;
        sprintf(result, "%d", numchanged);
        return result;
    }

    // general case
    bool matchr, matchg, matchb, matcha, matchpixel;
    int value = 0;
    bool changed = false;
    unsigned int clamp;
    for (int i = 0; i < allpixels; i++) {
        // read the clip pixel
        clipr = clipdata[0];
        clipg = clipdata[1];
        clipb = clipdata[2];
        clipa = clipdata[3];

        // check if the pixel components match
        matchr = (findr == matchany) || (findr == clipr);
        matchg = (findg == matchany) || (findg == clipg);
        matchb = (findb == matchany) || (findb == clipb);
        matcha = (finda == matchany) || (finda == clipa);

        // check for negative components
        if (negr) {
            matchpixel = !(matchr && matchg && matchb && matcha);
        } else {
            if (nega) {
                matchpixel = (matchr && matchg && matchb && !matcha);
            } else {
                matchpixel = (matchr && matchg && matchb && matcha);
            }
        }

        // did pixel match
        changed = false;
        if (matchpixel) {
            // match made so process r component
            switch (replacer) {
                case 0:
                    // use current RGBA r component
                    value = r ^ invr;
                    break;
                case 1:
                    // use clip r component
                    value = clipr ^ invr;
                    break;
                case 2:
                    // use clip g component
                    value = clipg ^ invr;
                    break;
                case 3:
                    // use clip b component
                    value = clipb ^ invr;
                    break;
                case 4:
                    // use clip a component
                    value = clipa ^ invr;
                    break;
            }
            if (deltar) {
                value += deltar;
                clamp = value >> bytebits;
                if (clamp) { value = ~clamp >> remainbits; }
            }
            if (value != clipr) {
                *clipdata = value;
                changed = true;
            }
            clipdata++;

            // g component
            switch (replaceg) {
                case 0:
                    // use current RGBA g component
                    value = g ^ invg;
                    break;
                case 1:
                    // use clip r component
                    value = clipr ^ invg;
                    break;
                case 2:
                    // use clip g component
                    value = clipg ^ invg;
                    break;
                case 3:
                    // use clip b component
                    value = clipb ^ invg;
                    break;
                case 4:
                    // use clip a component
                    value = clipa ^ invg;
                    break;
            }
            if (deltag) {
                value += deltag;
                clamp = value >> bytebits;
                if (clamp) { value = ~clamp >> remainbits; }
            }
            if (value != clipg) {
                *clipdata = value;
                changed = true;
            }
            clipdata++;

            // b component
            switch (replaceb) {
                case 0:
                    // use current RGBA b component
                    value = b ^ invb;
                    break;
                case 1:
                    // use clip r component
                    value = clipr ^ invb;
                    break;
                case 2:
                    // use clip g component
                    value = clipr ^ invb;
                    break;
                case 3:
                    // use clip b component
                    value = clipb ^ invb;
                    break;
                case 4:
                    // use clip a component
                    value = clipa ^ invb;
                    break;
            }
            if (deltab) {
                value += deltab;
                clamp = value >> bytebits;
                if (clamp) { value = ~clamp >> remainbits; }
            }
            if (value != clipb) {
                *clipdata = value;
                changed = true;
            }
            clipdata++;

            // a component
            switch (replacea) {
                case 0:
                    // use current RGBA a component
                    value = a ^ inva;
                    break;
                case 1:
                    // use clip r component
                    value = clipr ^ inva;
                    break;
                case 2:
                    // use clip g component
                    value = clipg ^ inva;
                    break;
                case 3:
                    // use clip b component
                    value = clipb ^ inva;
                    break;
                case 4:
                    // use clip a component
                    value = clipa ^ inva;
                    break;
            }
            if (deltaa) {
                value += deltaa;
                clamp = value >> bytebits;
                if (clamp) { value = ~clamp >> remainbits; }
            }
            if (value != clipa) {
                *clipdata = value;
                changed = true;
            }
            clipdata++;

            // check if pixel changed
            if (changed) {
                numchanged++;
            }
        } else {
            // no match so skip pixel
            clipdata += 4;
        }
    }

    // return number of pixels replaced
    sprintf(result, "%d", numchanged);
    return result;
}

// -----------------------------------------------------------------------------

const char *Overlay::DoSetRGBA(const char *cmd, lua_State *L, int n, int *nresults)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    // check if there are arguments
    int valid = false;
    int i = 2;

    if (n > 1) {
        // attempt to read the arguments
        lua_rawgeti(L, 1, i++);
        int a1 = (int)lua_tonumberx(L, -1, &valid);
        lua_pop(L, 1);
        if (!valid) return OverlayError("rgba command has illegal red argument");
        lua_rawgeti(L, 1, i++);
        int a2 = (int)lua_tonumberx(L, -1, &valid);
        lua_pop(L, 1);
        if (!valid) return OverlayError("rgba command has illegal green argument");
        lua_rawgeti(L, 1, i++);
        int a3 = (int)lua_tonumberx(L, -1, &valid);
        lua_pop(L, 1);
        if (!valid) return OverlayError("rgba command has illegal blue argument");
        lua_rawgeti(L, 1, i++);
        int a4 = (int)lua_tonumberx(L, -1, &valid);
        lua_pop(L, 1);
        if (!valid) return OverlayError("rgba command has illegal alpha argument");

        // validate argument range
        if (a1 < 0 || a1 > 255 ||
            a2 < 0 || a2 > 255 ||
            a3 < 0 || a3 > 255 ||
            a4 < 0 || a4 > 255) {
            return OverlayError("rgba values must be from 0 to 255");
        }

        // return the current rgba values as a table on the Lua stack
        // {"rgba", r, g, b, a}
        lua_newtable(L);
        i = 1;
        lua_pushstring(L, cmd);
        lua_rawseti(L, -2, i++);
        lua_pushinteger(L, r);
        lua_rawseti(L, -2, i++);
        lua_pushinteger(L, g);
        lua_rawseti(L, -2, i++);
        lua_pushinteger(L, b);
        lua_rawseti(L, -2, i++);
        lua_pushinteger(L, a);
        lua_rawseti(L, -2, i++);
        *nresults = 1;  // 1 result: the table

        // set the new values
        r = (unsigned char)a1;
        g = (unsigned char)a2;
        b = (unsigned char)a3;
        a = (unsigned char)a4;
        SetRGBA(r, g, b, a, &rgbadraw);
    } else {
        return OverlayError("rgba command requires 4 arguments");
    }

    return NULL;
}

// -----------------------------------------------------------------------------

const char *Overlay::DoSetRGBA(const char *args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    int a1, a2, a3, a4;
    if (sscanf(args, " %d %d %d %d", &a1, &a2, &a3, &a4) != 4) {
        return OverlayError("rgba command requires 4 arguments");
    }

    if (a1 < 0 || a1 > 255 ||
        a2 < 0 || a2 > 255 ||
        a3 < 0 || a3 > 255 ||
        a4 < 0 || a4 > 255) {
        return OverlayError("rgba values must be from 0 to 255");
    }

    unsigned char oldr = r;
    unsigned char oldg = g;
    unsigned char oldb = b;
    unsigned char olda = a;

    r = (unsigned char) a1;
    g = (unsigned char) a2;
    b = (unsigned char) a3;
    a = (unsigned char) a4;
    SetRGBA(r, g, b, a, &rgbadraw);

    // return old values
    static char result[16];
    sprintf(result, "%hhu %hhu %hhu %hhu", oldr, oldg, oldb, olda);
    return result;
}

// -----------------------------------------------------------------------------

void Overlay::DrawPixel(int x, int y)
{
    // caller must guarantee that pixel is within pixmap
    if (alphablend && a < 255) {
        // do nothing if source pixel is transparent
        if (a) {
            unsigned int *lp = ((unsigned int*)pixmap) + y * wd + x;
            const unsigned int alpha = a + 1;
            const unsigned int invalpha = 256 - a;
            const unsigned int dest = *lp;
            ALPHABLEND(rgbadraw, dest, lp, alpha, invalpha);
        }
    } else {
        unsigned int *lp = ((unsigned int*)pixmap) + y * wd + x;
        *lp = rgbadraw;
    }
}

// -----------------------------------------------------------------------------

const char *Overlay::GetCoordinatePair(char *args, int *x, int *y)
{
    // attempt to decode integers
    char c = *args++;
    bool sign = false;
    int newx = 0;
    int newy = 0;

    // skip whitespace
    while (c == ' ') {
        c = *args++;
    }
    if (!c) return NULL;

    // check for sign
    if (c == '-') {
        sign = true;
        c = *args++;
    }
    if (!c) return NULL;

    // read digits
    while (c >= '0' && c <= '9') {
        newx = 10 * newx + (c - '0');
        c = *args++;
    }
    if (sign) newx = -newx;

    // check for end of word
    if (c && c != ' ') return NULL;

    // skip whitespace
    while (c == ' ') {
        c = *args++;
    }
    if (!c) return NULL;

    // check for sign
    sign = false;
    if (c == '-') {
        sign = true;
        c = *args++;
    }

    // read digits
    while (c >= '0' && c <= '9') {
        newy = 10 * newy + (c - '0');
        c = *args++;
    }
    if (sign) newy = -newy;

    // check for end of word
    if (c && c != ' ') return NULL;
    while (c == ' ') {
        c = *args++;
    }
    args--;

    // return coordinates
    *x = newx;
    *y = newy;
    return args;
}

// -----------------------------------------------------------------------------

const char *Overlay::DoSetPixel(lua_State *L, int n, int *nresults)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    // check if there are arguments
    // note: it is possible that n > 1 and arguments have nil values
    int valid = false;
    int i = 2;
    int type = -1;

    // mark target clip as changed
    DisableTargetClipIndex();

    // check for alpha blending
    if (alphablend && a < 255) {
        // use alpha blending
        // do nothing if source pixel is transparent
        if (a) {
            // compute pixel alpha
            const unsigned int alpha = a + 1;
            const unsigned int invalpha = 256 - a;
            const unsigned int sourcearb = alpha * RBRIGHT(rgbadraw & RBMASK);
            const unsigned int sourceag =  alpha *        (rgbadraw & GMASK);
            if (alphablend == 1) {
                // full alpha blend
                do {
                    // get next pixel coordinate
                    lua_rawgeti(L, 1, i++);
                    int x = (int)lua_tonumberx(L, -1, &valid);
                    if (!valid) break;
                    lua_pop(L, 1);
                    lua_rawgeti(L, 1, i++);
                    int y = (int)lua_tonumberx(L, -1, &valid);
                    if (!valid) break;
                    lua_pop(L, 1);
    
                    // ignore pixel if outside pixmap edges
                    if (PixelInTarget(x, y)) {
                        unsigned int *lp = ((unsigned int*)pixmap) + y * wd + x;
                        const unsigned int dest = *lp;
                        ALPHABLENDPRE(rgbadraw, sourcearb, sourceag, dest, lp, alpha, invalpha);
                    }
                } while (i <= n);
            } else {
                // fast alpha blend (opaque destination)
                do {
                    // get next pixel coordinate
                    lua_rawgeti(L, 1, i++);
                    int x = (int)lua_tonumberx(L, -1, &valid);
                    if (!valid) break;
                    lua_pop(L, 1);
                    lua_rawgeti(L, 1, i++);
                    int y = (int)lua_tonumberx(L, -1, &valid);
                    if (!valid) break;
                    lua_pop(L, 1);
    
                    // ignore pixel if outside pixmap edges
                    if (PixelInTarget(x, y)) {
                        unsigned int *lp = ((unsigned int*)pixmap) + y * wd + x;
                        const unsigned int dest = *lp;
                        ALPHABLENDPREOPAQUEDEST(sourcearb, sourceag, dest, lp, invalpha);
                    }
                } while (i <= n);
            }

            // check if loop terminated because of failed number conversion
            if (!valid) {
                // get the type of the argument
                type = lua_type(L, -1);
                lua_pop(L, 1);
            }
        }
    } else {
        // use fast copy
        unsigned int rgba = rgbadraw;
        unsigned int *lpixmap = (unsigned int*)pixmap;
        do {
            // get next pixel coordinate
            lua_rawgeti(L, 1, i++);
            int x = (int)lua_tonumberx(L, -1, &valid);
            if (!valid) break;
            lua_pop(L, 1);
            lua_rawgeti(L, 1, i++);
            int y = (int)lua_tonumberx(L, -1, &valid);
            if (!valid) break;
            lua_pop(L, 1);

            // ignore pixel if outside pixmap edges
            if (PixelInTarget(x, y)) {
                *(lpixmap + y*wd + x) = rgba;
            }
        } while (i <= n);

        // check if loop terminated because of failed number conversion
        if (!valid) {
            // get the type of the argument
            type = lua_type(L, -1);
            lua_pop(L, 1);
        }
    }

    // check if there were errors
    if (!valid) {
        // check if the argument number is a multiple of 2 and the argument is nil
        if ((((i - 3) & 1) == 0) && (type == LUA_TNIL)) {
            // command was valid
            valid = true;
        }
    }

    if (!valid) {
        // return appropriate error message
        switch ((i - 3) & 1) {
            case 0:
                return OverlayError("set command has illegal x");
                break;
            case 1:
                return OverlayError("set command has illegal y");
                break;
        }
    }
    return NULL;
}

// -----------------------------------------------------------------------------

const char *Overlay::DoSetPixel(const char *args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    int x = 0;
    int y = 0;
    // get the first pixel coordinates (mandatory)
    args = GetCoordinatePair((char*)args, &x, &y);
    if (!args) return OverlayError("set command requires coordinate pairs");

    // mark target clip as changed
    DisableTargetClipIndex();

    // ignore pixel if outside pixmap edges
    if (PixelInTarget(x, y)) DrawPixel(x, y);

    // read any further coordinates
    while (*args) {
        args = GetCoordinatePair((char*)args, &x, &y);
        if (!args) return OverlayError("set command has illegal coordinates");
        if (PixelInTarget(x, y)) DrawPixel(x, y);
    }

    return NULL;
}

// -----------------------------------------------------------------------------

const char *Overlay::DoGet(lua_State *L, int n, int *nresults)
{
    if (pixmap == NULL) return "";

    // check if there are arguments
    int valid = false;
    int i = 2;

    if (n > 1) {
        // attempt to read the arguments
        lua_rawgeti(L, 1, i++);
        int x = (int)lua_tonumberx(L, -1, &valid);
        lua_pop(L,1);
        if (!valid) return OverlayError("get command has illegal x argument");
        lua_rawgeti(L, 1, i++);
        int y = (int)lua_tonumberx(L, -1, &valid);
        lua_pop(L,1);
        if (!valid) return OverlayError("get command has illegal y argument");

        // check if x,y is outside pixmap
        if (!PixelInTarget(x, y)) {
            // return -1 for all components to indicate outside pixmap
            lua_pushinteger(L, -1);
            lua_pushinteger(L, -1);
            lua_pushinteger(L, -1);
            lua_pushinteger(L, -1);
        } else {
            // get and return the pixel rgba values
            unsigned char *p = pixmap + y*wd*4 + x*4;
            lua_pushinteger(L, p[0]);
            lua_pushinteger(L, p[1]);
            lua_pushinteger(L, p[2]);
            lua_pushinteger(L, p[3]);
        }
        *nresults = 4;
    } else {
        return OverlayError("get command requires 2 arguments");
    }
    return NULL;
}

// -----------------------------------------------------------------------------

const char *Overlay::DoGetPixel(const char *args)
{
    if (pixmap == NULL) return "";

    int x, y;
    if (sscanf(args, "%d %d", &x, &y) != 2) {
        return OverlayError("get command requires 2 arguments");
    }

    // check if x,y is outside pixmap
    if (!PixelInTarget(x, y)) return "";

    unsigned char *p = pixmap + y*wd*4 + x*4;
    static char result[16];
    sprintf(result, "%hhu %hhu %hhu %hhu", p[0], p[1], p[2], p[3]);
    return result;
}

// -----------------------------------------------------------------------------

bool Overlay::TransparentPixel(int x, int y)
{
    if (ovpixmap == NULL) return false;

    // check if x,y is outside pixmap
    if (!PixelInOverlay(x, y)) return false;

    unsigned char *p = ovpixmap + y*ovwd*4 + x*4;

    // return true if alpha value is 0
    return p[3] == 0;
}

// -----------------------------------------------------------------------------

void Overlay::SetOverlayCursor()
{
    if (cursname == "current") {
        // currlayer->curs might have changed
        ovcursor = currlayer->curs;
    }
    viewptr->SetCursor(*ovcursor);
}

// -----------------------------------------------------------------------------

const char *Overlay::DoCursor(const char *args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    if (strncmp(args+1, "arrow", 5) == 0) {
        ovcursor = wxSTANDARD_CURSOR;

    } else if (strncmp(args+1, "current", 7) == 0) {
        ovcursor = currlayer->curs;

    } else if (strncmp(args+1, "pencil", 6) == 0) {
        ovcursor = curs_pencil;

    } else if (strncmp(args+1, "pick", 4) == 0) {
        ovcursor = curs_pick;

    } else if (strncmp(args+1, "cross", 5) == 0) {
        ovcursor = curs_cross;

    } else if (strncmp(args+1, "hand", 4) == 0) {
        ovcursor = curs_hand;

    } else if (strncmp(args+1, "zoomin", 6) == 0) {
        ovcursor = curs_zoomin;

    } else if (strncmp(args+1, "zoomout", 7) == 0) {
        ovcursor = curs_zoomout;

    } else if (strncmp(args+1, "wait", 4) == 0) {
        ovcursor = curs_wait;

    } else if (strncmp(args+1, "hidden", 6) == 0) {
        ovcursor = curs_hidden;

    } else {
        return OverlayError("unknown cursor");
    }

    std::string oldcursor = cursname;
    cursname = args+1;

    viewptr->CheckCursor(mainptr->infront);

    // return old cursor name
    static std::string result;
    result = oldcursor;
    return result.c_str();
}

// -----------------------------------------------------------------------------

void Overlay::CheckCursor()
{
    // the cursor needs to be checked if the pixmap data has changed, but that's
    // highly likely if we call this routine at the end of DrawOverlay
    viewptr->CheckCursor(mainptr->infront);
}

// -----------------------------------------------------------------------------

const char *Overlay::DoGetXY()
{
    if (pixmap == NULL) return "";
    if (!mainptr->infront) return "";

    wxPoint pt = viewptr->ScreenToClient( wxGetMousePosition() );

    int ox, oy;
    if (PointInOverlay(pt.x, pt.y, &ox, &oy)) {
        static char result[32];
        sprintf(result, "%d %d", ox, oy);
        return result;
    } else {
        return "";
    }
}

// -----------------------------------------------------------------------------

const char *Overlay::LineOptionWidth(const char *args)
{
    int w, oldwidth;
    if (sscanf(args, " %d", &w) != 1) {
        return OverlayError("lineoption width command requires 1 argument");
    }

    if (w < 1) return OverlayError("line width must be > 0");
    if (w > 10000) return OverlayError("line width must be <= 10000");

    oldwidth = linewidth;
    linewidth = w;

    static char result[32];
    sprintf(result, "%d", oldwidth);
    return result;
}

// -----------------------------------------------------------------------------

const char *Overlay::DoLineOption(const char *args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    if (strncmp(args, "width ", 6) == 0) return LineOptionWidth(args+6);

    return OverlayError("unknown lineoption command");
}

// -----------------------------------------------------------------------------

void Overlay::DrawAAPixel(int x, int y, double opac)
{
    if (PixelInTarget(x, y)) {
        unsigned char newalpha = 255-int(opac);
        if (newalpha == 0) return;

        if (!alphablend) {
            // only true in DrawThickEllipse
            if (newalpha > 127) {
                // don't adjust current alpha value
                DrawPixel(x, y);
            }
            return;
        }

        // temporarily adjust current alpha value
        unsigned char olda = a;
        if (a < 255) newalpha = int(newalpha * a / 255);
        a = newalpha;

        DrawPixel(x, y);

        // restore alpha
        a = olda;
    }
}

// -----------------------------------------------------------------------------

// need an adjustment for thick lines when linewidth is an even number
static double even_w;

void Overlay::PerpendicularX(int x0, int y0, int dx, int dy, int xstep, int ystep,
                             int einit, int winit, double w, double D2)
{
    // dx > dy
    int threshold = dx - 2*dy;
    int E_diag = -2*dx;
    int E_square = 2*dy;
    int x = x0;
    int y = y0;
    int err = einit;
    int tk = dx+dy-winit;

    // draw top/bottom half of line
    int q = 0;
    while (tk <= even_w) {
        if (alphablend) {
            double alfa = 255 * (w - tk) / D2;
            if (alfa < 255) {
                if (even_w != w) alfa = 128;
                DrawAAPixel(x, y, 255-alfa);
            } else {
                if (PixelInTarget(x, y)) DrawPixel(x, y);
            }
        } else {
            if (PixelInTarget(x, y)) DrawPixel(x, y);
        }
        if (err >= threshold) {
            x += xstep;
            err += E_diag;
            tk += 2*dy;
        }
        err += E_square;
        y += ystep;
        tk += 2*dx;
        q++;
    }

    y = y0;
    x = x0;
    err = -einit;
    tk = dx+dy+winit;

    // draw other half of line
    int p = 0;
    while (tk <= w) {
        if (p > 0) {
            if (alphablend) {
                double alfa = 255 * (w - tk) / D2;
                if (alfa < 255) {
                    if (even_w != w) alfa = 128;
                    DrawAAPixel(x, y, 255-alfa);
                } else {
                    if (PixelInTarget(x, y)) DrawPixel(x, y);
                }
            } else {
                if (PixelInTarget(x, y)) DrawPixel(x, y);
            }
        }
        if (err > threshold) {
            x -= xstep;
            err += E_diag;
            tk += 2*dy;
        }
        err += E_square;
        y -= ystep;
        tk += 2*dx;
        p++;
    }

    if (q == 0 && p < 2) {
        // needed for very thin lines
        if (PixelInTarget(x0, y0)) DrawPixel(x0, y0);
    }
}

// -----------------------------------------------------------------------------

void Overlay::PerpendicularY(int x0, int y0, int dx, int dy, int xstep, int ystep,
                             int einit, int winit, double w, double D2)
{
    // dx <= dy
    int threshold = dy - 2*dx;
    int E_diag = -2*dy;
    int E_square = 2*dx;
    int x = x0;
    int y = y0;
    int err = -einit;
    int tk = dx+dy+winit;

    // draw left/right half of line
    int q = 0;
    while (tk <= w) {
        if (alphablend) {
            double alfa = 255 * (w - tk) / D2;
            if (alfa < 255) {
                if (even_w != w) alfa = 128;
                DrawAAPixel(x, y, 255-alfa);
            } else {
                if (PixelInTarget(x, y)) DrawPixel(x, y);
            }
        } else {
            if (PixelInTarget(x, y)) DrawPixel(x, y);
        }
        if (err > threshold) {
            y += ystep;
            err += E_diag;
            tk += 2*dx;
        }
        err += E_square;
        x += xstep;
        tk += 2*dy;
        q++;
    }

    y = y0;
    x = x0;
    err = einit;
    tk = dx+dy-winit;

    // draw other half of line
    int p = 0;
    while (tk <= even_w) {
        if (p > 0) {
            if (alphablend) {
                double alfa = 255 * (w - tk) / D2;
                if (alfa < 255) {
                    if (even_w != w) alfa = 128;
                    DrawAAPixel(x, y, 255-alfa);
                } else {
                    if (PixelInTarget(x, y)) DrawPixel(x, y);
                }
            } else {
                if (PixelInTarget(x, y)) DrawPixel(x, y);
            }
        }
        if (err >= threshold) {
            y -= ystep;
            err += E_diag;
            tk += 2*dx;
        }
        err += E_square;
        x -= xstep;
        tk += 2*dy;
        p++;
    }

    if (q == 0 && p < 2) {
        // needed for very thin lines
        if (PixelInTarget(x0, y0)) DrawPixel(x0, y0);
    }
}

// -----------------------------------------------------------------------------

void Overlay::DrawThickLine(int x0, int y0, int x1, int y1)
{
    // based on code from http://kt8216.unixcab.org/murphy/index.html

    // following code fixes alignment problems when linewidth is an even number
    if (x0 > x1) {
        // swap starting and end points so we always draw lines from left to right
        int tempx = x0; x0 = x1; x1 = tempx;
        int tempy = y0; y0 = y1; y1 = tempy;
    } else if (x0 == x1 && y0 > y1) {
        // swap y coords so vertical lines are always drawn from top to bottom
        int tempy = y0; y0 = y1; y1 = tempy;
    }

    int dx = x1 - x0;
    int dy = y1 - y0;
    int xstep = 1;
    int ystep = 1;
    int pxstep = 0;
    int pystep = 0;

    if (dx < 0) { dx = -dx; xstep = -1; }
    if (dy < 0) { dy = -dy; ystep = -1; }

    if (dx == 0 && dy == 0) {
        if (PixelInTarget(x0, y0)) DrawPixel(x0, y0);
        return;
    }

    if (dx == 0) xstep = 0;
    if (dy == 0) ystep = 0;

    switch (xstep + ystep*4) {
        case -1 + -1*4 : pystep = -1; pxstep =  1; break;   // -5
        case -1 +  0*4 : pystep = -1; pxstep =  0; break;   // -1
        case -1 +  1*4 : pystep =  1; pxstep =  1; break;   //  3
        case  0 + -1*4 : pystep =  0; pxstep = -1; break;   // -4
        case  0 +  0*4 : pystep =  0; pxstep =  0; break;   //  0
        case  0 +  1*4 : pystep =  0; pxstep =  1; break;   //  4
        case  1 + -1*4 : pystep = -1; pxstep = -1; break;   // -3
        case  1 +  0*4 : pystep = -1; pxstep =  0; break;   //  1
        case  1 +  1*4 : pystep =  1; pxstep = -1; break;   //  5
    }

    double D = sqrt(double(dx*dx + dy*dy));
    double D2 = 2*D;
    double w = (linewidth + 1) * D;

    // need to reduce thickness of line if linewidth is an even number
    // and line is vertical or horizontal
    if (linewidth % 2 == 0 && (dx == 0 || dy == 0)) {
        even_w = linewidth * D;
    } else {
        even_w = w;
    }

    // this hack is needed to improve antialiased sloped lines of width 2
    if (alphablend && linewidth == 2 && dx != 0 && dy != 0) {
        even_w = (linewidth + 1.75) * D;
        w = even_w;
    }

    int p_error = 0;
    int err = 0;
    int x = x0;
    int y = y0;

    if (dx > dy) {
        int threshold = dx - 2*dy;
        int E_diag = -2*dx;
        int E_square = 2*dy;
        int length = dx + 1;
        for (int p = 0; p < length; p++) {
            PerpendicularX(x, y, dx, dy, pxstep, pystep, p_error, err, w, D2);
            if (err >= threshold) {
                y += ystep;
                err += E_diag;
                if (p_error >= threshold) {
                    p_error += E_diag;
                    PerpendicularX(x, y, dx, dy, pxstep, pystep, p_error+E_square, err, w, D2);
                }
                p_error += E_square;
            }
            err += E_square;
            x += xstep;
        }
    } else {
        int threshold = dy - 2*dx;
        int E_diag = -2*dy;
        int E_square = 2*dx;
        int length = dy + 1;
        for (int p = 0; p < length; p++) {
            PerpendicularY(x, y, dx, dy, pxstep, pystep, p_error, err, w, D2);
            if (err >= threshold) {
                x += xstep;
                err += E_diag;
                if (p_error >= threshold) {
                    p_error += E_diag;
                    PerpendicularY(x, y, dx, dy, pxstep, pystep, p_error+E_square, err, w, D2);
                }
                p_error += E_square;
            }
            err += E_square;
            y += ystep;
        }
    }
}

// -----------------------------------------------------------------------------

void Overlay::DrawAntialiasedLine(int x0, int y0, int x1, int y1)
{
    // based on code from http://members.chello.at/~easyfilter/bresenham.html

    long dx = abs(x1-x0);
    long dy = abs(y1-y0);
    long err = dx-dy;
    long e2, x2;
    double ed = dx+dy == 0 ? 1 : sqrt(double(dx*dx+dy*dy));
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;

    while (true) {
        DrawAAPixel(x0, y0, 255*abs(err-dx+dy)/ed);
        e2 = err;
        x2 = x0;
        if (2*e2 >= -dx) {
            if (x0 == x1) break;
            if (e2+dy < ed) DrawAAPixel(x0, y0+sy, 255*(e2+dy)/ed);
            err -= dy;
            x0 += sx;
        }
        if (2*e2 <= dy) {
            if (y0 == y1) break;
            if (dx-e2 < ed) DrawAAPixel(x2+sx, y0, 255*(dx-e2)/ed);
            err += dx;
            y0 += sy;
        }
    }
}

// -----------------------------------------------------------------------------

const char *Overlay::DoLine(lua_State *L, int n, bool connected, int *nresults)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    // check if there are arguments
    // note: it is possible that n > 1 and arguments have nil values
    int valid = false;
    int i = 2;
    int type = -1;

    if (n > 1) {
        // get line start coordinate pair
        lua_rawgeti(L, 1, i++);
        int x1 = (int)lua_tonumberx(L, -1, &valid);
        lua_pop(L, 1);
        if (!valid) {
            if (connected) {
                return OverlayError("line command has illegal start x");
            } else {
                return OverlayError("lines command has illegal start x");
            }
        }
        lua_rawgeti(L, 1, i++);
        int y1 = (int)lua_tonumberx(L, -1, &valid);
        lua_pop(L, 1);
        if (!valid) {
            if (connected) {
                return OverlayError("line command has illegal start y");
            } else {
                return OverlayError("lines command has illegal start y");
            }
        }
        // get line end coordinate pair
        lua_rawgeti(L, 1, i++);
        int x2 = (int)lua_tonumberx(L, -1, &valid);
        lua_pop(L, 1);
        if (!valid) {
            if (connected) {
                return OverlayError("line command has illegal end x");
            } else {
                return OverlayError("lines command has illegal end x");
            }
        }
        lua_rawgeti(L, 1, i++);
        int y2 = (int)lua_tonumberx(L, -1, &valid);
        lua_pop(L, 1);
        if (!valid) {
            if (connected) {
                return OverlayError("line command has illegal end y");
            } else {
                return OverlayError("lines command has illegal end y");
            }
        }

        // mark target clip as changed
        DisableTargetClipIndex();

        // draw the first line
        RenderLine(x1, y1, x2, y2);

        // draw any follow on lines
        while (i <= n) {
            if (connected) {
                // start point is previous line's end point
                x1 = x2;
                y1 = y2;
            } else {
                // read the next start point
                lua_rawgeti(L, 1, i++);
                x1 = (int)lua_tonumberx(L, -1, &valid);
                if (!valid) break;
                lua_pop(L, 1);
                lua_rawgeti(L, 1, i++);
                y1 = (int)lua_tonumberx(L, -1, &valid);
                if (!valid) break;
                lua_pop(L, 1);
            }
            // read the next end point
            lua_rawgeti(L, 1, i++);
            x2 = (int)lua_tonumberx(L, -1, &valid);
            if (!valid) break;
            lua_pop(L, 1);
            lua_rawgeti(L, 1, i++);
            y2 = (int)lua_tonumberx(L, -1, &valid);
            if (!valid) break;
            lua_pop(L, 1);

            // draw the line
            RenderLine(x1, y1, x2, y2);
        }

        // check if loop terminated because of failed number conversion
        if (!valid) {
            // get the type of the argument
            type = lua_type(L, -1);
            lua_pop(L, 1);
        }
    } else {
        // no arguments supplied
        if (connected) {
            return OverlayError("line command requires at least two coordinate pairs");
        } else {
            return OverlayError("lines command requires at least two coordinate pairs");
        }
    }

    // check if there were errors
    if (!valid) {
        if (connected) {
            // check if the argument number is a multiple of 2 and the argument is nil
            if (!((((i - 3) & 1) == 0) && (type == LUA_TNIL))) {
                switch ((i - 3) & 1) {
                    case 0:
                        return OverlayError("line command has illegal end x");
                        break;
                    case 1:
                        return OverlayError("line command has illegal end y");
                        break;
                }
            }
        } else {
            // check if the argument number is a multiple of 4 and the argument is nil
            if (!((((i - 3) & 3) == 0) && (type == LUA_TNIL))) {
                switch ((i - 3) & 1) {
                    case 0:
                        return OverlayError("lines command has illegal start x");
                        break;
                    case 1:
                        return OverlayError("lines command has illegal start y");
                        break;
                    case 2:
                        return OverlayError("lines command has illegal end x");
                        break;
                    case 3:
                        return OverlayError("lines command has illegal end y");
                        break;
                }
            }
        }
    }

    return NULL;
}

// -----------------------------------------------------------------------------

const char *Overlay::DoLine(const char *args, bool connected)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    args = GetCoordinatePair((char*)args, &x1, &y1);
    if (!args) {
        if (connected) {
            return OverlayError("line command requires at least two coordinate pairs");
        } else {
            return OverlayError("lines command requires at least two coordinate pairs");
        }
    }
    args = GetCoordinatePair((char*)args, &x2, &y2);
    if (!args) {
        if (connected) {
            return OverlayError("line command requires at least two coordinate pairs");
        } else {
            return OverlayError("lines command requires at least two coordinate pairs");
        }
    }

    // mark target clip as changed
    DisableTargetClipIndex();

    // draw the line
    RenderLine(x1, y1, x2, y2);

    // read any further coordinates
    while (*args) {
        if (connected) {
            x1 = x2;
            y1 = y2;
            args = GetCoordinatePair((char*)args, &x2, &y2);
            if (!args) return OverlayError("line command has illegal coordinates");
        } else {
            args = GetCoordinatePair((char*)args, &x1, &y1);
            if (!args) return OverlayError("lines command has illegal coordinates");
            args = GetCoordinatePair((char*)args, &x2, &y2);
            if (!args) return OverlayError("lines command has illegal coordinates");
        }
        RenderLine(x1, y1, x2, y2);
    }
    return NULL;
}


// -----------------------------------------------------------------------------

void Overlay::RenderLine(int x0, int y0, int x1, int y1) {
    if (linewidth > 1) {
        DrawThickLine(x0, y0, x1, y1);
        return;
    }

    if (x0 == x1 && y0 == y1) {
        if (PixelInTarget(x0, y0)) DrawPixel(x0, y0);
        return;
    }

    if (alphablend) {
        DrawAntialiasedLine(x0, y0, x1, y1);
        return;
    }

    // no alpha blending so use fast copy
    unsigned int rgba = rgbadraw;
    unsigned int *lpixmap = (unsigned int*)pixmap;

    // draw a line of pixels from x0,y0 to x1,y1 using Bresenham's algorithm
    int dx = x1 - x0;
    int ax = abs(dx) * 2;
    int sx = dx < 0 ? -1 : 1;

    int dy = y1 - y0;
    int ay = abs(dy) * 2;
    int sy = dy < 0 ? -1 : 1;

    if (ax > ay) {
        int d = ay - (ax / 2);
        while (x0 != x1) {
            if (PixelInTarget(x0, y0)) *(lpixmap + y0*wd + x0) = rgba;
            if (d >= 0) {
                y0 = y0 + sy;
                d = d - ax;
            }
            x0 = x0 + sx;
            d = d + ay;
        }
    } else {
        int d = ax - (ay / 2);
        while (y0 != y1) {
            if (PixelInTarget(x0, y0)) *(lpixmap + y0*wd + x0) = rgba;
            if (d >= 0) {
                x0 = x0 + sx;
                d = d - ay;
            }
            y0 = y0 + sy;
            d = d + ax;
        }
    }
    if (PixelInTarget(x1, y1)) *(lpixmap + y1*wd + x1) = rgba;
}

// -----------------------------------------------------------------------------

void Overlay::DrawThickEllipse(int x0, int y0, int x1, int y1)
{
    // based on code from http://members.chello.at/~easyfilter/bresenham.html

    if (linewidth == 1) {
        if (alphablend) {
            DrawAntialiasedEllipse(x0, y0, x1, y1);
        } else {
            DrawEllipse(x0, y0, x1, y1);
        }
        return;
    }

    if (x1 == x0 || y1 == y0) {
        DrawThickLine(x0, y0, x1, y1);
        return;
    }

    double th = linewidth;
    long a0 = abs(x1-x0);
    long b0 = abs(y1-y0);
    long b1 = b0&1;
    double a2 = a0-2*th;
    double b2 = b0-2*th;
    double dx = 4*(a0-1)*b0*b0;
    double dy = 4*(b1-1)*a0*a0;
    double i = a0+b2;
    double err = b1*a0*a0;
    double dx2, dy2, e2, ed;

    if ((th-1)*(2*b0-th) > a0*a0) {
        b2 = sqrt(a0*(b0-a0)*i*a2)/(a0-th);
    }
    if ((th-1)*(2*a0-th) > b0*b0) {
        a2 = sqrt(b0*(a0-b0)*i*b2)/(b0-th);
        th = (a0-a2)/2;
    }

    if (b2 <= 0) th = a0;    // filled ellipse

    e2 = th-floor(th);
    th = x0+th-e2;
    dx2 = 4*(a2+2*e2-1)*b2*b2;
    dy2 = 4*(b1-1)*a2*a2;
    e2 = dx2*e2;
    y0 += (b0+1)>>1;
    y1 = y0-b1;
    a0 = 8*a0*a0;
    b1 = 8*b0*b0;
    a2 = 8*a2*a2;
    b2 = 8*b2*b2;

    do {
        while (true) {
            if (err < 0 || x0 > x1) { i = x0; break; }
            // do outside antialiasing
            i = dx < dy ? dx : dy;
            ed = dx > dy ? dx : dy;
            if (y0 == y1+1 && 2*err > dx && a0 > b1) {
                ed = a0/4;
            } else {
                ed += 2*ed*i*i/(4*ed*ed+i*i+1)+1;
            }
            i = 255*err/ed;
            // i can be > 255
            if (i <= 255) {
                // extra tests avoid some pixels being drawn twice
                if (x0 == x1) {
                    DrawAAPixel(x0, y0, i);
                    DrawAAPixel(x0, y1, i);
                } else if (y0 == y1) {
                    DrawAAPixel(x0, y0, i);
                    DrawAAPixel(x1, y0, i);
                } else {
                    // x0 != x1 and y0 != y1
                    DrawAAPixel(x0, y0, i);
                    DrawAAPixel(x0, y1, i);
                    DrawAAPixel(x1, y0, i);
                    DrawAAPixel(x1, y1, i);
                }
            }
            if (err+dy+a0 < dx) { i = x0+1; break; }
            x0++;
            x1--;
            err -= dx;
            dx -= b1;
        }
        while (i < th && 2*i <= x0+x1) {
            // set pixel within line
            int x = x0+x1-i;
            // extra tests avoid some pixels being drawn twice
            if (x == i && y0 == y1) {
                if (PixelInTarget(i, y0)) DrawPixel(i, y0);
            } else if (x == i) {
                if (PixelInTarget(i, y0)) DrawPixel(i, y0);
                if (PixelInTarget(i, y1)) DrawPixel(i, y1);
            } else if (y0 == y1) {
                if (PixelInTarget(i, y0)) DrawPixel(i, y0);
                if (PixelInTarget(x, y0)) DrawPixel(x, y0);
            } else {
                // x != i and y0 != y1
                if (PixelInTarget(i, y0)) DrawPixel(i, y0);
                if (PixelInTarget(x, y0)) DrawPixel(x, y0);
                if (PixelInTarget(i, y1)) DrawPixel(i, y1);
                if (PixelInTarget(x, y1)) DrawPixel(x, y1);
            }
            i++;
        }
        while (e2 > 0 && x0+x1 >= 2*th) {
            // do inside antialiasing
            i = dx2 < dy2 ? dx2 : dy2;
            ed = dx2 > dy2 ? dx2 : dy2;
            if (y0 == y1+1 && 2*e2 > dx2 && a2 > b2) {
                ed = a2/4;
            } else {
                ed += 2*ed*i*i/(4*ed*ed+i*i);
            }
            i = 255-255*e2/ed;
            // i can be -ve
            if (i < 0) i = 0;
            int x = x0+x1-th;
            // extra test avoids some pixels being drawn twice
            if (x == th) {
                DrawAAPixel(x, y0, i);
                DrawAAPixel(x, y1, i);
            } else {
                DrawAAPixel(th, y0, i);
                DrawAAPixel(x,  y0, i);
                DrawAAPixel(th, y1, i);
                DrawAAPixel(x,  y1, i);
            }
            if (e2+dy2+a2 < dx2) break;
            th++;
            e2 -= dx2;
            dx2 -= b2;
        }
        e2 += dy2 += a2;
        y0++;
        y1--;
        err += dy += a0;
    } while (x0 < x1);

    if (y0-y1 <= b0) {
        if (err > dy+a0) {
            y0--;
            y1++;
            err -= dy -= a0;
        }
        while (y0-y1 <= b0) {
            i = 255*4*err/b1;
            DrawAAPixel(x0, y0,   i);
            DrawAAPixel(x1, y0++, i);
            DrawAAPixel(x0, y1,   i);
            DrawAAPixel(x1, y1--, i);
            err += dy += a0;
        }
    }
}

// -----------------------------------------------------------------------------

void Overlay::DrawAntialiasedEllipse(int x0, int y0, int x1, int y1)
{
    // based on code from http://members.chello.at/~easyfilter/bresenham.html

    long a0 = abs(x1-x0);
    long b0 = abs(y1-y0);
    long b1 = b0&1;
    double dx = 4*(a0-1.0)*b0*b0;
    double dy = 4*(b1+1.0)*a0*a0;
    double err = b1*a0*a0-dx+dy;
    double ed, i;
    bool f;

    if (a0 == 0 || b0 == 0) {
        DrawAntialiasedLine(x0, y0, x1, y1);
        return;
    }

    y0 += (b0+1)/2;
    y1 = y0-b1;
    a0 = 8*a0*a0;
    b1 = 8*b0*b0;

    while (true) {
        i = dx < dy ? dx : dy;
        ed = dx > dy ? dx : dy;
        if (y0 == y1+1 && err > dy && a0 > b1) {
            ed = 255*4.0/a0;
        } else {
            ed = 255/(ed+2*ed*i*i/(4*ed*ed+i*i));
        }
        i = ed*fabs(err+dx-dy);     // intensity depends on pixel error

        // extra tests avoid pixels at extremities being drawn twice
        if (x0 == x1) {
            DrawAAPixel(x0, y0, i);
            DrawAAPixel(x0, y1, i);
        } else if (y0 == y1) {
            DrawAAPixel(x0, y0, i);
            DrawAAPixel(x1, y0, i);
        } else {
            // x0 != x1 and y0 != y1
            DrawAAPixel(x0, y0, i);
            DrawAAPixel(x0, y1, i);
            DrawAAPixel(x1, y0, i);
            DrawAAPixel(x1, y1, i);
        }

        f = 2*err+dy >= 0;
        if (f) {
            if (x0 >= x1) break;
            i = ed*(err+dx);
            if (i < 255) {
                DrawAAPixel(x0, y0+1, i);
                DrawAAPixel(x0, y1-1, i);
                DrawAAPixel(x1, y0+1, i);
                DrawAAPixel(x1, y1-1, i);
            }
        }
        if (2*err <= dx) {
            i = ed*(dy-err);
            if (i < 255) {
                DrawAAPixel(x0+1, y0, i);
                DrawAAPixel(x1-1, y0, i);
                DrawAAPixel(x0+1, y1, i);
                DrawAAPixel(x1-1, y1, i);
            }
            y0++;
            y1--;
            err += dy += a0;
        }
        if (f) {
            x0++;
            x1--;
            err -= dx -= b1;
        }
    }

    if (--x0 == x1++) {
        while (y0-y1 < b0) {
            i = 255*4*fabs(err+dx)/b1;
            DrawAAPixel(x0, ++y0, i);
            DrawAAPixel(x1,   y0, i);
            DrawAAPixel(x0, --y1, i);
            DrawAAPixel(x1,   y1, i);
            err += dy += a0;
        }
    }
}

// -----------------------------------------------------------------------------

void Overlay::DrawEllipse(int x0, int y0, int x1, int y1)
{
    // based on code from http://members.chello.at/~easyfilter/bresenham.html

    long a0 = abs(x1-x0);
    long b0 = abs(y1-y0);
    long b1 = b0&1;
    double dx = 4*(1.0-a0)*b0*b0;
    double dy = 4*(b1+1.0)*a0*a0;
    double err = dx+dy+b1*a0*a0;
    double e2;

    y0 += (b0+1)/2;
    y1 = y0-b1;
    a0 *= 8*a0;
    b1 = 8*b0*b0;

    do {
        if (PixelInTarget(x1, y0)) DrawPixel(x1, y0);
        if (PixelInTarget(x0, y0)) DrawPixel(x0, y0);
        if (PixelInTarget(x0, y1)) DrawPixel(x0, y1);
        if (PixelInTarget(x1, y1)) DrawPixel(x1, y1);
        e2 = 2*err;
        if (e2 <= dy) {
            y0++;
            y1--;
            err += dy += a0;
        }
        if (e2 >= dx || 2*err > dy) {
            x0++;
            x1--;
            err += dx += b1;
        }
    } while (x0 <= x1);

    // note that next test must be <= b0
    while (y0-y1 <= b0) {
        // finish tip of ellipse
        if (PixelInTarget(x0-1, y0)) DrawPixel(x0-1, y0);
        if (PixelInTarget(x1+1, y0)) DrawPixel(x1+1, y0);
        if (PixelInTarget(x0-1, y1)) DrawPixel(x0-1, y1);
        if (PixelInTarget(x1+1, y1)) DrawPixel(x1+1, y1);
        y0++;
        y1--;
    }
}

// -----------------------------------------------------------------------------

const char *Overlay::DoEllipse(const char *args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    int x, y, w, h;
    if (sscanf(args, " %d %d %d %d", &x, &y, &w, &h) != 4) {
        return OverlayError("ellipse command requires 4 arguments");
    }

    // treat non-positive w/h as inset from overlay's width/height
    if (w <= 0) w = wd + w;
    if (h <= 0) h = ht + h;
    if (w <= 0) return OverlayError("ellipse width must be > 0");
    if (h <= 0) return OverlayError("ellipse height must be > 0");

    // mark target clip as changed
    DisableTargetClipIndex();

    if (linewidth > 1) {
        DrawThickEllipse(x, y, x+w-1, y+h-1);
        return NULL;
    }

    if (alphablend) {
        DrawAntialiasedEllipse(x, y, x+w-1, y+h-1);
        return NULL;
    }

    // draw a non-antialiased ellipse where linewidth is 1
    DrawEllipse(x, y, x+w-1, y+h-1);
    return NULL;
}

// -----------------------------------------------------------------------------

void Overlay::FillRect(int x, int y, int w, int h)
{
    // get rgba drawing color
    const unsigned int source = rgbadraw;
    // get destination location
    unsigned int *lp = ((unsigned int*)pixmap) + y * wd + x;
    // check for alphablending
    if (alphablend && a < 255) {
        // only draw if source not transparent
        if (a) {
            const unsigned int alpha = a + 1;
            const unsigned int invalpha = 256 - a;
            const unsigned int sourcearb = alpha * RBRIGHT(source & RBMASK);
            const unsigned int sourceag =  alpha *        (source & GMASK);
            unsigned int dest;
            if (alphablend == 1) {
                // full alpha blend
                for (int j = 0; j < h; j++) {
                    for (int i = 0; i < w; i++) {
                        dest = *lp;
                        ALPHABLENDPRE(source, sourcearb, sourceag, dest, lp, alpha, invalpha);
                        lp++;
                    }
                    lp += wd - w;
                }
            } else {
                // fast alpha blend (opaque destination)
                for (int j = 0 ; j < h; j++) {
                    for (int i = 0; i < w; i++) {
                        dest = *lp;
                        ALPHABLENDPREOPAQUEDEST(sourcearb, sourceag, dest, lp, invalpha);
                        lp++;
                    }
                    lp += wd - w;
                }
            }
        }
    } else {
        // create first row
        unsigned int *dest = lp;
        for (int i = 0; i < w; i++) {
            *dest++ = source;
        }

        // copy first row to remaining rows
        dest = lp;
        int wbytes = w * 4;
        for (int i = 1; i < h; i++) {
            dest += wd;
            memcpy(dest, lp, wbytes);
        }
    }
}

// -----------------------------------------------------------------------------

const char *Overlay::DoFill(lua_State *L, int n, int *nresults)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    // check if there are arguments
    // note: it is possible that n > 1 and arguments have nil values
    int valid = false;
    int i = 2;
    int type = -1;

    if (n > 1) {
        // mark target clip as changed
        DisableTargetClipIndex();

        // draw each rectangle
        do {
            // get the coordinates
            lua_rawgeti(L, 1, i++);
            int x = (int)lua_tonumberx(L, -1, &valid);
            if (!valid) break;
            lua_pop(L, 1);
            lua_rawgeti(L, 1, i++);
            int y = (int)lua_tonumberx(L, -1, &valid);
            if (!valid) break;
            lua_pop(L, 1);
            lua_rawgeti(L, 1, i++);
            int w = (int)lua_tonumberx(L, -1, &valid);
            if (!valid) break;
            lua_pop(L, 1);
            lua_rawgeti(L, 1, i++);
            int h = (int)lua_tonumberx(L, -1, &valid);
            if (!valid) break;
            lua_pop(L, 1);

            // treat non-positive w/h as inset from overlay's width/height
            if (w <= 0) w = wd + w;
            if (h <= 0) h = ht + h;
            if (w <= 0) return OverlayError("fill width must be > 0");
            if (h <= 0) return OverlayError("fill height must be > 0");

            // ignore rect if completely outside target edges
            if (!RectOutsideTarget(x, y, w, h)) {
                // clip any part of rect outside target edges
                int xmax = x + w - 1;
                int ymax = y + h - 1;
                if (x < 0) x = 0;
                if (y < 0) y = 0;
                if (xmax >= wd) xmax = wd - 1;
                if (ymax >= ht) ymax = ht - 1;
                w = xmax - x + 1;
                h = ymax - y + 1;

                // fill visible rect with current RGBA values
                FillRect(x, y, w, h);
            }
        } while (i <= n);

        // check if loop terminated because of failed number conversion
        if (!valid) {
            // get the type of the argument
            type = lua_type(L, -1);
            lua_pop(L, 1);
        }
    }

    // check if there were no arguments
    // either none supplied, or the first argument value was nil
    if (n == 1 || (i == 3 && type == LUA_TNIL)) {
        // fill entire target with current RGBA values
        FillRect(0, 0, wd, ht);
        valid = true;
    }

    // check if there were errors
    if (!valid) {
        // check if the argument number is a multiple of 4 and the argument is nil
        if ((((i - 3) & 3) == 0) && (type == LUA_TNIL)) {
            // command was valid
            valid = true;
        }
    }

    if (!valid) {
        // return appropriate error message
        switch ((i - 3) & 3) {
            case 0:
                return OverlayError("fill command has illegal x");
                break;
            case 1:
                return OverlayError("fill command has illegal y");
                break;
            case 2:
                return OverlayError("fill command has illegal width");
                break;
            case 3:
                return OverlayError("fill command has illegal height");
                break;
        }
    }

    return NULL;
}

// -----------------------------------------------------------------------------

const char *Overlay::DoFill(const char *args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    if (*args == ' ') {
        int x = 0, y = 0, w = 0, h = 0;
        args = GetCoordinatePair((char*)args, &x, &y);
        if (!args) return OverlayError("fill command requires 0 or at least 4 arguments");
        args = GetCoordinatePair((char*)args, &w, &h);
        if (!args) return OverlayError("fill command requires 0 or at least 4 arguments");

        // treat non-positive w/h as inset from overlay's width/height
        if (w <= 0) w = wd + w;
        if (h <= 0) h = ht + h;
        if (w <= 0) return OverlayError("fill width must be > 0");
        if (h <= 0) return OverlayError("fill height must be > 0");

        // mark target clip as changed
        DisableTargetClipIndex();

        // ignore rect if completely outside target edges
        if (!RectOutsideTarget(x, y, w, h)) {
            // clip any part of rect outside target edges
            int xmax = x + w - 1;
            int ymax = y + h - 1;
            if (x < 0) x = 0;
            if (y < 0) y = 0;
            if (xmax >= wd) xmax = wd - 1;
            if (ymax >= ht) ymax = ht - 1;
            w = xmax - x + 1;
            h = ymax - y + 1;

            // fill visible rect with current RGBA values
            FillRect(x, y, w, h);
        }

        while (*args) {
            args = GetCoordinatePair((char*)args, &x, &y);
            if (!args) return OverlayError("fill command invalid arguments");
            args = GetCoordinatePair((char*)args, &w, &h);
            if (!args) return OverlayError("fill command invalid arguments");

            // treat non-positive w/h as inset from overlay's width/height
            if (w <= 0) w = wd + w;
            if (h <= 0) h = ht + h;
            if (w <= 0) return OverlayError("fill width must be > 0");
            if (h <= 0) return OverlayError("fill height must be > 0");

            // ignore rect if completely outside target edges
            if (!RectOutsideTarget(x, y, w, h)) {
                // clip any part of rect outside target edges
                int xmax = x + w - 1;
                int ymax = y + h - 1;
                if (x < 0) x = 0;
                if (y < 0) y = 0;
                if (xmax >= wd) xmax = wd - 1;
                if (ymax >= ht) ymax = ht - 1;
                w = xmax - x + 1;
                h = ymax - y + 1;

                // fill visible rect with current RGBA values
                FillRect(x, y, w, h);
            }
        }
    } else {
        // mark target clip as changed
        DisableTargetClipIndex();

        // fill entire target with current RGBA values
        FillRect(0, 0, wd, ht);
    }

    return NULL;
}

// -----------------------------------------------------------------------------

const char *Overlay::DoCopy(const char *args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    int x, y, w, h;
    int namepos;
    char dummy;
    if (sscanf(args, " %d %d %d %d %n%c", &x, &y, &w, &h, &namepos, &dummy) != 5) {
        // note that %n is not included in the count
        return OverlayError("copy command requires 5 arguments");
    }

    // treat non-positive w/h as inset from overlay's width/height
    // (makes it easy to copy entire overlay via "copy 0 0 0 0 all")
    if (w <= 0) w = wd + w;
    if (h <= 0) h = ht + h;
    if (w <= 0) return OverlayError("copy width must be > 0");
    if (h <= 0) return OverlayError("copy height must be > 0");

    std::string name = args + namepos;

    // delete any existing clip data with the given name
    std::map<std::string,Clip*>::iterator it;
    it = clips.find(name);
    if (it != clips.end()) {
        delete it->second;
        clips.erase(it);
    }

    bool use_calloc;
    if (RectInsideTarget(x, y, w, h)) {
        // use malloc to allocate clip memory
        use_calloc = false;
    } else {
        // use calloc so parts outside target will be transparent
        use_calloc = true;
    }

    Clip *newclip = new Clip(w, h, use_calloc);
    if (newclip == NULL || newclip->cdata == NULL) {
        delete newclip;
        return OverlayError("not enough memory to copy pixels");
    }

    if (use_calloc) {
        if (RectOutsideTarget(x, y, w, h)) {
            // clip rect is completely outside target so no need to copy
            // target pixels (clip pixels are all transparent)
        } else {
            // calculate offsets in clip data and bytes per row
            int clipx = x >= 0 ? 0 : -x;
            int clipy = y >= 0 ? 0 : -y;
            int cliprowbytes = w * 4;

            // set x,y,w,h to intersection with target
            int xmax = x + w - 1;
            int ymax = y + h - 1;
            if (x < 0) x = 0;
            if (y < 0) y = 0;
            if (xmax >= wd) xmax = wd - 1;
            if (ymax >= ht) ymax = ht - 1;
            w = xmax - x + 1;
            h = ymax - y + 1;

            // copy intersection rect from target into corresponding area of clip data
            unsigned char *dest = newclip->cdata + clipy*cliprowbytes + clipx*4;
            int rowbytes = wd * 4;
            int wbytes = w * 4;
            unsigned char *src = pixmap + y*rowbytes + x*4;
            for (int i = 0; i < h; i++) {
                memcpy(dest, src, wbytes);
                src += rowbytes;
                dest += cliprowbytes;
            }
        }
    } else {
        // given rectangle is within target so fill newclip->cdata with
        // pixel data from that rectangle in pixmap
        unsigned char *dest = newclip->cdata;

        if (x == 0 && y == 0 && w == wd && h == ht) {
            // clip and overlay are the same size so do a fast copy
            memcpy(dest, pixmap, w * h * 4);

        } else {
            // use memcpy to copy each row
            int rowbytes = wd * 4;
            int wbytes = w * 4;
            unsigned char *src = pixmap + y*rowbytes + x*4;
            for (int i = 0; i < h; i++) {
                memcpy(dest, src, wbytes);
                src += rowbytes;
                dest += wbytes;
            }
        }
    }

    clips[name] = newclip;      // create named clip for later use by paste, scale, etc

    return NULL;
}

// -----------------------------------------------------------------------------

void Overlay::DisableTargetClipIndex()
{
    // if the current target is a clip then it may have been modified so disable index
    if (renderclip) {
        renderclip->RemoveIndex();
    }
}

// -----------------------------------------------------------------------------

const char *Overlay::DoOptimize(const char *args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    int namepos;
    char dummy;
    if (sscanf(args, " %n%c", &namepos, &dummy) != 1) {
        // note that %n is not included in the count
        return OverlayError("optimize command requires an argument");
    }

    std::string name = args + namepos;
    std::map<std::string,Clip*>::iterator it;
    it = clips.find(name);
    if (it == clips.end()) {
        static std::string msg;
        msg = "unknown optimize clip (";
        msg += name;
        msg += ")";
        return OverlayError(msg.c_str());
    }
    Clip *clipptr = it->second;

    // add index to the clip
    clipptr->AddIndex();

    // return the bounding box x, y, w, h of non-transparent pixels in the clip
    static char result[64];
    sprintf(result, "%d %d %d %d", clipptr->xbb, clipptr->ybb, clipptr->wbb, clipptr->hbb);
    return result;
}

// -----------------------------------------------------------------------------

const char *Overlay::DoPaste(lua_State *L, int n, int *nresults)
{
    const char *result = NULL;

    // clip name
    const char *clipname = NULL;
    int clipi = 0;

    // allocate space for coordinate values
    if (n > 1) {
        int *coords = (int*)malloc((n - 1) * sizeof(int));
        int j = 0;

        // get the array of coordinates
        int valid = true;
        int i = 2;
        while (i <= n && valid) {
            // read the element at the next index
            lua_rawgeti(L, 1, i);
            // attempt to decode as a number
            lua_Number value = lua_tonumberx(L, -1, &valid);
            if (valid) {
                // store the number
                coords[j++] = (int)value;
            } else {
                // was not a number so check the type
                int type = lua_type(L, -1);
                if (type == LUA_TSTRING) {
                    // first time decode as a string after that it's an error
                    if (clipname == NULL) {
                        clipname = lua_tostring(L, -1);
                        clipi = i;
                        valid = true;
                    }
                } else {
                    if (type == LUA_TNIL) {
                        // if it's nil then stop
                        n = i - 1;
                        valid = true;
                    }
                }
            }
            lua_pop(L, 1);
            i++;
        }

        // clip name must be last argument
        if (clipname && (clipi != n)) {
            valid = false;
        }

        // check if the coordinates were all numbers
        static std::string msg;
        if (valid) {
            // lookup the named clip
            std::string name = clipname;
            std::map<std::string,Clip*>::iterator it;
            it = clips.find(name);
            if (it == clips.end()) {
                msg = "unknown paste clip (";
                msg += name;
                msg += ")";
                result = OverlayError(msg.c_str());
            } else {
                // mark target clip as changed
                DisableTargetClipIndex();

                // call the required function
                Clip *clipptr = it->second;
                result = DoPaste(coords, j, clipptr);
            }
        }

        // free argument list
        free(coords);

        if (!valid) {
            result = OverlayError("paste command has invalid arguments");
        }
    }

    return result;
}

// -----------------------------------------------------------------------------

const char *Overlay::DoPaste(const int *coords, int n, const Clip *clipptr)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    // check that coordinates are supplied and that there are an even number
    if (clipptr == NULL) return OverlayError("paste command requires a clip");
    if (n < 2) return OverlayError("paste command requires coordinate pairs");
    if ((n & 1) != 0) return OverlayError("paste command has illegal coordinates");

    // clip dimensions and data
    int w, h, xoff, yoff;
    unsigned int *clipdata;
    if (alphablend) {
        // use non-zero alpha bounding box if alphablending
        w = clipptr->wbb;
        h = clipptr->hbb;
        xoff = clipptr->xbb;
        yoff = clipptr->ybb;
        clipdata = (unsigned int*)clipptr->cdatabb;
    } else {
        // use entire clip if not
        w = clipptr->cwd;
        h = clipptr->cht;
        xoff = 0;
        yoff = 0;
        clipdata = (unsigned int*)clipptr->cdata;
    }

    // paste at each coordinate pair
    const int ow = w;
    const int oh = h;
    int ci = 0;
    do {
        // add the bounding box offset to the coordinates
        int x = coords[ci++] + xoff;
        int y = coords[ci++] + yoff;

        // set original width and height since these can be changed below if clipping required
        w = ow;
        h = oh;

        // discard if location is completely outside target
        if (!RectOutsideTarget(x, y, w, h)) {
            // check for transformation
            if (identity) {
                // no transformation, check for clip and target the same size without alpha blending
                if (!alphablend && x == 0 && y == 0 && w == wd && h == ht) {
                    // fast paste with single memcpy call
                    memcpy(pixmap, clipptr->cdata, w * h * 4);
                } else {
                    // get the clip data
                    unsigned int *ldata = clipdata;
                    int cliprowpixels = clipptr->cwd;
                    int rowoffset = yoff;

                    // check for clipping
                    int xmax = x + w - 1;
                    int ymax = y + h - 1;
                    if (x < 0) {
                        // skip pixels off left edge
                        ldata += -x;
                        x = 0;
                    }
                    if (y < 0) {
                        // skip pixels off top edge
                        ldata += -y * cliprowpixels;
                        rowoffset += -y;
                        y = 0;
                    }
                    if (xmax >= wd) xmax = wd - 1;
                    if (ymax >= ht) ymax = ht - 1;
                    w = xmax - x + 1;
                    h = ymax - y + 1;

                    // get the paste target data
                    int targetrowpixels = wd;
                    unsigned int *lp = (unsigned int*)pixmap;
                    lp += y * targetrowpixels + x;
                    unsigned int source, dest, pa, alpha, invalpha;

                    // check for alpha blending
                    if (alphablend) {
                        // alpha blending
                        const rowtype *rowindex = clipptr->rowindex;
                        if (!rowindex) {
                            // clip only has mixed alpha rows
                            for (int j = 0; j < h; j++) {
                                // row contains pixels with different alpha values
                                if (alphablend == 1) {
                                    // full alpha blend
                                    for (int i = 0; i < w ; i++) {
                                        source = *ldata;
                                        pa = ALPHA2BYTE(source);
                                        if (pa < 255) {
                                            // source pixel is not opaque
                                            if (pa) {
                                                // source pixel is translucent so blend with destination pixel
                                                alpha = pa + 1;
                                                invalpha = 256 - pa;
                                                dest = *lp;
                                                ALPHABLEND(source, dest, lp, alpha, invalpha);
                                            }
                                        } else {
                                            // pixel is opaque so copy it
                                            *lp = source;
                                        }
                                        lp++;
                                        ldata++;
                                    }
                                } else {
                                    // fast alpha blend (opaque destination)
                                    for (int i = 0; i < w; i++) {
                                        source = *ldata++;
                                        pa = ALPHA2BYTE(source);
                                        alpha = pa + 1;
                                        invalpha = 256 - pa;
                                        dest = *lp;
                                        ALPHABLENDOPAQUEDEST(source, dest, lp, alpha, invalpha);
                                        lp++;
                                    }
                                }
                                // next clip and target row
                                lp += targetrowpixels - w;
                                ldata += cliprowpixels - w;
                            }
                        } else {
                            for (int j = rowoffset; j < h + rowoffset; j++) {
                                rowtype rowflag = rowindex[j];
                                switch (rowflag) {
                                case alpha0:
                                    // if all pixels are transparent then skip the row
                                    ldata += w;
                                    lp += w;
                                    break;
                                case opaque:
                                    // if all pixels are opaque then use memcpy
                                    memcpy(lp, ldata, w << 2);
                                    ldata += w;
                                    lp += w;
                                    break;
                                case both:
                                    // row contains mix of opaque and transparent pixels
                                    for (int i = 0; i < w; i++) {
                                        source = *ldata++;
                                        // copy pixel if not transparent
                                        if (source & AMASK) {
                                            *lp = source;
                                        }
                                        lp++;
                                    }
                                    break;
                                case mixed:
                                    // row contains pixels with different alpha values
                                    if (alphablend == 1) {
                                        // full alpha blend
                                        for (int i = 0; i < w; i++) {
                                            source = *ldata;
                                            pa = ALPHA2BYTE(source);
                                            if (pa < 255) {
                                                // source pixel is not opaque
                                                if (pa) {
                                                    // source pixel is translucent so blend with destination pixel
                                                    alpha = pa + 1;
                                                    invalpha = 256 - pa;
                                                    dest = *lp;
                                                    ALPHABLEND(source, dest, lp, alpha, invalpha);
                                                }
                                            } else {
                                                // pixel is opaque so copy it
                                                *lp = source;
                                            }
                                            lp++;
                                            ldata++;
                                        }
                                    } else {
                                        // fast alpha blend (destination is opaque)
                                        for (int i = 0; i < w; i++) {
                                            source = *ldata++;
                                            pa = ALPHA2BYTE(source);
                                            alpha = pa + 1;
                                            invalpha = 256 - pa;
                                            dest = *lp;
                                            ALPHABLENDOPAQUEDEST(source, dest, lp, alpha, invalpha);
                                            lp++;
                                        }
                                    }
                                    break;
                                }
                                // next clip and target row
                                lp += targetrowpixels - w;
                                ldata += cliprowpixels - w;
                            }
                        }
                    } else {
                        // no alpha blending
                        for (int j = 0; j < h; j++) {
                            // copy each row with memcpy
                            memcpy(lp, ldata, w << 2);
                            lp += targetrowpixels;
                            ldata += cliprowpixels;
                        }
                    }
                }
            } else {
                // do an affine transformation
                unsigned int *data = (unsigned int*)clipptr->cdata;
                w = clipptr->cwd;
                h = clipptr->cht;
                x -= xoff;
                y -= yoff;
                const int x0 = x - (x * axx + y * axy);
                const int y0 = y - (x * ayx + y * ayy);

                // check for alpha blend
                if (alphablend) {
                    // save RGBA values
                    unsigned int savergba = rgbadraw;
                    unsigned char savea = a;

                    for (int j = 0; j < h; j++) {
                        for (int i = 0; i < w; i++) {
                            rgbadraw = *data++;
                            a = ALPHA2BYTE(rgbadraw);
                            int newx = x0 + x * axx + y * axy;
                            int newy = y0 + x * ayx + y * ayy;
                            if (PixelInTarget(newx, newy)) DrawPixel(newx, newy);
                            x++;
                        }
                        y++;
                        x -= w;
                    }

                    // restore saved RGBA values
                    rgbadraw = savergba;
                    a = savea;
                } else {
                    // no alpha blend
                    unsigned int *ldata = (unsigned int*)data;
                    unsigned int *lp = (unsigned int*)pixmap;
                    for (int j = 0; j < h; j++) {
                        for (int i = 0; i < w; i++) {
                            int newx = x0 + x * axx + y * axy;
                            int newy = y0 + x * ayx + y * ayy;
                            if (PixelInTarget(newx, newy)) *(lp + newy * wd + newx) = *ldata;
                            ldata++;
                            x++;
                        }
                        y++;
                        x -= w;
                    }
                }
            }
        }
    }
    while (ci < n - 1);

    return NULL;
}

// -----------------------------------------------------------------------------

const char *Overlay::DoPaste(const char *args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    int x, y;

    // find out the length of the argument string
    int arglen = strlen(args);
    if (arglen == 0) {
        return OverlayError("paste command requires at least 3 arguments");
    }

    // make a copy of the arguments so we can change them
    char *buffer = (char*)malloc(arglen + 1);   // add 1 for the terminating nul
    if (buffer == NULL) return OverlayError("not enough memory for paste");
    char *copy = buffer;
    strcpy(copy, args);

    // find the last argument which should be the clip name
    char *lastarg = copy + arglen - 1;

    // skip trailing whitespace
    while (lastarg >= copy && *lastarg == ' ') {
        lastarg--;
    }

    // skip until whitespace
    while (lastarg >= copy && *lastarg != ' ') {
        lastarg--;
    }

    // check if clip name was found
    if (lastarg < copy) {
        free(buffer);
        return OverlayError("paste command requires at least 3 arguments");
    }

    // null terminate the arguments before the clip name
    *lastarg++ = 0;

    // lookup the named clip
    std::string name = lastarg;
    std::map<std::string,Clip*>::iterator it;
    it = clips.find(name);
    if (it == clips.end()) {
        static std::string msg;
        msg = "unknown paste clip (";
        msg += name;
        msg += ")";
        free(buffer);
        return OverlayError(msg.c_str());
    }
    Clip *clipptr = it->second;

    // read the first coordinate pair
    copy = (char*)GetCoordinatePair(copy, &x, &y);
    if (!copy) {
        free(buffer);
        return OverlayError("paste command requires a least one coordinate pair");
    }

    // mark target clip as changed
    DisableTargetClipIndex();

    // clip dimensions and data
    int w, h, xoff, yoff;
    unsigned int *clipdata;
    if (alphablend) {
        // use non-zero alpha bounding box if alphablending
        w = clipptr->wbb;
        h = clipptr->hbb;
        xoff = clipptr->xbb;
        yoff = clipptr->ybb;
        clipdata = (unsigned int*)clipptr->cdatabb;
    } else {
        // use entire clip if not
        w = clipptr->cwd;
        h = clipptr->cht;
        xoff = 0;
        yoff = 0;
        clipdata = (unsigned int*)clipptr->cdata;
    }

    // paste at each coordinate pair
    const int ow = w;
    const int oh = h;
    do {
        // add the bounding box offset to the coordinates
        x += xoff;
        y += yoff;

        // set original width and height since these can be changed below if clipping required
        w = ow;
        h = oh;

        if (!RectOutsideTarget(x, y, w, h)) {
            // check for transformation
            if (identity) {
                // no transformation, check for clip and target the same size without alpha blending
                if (!alphablend && x == 0 && y == 0 && w == wd && h == ht) {
                    // fast paste with single memcpy call
                    memcpy(pixmap, clipptr->cdata, w * h * 4);
                } else {
                    // get the clip data
                    unsigned int *ldata = clipdata;
                    int cliprowpixels = clipptr->cwd;
                    int rowoffset = yoff;

                    // check for clipping
                    int xmax = x + w - 1;
                    int ymax = y + h - 1;
                    if (x < 0) {
                        // skip pixels off left edge
                        ldata += -x;
                        x = 0;
                    }
                    if (y < 0) {
                        // skip pixels off top edge
                        ldata += -y * cliprowpixels;
                        rowoffset += -y;
                        y = 0;
                    }
                    if (xmax >= wd) xmax = wd - 1;
                    if (ymax >= ht) ymax = ht - 1;
                    w = xmax - x + 1;
                    h = ymax - y + 1;

                    // get the paste target data
                    int targetrowpixels = wd;
                    unsigned int *lp = (unsigned int*)pixmap;
                    lp += y * targetrowpixels + x;
                    unsigned int source, dest, pa, alpha, invalpha;

                    // check for alpha blending
                    if (alphablend) {
                        // alpha blending
                        const rowtype *rowindex = clipptr->rowindex;
                        if (!rowindex) {
                            // clip only has mixed alpha rows
                            for (int j = 0; j < h; j++) {
                                // row contains pixels with different alpha values
                                if (alphablend == 1) {
                                    // full alpha blend
                                    for (int i = 0; i < w; i++) {
                                        // get the source pixel
                                        source = *ldata;
                                        pa = ALPHA2BYTE(source);
                                        if (pa < 255) {
                                            // source pixel is not opaque
                                            if (pa) {
                                                // source pixel is translucent so blend with destination pixel
                                                alpha = pa + 1;
                                                invalpha = 256 - pa;
                                                dest = *lp;
                                                ALPHABLEND(source, dest, lp, alpha, invalpha);
                                            }
                                        } else {
                                            // pixel is opaque so copy it
                                            *lp = source;
                                        }
                                        lp++;
                                        ldata++;
                                    }
                                } else {
                                    // fast alpha blend (opaque destination)
                                    for (int i = 0; i < w; i++) {
                                        source = *ldata++;
                                        pa = ALPHA2BYTE(source);
                                        alpha = pa + 1;
                                        invalpha = 256 - pa;
                                        dest = *lp;
                                        ALPHABLENDOPAQUEDEST(source, dest, lp, alpha, invalpha);
                                        lp++;
                                    }
                                }
                                // next clip and target row
                                lp += targetrowpixels - w;
                                ldata += cliprowpixels - w;
                            }
                        } else {
                            for (int j = rowoffset; j < h + rowoffset; j++) {
                                rowtype rowflag = rowindex[j];
                                switch (rowflag) {
                                case alpha0:
                                    // if all pixels are transparent then skip the row
                                    ldata += w;
                                    lp += w;
                                    break;
                                case opaque:
                                    // if all pixels are opaque then use memcpy
                                    memcpy(lp, ldata, w << 2);
                                    ldata += w;
                                    lp += w;
                                    break;
                                case both:
                                    // row contains mix of opaque and transparent pixels
                                    for (int i = 0; i < w; i++) {
                                        source = *ldata++;
                                        // copy pixel if not transparent
                                        if (source & AMASK) {
                                            *lp = source;
                                        }
                                        lp++;
                                    }
                                    break;
                                case mixed:
                                    // row contains pixels with different alpha values
                                    if (alphablend == 1) {
                                        // full alpha blend
                                        for (int i = 0; i < w; i++) {
                                            // get the source pixel
                                            source = *ldata;
                                            pa = ALPHA2BYTE(source);
                                            if (pa < 255) {
                                                // source pixel is not opaque
                                                if (pa) {
                                                    // source pixel is translucent so blend with destination pixel
                                                    alpha = pa + 1;
                                                    invalpha = 256 - pa;
                                                    dest = *lp;
                                                    ALPHABLEND(source, dest, lp, alpha, invalpha);
                                                }
                                            } else {
                                                // pixel is opaque so copy it
                                                *lp = source;
                                            }
                                            lp++;
                                            ldata++;
                                        }
                                    } else {
                                        // fast alpha blend (destination is opaque)
                                        for (int i = 0; i < w; i++) {
                                            source = *ldata++;
                                            pa = ALPHA2BYTE(source);
                                            alpha = pa + 1;
                                            invalpha = 256 - pa;
                                            dest = *lp;
                                            ALPHABLENDOPAQUEDEST(source, dest, lp, alpha, invalpha);
                                            lp++;
                                        }
                                    }
                                    break;
                                }
                                // next clip and target row
                                lp += targetrowpixels - w;
                                ldata += cliprowpixels - w;
                            }
                        }
                    } else {
                        // no alpha blending
                        for (int j = 0; j < h; j++) {
                            // copy each row with memcpy
                            memcpy(lp, ldata, w << 2);
                            lp += targetrowpixels;
                            ldata += cliprowpixels;
                        }
                    }
                }
            } else {
                // do an affine transformation
                unsigned int *data = (unsigned int*)clipptr->cdata;
                w = clipptr->cwd;
                h = clipptr->cht;
                x -= xoff;
                y -= yoff;
                const int x0 = x - (x * axx + y * axy);
                const int y0 = y - (x * ayx + y * ayy);

                // check for alpha blend
                if (alphablend) {
                    // save RGBA values
                    unsigned int savergba = rgbadraw;
                    unsigned char savea = a;

                    for (int j = 0; j < h; j++) {
                        for (int i = 0; i < w; i++) {
                            rgbadraw = *data++;
                            a = ALPHA2BYTE(rgbadraw);
                            int newx = x0 + x * axx + y * axy;
                            int newy = y0 + x * ayx + y * ayy;
                            if (PixelInTarget(newx, newy)) DrawPixel(newx, newy);
                            x++;
                        }
                        y++;
                        x -= w;
                    }

                    // restore saved RGBA values
                    rgbadraw = savergba;
                    a = savea;
                } else {
                    // no alpha blend
                    unsigned int *ldata = (unsigned int*)data;
                    unsigned int *lp = (unsigned int*)pixmap;
                    for (int j = 0; j < h; j++) {
                        for (int i = 0; i < w; i++) {
                            int newx = x0 + x * axx + y * axy;
                            int newy = y0 + x * ayx + y * ayy;
                            if (PixelInTarget(newx, newy)) *(lp + newy * wd + newx) = *ldata;
                            ldata++;
                            x++;
                        }
                        y++;
                        x -= w;
                    }
                }
            }
        }
    }
    while ((copy = (char*)GetCoordinatePair(copy, &x, &y)) != 0);

    // free the buffer
    free(buffer);

    return NULL;
}

// assumes alpha blend, identity transformation and opaque destination pixels
void Overlay::Draw3DCell(int x, int y, const Clip *clipptr)
{
    // check that a clip is supplied
    if (clipptr == NULL) return;

    // add bounding box to drawing location
    y += clipptr->ybb;
    x += clipptr->xbb;

    // get bounding box width and height
    int h = clipptr->hbb;
    int w = clipptr->wbb;

    // discard if location is completely outside target
    if (RectOutsideTarget(x, y, w, h)) return;

    // get the clip data
    unsigned int *ldata = (unsigned int*)clipptr->cdatabb;
    const int cliprowpixels = clipptr->cwd;
    int rowoffset = clipptr->ybb;

    // check for clipping
    int xmax = x + w - 1;
    int ymax = y + h - 1;
    if (x < 0) {
        // skip pixels off left edge
        ldata += -x;
        x = 0;
    }
    if (y < 0) {
        // skip pixels off top edge
        ldata += -y * cliprowpixels;
        rowoffset -= y;
        y = 0;
    }
    if (xmax >= wd) xmax = wd - 1;
    if (ymax >= ht) ymax = ht - 1;
    w = xmax - x + 1;
    h = ymax - y + 1;

    // get the paste target data
    const int targetrowpixels = wd;
    unsigned int *lp = ((unsigned int*)pixmap) + y * targetrowpixels + x;
    unsigned int source, dest, pa, alpha, invalpha;

    // check if the clip has a row index
    const rowtype *rowindex = clipptr->rowindex;
    if (!rowindex) {
        // clip only has mixed alpha rows
        for (int j = 0; j < h; j++) {
            // row contains pixels with different alpha values
            for (int i = 0; i < w; i++) {
                source = *ldata++;
                pa = ALPHA2BYTE(source);
                alpha = pa + 1;
                invalpha = 256 - pa;
                dest = *lp;
                ALPHABLENDOPAQUEDEST(source, dest, lp, alpha, invalpha);
                lp++;
            }
            // next clip and target row
            lp += targetrowpixels - w;
            ldata += cliprowpixels - w;
        }
    } else {
        for (int j = rowoffset; j < h + rowoffset; j++) {
            rowtype rowflag = rowindex[j];
            switch (rowflag) {
            case alpha0:
                // if all pixels are transparent then skip the row
                ldata += w;
                lp += w;
                break;
            case opaque:
                // if all pixels are opaque then use memcpy
                memcpy(lp, ldata, w << 2);
                ldata += w;
                lp += w;
                break;
            case both:
                // row contains mix of opaque and transparent pixels
                for (int i = 0; i < w; i++) {
                    source = *ldata++;
                    // copy pixel if not transparent
                    if (source & AMASK) {
                        *lp = source;
                    }
                    lp++;
                }
                break;
            case mixed:
                // row contains pixels with different alpha values
                for (int i = 0; i < w; i++) {
                    source = *ldata++;
                    pa = ALPHA2BYTE(source);
                    alpha = pa + 1;
                    invalpha = 256 - pa;
                    dest = *lp;
                    ALPHABLENDOPAQUEDEST(source, dest, lp, alpha, invalpha);
                    lp++;
                }
                break;
            }
            // next clip and target row
            lp += targetrowpixels - w;
            ldata += cliprowpixels - w;
        }
    }
}

// -----------------------------------------------------------------------------

const char *Overlay::DoScale(const char *args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    wxImageResizeQuality quality;
    if (strncmp(args, " best ", 6) == 0) {
        quality = wxIMAGE_QUALITY_HIGH;
        args += 6;
    } else if (strncmp(args, " fast ", 6) == 0) {
        quality = wxIMAGE_QUALITY_NORMAL;
        args += 6;
    } else {
        return OverlayError("scale quality must be best or fast");
    }

    int x, y, w, h;
    int namepos;
    char dummy;
    if (sscanf(args, "%d %d %d %d %n%c", &x, &y, &w, &h, &namepos, &dummy) != 5) {
        // note that %n is not included in the count
        return OverlayError("scale command requires 5 arguments");
    }

    // treat non-positive w/h as inset from target's width/height
    if (w <= 0) w = wd + w;
    if (h <= 0) h = ht + h;
    if (w <= 0) return OverlayError("scale width must be > 0");
    if (h <= 0) return OverlayError("scale height must be > 0");

    std::string name = args + namepos;
    std::map<std::string,Clip*>::iterator it;
    it = clips.find(name);
    if (it == clips.end()) {
        static std::string msg;
        msg = "unknown scale clip (";
        msg += name;
        msg += ")";
        return OverlayError(msg.c_str());
    }

    // do nothing if scaled rect is completely outside target
    if (RectOutsideTarget(x, y, w, h)) return NULL;

    Clip *clipptr = it->second;
    int clipw = clipptr->cwd;
    int cliph = clipptr->cht;

    if (w > clipw && w % clipw == 0 &&
        h > cliph && h % cliph == 0 && quality == wxIMAGE_QUALITY_NORMAL) {
        // no need to create a wxImage to expand pixels by integer multiples
        DisableTargetClipIndex();
        int xscale = w / clipw;
        int yscale = h / cliph;
        unsigned int *data = (unsigned int*)clipptr->cdata;

        // save current RGBA values
        unsigned int savergba = rgbadraw;
        unsigned char savea = a;

        if (RectInsideTarget(x, y, w, h)) {
            for (int j = 0; j < cliph; j++) {
                for (int i = 0; i < clipw; i++) {
                    rgbadraw = *data++;
                    a = ALPHA2BYTE(rgbadraw);
                    FillRect(x, y, xscale, yscale);
                    x += xscale;
                }
                y += yscale;
                x -= clipw * xscale;
            }
        } else {
            for (int j = 0; j < cliph; j++) {
                for (int i = 0; i < clipw; i++) {
                    rgbadraw = *data++;
                    a = ALPHA2BYTE(rgbadraw);
                    if (RectOutsideTarget(x, y, xscale, yscale)) {
                        // expanded pixel is outside target
                    } else {
                        for (int row = 0; row < yscale; row++) {
                            for (int col = 0; col < xscale; col++) {
                                if (PixelInTarget(x+col, y+row)) DrawPixel(x+col, y+row);
                            }
                        }
                    }
                    x += xscale;
                }
                y += yscale;
                x -= clipw * xscale;
            }
        }

        // restore saved RGBA values
        rgbadraw = savergba;
        a = savea;

        return NULL;
    }

    // get the clip's RGB and alpha data so we can create a wxImage
    unsigned char *rgbdata = (unsigned char*) malloc(clipw * cliph * 3);
    if (rgbdata== NULL) {
        return OverlayError("not enough memory to scale rgb data");
    }
    unsigned char *alphadata = (unsigned char*) malloc(clipw * cliph);
    if (alphadata == NULL) {
        free(rgbdata);
        return OverlayError("not enough memory to scale alpha data");
    }

    unsigned char *p = clipptr->cdata;
    int rgbpos = 0;
    int alphapos = 0;
    for (int j = 0; j < cliph; j++) {
        for (int i = 0; i < clipw; i++) {
            rgbdata[rgbpos++] = *p++;
            rgbdata[rgbpos++] = *p++;
            rgbdata[rgbpos++] = *p++;
            alphadata[alphapos++] = *p++;
        }
    }

    // create wxImage with the given clip's size and using its RGB and alpha data;
    // static_data flag is false so wxImage dtor will free rgbdata and alphadata
    wxImage image(clipw, cliph, rgbdata, alphadata, false);

    // scale the wxImage to the requested width and height
    image.Rescale(w, h, quality);

    // mark target clip as changed
    DisableTargetClipIndex();

    // save current RGBA values
    unsigned int savergba = rgbadraw;
    unsigned char savea = a;

    // copy the pixels from the scaled wxImage into the current target
    unsigned char *rdata = image.GetData();
    unsigned char *adata = image.GetAlpha();
    rgbpos = 0;
    alphapos = 0;
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            if (PixelInTarget(x, y)) {
                rgbadraw =  BYTE2RED(rdata[rgbpos++]);
                rgbadraw |= BYTE2GREEN(rdata[rgbpos++]);
                rgbadraw |= BYTE2BLUE(rdata[rgbpos++]);
                rgbadraw |= BYTE2ALPHA(adata[alphapos++]);
                a = ALPHA2BYTE(rgbadraw);
                DrawPixel(x, y);
            } else {
                rgbpos += 3;
                alphapos++;
            }
            x++;
        }
        y++;
        x -= w;
    }

    // restore saved RGBA values
    rgbadraw = savergba;
    a = savea;

    return NULL;
}

// -----------------------------------------------------------------------------

const char *Overlay::DoTarget(const char *args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    int namepos;
    char dummy;
    int numargs = sscanf(args, " %n%c", &namepos, &dummy);
    if (numargs != 1) {
        if (*args == 0 || *args == ' ') {
            numargs = 0;
        } else {
            return OverlayError("target command requires 0 or 1 arguments");
        }
    }

    // previous target name
    static std::string result;
    result = targetname;

    // no arguments means overlay is the target
    if (numargs == 0) {
        SetRenderTarget(ovpixmap, ovwd, ovht, NULL);
        targetname = "";
    } else {
        // one argument means clip is the target
        std::string name = args + namepos;
        std::map<std::string,Clip*>::iterator it;
        it = clips.find(name);
        if (it == clips.end()) {
            static std::string msg;
            msg = "unknown target name (";
            msg += name;
            msg += ")";
            return OverlayError(msg.c_str());
        } else {
            // set clip as the target
            Clip *clipptr = it->second;
            SetRenderTarget(clipptr->cdata, clipptr->cwd, clipptr->cht, clipptr);
            targetname = name;
        }
    }

    // return previous target
    return result.c_str();
}

// -----------------------------------------------------------------------------

const char *Overlay::DoDelete(const char *args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    // check for optional clip name
    int namepos;
    char dummy;
    int numargs = sscanf(args, " %n%c", &namepos, &dummy);
    if (numargs != 1) {
        if (*args == 0 || *args == ' ') {
            numargs = 0;
        } else {
            return OverlayError("delete command requires 0 or 1 arguments");
        }
    }

    // was optional clip name specified
    if (numargs == 0) {
        // no so delete overlay
        DeleteOverlay();
    } else {
        // yes so look up clip by name
        std::string name = args + namepos;
        std::map<std::string,Clip*>::iterator it;
        it = clips.find(name);
        if (it == clips.end()) {
            static std::string msg;
            msg = "unknown delete clip (";
            msg += name;
            msg += ")";
            return OverlayError(msg.c_str());
        }
        // check if the clip is the current render target
        if (name == targetname) {
            return OverlayError("delete clip is current render target");
        } else {
            // delete the clip
            delete it->second;
            clips.erase(it);
        }
    }

    return NULL;
}

// -----------------------------------------------------------------------------

const char *Overlay::DoLoad(const char *args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    int x, y;
    int filepos;
    char dummy;
    if (sscanf(args, " %d %d %n%c", &x, &y, &filepos, &dummy) != 3) {
        // note that %n is not included in the count
        return OverlayError("load command requires 3 arguments");
    }

    wxString filepath = wxString(args + filepos, wxConvLocal);
    if (!wxFileExists(filepath)) {
        return OverlayError("given file does not exist");
    }

    wxImage image;
    if (!image.LoadFile(filepath)) {
        return OverlayError("failed to load image from given file");
    }

    int imgwd = image.GetWidth();
    int imght = image.GetHeight();
    if (RectOutsideTarget(x, y, imgwd, imght)) {
        // do nothing if image rect is completely outside target,
        // but we still return the image dimensions so users can do things
        // like center the image within the target
    } else {
        // mark target clip as changed
        DisableTargetClipIndex();

        // use alpha data if it exists otherwise try looking for mask
        unsigned char *alphadata = NULL;
        if (image.HasAlpha()) {
            alphadata = image.GetAlpha();
        }
        unsigned char maskr = 0;
        unsigned char maskg = 0;
        unsigned char maskb = 0;
        bool hasmask = false;
        if (alphadata == NULL) {
            hasmask = image.GetOrFindMaskColour(&maskr, &maskg, &maskb);
        }

        // save current RGBA values
        unsigned int savergba = rgbadraw;
        unsigned char saver = r;
        unsigned char saveg = g;
        unsigned char saveb = b;
        unsigned char savea = a;

        unsigned char *rgbdata = image.GetData();
        int rgbpos = 0;
        int alphapos = 0;
        for (int j = 0; j < imght; j++) {
            for (int i = 0; i < imgwd; i++) {
                r = rgbdata[rgbpos++];
                g = rgbdata[rgbpos++];
                b = rgbdata[rgbpos++];
                if (alphadata) {
                    a = alphadata[alphapos++];
                } else if (hasmask && r == maskr && g == maskg && b == maskb) {
                    // transparent pixel
                    a = 0;
                } else {
                    a = 255;
                }
                rgbadraw = BYTE2RED(r) | BYTE2GREEN(g) | BYTE2BLUE(b) | BYTE2ALPHA(a);
                if (PixelInTarget(x, y)) DrawPixel(x, y);
                x++;
            }
            y++;
            x -= imgwd;
        }

        // restore saved RGBA values
        rgbadraw = savergba;
        r = saver;
        g = saveg;
        b = saveb;
        a = savea;
    }

    // return image dimensions
    static char result[32];
    sprintf(result, "%d %d", imgwd, imght);
    return result;
}

// -----------------------------------------------------------------------------

const char *Overlay::DoSave(const char *args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    int x, y, w, h;
    int filepos;
    char dummy;
    if (sscanf(args, " %d %d %d %d %n%c", &x, &y, &w, &h, &filepos, &dummy) != 5) {
        // note that %n is not included in the count
        return OverlayError("save command requires 5 arguments");
    }

    // treat non-positive w/h as inset from overlay's width/height
    // (makes it easy to save entire overlay via "save 0 0 0 0 foo.png")
    if (w <= 0) w = wd + w;
    if (h <= 0) h = ht + h;
    if (w <= 0) return OverlayError("save width must be > 0");
    if (h <= 0) return OverlayError("save height must be > 0");

    if (x < 0 || x+w > wd || y < 0 || y+h > ht) {
        return OverlayError("save rectangle must be within overlay");
    }

    wxString filepath = wxString(args + filepos, wxConvLocal);
    wxString ext = filepath.AfterLast('.');
    if (!ext.IsSameAs(wxT("png"),false)) {
        return OverlayError("save file must have a .png extension");
    }

    unsigned char *rgbdata = (unsigned char*) malloc(w * h * 3);
    if (rgbdata== NULL) {
        return OverlayError("not enough memory to save RGB data");
    }
    unsigned char *alphadata = (unsigned char*) malloc(w * h);
    if (alphadata == NULL) {
        free(rgbdata);
        return OverlayError("not enough memory to save alpha data");
    }

    int rgbpos = 0;
    int alphapos = 0;
    int rowbytes = wd * 4;
    for (int j=y; j<y+h; j++) {
        for (int i=x; i<x+w; i++) {
            // get pixel at i,j
            unsigned char *p = pixmap + j*rowbytes + i*4;
            rgbdata[rgbpos++] = p[0];
            rgbdata[rgbpos++] = p[1];
            rgbdata[rgbpos++] = p[2];
            alphadata[alphapos++] = p[3];
        }
    }

    // create image of requested size using the given RGB and alpha data;
    // static_data flag is false so wxImage dtor will free rgbdata and alphadata
    wxImage image(w, h, rgbdata, alphadata, false);

    if (!image.SaveFile(filepath)) {
        return OverlayError("failed to save image in given file");
    }

    return NULL;
}

// -----------------------------------------------------------------------------

void Overlay::SaveOverlay(const wxString &pngpath)
{
    if (ovpixmap == NULL) {
        Warning(_("There is no overlay data to save!"));
        return;
    }

    unsigned char *rgbdata = (unsigned char*) malloc(ovwd * ovht * 3);
    if (rgbdata== NULL) {
        Warning(_("Not enough memory to copy RGB data."));
        return;
    }
    unsigned char *alphadata = (unsigned char*) malloc(ovwd * ovht);
    if (alphadata == NULL) {
        free(rgbdata);
        Warning(_("Not enough memory to copy alpha data."));
        return;
    }

    unsigned char *p = ovpixmap;
    int rgbpos = 0;
    int alphapos = 0;
    for (int j=0; j<ht; j++) {
        for (int i=0; i<wd; i++) {
            rgbdata[rgbpos++] = p[0];
            rgbdata[rgbpos++] = p[1];
            rgbdata[rgbpos++] = p[2];
            alphadata[alphapos++] = p[3];
            p += 4;
        }
    }

    // create image using the given RGB and alpha data;
    // static_data flag is false so wxImage dtor will free rgbdata and alphadata
    wxImage image(wd, ht, rgbdata, alphadata, false);

    if (!image.SaveFile(pngpath)) {
        Warning(_("Failed to save overlay in given file."));
    }
}

// -----------------------------------------------------------------------------

const char *Overlay::DoFlood(const char *args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    int x, y;
    if (sscanf(args, " %d %d", &x, &y) != 2) {
        return OverlayError("flood command requires 2 arguments");
    }

    // // check if x,y is outside pixmap
    if (!PixelInTarget(x, y)) return NULL;

    unsigned int *lp = (unsigned int*)pixmap;
    unsigned int oldpxl = *(lp + y * wd + x);

    // do nothing if color of given pixel matches current RGBA values
    if (oldpxl == rgbadraw) return NULL;

    // mark target clip as changed
    DisableTargetClipIndex();

    // do flood fill using fast scanline algorithm
    // (based on code at http://lodev.org/cgtutor/floodfill.html)
    bool slowdraw = alphablend && a < 255;
    int maxyv = ht - 1;
    std::vector<int> xcoord;
    std::vector<int> ycoord;
    xcoord.push_back(x);
    ycoord.push_back(y);
    while (!xcoord.empty()) {
        // get pixel coords from end of vectors
        x = xcoord.back();
        y = ycoord.back();
        xcoord.pop_back();
        ycoord.pop_back();

        bool above = false;
        bool below = false;

        unsigned int *newpxl = lp + y * wd + x;
        while (x >= 0 && *newpxl == oldpxl) {
            x--;
            newpxl--;
        }
        x++;
        newpxl++;

        while (x < wd && *newpxl == oldpxl) {
            if (slowdraw) {
                // pixel is within pixmap
                DrawPixel(x,y);
            } else {
                *newpxl = rgbadraw;
            }

            if (y > 0) {
                unsigned int *apxl = newpxl - wd;    // pixel at x, y-1

                if (!above && *apxl == oldpxl) {
                    xcoord.push_back(x);
                    ycoord.push_back(y-1);
                    above = true;
                } else if (above && !(*apxl == oldpxl)) {
                    above = false;
                }
            }

            if (y < maxyv) {
                unsigned int *bpxl = newpxl + wd;    // pixel at x, y+1

                if (!below && *bpxl == oldpxl) {
                    xcoord.push_back(x);
                    ycoord.push_back(y+1);
                    below = true;
                } else if (below && !(*bpxl == oldpxl)) {
                    below = false;
                }
            }

            x++;
            newpxl++;
        }
    }

    return NULL;
}

// -----------------------------------------------------------------------------

const char *Overlay::DoBlend(const char *args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    int i;
    if (sscanf(args, " %d", &i) != 1) {
        return OverlayError("blend command requires 1 argument");
    }

    if (i < 0 || i > 2) {
        return OverlayError("blend value must be 0, 1 or 2");
    }

    int oldblend = alphablend;
    alphablend = i;

    // return old value
    static char result[2];
    sprintf(result, "%d", oldblend);
    return result;
}

// -----------------------------------------------------------------------------

const char *Overlay::DoFont(const char *args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    bool samename = false;      // only change font size?
    const char *newname = NULL;
    int newsize;
    int namepos;
    char dummy;
    int numargs = sscanf(args, " %d %n%c", &newsize, &namepos, &dummy);
    if (numargs == 1) {
        samename = true;
    } else if (numargs != 2) {
        // note that %n is not included in the count
        return OverlayError("font command requires 1 or 2 arguments");
    }

    if (newsize <= 0 || newsize >= 1000) {
        return OverlayError("font size must be > 0 and < 1000");
    }

    #ifdef __WXMAC__
        // need to increase Mac font size by 25% to match text size on Win/Linux
        int ptsize = int(newsize * 1.25 + 0.5);

        // set extraht to avoid GetTextExtent bug that clips descenders
        extraht = 1;
        if (strncmp(args+namepos, "default", 7) == 0 &&
            (newsize == 20 || newsize == 24 || newsize == 47)) {
            // strange but true
            extraht = 2;
        }
    #else
        int ptsize = newsize;
    #endif

    if (samename) {
        // just change the current font's size
        currfont.SetPointSize(ptsize);

    } else {
        newname = args + namepos;

        // check if given font name is valid
        if (strcmp(newname, "default") == 0) {
            currfont = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);

        } else if (strcmp(newname, "default-bold") == 0) {
            currfont = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
            currfont.SetWeight(wxFONTWEIGHT_BOLD);

        } else if (strcmp(newname, "default-italic") == 0) {
            currfont = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
            currfont.SetStyle(wxFONTSTYLE_ITALIC);

        } else if (strcmp(newname, "mono") == 0) {
            currfont = wxFont(ptsize, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);

        } else if (strcmp(newname, "mono-bold") == 0) {
            currfont = wxFont(ptsize, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);

        } else if (strcmp(newname, "mono-italic") == 0) {
            currfont = wxFont(ptsize, wxFONTFAMILY_MODERN, wxFONTSTYLE_ITALIC, wxFONTWEIGHT_NORMAL);

        } else if (strcmp(newname, "roman") == 0) {
            currfont = wxFont(ptsize, wxFONTFAMILY_ROMAN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);

        } else if (strcmp(newname, "roman-bold") == 0) {
            currfont = wxFont(ptsize, wxFONTFAMILY_ROMAN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);

        } else if (strcmp(newname, "roman-italic") == 0) {
            currfont = wxFont(ptsize, wxFONTFAMILY_ROMAN, wxFONTSTYLE_ITALIC, wxFONTWEIGHT_NORMAL);

        } else {
            return OverlayError("unknown font name");
        }

        // note that calling SetPointSize here avoids a bug in wxFont
        // that causes a 70pt font to end up as 8pt
        currfont.SetPointSize(ptsize);
    }

    int oldfontsize = fontsize;
    std::string oldfontname = fontname;

    fontsize = newsize;
    if (!samename) fontname = newname;

    // return old fontsize and fontname
    char ibuff[16];
    sprintf(ibuff, "%d", oldfontsize);
    static std::string result;
    result = ibuff;
    result += " ";
    result += oldfontname;
    return result.c_str();
}

// -----------------------------------------------------------------------------

const char *Overlay::TextOptionAlign(const char *args)
{
    text_alignment newalign;

    // check the specified alignment
    if (strcmp(args, "left") == 0) {
        newalign = left;
    } else if (strcmp(args, "right") == 0) {
        newalign = right;
    } else if (strcmp(args, "center") == 0) {
        newalign = center;
    } else {
        return OverlayError("unknown text alignment");
    }

    // get old value as string
    static char result[8];

    if (align == left) {
        sprintf(result, "left");
    } else if (align == right) {
        sprintf(result, "right");
    } else {
        sprintf(result, "center");
    }

    // save alignment settings
    align = newalign;

    return result;
}

// -----------------------------------------------------------------------------

const char *Overlay::TextOptionBackground(const char *args)
{
    int a1, a2, a3, a4;
    if (sscanf(args, " %d %d %d %d", &a1, &a2, &a3, &a4) != 4) {
        return OverlayError("textoption background command requires 4 arguments");
    }

    if (a1 < 0 || a1 > 255 ||
        a2 < 0 || a2 > 255 ||
        a3 < 0 || a3 > 255 ||
        a4 < 0 || a4 > 255) {
        return OverlayError("background rgba values must be from 0 to 255");
    }

    unsigned char oldr;
    unsigned char oldg;
    unsigned char oldb;
    unsigned char olda;
    GetRGBA(&oldr, &oldg, &oldb, &olda, textbgRGBA);

    SetRGBA(a1, a2, a3, a4, &textbgRGBA);

    // return old values
    static char result[16];
    sprintf(result, "%hhu %hhu %hhu %hhu", oldr, oldg, oldb, olda);
    return result;
}

// -----------------------------------------------------------------------------

const char *Overlay::DoTextOption(const char *args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    if (strncmp(args, "align ", 6) == 0)       return TextOptionAlign(args+6);
    if (strncmp(args, "background ", 11) == 0) return TextOptionBackground(args+11);

    return OverlayError("unknown textoption command");
}

// -----------------------------------------------------------------------------

const char *Overlay::DoText(const char *args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    // we don't use sscanf to parse the args because we want to allow the
    // text to start with a space
    int namepos = 0;
    int textpos = 0;
    const char *p = args;
    while (*p && *p == ' ') {
        namepos++;
        p++;
    }
    if (namepos > 0 && *p) {
        textpos = namepos;
        while (*p && *p != ' ') {
            textpos++;
            p++;
        }
        if (*p) p++;        // skip past space after clip name
        if (*p) {
            textpos++;
        } else {
            textpos = 0;    // no text supplied
        }
    }
    if (namepos == 0 || textpos == 0) {
        return OverlayError("text command requires 2 arguments");
    }

    std::string name = args + namepos;
    name = name.substr(0, name.find(" "));
    // check if the clip is the current render target
    if (name == targetname) {
        return OverlayError("text clip is current render target");
    }

    // create memory drawing context
    wxMemoryDC dc;
    dc.SetFont(currfont);

    // get the line height and descent (leading is ignored)
    wxString textstr = _("M");
    int textwd, descent, ignored;
    int lineht = 0;
    dc.GetTextExtent(textstr, &textwd, &lineht, &descent, &ignored);

    #ifdef __WXMAC__
        // increase lineht and descent on Mac to avoid clipping descenders
        lineht += extraht;
        descent += extraht;
    #endif

    // count lines
    char *textarg = (char*)args + textpos;
    char *index = textarg;
    int lines = 1;
    while (*index) {
        if (*index == '\n') lines++;
        index++;
    }

    // allocate buffers for line width and start position
    int *width = (int*) malloc(lines * sizeof(int));
    char **line = (char**) malloc(lines * sizeof(char*));

    // find first line
    char *textlines = textarg;
    index = strchr(textlines, '\n');

    // process each line of text to size the bitmap
    int bitmapwd = 0;
    int bitmapht = 0;
    int i = 0;
    do {
        // save pointer to the line start
        line[i] = textlines;
        if (index) {
            // null terminate line
            *index = 0;
        }

        // get the drawn string width
        textstr = wxString(textlines, wxConvUTF8);
        dc.GetTextExtent(textstr, &textwd, &ignored, &ignored, &ignored);

        // save the line width
        width[i] = textwd;

        // update bitmap width
        if (bitmapwd < textwd) bitmapwd = textwd;

        // update the bitmap height
        bitmapht += lineht;

        // next line
        if (index) {
            textlines = index + 1;
            index = strchr(textlines, '\n');
        }
        i++;
    } while (i < lines);

    // delete any existing clip data with the given name
    std::map<std::string,Clip*>::iterator it;
    it = clips.find(name);
    if (it != clips.end()) {
        delete it->second;
        clips.erase(it);
    }

    // create clip data with given name and big enough to enclose text
    Clip *textclip = new Clip(bitmapwd, bitmapht);
    if (textclip == NULL || textclip->cdata == NULL) {
        delete textclip;
        free(width);
        free(line);
        return OverlayError("not enough memory for text clip");
    }

    // get background color
    unsigned char bgr, bgg, bgb, bga;
    GetRGBA(&bgr, &bgg, &bgb, &bga, textbgRGBA);
    wxColour textbgcol(bgr, bgg, bgb, bga);
    wxColour transbgcol(255, 255, 255, 255);

    // get text foreground color
    wxColour textfgcol(r, g, b, a);
    wxColour transfgcol(255 - a, 255 - a, 255 - a, 255);

    // create the bitmap
    wxBitmap bitmap(bitmapwd, bitmapht, 32);

    // select the bitmap
    dc.SelectObject(bitmap);

    // create a rectangle to fill the bitmap
    wxRect rect(0, 0, bitmapwd, bitmapht);
    dc.SetPen(*wxTRANSPARENT_PEN);
    wxBrush brush(textbgcol);

    // if blending use transparent and replace later
    if (bga < 255) brush.SetColour(transbgcol);

    // fill the bitmap
    dc.SetBrush(brush);
    dc.DrawRectangle(rect);
    dc.SetBrush(wxNullBrush);
    dc.SetPen(wxNullPen);

    // set text background color to transparent
    dc.SetBackgroundMode(wxTRANSPARENT);

    // set text foreground color
    if (bga < 255) {
        dc.SetTextForeground(transfgcol);
    } else {
        dc.SetTextForeground(textfgcol);
    }

    // draw each text line
    int xpos = 0;
    int textrow = 0;
    for (i = 0; i < lines; i++) {
        // check if the line is empty
        if (*line[i]) {
            textstr = wxString(line[i], wxConvUTF8);

            // check text alignment
            xpos = 0;
            if (align != left) {
                if (align == right) {
                    xpos = bitmapwd - width[i];
                } else {
                    xpos = (bitmapwd - width[i]) / 2;
                }
            }

            // draw text
            dc.DrawText(textstr, xpos, textrow);
        }

        // next line
        textrow += lineht;
    }

    // deallocate buffers
    free(width);
    free(line);

    // deselect the bitmap
    dc.SelectObject(wxNullBitmap);

    // copy text from top left corner of offscreen image into clip data
    unsigned int *m = (unsigned int*)textclip->cdata;
    unsigned char bitmapr;
    const unsigned int rgbdraw = rgbadraw & RGBMASK;

    // get iterator over bitmap data
    wxAlphaPixelData data(bitmap);
    wxAlphaPixelData::Iterator iter(data);

    // check for transparent background
    if (bga < 255) {
        // transparent so look for background pixels to swap
        for (int y = 0; y < bitmapht; y++) {
            wxAlphaPixelData::Iterator rowstart = iter;
            for (int x = 0; x < bitmapwd; x++) {
                // get pixel RGB components
                bitmapr = iter.Red();

                if ((BYTE2RED(bitmapr) | BYTE2GREEN(iter.Green()) | BYTE2BLUE(iter.Blue())) == RGBMASK) {
                    // background found so replace with transparent pixel
                    *m++ = 0;
                } else {
                    // foreground found so replace with foreground color and set alpha based on grayness
                    *m++ = rgbdraw | BYTE2ALPHA(255 - bitmapr);
                }

                // pre-increment is faster
                ++iter;
            }
            iter = rowstart;
            iter.OffsetY(data, 1);
        }
    } else {
        // opaque background so just copy pixels
        for (int y = 0; y < bitmapht; y++) {
            wxAlphaPixelData::Iterator rowstart = iter;
            for (int x = 0; x < bitmapwd; x++) {
                *m++ = BYTE2RED(iter.Red()) | BYTE2GREEN(iter.Green()) | BYTE2BLUE(iter.Blue()) | AMASK;

                // pre-increment is faster
                ++iter;
            }
            iter = rowstart;
            iter.OffsetY(data, 1);
        }
    }

    // create named clip for later use by paste, scale, etc
    clips[name] = textclip;

    // return text info
    static char result[48];
    sprintf(result, "%d %d %d", bitmapwd, bitmapht, descent);
    return result;
}

// -----------------------------------------------------------------------------

#ifdef ENABLE_SOUND
const char *Overlay::SoundPlay(const char *args, bool loop)
{
    // check for engine
    if (engine) {
        if (*args == 0) {
            if (loop) {
                return OverlayError("sound loop requires an argument");
            } else {
                return OverlayError("sound play requires an argument");
            }
        }

        // check for the optional volume argument
        float v = 1;
        const char *name = args;

        // skip name
        char *scan = (char*)args;
        while (*scan && *scan != ' ') {
            scan++;
        }

        // check if there is a volume argument
        if (*scan) {
            if (sscanf(scan, " %f", &v) == 1) {
                if (v < 0.0 || v > 1.0) {
                    if (loop) {
                        return OverlayError("sound loop volume must be in the range 0 to 1");
                    } else {
                        return OverlayError("sound play volume must be in the range 0 to 1");
                    }
                }
            }

            // null terminate name
            *scan = 0;
        }

        // lookup the sound source
        ISoundSource *source = engine->getSoundSource(name, false);
        if (!source) {
            // create and preload the sound source
            source = engine->addSoundSourceFromFile(name, ESM_AUTO_DETECT, true);
            if (!source) {
                // don't throw error just return error message
                return "could not find sound";
            }
        }

        // check if the sound exists
        ISound *sound = NULL;
        std::map<std::string,ISound*>::iterator it;
        it = sounds.find(name);
        if (it != sounds.end()) {
            // sound exists so drop it
            sound = it->second;
            if (!sound->isFinished()) {
                sound->stop();
            }
            sound->drop();
            sounds.erase(it);
        }

        // prepare to play the sound (but pause it so we can adjust volume first)
        sound = engine->play2D(source, loop, true);
        if (!sound) {
            // don't throw error just return error message
            return "could not play sound";
        }

        // set the volume and then play
        sound->setVolume(v);
        sound->setIsPaused(false);

        // cache the sound
        sounds[name] = sound;
    }

    return NULL;
}
#endif

// -----------------------------------------------------------------------------

#ifdef ENABLE_SOUND
const char *Overlay::SoundStop(const char *args)
{
    // check for engine
    if (engine) {
        // check for argument
        if (*args == 0) {
            // stop all sounds
            engine->stopAllSounds();
        } else {
            // skip whitespace
            while (*args == ' ') {
                args++;
            }

            // stop named sound
            ISoundSource *source = engine->getSoundSource(args, false);
            if (source) {
                // find the sound
                std::map<std::string,ISound*>::iterator it;
                it = sounds.find(args);
                if (it != sounds.end()) {
                   // stop the sound
                   ISound *sound = it->second;
                   if (!sound->isFinished()) {
                       sound->stop();
                   }
                }
            }
        }
    }

    return NULL;
}
#endif

// -----------------------------------------------------------------------------

#ifdef ENABLE_SOUND
const char *Overlay::SoundState(const char *args)
{
    bool playing = false;
    bool paused = false;

    // check for engine
    if (engine) {
        // check for argument
        if (*args == 0) {
            // see if any sounds are playing
            for (int i = 0; i < engine->getSoundSourceCount(); i++) {
                if (engine->isCurrentlyPlaying(engine->getSoundSource(i))) {
                    playing = true;
                }
            }
        } else {
            // skip whitespace
            while (*args == ' ') {
                args++;
            }

            // see if named sound is playing
            ISoundSource *source = engine->getSoundSource(args, false);
            if (!source) {
                return "unknown";
            } else {
                // find the sound
                std::map<std::string,ISound*>::iterator it;
                it = sounds.find(args);
                if (it != sounds.end()) {
                    ISound *sound = it->second;
                    if (sound->getIsPaused()) {
                        paused = true;
                    }
                    if (engine->isCurrentlyPlaying(source)) {
                        playing = true;
                    }
                }
            }
        }
    }

    // return status as string
    if (paused && playing) {
        return "paused";
    } else {
        if (playing) {
            return "playing";
        } else {
            return "stopped";
        }
    }
}
#endif

// -----------------------------------------------------------------------------

#ifdef ENABLE_SOUND
const char *Overlay::SoundVolume(const char *args)
{
    // check for engine
    if (engine) {
        float v = 1;
        const char *name = args;

        // skip name
        char *scan = (char*)args;
        while (*scan && *scan != ' ') {
            scan++;
        }

        // check if there is a volume argument
        if (*scan) {
            if (sscanf(scan, " %f", &v) == 1) {
                if (v < 0.0 || v > 1.0) {
                    return OverlayError("sound volume must be in the range 0 to 1");
                }
            } else {
                return OverlayError("sound volume command requires two arguments");
            }

            // null terminate name
            *scan = 0;
        }

        // lookup the sound
        ISoundSource *source = engine->getSoundSource(name, false);
        if (source) {
            // set the default volume for the source
            source->setDefaultVolume(v);

            // check if the sound is playing
            std::map<std::string,ISound*>::iterator it;
            it = sounds.find(name);
            if (it != sounds.end()) {
               // set the sound volume
               ISound *sound = it->second;
               if (!sound->isFinished()) {
                   sound->setVolume(v);
               }
            }
        }
    }

    return NULL;
}
#endif

// -----------------------------------------------------------------------------

#ifdef ENABLE_SOUND
const char *Overlay::SoundPause(const char *args)
{
    // check for engine
    if (engine) {
        // check for argument
        if (*args == 0) {
            // pause all sounds
            engine->setAllSoundsPaused();
        } else {
            // skip whitespace
            while (*args == ' ') {
                args++;
            }

            // pause named sound
            ISoundSource *source = engine->getSoundSource(args, false);
            if (source) {
                // find the sound
                std::map<std::string,ISound*>::iterator it;
                it = sounds.find(args);
                if (it != sounds.end()) {
                   // pause the sound
                   ISound *sound = it->second;
                   if (!sound->isFinished()) {
                       sound->setIsPaused();
                   }
                }
            }
        }
    }

    return NULL;
}
#endif

// -----------------------------------------------------------------------------

#ifdef ENABLE_SOUND
const char *Overlay::SoundResume(const char *args)
{
    // check for engine
    if (engine) {
        // check for argument
        if (*args == 0) {
            // resume all paused sounds
            engine->setAllSoundsPaused(false);
        } else {
            // skip whitespace
            while (*args == ' ') {
                args++;
            }

            // resume named sound
            ISoundSource *source = engine->getSoundSource(args, false);
            if (source) {
                // find the sound
                std::map<std::string,ISound*>::iterator it;
                it = sounds.find(args);
                if (it != sounds.end()) {
                   // resume the sound
                   ISound *sound = it->second;
                   if (!sound->isFinished()) {
                       sound->setIsPaused(false);
                   }
                }
            }
        }
    }

    return NULL;
}
#endif

// -----------------------------------------------------------------------------

const char *Overlay::DoSound(const char *args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    #ifdef ENABLE_SOUND
    // check for sound engine query
    if (!*args) {
        if (engine) {
            // sound engine enabled
            return "2";
        } else {
            // sound engine failed to start
            return "1";
        }
    }

    // skip whitespace
    while (*args == ' ') {
        args++;
    }

    // check which sound command is specified
    if (strncmp(args, "play ", 5) == 0)    return SoundPlay(args+5, false);
    if (strncmp(args, "loop ", 5) == 0)    return SoundPlay(args+5, true);
    if (strncmp(args, "stop", 4) == 0)     return SoundStop(args+4);
    if (strncmp(args, "state", 5) == 0)    return SoundState(args+5);
    if (strncmp(args, "volume ", 7) == 0)  return SoundVolume(args+7);
    if (strncmp(args, "pause", 5) == 0)    return SoundPause(args+5);
    if (strncmp(args, "resume", 6) == 0)   return SoundResume(args+6);

    return OverlayError("unknown sound command");
    #else
    // if sound support not enabled then just return
    return "0";
    #endif
}

// -----------------------------------------------------------------------------

const char *Overlay::DoTransform(const char *args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    int a1, a2, a3, a4;
    if (sscanf(args, " %d %d %d %d", &a1, &a2, &a3, &a4) != 4) {
        return OverlayError("transform command requires 4 arguments");
    }

    if (a1 < -1 || a1 > 1 ||
        a2 < -1 || a2 > 1 ||
        a3 < -1 || a3 > 1 ||
        a4 < -1 || a4 > 1) {
        return OverlayError("transform values must be 0, 1 or -1");
    }

    int oldaxx = axx;
    int oldaxy = axy;
    int oldayx = ayx;
    int oldayy = ayy;

    axx = a1;
    axy = a2;
    ayx = a3;
    ayy = a4;
    identity = (axx == 1) && (axy == 0) && (ayx == 0) && (ayy == 1);

    // return old values
    static char result[16];
    sprintf(result, "%d %d %d %d", oldaxx, oldaxy, oldayx, oldayy);
    return result;
}

// -----------------------------------------------------------------------------

bool Overlay::OnlyDrawOverlay()
{
    // only use the overlay
    if (ovpixmap == NULL) return false;

    if (only_draw_overlay) {
        // this flag must only be used for one refresh so reset it immediately
        only_draw_overlay = false;
        return showoverlay && !(numlayers > 1 && tilelayers);
    } else {
        return false;
    }
}

// -----------------------------------------------------------------------------

const char *Overlay::DoUpdate()
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    #ifdef ENABLE_SOUND
    // update sound engine (in case threading not supported)
    if (engine) {
        engine->update();
    }
    #endif
    
    if (mainptr->IsIconized()) return NULL;

    only_draw_overlay = true;
    viewptr->Refresh(false);
    viewptr->Update();
    // DrawView in wxrender.cpp will call OnlyDrawOverlay (see above)

    #ifdef __WXGTK__
        // needed on Linux to see update immediately
        insideYield = true;
        wxGetApp().Yield(true);
        insideYield = false;
    #endif

    return NULL;
}

// -----------------------------------------------------------------------------

const char *Overlay::OverlayError(const char *msg)
{
    static std::string err;
    err = "ERR:";
    err += msg;
    return err.c_str();
}

// -----------------------------------------------------------------------------

const char *Overlay::ReadLuaBoolean(lua_State *L, const int n, int i, bool *value, const char *name) {
    static std::string err;
    if (i > n)  {
        err = "missing argument: ";
        err += name;
        return OverlayError(err.c_str());
    }
    lua_rawgeti(L, 1, i);
    int type = lua_type(L, -1);
    if (type != LUA_TBOOLEAN) {
        lua_pop(L, 1);
        err = "argument is not a boolean: ";
        err += name;
        return OverlayError(err.c_str());
    }
    *value = (bool)lua_toboolean(L, -1);
    lua_pop(L, 1);
    return NULL;
}

// -----------------------------------------------------------------------------

const char *Overlay::ReadLuaNumber(lua_State *L, const int n, int i, double *value, const char *name) {
    static std::string err;
    if (i > n)  {
        err = "missing argument: ";
        err += name;
        return OverlayError(err.c_str());
    }
    lua_rawgeti(L, 1, i);
    int type = lua_type(L, -1);
    if (type != LUA_TNUMBER) {
        lua_pop(L, 1);
        err = "argument is not a number: ";
        err += name;
        return OverlayError(err.c_str());
    }
    *value = (double)lua_tonumber(L, -1);
    lua_pop(L, 1);
    return NULL;
}

// -----------------------------------------------------------------------------

const char *Overlay::ReadLuaInteger(lua_State *L, const int n, int i, int *value, const char *name) {
    static std::string err;
    if (i > n)  {
        err = "missing argument: ";
        err += name;
        return OverlayError(err.c_str());
    }
    lua_rawgeti(L, 1, i);
    int type = lua_type(L, -1);
    if (type != LUA_TNUMBER) {
        lua_pop(L, 1);
        err = "argument is not a number: ";
        err += name;
        return OverlayError(err.c_str());
    }
    *value = (int)lua_tonumber(L, -1);
    lua_pop(L, 1);
    return NULL;
}

// -----------------------------------------------------------------------------

const char *Overlay::ReadLuaString(lua_State *L, const int n, int i, const char **value, const char *name) {
    static std::string err;
    if (i > n)  {
        err = "missing argument: ";
        err += name;
        return OverlayError(err.c_str());
    }
    lua_rawgeti(L, 1, i);
    int type = lua_type(L, -1);
    if (type != LUA_TSTRING) {
        lua_pop(L, 1);
        err = "argument is not a string: ";
        err += name;
        return OverlayError(err.c_str());
    }
    *value = lua_tostring(L, -1);
    lua_pop(L, 1);
    return NULL;
}

// -----------------------------------------------------------------------------
// Arguments:
// type         string      "cube", "sphere", "point"

const char *Overlay::Do3DSetCellType(lua_State *L, const int n, int *nresults) {
    const char *error = NULL;

    // get cell type
    const char *type = NULL;

    int idx = 2;
    if ((error = ReadLuaString(L, n, idx++, &type, "type")) != NULL) return error;
    if (strcmp(type, "cube") == 0) {
        celltype = cube;
    } else if (strcmp(type, "sphere") == 0) {
        celltype = sphere;
    } else if (strcmp(type, "point") == 0) {
        celltype = point;
    } else {
        return OverlayError("illegal cell type");
    }

    return error;
}

// -----------------------------------------------------------------------------
// Arguments:
// depthshading boolean
// depthlayers  integer
// mindepth     integer
// maxdepth     integer

const char *Overlay::Do3DSetDepthShading(lua_State *L, const int n, int *nresults) {
    const char *error = NULL;

    // get depth shading flag
    int idx = 2;
    if ((error = ReadLuaBoolean(L, n, idx++, &depthshading, "depthshading")) != NULL) return error;
    if ((error = ReadLuaInteger(L, n, idx++, &depthlayers, "depthlayers")) != NULL) return error;
    if ((error = ReadLuaInteger(L, n, idx++, &mindepth, "mindepth")) != NULL) return error;
    if ((error = ReadLuaInteger(L, n, idx++, &maxdepth, "maxdepth")) != NULL) return error;

    return error;
}

// -----------------------------------------------------------------------------
// Arguments:
// xixo         number
// xiyo         number
// xizo         number
// yixo         number
// yiyo         number
// yizo         number
// zixo         number
// ziyo         number
// zizo         number

const char *Overlay::Do3DSetTransform(lua_State *L, const int n, int *nresults) {
    const char *error = NULL;
    const double digits = 100000000.0;   // for rounding values from Lua

    // get transformation matrix
    int idx = 2;
    if ((error = ReadLuaNumber(L, n, idx++, &xixo, "xixo")) != NULL) return error;
    if ((error = ReadLuaNumber(L, n, idx++, &xiyo, "xiyo")) != NULL) return error;
    if ((error = ReadLuaNumber(L, n, idx++, &xizo, "xizo")) != NULL) return error;
    if ((error = ReadLuaNumber(L, n, idx++, &yixo, "yixo")) != NULL) return error;
    if ((error = ReadLuaNumber(L, n, idx++, &yiyo, "yiyo")) != NULL) return error;
    if ((error = ReadLuaNumber(L, n, idx++, &yizo, "yizo")) != NULL) return error;
    if ((error = ReadLuaNumber(L, n, idx++, &zixo, "zixo")) != NULL) return error;
    if ((error = ReadLuaNumber(L, n, idx++, &ziyo, "ziyo")) != NULL) return error;
    if ((error = ReadLuaNumber(L, n, idx++, &zizo, "zizo")) != NULL) return error;

    // round values to prevent rendering glitches
    xixo = round(digits * xixo) / digits;
    xiyo = round(digits * xiyo) / digits;
    xizo = round(digits * xizo) / digits;

    yixo = round(digits * yixo) / digits;
    yiyo = round(digits * yiyo) / digits;
    yizo = round(digits * yizo) / digits;

    zixo = round(digits * zixo) / digits;
    ziyo = round(digits * ziyo) / digits;
    zizo = round(digits * zizo) / digits;

    return error;
}

// -----------------------------------------------------------------------------

const Clip *Overlay::GetClip(const char *clipname) {
    Clip *result = NULL;

    // lookup the named clip
    std::string name = clipname;
    std::map<std::string,Clip*>::iterator it;
    it = clips.find(name);
    if (it != clips.end()) {
        result = it->second;
    }

    return result;
}

// -----------------------------------------------------------------------------

const char *Overlay::Update3DClips(const bool editing) {
    char clipname[20];
    const Clip *current;
    int numclips = maxdepth - mindepth + 1;

    // clear current clips
    clipmanager.Clear();

    // get history clips
    if (showhistory > 0) {
        if (fadehistory) {
            // history fading clips
            for (int i = 1; i <= showhistory; i++) {
                sprintf(clipname, "h%d", i);
                if ((current = GetClip(clipname)) == NULL) return OverlayError("missing history fade clip");
                clipmanager.AddHistoryClip(current);
            }
        } else {
            // history clip
            if ((current = GetClip("h")) == NULL) return OverlayError("missing history clip");
            clipmanager.SetHistoryClip(current);
        }
    }

    // check algo
    if (ruletype == bb || ruletype == bbw) {
        // busyboxes
        if (depthshading && celltype != point) {
            // get depth shading clips
            for (int i = 0; i < numclips; i++) {
                sprintf(clipname, "E%d", i + mindepth);
                if ((current = GetClip(clipname)) == NULL) return OverlayError("missing even depth clip");
                clipmanager.AddEvenClip(current);
                sprintf(clipname, "O%d", i + mindepth);
                if ((current = GetClip(clipname)) == NULL) return OverlayError("missing odd depth clip");
                clipmanager.AddOddClip(current);
            }
        } else {
            // get standard clips
            if ((current = GetClip("E")) == NULL) return OverlayError("missing even clip");
            clipmanager.SetEvenClip(current);
            if ((current = GetClip("O")) == NULL) return OverlayError("missing odd clip");
            clipmanager.SetOddClip(current);
        }
    } else {
        // standard algos
        if (depthshading && celltype != point) {
            // get depth shading clips
            for (int i = 0; i < numclips; i++) {
                sprintf(clipname, "L%d", i + mindepth);
                if ((current = GetClip(clipname)) == NULL) return OverlayError("missing live depth clip");
                clipmanager.AddLiveClip(current);
            }
        } else {
            // get standard clips
            if ((current = GetClip("L")) == NULL) return OverlayError("missing live clip");
            clipmanager.SetLiveClip(current);
        }
    }

    // check for select
    if (select3d.GetNumKeys() > 0) {
        if ((current = GetClip("s")) == NULL) return OverlayError("missing select clip");
        clipmanager.SetSelectClip(current);
    }

    // check for paste
    if (paste3d.GetNumKeys() > 0) {
        if ((current = GetClip("p")) == NULL) return OverlayError("missing paste clip");
        clipmanager.SetPasteClip(current);
    }

    // check for editing
    if (editing) {
        // check for active
        if (active3d.GetNumKeys() > 0) {
            if ((current = GetClip("a")) == NULL) return OverlayError("missing active clip");
            clipmanager.SetActiveClip(current);
        }
        // check for live not active clips
        if (ruletype == bb || ruletype == bbw) {
            if ((current = GetClip("EN")) == NULL) return OverlayError("missing even live not active clip");
            clipmanager.SetEvenLiveNotActiveClip(current);
            if ((current = GetClip("ON")) == NULL) return OverlayError("missing odd live not active clip");
            clipmanager.SetOddLiveNotActiveClip(current);
        } else {
            if ((current = GetClip("LN")) == NULL) return OverlayError("missing live not active clip");
            clipmanager.SetLiveNotActiveClip(current);
        }
        // select not active clip
        if ((current = GetClip("sN")) == NULL) return OverlayError("missing select not active clip");
        clipmanager.SetSelectNotActiveClip(current);
        // history not active clip
        if (showhistory > 0) {
            if ((current = GetClip("hN")) == NULL) return OverlayError("missing history not active clip");
            clipmanager.SetHistoryNotActiveClip(current);
        }
    }

    return NULL;
}

// -----------------------------------------------------------------------------
// Arguments:
// fromx        integer
// tox          integer
// stepx        integer
// fromy        integer
// toy          integer
// stepy        integer
// fromx        integer
// tox          integer
// stepx        integer
// cellsize     integer
// editing      boolean
// toolbarht    integer

const char *Overlay::Do3DDisplayCells(lua_State *L, const int n, int *nresults) {
    const char *error = NULL;

    const int N = gridsize;
    if (N == 0) return OverlayError("grid size not set");

    // check div table exists
    if (modN == NULL) if (!CreateDivTable()) return OverlayError("could not allocate div table");

    // whether editing flag
    bool editing = false;

    int idx = 2;
    if ((error = ReadLuaInteger(L, n, idx++, &fromx, "fromx")) != NULL) return error;
    if ((error = ReadLuaInteger(L, n, idx++, &tox, "tox")) != NULL) return error;
    if ((error = ReadLuaInteger(L, n, idx++, &stepx, "stepx")) != NULL) return error;
    if ((error = ReadLuaInteger(L, n, idx++, &fromy, "fromy")) != NULL) return error;
    if ((error = ReadLuaInteger(L, n, idx++, &toy, "toy")) != NULL) return error;
    if ((error = ReadLuaInteger(L, n, idx++, &stepy, "stepy")) != NULL) return error;
    if ((error = ReadLuaInteger(L, n, idx++, &fromz, "fromz")) != NULL) return error;
    if ((error = ReadLuaInteger(L, n, idx++, &toz, "toz")) != NULL) return error;
    if ((error = ReadLuaInteger(L, n, idx++, &stepz, "stepz")) != NULL) return error;
    if ((error = ReadLuaInteger(L, n, idx++, &cellsize, "cellsize")) != NULL) return error;
    if ((error = ReadLuaBoolean(L, n, idx++, &editing, "editing")) != NULL) return error;
    if ((error = ReadLuaInteger(L, n, idx++, &toolbarht, "toolbarht")) != NULL) return error;

    // ensure required clips are present
    if ((error = Update3DClips(editing)) != NULL) return error;

    // compute midcell
    midcell = (cellsize / 2) - ((gridsize + 1 - (gridsize % 2)) * cellsize / 2);

    // set flag if overlays (select, paste, active or history) need to be drawn
    const int numselectkeys = select3d.GetNumKeys();
    const int numpastekeys = paste3d.GetNumKeys();
    const int numactivekeys = active3d.GetNumKeys();
    const bool drawover = editing || numselectkeys > 0 || numpastekeys > 0 || numactivekeys > 0 || showhistory > 0;

    // compute midpoint of overlay
    const int midx = ovwd / 2;
    const int midy = ovht / 2 + toolbarht / 2;

    // compute loop increments
    const int stepi = gridsize * stepy;
    const int stepj = gridsize * stepz;

    // check for history display
    if (showhistory > 0) UpdateBoundingBoxFromHistory();

    // adjust for loop
    tox += stepx;
    toy += stepy;
    toz += stepz;

    // enable blending
    alphablend = true;

    // mark target clip as changed
    DisableTargetClipIndex();

    // check rule family
    if (ruletype == bb || ruletype == bbw) {
        if (drawover) {
            Display3DBusyBoxesEditing(midx, midy, stepi, stepj, editing);
        } else {
            Display3DBusyBoxes(midx, midy, stepi, stepj);
        }
    } else {
        if (drawover) {
            Display3DNormalEditing(midx, midy, stepi, stepj, editing);
        } else {
            Display3DNormal(midx, midy, stepi, stepj);
        }
    }

    // disable blending
    alphablend = false;

    return error;
}

// -----------------------------------------------------------------------------

void Overlay::Display3DNormal(const int midx, const int midy, const int stepi, const int stepj) {
    // iterate over the grid in the order specified
    const unsigned char *grid3values = grid3d.GetValues();

    // get midpoint
    int mx = 0;
    int my = 0;
    int x, y, z;
    int drawx, drawy;

    // check for depth shading
    int j = gridsize * fromz;
    if (depthshading && celltype != point) {
        const double zdepth = gridsize * cellsize * 0.5;
        const double zdepth2 = zdepth + zdepth;

        // get depth shading clips
        int numclips;
        const Clip **liveclips = clipmanager.GetLiveClips(&numclips);
        const Clip *liveclip = *liveclips;
        // get midpoint of clip
        int livew = liveclip->cwd >> 1;
        mx = midx - livew;
        my = midy - livew;

        // iterate over cells back to front
        for (z = fromz; z != toz; z += stepz) {
            if (zaxis[z]) {
                int i = gridsize * (fromy + j);
                for (y = fromy; y != toy; y += stepy) {
                    if (yaxis[y]) {
                        for (x = fromx; x != tox; x += stepx) {
                            if (grid3values[i + x]) {
                                // use orthographic projection
                                int xc = x * cellsize + midcell;
                                int yc = y * cellsize + midcell;
                                int zc = z * cellsize + midcell;
                                double zval = xc * zixo + yc * ziyo + zc * zizo;
                                int layer = depthlayers * (zval + zdepth) / zdepth2 - mindepth;
                                drawx = mx + xc * xixo + yc * xiyo + zc * xizo;
                                drawy = my + xc * yixo + yc * yiyo + zc * yizo;
                                Draw3DCell(drawx, drawy, liveclips[layer]);
                            }
                        }
                    }
                    i += stepi;
                }
            }
            j += stepj;
        }
    } else {
        // flat shading
        int livew = 0;
        const Clip *liveclip = clipmanager.GetLiveClip(&livew);
        // get midpoint of clip
        livew >>= 1;
        mx = midx - livew;
        my = midy - livew;

        // check if drawing points and point is opaque
        if (celltype == point && liveclip->cdata[3] == 255) {
            // set the drawing color to the pixel in the clip
            unsigned int rgba = *(unsigned int*)liveclip->cdata;
            // iterate over cells back to front
            unsigned int *lpixmap = (unsigned int*)pixmap;
            for (z = fromz; z != toz; z += stepz) {
                if (zaxis[z]) {
                    int i = gridsize * (fromy + j);
                    for (y = fromy; y != toy; y += stepy) {
                        if (yaxis[y]) {
                            for (x = fromx; x != tox; x += stepx) {
                                if (grid3values[i + x]) {
                                    // use orthographic projection
                                    int xc = x * cellsize + midcell;
                                    int yc = y * cellsize + midcell;
                                    int zc = z * cellsize + midcell;
                                    drawx = mx + xc * xixo + yc * xiyo + zc * xizo;
                                    drawy = my + xc * yixo + yc * yiyo + zc * yizo;
                                    if (PixelInTarget(drawx, drawy)) *(lpixmap + drawy*wd + drawx) = rgba;
                                }
                            }
                        }
                        i += stepi;
                    }
                }
                j += stepj;
            }
        } else {
            for (z = fromz; z != toz; z += stepz) {
                if (zaxis[z]) {
                    int i = gridsize * (fromy + j);
                    for (y = fromy; y != toy; y += stepy) {
                        if (yaxis[y]) {
                            for (x = fromx; x != tox; x += stepx) {
                                if (grid3values[i + x]) {
                                    // use orthographic projection
                                    int xc = x * cellsize + midcell;
                                    int yc = y * cellsize + midcell;
                                    int zc = z * cellsize + midcell;
                                    drawx = mx + xc * xixo + yc * xiyo + zc * xizo;
                                    drawy = my + xc * yixo + yc * yiyo + zc * yizo;
                                    Draw3DCell(drawx, drawy, liveclip);
                                }
                            }
                        }
                        i += stepi;
                    }
                }
                j += stepj;
            }
        }
    }
}

// -----------------------------------------------------------------------------

void Overlay::Display3DNormalEditing(const int midx, const int midy, const int stepi, const int stepj, const bool editing) {
    // iterate over the grid in the order specified
    const unsigned char *grid3values = grid3d.GetValues();
    int j = gridsize * fromz;
    int x, y, z;
    const Clip **liveclips = NULL;
    const Clip *liveclip = NULL;
    int livew = 0;
    const Clip **historyclips = NULL;
    const Clip *historyclip = NULL;
    int historyw = 0;
    double zd = 0;
    double zd2 = 0;
    const bool usedepth = (depthshading && celltype != point);

    // get required clips and compute midpoints
    // history cells
    if (showhistory > 0) {
        int numclips;
        if (fadehistory) {
            historyclips = clipmanager.GetHistoryClips(&numclips);
            historyclip = *historyclips;
            historyw = historyclip->cwd;
        } else {
            historyclip = clipmanager.GetHistoryClip(&historyw);
        }
    }
    // live cells
    if (usedepth) {
        int numclips;
        liveclips = clipmanager.GetLiveClips(&numclips);
        liveclip = *liveclips;
        livew = liveclip->cwd;
        zd = gridsize * cellsize * 0.5;
        zd2 = zd + zd;
    } else {
        liveclip = clipmanager.GetLiveClip(&livew);
    }
    // select paste, active, etc.
    int selectw = 0;
    const Clip *selectclip = clipmanager.GetSelectClip(&selectw);
    int pastew = 0;
    const Clip *pasteclip = clipmanager.GetPasteClip(&pastew);
    int activew = 0;
    const Clip *activeclip = clipmanager.GetActiveClip(&activew);
    int livenotw = 0;
    const Clip *livenotclip = clipmanager.GetLiveNotActiveClip(&livenotw);
    int selectnotw = 0;
    const Clip *selectnotclip = clipmanager.GetSelectNotActiveClip(&selectnotw);
    int historynotw = 0;
    const Clip *historynotclip = clipmanager.GetHistoryNotActiveClip(&historynotw);
    historyw >>= 1;
    livew >>= 1;
    selectw >>= 1;
    pastew >>= 1;
    activew >>= 1;
    livenotw >>= 1;
    selectnotw >>= 1;
    historynotw >>= 1;
    const double zdepth = zd;
    const double zdepth2 = zd2;

    // lookup the select, paste, active and history grids
    const unsigned char *select3values = select3d.GetValues();
    const unsigned char *paste3values = paste3d.GetValues();
    const unsigned char *active3values = active3d.GetValues();
    const unsigned char *history3values = history3d.GetValues();

    unsigned char gv = 0;
    unsigned char sv = 0;
    unsigned char pv = 0;
    unsigned char av = 0;
    unsigned char hv = 0;
    int ix;
    int drawx, drawy;

    // draw cells with select/paste/active/history
    for (z = fromz; z != toz; z += stepz) {
        int i = gridsize * (fromy + j);
        for (y = fromy; y != toy; y += stepy) {
            for (x = fromx; x != tox; x += stepx) {
                ix = i + x;
                gv = grid3values[ix];
                sv = select3values[ix];
                pv = paste3values[ix];
                av = active3values[ix];
                hv = history3values[ix];
                if (gv || sv || pv || av || hv) {
                    // use orthographic projection
                    int xc = x * cellsize + midcell;
                    int yc = y * cellsize + midcell;
                    int zc = z * cellsize + midcell;
                    if (usedepth) {
                        double zval = xc * zixo + yc * ziyo + zc * zizo;
                        int layer = depthlayers * (zval + zdepth) / zdepth2 - mindepth;
                        liveclip = liveclips[layer];
                    }
                    drawx = midx + xc * xixo + yc * xiyo + zc * xizo;
                    drawy = midy + xc * yixo + yc * yiyo + zc * yizo;
                    // check for editing
                    if (editing) {
                        // draw the cell
                        if (av) {
                            // cell is within active plane
                            if (gv) {
                                // draw live cell
                                Draw3DCell(drawx - livew, drawy - livew, liveclip);
                            }
                            // draw active plane cell
                            Draw3DCell(drawx - activew, drawy - activew, activeclip);
                            if (sv) {
                                // draw selected cell
                                Draw3DCell(drawx - selectw, drawy - selectw, selectclip);
                            }
                            // check for history
                            if (hv) {
                                // draw history cell
                                if (fadehistory) {
                                    Draw3DCell(drawx - historyw, drawy - historyw, historyclips[showhistory - hv]);
                                } else {
                                    Draw3DCell(drawx - historyw, drawy - historyw, historyclip);
                                }
                            }
                        } else {
                            // cell is outside of active plan
                            if (gv) {
                                // draw live cell as a point
                                Draw3DCell(drawx - livenotw, drawy - livenotw, livenotclip);
                            }
                            if (sv) {
                                // draw selected cell as a point
                                Draw3DCell(drawx - selectnotw, drawy - selectnotw, selectnotclip);
                            }
                            if (hv) {
                                // draw history cell as a point
                                Draw3DCell(drawx - historynotw, drawy - historynotw, historynotclip);
                            }
                        }
                    } else {
                        // active plane is not displayed
                        if (gv) {
                            // draw live cell
                            Draw3DCell(drawx - livew, drawy - livew, liveclip);
                        }
                        if (sv) {
                            // draw selected cell
                            Draw3DCell(drawx - selectw, drawy - selectw, selectclip);
                        }
                        // check for history
                        if (hv) {
                            // draw history cell
                            if (fadehistory) {
                                Draw3DCell(drawx - historyw, drawy - historyw, historyclips[showhistory - hv]);
                            } else {
                                Draw3DCell(drawx - historyw, drawy - historyw, historyclip);
                            }
                        }
                    }
                    // check for paste
                    if (pv) {
                        // draw live cell
                        Draw3DCell(drawx - livew, drawy - livew, liveclip);
                        // draw paste cell
                        Draw3DCell(drawx - pastew, drawy - pastew, pasteclip);
                    }
                }
            }
            i += stepi;
        }
        j += stepj;
    }
}

// -----------------------------------------------------------------------------

void Overlay::Display3DBusyBoxes(const int midx, const int midy, const int stepi, const int stepj) {
    // iterate over the grid in the order specified
    const unsigned char *grid3values = grid3d.GetValues();
    int x, y, z;

    // lookup busyboxes clips
    const Clip **eclips = NULL;
    const Clip **oclips = NULL;
    const Clip *eclip = NULL;
    const Clip *oclip = NULL;
    int evenw = 0;
    int oddw = 0;
    double zd = 0;
    double zd2 = 0;
    const bool usedepth = (depthshading && celltype != point);

    // check for depth shading
    if (usedepth) {
        // get the depth shading clip lists
        int numevenclips, numoddclips;
        eclips = clipmanager.GetEvenClips(&numevenclips);
        oclips = clipmanager.GetOddClips(&numoddclips);
        // get width of first clip in each list
        eclip = *eclips;
        evenw = eclip->cwd;
        oclip = *oclips;
        oddw = oclip->cwd;
        zd = gridsize * cellsize * 0.5;
        zd2 = zd + zd;
    } else {
        // get the odd and even clips
        eclip = clipmanager.GetEvenClip(&evenw);
        oclip = clipmanager.GetOddClip(&oddw);
    }
    // get midpoint of clips
    evenw >>= 1;
    oddw >>= 1;
    const double zdepth = zd;
    const double zdepth2 = zd2;
    int drawx, drawy;
    int j = gridsize * fromz;

    // check if drawing points and points are opaque
    if (celltype == point && eclip->cdata[3] == 255 && oclip->cdata[3] == 255) {
        // set the even drawing color to the pixel in the clip
        unsigned int evenrgba = *(unsigned int*)eclip->cdata;
        // set the odd drawing color to the pixel in the clip
        unsigned int oddrgba = *(unsigned int*)oclip->cdata;
        unsigned int *lpixmap = (unsigned int*)pixmap;
        // iterate over cells back to front
        for (z = fromz; z != toz; z += stepz) {
            if (zaxis[z]) {
                int i = gridsize * (fromy + j);
                for (y = fromy; y != toy; y += stepy) {
                    if (yaxis[y]) {
                        int evencell = ((fromx + y + z) & 1) == 0;
                        for (x = fromx; x != tox; x += stepx) {
                            if (grid3values[i + x]) {
                                // use orthographic projection
                                int xc = x * cellsize + midcell;
                                int yc = y * cellsize + midcell;
                                int zc = z * cellsize + midcell;
                                drawx = midx + xc * xixo + yc * xiyo + zc * xizo;
                                drawy = midy + xc * yixo + yc * yiyo + zc * yizo;
                                if (evencell) {
                                    if (PixelInTarget(drawx, drawy)) *(lpixmap + drawy*wd + drawx) = evenrgba;
                                } else {
                                    if (PixelInTarget(drawx, drawy)) *(lpixmap + drawy*wd + drawx) = oddrgba;
                                }
                            }
                            evencell = !evencell;
                        }
                    }
                    i += stepi;
                }
            }
            j += stepj;
        }
    } else {
        // iterate over cells back to front
        for (z = fromz; z != toz; z += stepz) {
            if (zaxis[z]) {
                int i = gridsize * (fromy + j);
                for (y = fromy; y != toy; y += stepy) {
                    if (yaxis[y]) {
                        int evencell = ((fromx + y + z) & 1) == 0;
                        for (x = fromx; x != tox; x += stepx) {
                            if (grid3values[i + x]) {
                                // use orthographic projection
                                int xc = x * cellsize + midcell;
                                int yc = y * cellsize + midcell;
                                int zc = z * cellsize + midcell;
                                drawx = midx + xc * xixo + yc * xiyo + zc * xizo;
                                drawy = midy + xc * yixo + yc * yiyo + zc * yizo;
                                if (usedepth) {
                                    double zval = xc * zixo + yc * ziyo + zc * zizo;
                                    int layer = depthlayers * (zval + zdepth) / zdepth2;
                                    if (evencell) {
                                        Draw3DCell(drawx - evenw, drawy - evenw, eclips[layer - mindepth]);
                                    } else {
                                        Draw3DCell(drawx - oddw, drawy - oddw, oclips[layer - mindepth]);
                                    }
                                } else {
                                    if (evencell) {
                                        Draw3DCell(drawx - evenw, drawy - evenw, eclip);
                                    } else {
                                        Draw3DCell(drawx - oddw, drawy - oddw, oclip);
                                    }
                                }
                            }
                            evencell = !evencell;
                        }
                    }
                    i += stepi;
                }
            }
            j += stepj;
        }
    }
}

// -----------------------------------------------------------------------------

void Overlay::Display3DBusyBoxesEditing(const int midx, const int midy, const int stepi, const int stepj, const bool editing) {
    // iterate over the grid in the order specified
    const unsigned char *grid3values = grid3d.GetValues();
    int x, y, z;

    // lookup busyboxes clips
    const Clip **evenclips = NULL;
    const Clip **oddclips = NULL;
    const Clip **historyclips = NULL;
    const Clip *evenclip = NULL;
    const Clip *oddclip = NULL;
    const Clip *historyclip = NULL;
    int evenw = 0;
    int oddw = 0;
    int historyw = 0;
    double zd = 0;
    double zd2 = 0;
    const bool usedepth = (depthshading && celltype != point);

    // get history clip
    if (showhistory > 0) {
        int numclips;
        if (fadehistory) {
            historyclips = clipmanager.GetHistoryClips(&numclips);
            historyclip = *historyclips;
            historyw = historyclip->cwd;
        } else {
            historyclip = clipmanager.GetHistoryClip(&historyw);
        }
    }
    // check for depth shading
    if (usedepth) {
        // get the depth shading clip lists
        int numevenclips, numoddclips;
        evenclips = clipmanager.GetEvenClips(&numevenclips);
        oddclips = clipmanager.GetOddClips(&numoddclips);
        // get width of first clip in each list
        evenclip = *evenclips;
        evenw = evenclip->cwd;
        oddclip = *oddclips;
        oddw = oddclip->cwd;
        zd = gridsize * cellsize * 0.5;
        zd2 = zd + zd;
    } else {
        // get the odd and even clips
        evenclip = clipmanager.GetEvenClip(&evenw);
        oddclip = clipmanager.GetOddClip(&oddw);
    }
    int selectw = 0;
    const Clip *selectclip = clipmanager.GetSelectClip(&selectw);
    int pastew = 0;
    const Clip *pasteclip = clipmanager.GetPasteClip(&pastew);
    int activew = 0;
    const Clip *activeclip = clipmanager.GetActiveClip(&activew);
    int evenlivenotw = 0;
    const Clip *evenlivenotclip = clipmanager.GetEvenLiveNotActiveClip(&evenlivenotw);
    int oddlivenotw = 0;
    const Clip *oddlivenotclip = clipmanager.GetOddLiveNotActiveClip(&oddlivenotw);
    int selectnotw = 0;
    const Clip *selectnotclip = clipmanager.GetSelectNotActiveClip(&selectnotw);
    int historynotw = 0;
    const Clip *historynotclip = clipmanager.GetHistoryNotActiveClip(&historynotw);
    evenw >>= 1;
    oddw >>= 1;
    selectw >>= 1;
    pastew >>= 1;
    activew >>= 1;
    evenlivenotw >>= 1;
    oddlivenotw >>= 1;
    selectnotw >>= 1;
    historyw >>= 1;
    historynotw >>= 1;
    const double zdepth = zd;
    const double zdepth2 = zd2;

    // lookup the select, paste, active and history grids
    const unsigned char *select3values = select3d.GetValues();
    const unsigned char *paste3values = paste3d.GetValues();
    const unsigned char *active3values = active3d.GetValues();
    const unsigned char *history3values = history3d.GetValues();

    unsigned char gv = 0;
    unsigned char sv = 0;
    unsigned char pv = 0;
    unsigned char av = 0;
    unsigned char hv = 0;
    int ix;
    int drawx, drawy;
    const Clip *liveclip = NULL;
    int livew = evenw;  // assume odd and even clips are the same size

    // iterate over cells back to front
    int j = gridsize * fromz;
    for (z = fromz; z != toz; z += stepz) {
        int i = gridsize * (fromy + j);
        for (y = fromy; y != toy; y += stepy) {
            int evencell = ((fromx + y + z) & 1) == 0;
            for (x = fromx; x != tox; x += stepx) {
                ix = i + x;
                gv = grid3values[ix];
                sv = select3values[ix];
                pv = paste3values[ix];
                av = active3values[ix];
                hv = history3values[ix];
                if (gv || sv || pv || av || hv) {
                    // use orthographic projection
                    int xc = x * cellsize + midcell;
                    int yc = y * cellsize + midcell;
                    int zc = z * cellsize + midcell;
                    if (usedepth) {
                        double zval = xc * zixo + yc * ziyo + zc * zizo;
                        int layer = depthlayers * (zval + zdepth) / zdepth2 - mindepth;
                        if (evencell) {
                            liveclip = evenclips[layer];
                        } else {
                            liveclip = oddclips[layer];
                        }
                    } else {
                        if (evencell) {
                            liveclip = evenclip;
                        } else {
                            liveclip = oddclip;
                        }
                    }
                    drawx = midx + xc * xixo + yc * xiyo + zc * xizo;
                    drawy = midy + xc * yixo + yc * yiyo + zc * yizo;
                    // check for editing
                    if (editing) {
                        // draw the cell
                        if (av) {
                            // cell is within active plane
                            if (gv) {
                                // draw live cell
                                Draw3DCell(drawx - livew, drawy - livew, liveclip);
                            }
                            // draw active plane cell
                            Draw3DCell(drawx - activew, drawy - activew, activeclip);
                            if (sv) {
                                // draw selected cell
                                Draw3DCell(drawx - selectw, drawy - selectw, selectclip);
                            }
                            // check for history
                            if (hv) {
                                // draw history cell
                                if (fadehistory) {
                                    Draw3DCell(drawx - historyw, drawy - historyw, historyclips[showhistory - hv]);
                                } else {
                                    Draw3DCell(drawx - historyw, drawy - historyw, historyclip);
                                }
                            }
                        } else {
                            // cell is outside of active plan
                            if (gv) {
                                // draw live cell as a point
                                if (evencell) {
                                    Draw3DCell(drawx - evenlivenotw, drawy - evenlivenotw, evenlivenotclip);
                                } else {
                                    Draw3DCell(drawx - oddlivenotw, drawy - oddlivenotw, oddlivenotclip);
                                }
                            }
                            if (sv) {
                                // draw selected cell as a point
                                Draw3DCell(drawx - selectnotw, drawy - selectnotw, selectnotclip);
                            }
                            if (hv) {
                                // draw history cell as a point
                                Draw3DCell(drawx - historynotw, drawy - historynotw, historynotclip);
                            }
                        }
                    } else {
                        // active plane is not displayed
                        if (gv) {
                            // draw live cell
                            Draw3DCell(drawx - livew, drawy - livew, liveclip);
                        }
                        if (sv) {
                            // draw selected cell
                            Draw3DCell(drawx - selectw, drawy - selectw, selectclip);
                        }
                        // check for history
                        if (hv) {
                            // draw history cell
                            if (fadehistory) {
                                Draw3DCell(drawx - historyw, drawy - historyw, historyclips[showhistory - hv]);
                            } else {
                                Draw3DCell(drawx - historyw, drawy - historyw, historyclip);
                            }
                        }
                    }
                    // check for paste
                    if (pv) {
                        // draw live cell
                        Draw3DCell(drawx - livew, drawy - livew, liveclip);
                        // draw paste cell
                        Draw3DCell(drawx - pastew, drawy - pastew, pasteclip);
                    }
                }
                evencell = !evencell;
            }
            i += stepi;
        }
        j += stepj;
    }
}

// -----------------------------------------------------------------------------
// Arguments:
// step         integer      >= 1

const char *Overlay::Do3DSetStepSize(lua_State *L, const int n, int *nresults) {
    const char *error = NULL;

    // get step size
    int idx = 2;
    int N;
    if ((error = ReadLuaInteger(L, n, idx++, &N, "step")) != NULL) return error;
    if (N < 1) return OverlayError("step must be at least 1");

    // set the step size
    stepsize = N;

    return error;
}

// -----------------------------------------------------------------------------
// Arguments:
// size         integer      >= 1

const char *Overlay::Do3DSetGridSize(lua_State *L, const int n, int *nresults) {
    const char *error = NULL;

    // get grid size
    int idx = 2;
    int N;
    if ((error = ReadLuaInteger(L, n, idx++, &N, "size")) != NULL) return error;
    if (N < 1 || N > 256) return OverlayError("size must be from 1 to 256");

    // set the grid size
    gridsize = N;
    const int NNN = N * N * N;

    // create the div table
    if (!CreateDivTable()) return OverlayError("could not allocate div table");

    // create the axis flags
    if (!CreateAxisFlags()) return OverlayError("could not allocate axis flags");

    // resize tables
    if (!grid3d.SetSize(NNN)) return OverlayError("could not allocate grid3d");
    if (!count1.SetSize(NNN)) return OverlayError("could not allocate count1");
    if (!count2.SetSize(NNN)) return OverlayError("could not allocate count2");
    if (!next3d.SetSize(NNN)) return OverlayError("could not allocate next3d");
    if (!paste3d.SetSize(NNN)) return OverlayError("could not allocate paste3d");
    if (!select3d.SetSize(NNN)) return OverlayError("could not allocate select3d");
    if (!active3d.SetSize(NNN)) return OverlayError("could not allocate active3d");
    if (!history3d.SetSize(NNN)) return OverlayError("could not allocate history3d");

    return error;
}

// -----------------------------------------------------------------------------
// Arguments:
// type         string      "", "F", "C", "E", "H", "BB" or "BBW"
// survivals    table       boolean
// births       table       boolean

const char *Overlay::Do3DSetRule(lua_State *L, const int n, int *nresults) {
    const char *error = NULL;

    // read rule type
    int idx = 2;
    const char *rulestring;
    if ((error = ReadLuaString(L, n, idx++, &rulestring, "type")) != NULL) return error;
    if (strcmp(rulestring, "") == 0) {
        ruletype = moore;
    } else if (strcmp(rulestring, "F") == 0) {
        ruletype = face;
    } else if (strcmp(rulestring, "C") == 0) {
        ruletype = corner;
    } else if (strcmp(rulestring, "E") == 0) {
        ruletype = edge;
    } else if (strcmp(rulestring, "H") == 0) {
        ruletype = hexahedral;
    } else if (strcmp(rulestring, "BB") == 0) {
        ruletype = bb;
    } else if (strcmp(rulestring, "BBW") == 0) {
        ruletype = bbw;
    } else {
        return OverlayError("type argument is invalid");
    }

    // don't need survivals and births for BusyBoxes rules
    if (ruletype != bb && ruletype != bbw) {
        // initialize survivals and births
        for (int i = 0; i < 27; i++) {
            survivals[i] = false;
            births[i]    = false;
        }
        bool valid = true;

        // read survivals list
        if (idx > n) return OverlayError("missing survivals argument");
        lua_rawgeti(L, 1, idx);
        int type = lua_type(L, -1);
        if (type != LUA_TTABLE) {
            lua_pop(L, 1);
            return OverlayError("survivals argument is not a table");
        }
        lua_pushvalue(L, -1);
        lua_pushnil(L);
        while (lua_next(L, -2)) {
            lua_pushvalue(L, -2);
            int k = lua_tointeger(L, -1);
            if (k < 0 || k >= 27) {
                valid = false;
                break;
            }
            lua_pop(L, 2);
            survivals[k] = true;
        }
        lua_pop(L, 1);
        if (!valid) return OverlayError("survivals element is out of range");

        // read births list
        idx++;
        if (idx > n) return OverlayError("missing births argument");
        lua_rawgeti(L, 1, idx);
        type = lua_type(L, -1);
        if (type != LUA_TTABLE) {
            lua_pop(L, 1);
            return OverlayError("births argument is not a table");
        }
        lua_pushvalue(L, -1);
        lua_pushnil(L);
        while (lua_next(L, -2)) {
            lua_pushvalue(L, -2);
            int k = lua_tointeger(L, -1);
            if (k < 0 || k >= 27) {
                valid = false;
                break;
            }
            lua_pop(L, 2);
            births[k] = true;
        }
        lua_pop(L, 1);
        if (!valid) return OverlayError("births element is out of range");
    }

    return error;
}

// -----------------------------------------------------------------------------

void Overlay::FreeDivTable() {
    if (modN) {
        free(modN);
        modN = NULL;
    }
    if (modNN) {
        free(modNN);
        modNN = NULL;
    }
    if (xyz) {
        free(xyz);
        xyz = NULL;
    }
}

// -----------------------------------------------------------------------------

bool Overlay::CreateDivTable() {
    if (gridsize == 0) return false;

    const int N = gridsize;
    const int NN = N * N;
    const int NNN = NN * N;

    // free existing table
    FreeDivTable();

    // allocate new table
    modN     = (int*)malloc(NNN * sizeof(*modN));
    modNN    = (int*)malloc(NNN * sizeof(*modNN));
    xyz      = (unsigned int*)malloc(NNN * sizeof(*xyz));

    // check allocation succeeded
    if (modN == NULL || modNN == NULL || xyz == NULL) {
        FreeDivTable();
        return false;
    }

    // populate table
    for (int i = 0; i < NNN; i++) {
        modN[i]  = i % N;
        modNN[i] = i % NN;
    }
    for (int i = 0; i < NNN; i++) {
        xyz[i] = (modN[i] << 16) | (modN[i / N] << 8) | i / NN;
    }

    return true;
}

// -----------------------------------------------------------------------------

void Overlay::FreeAxisFlags() {
    if (xaxis) {
        free(xaxis);
        xaxis = NULL;
    }
    if (yaxis) {
        free(yaxis);
        yaxis = NULL;
    }
    if (zaxis) {
        free(zaxis);
        zaxis = NULL;
    }
}

// -----------------------------------------------------------------------------

bool Overlay::CreateAxisFlags() {
    if (gridsize == 0) return false;

    const int N = gridsize;

    // free existing flags
    FreeAxisFlags();

    // allocate flags
    xaxis = (char*)malloc(N * sizeof(*xaxis));
    yaxis = (char*)malloc(N * sizeof(*yaxis));
    zaxis = (char*)malloc(N * sizeof(*zaxis));

    // check allocation succeeded
    if (xaxis == NULL || yaxis == NULL || zaxis == NULL) {
        FreeAxisFlags();
        return false;
    }

    return true;
}

// -----------------------------------------------------------------------------

void Overlay::ClearAxisFlags() {
    if (gridsize == 0) return;

    const int N = gridsize;

    // clear the axis flags
    memset(xaxis, 0, N * sizeof(*xaxis));
    memset(yaxis, 0, N * sizeof(*yaxis));
    memset(zaxis, 0, N * sizeof(*zaxis));
}

// -----------------------------------------------------------------------------

void Overlay::UpdateBoundingBox() {
    if (gridsize == 0) return;

    const int Nm1 = gridsize - 1;

    // guaranteed at least one live cell
    minx = 0;
    while (!xaxis[minx]) minx++;
    miny = 0;
    while (!yaxis[miny]) miny++;
    minz = 0;
    while (!zaxis[minz]) minz++;
    maxx = Nm1;
    while (!xaxis[maxx]) maxx--;
    maxy = Nm1;
    while (!yaxis[maxy]) maxy--;
    maxz = Nm1;
    while (!zaxis[maxz]) maxz--;

    if (minx == 0 || miny == 0 || minz == 0 || maxx == Nm1 || maxy == Nm1 || maxz == Nm1) {
        liveedge = true;
    }
}

// -----------------------------------------------------------------------------

int Overlay::CreateResultsFromC1(lua_State *L, const bool laststep) {
    // create results for BusyBoxes
    if (laststep) {
        lua_newtable(L);
    }
    next3d.Clear();

    // clear axes
    ClearAxisFlags();

    int numkeys;
    const int *count1keys = count1.GetKeys(&numkeys);
    const unsigned char *count1values = count1.GetValues();

    if (laststep) {
        // on the last step update the return grid
        for (int i = 0; i < numkeys; i++) {
            const int k = count1keys[i];
            if (count1values[k]) {
                // create a live cell in next grid
                lua_pushnumber(L, 1);
                lua_rawseti(L, -2, k);
                next3d.SetTo1(k);
                const unsigned int loc = xyz[k];
                xaxis[loc >> 16] = 1;
                yaxis[(loc >> 8) & 0xff] = 1;
                zaxis[loc & 0xff] = 1;
            }
        }
    } else {
        for (int i = 0; i < numkeys; i++) {
            const int k = count1keys[i];
            if (count1values[k]) {
                // create a live cell in next grid
                next3d.SetTo1(k);
                const unsigned int loc = xyz[k];
                xaxis[loc >> 16] = 1;
                yaxis[(loc >> 8) & 0xff] = 1;
                zaxis[loc & 0xff] = 1;
            }
        }
    }
    grid3d.Copy(next3d);
    UpdateBoundingBox();

    // return the population
    return next3d.GetNumKeys();
}

// -----------------------------------------------------------------------------

int Overlay::CreateResultsFromC1G3(lua_State *L, const bool laststep) {
    // create results for Moore
    if (laststep) {
        lua_newtable(L);
    }
    next3d.Clear();
    int numkeys;
    const int *count1keys = count1.GetKeys(&numkeys);
    const unsigned char *count1values = count1.GetValues();
    const unsigned char *grid3dvalues = grid3d.GetValues();

    // clear axes
    ClearAxisFlags();

    if (laststep) {
        // on the last step update the return grid
        for (int i = 0; i < numkeys; i++) {
            const int k = count1keys[i];
            const unsigned char v = count1values[k];
            const unsigned char src = grid3dvalues[k];
            if ((src && survivals[v - 1]) || (births[v] && !src)) {
                // create a live cell in next grid
                lua_pushnumber(L, 1);
                lua_rawseti(L, -2, k);
                next3d.SetTo1(k);
                const unsigned int loc = xyz[k];
                xaxis[loc >> 16] = 1;
                yaxis[(loc >> 8) & 0xff] = 1;
                zaxis[loc & 0xff] = 1;
            }
        }
    } else {
        for (int i = 0; i < numkeys; i++) {
            const int k = count1keys[i];
            const unsigned char v = count1values[k];
            const unsigned char src = grid3dvalues[k];
            if ((src && survivals[v - 1]) || (births[v] && !src)) {
                // create a live cell in next grid
                next3d.SetTo1(k);
                const unsigned int loc = xyz[k];
                xaxis[loc >> 16] = 1;
                yaxis[(loc >> 8) & 0xff] = 1;
                zaxis[loc & 0xff] = 1;
            }
        }
    }
    grid3d.Copy(next3d);
    UpdateBoundingBox();

    // return the population
    return next3d.GetNumKeys();
}

// -----------------------------------------------------------------------------

int Overlay::CreateResultsFromC1C2(lua_State *L, const bool laststep) {
    // create results for Face, Corner, Edge or Hexahedral
    if (laststep) {
        lua_newtable(L);
    }
    next3d.Clear();

    // clear axes
    ClearAxisFlags();

    // use count1 and survivals to put live cells in grid
    int numkeys;
    const int *count1keys = count1.GetKeys(&numkeys);
    const unsigned char *count1values = count1.GetValues();

    if (laststep) {
        // on the last step update the return grid
        for (int i = 0; i < numkeys; i++) {
            const int k = count1keys[i];
            const unsigned char v = count1values[k];
            if (survivals[v]) {
                // create a live cell in next grid
                lua_pushnumber(L, 1);
                lua_rawseti(L, -2, k);
                next3d.SetValue(k, 1);
                const unsigned int loc = xyz[k];
                xaxis[loc >> 16] = 1;
                yaxis[(loc >> 8) & 0xff] = 1;
                zaxis[loc & 0xff] = 1;
            }
        }
    } else {
        for (int i = 0; i < numkeys; i++) {
            const int k = count1keys[i];
            const unsigned char v = count1values[k];
            if (survivals[v]) {
                // create a live cell in next grid
                next3d.SetValue(k, 1);
                const unsigned int loc = xyz[k];
                xaxis[loc >> 16] = 1;
                yaxis[(loc >> 8) & 0xff] = 1;
                zaxis[loc & 0xff] = 1;
            }
        }
    }

    // use count2 and births to put live cells in grid
    const int *count2keys = count2.GetKeys(&numkeys);
    const unsigned char *count2values = count2.GetValues();

    if (laststep) {
        for (int i = 0; i < numkeys; i++) {
            const int k = count2keys[i];
            const unsigned char v = count2values[k];
            if (births[v]) {
                // create a live cell in next grid
                lua_pushnumber(L, 1);
                lua_rawseti(L, -2, k);
                next3d.SetValue(k, 1);
                const unsigned int loc = xyz[k];
                xaxis[loc >> 16] = 1;
                yaxis[(loc >> 8) & 0xff] = 1;
                zaxis[loc & 0xff] = 1;
            }
        }
    } else {
        for (int i = 0; i < numkeys; i++) {
            const int k = count2keys[i];
            const unsigned char v = count2values[k];
            if (births[v]) {
                // create a live cell in next grid
                next3d.SetValue(k, 1);
                const unsigned int loc = xyz[k];
                xaxis[loc >> 16] = 1;
                yaxis[(loc >> 8) & 0xff] = 1;
                zaxis[loc & 0xff] = 1;
            }
        }
    }
    grid3d.Copy(next3d);
    UpdateBoundingBox();

    // return the population
    return next3d.GetNumKeys();
}

// -----------------------------------------------------------------------------

void Overlay::PopulateAxis() {
    if (gridsize == 0) return;

    int numkeys;
    const int *grid3dkeys = grid3d.GetKeys(&numkeys);
    for (int i = 0; i < numkeys; i++) {
        int k = grid3dkeys[i];
        const unsigned int loc = xyz[k];
        xaxis[loc >> 16] = 1;
        yaxis[(loc >> 8) & 0xff] = 1;
        zaxis[loc & 0xff] = 1;
    }
    UpdateBoundingBox();
}

// -----------------------------------------------------------------------------

const char *Overlay::PopulateGrid(lua_State *L, const int n, int idx, Table &destgrid) {
    const int N = gridsize;
    const unsigned int NNN = N * N * N;

    // fill grid
    destgrid.Clear();
    if (idx > n) return OverlayError("missing grid argument");
    lua_rawgeti(L, 1, idx);
    int type = lua_type(L, -1);
    if (type != LUA_TTABLE) {
        lua_pop(L, 1);
        return OverlayError("grid argument is not a table");
    }
    lua_pushvalue(L, -1);
    lua_pushnil(L);
    bool valid = true;
    while (lua_next(L, -2)) {
        lua_pushvalue(L, -2);
        int k = lua_tointeger(L, -1);
        lua_pop(L, 2);
        // check that the cell coordinates are within the grid
        if ((unsigned int)k >= NNN) {
            valid = false;
            break;
        }
        destgrid.SetTo1(k);
    }
    lua_pop(L, 1);
    if (!valid) return OverlayError("pattern is larger than the grid");

    return NULL;
}

// -----------------------------------------------------------------------------
// Arguments:
// grid             table      integer
// clearhistory     boolean

const char *Overlay::Do3DSetPattern(lua_State *L, const int n, int *nresults) {
    const char *error = NULL;
    if (gridsize == 0) return OverlayError("grid size not set");

    // populate the grid from the supplied pattern
    int idx = 2;
    if ((error = PopulateGrid(L, n, idx++, grid3d)) != NULL) return error;
    PopulateAxis();

    // read the clear history flag
    bool clearhistory = false;
    if ((error = ReadLuaBoolean(L, n, idx++, &clearhistory, "clearhistory")) != NULL) return error;

    // clear history if requested
    if (clearhistory) {
        history3d.Clear();
    }

    return NULL;
}

// -----------------------------------------------------------------------------
// Arguments:
// selected         table       integer
// pastepatt        table       integer
// active           table       integer

const char *Overlay::Do3DSetSelectPasteActive(lua_State *L, const int n, int *nresults) {
    const char *error = NULL;
    if (gridsize == 0) return OverlayError("grid size not set");

    int idx = 2;
    if ((error = PopulateGrid(L, n, idx++, select3d)) != NULL) return error;
    if ((error = PopulateGrid(L, n, idx++, paste3d)) != NULL) return error;
    if ((error = PopulateGrid(L, n, idx++, active3d)) != NULL) return error;

    return error;
}

// -----------------------------------------------------------------------------

void Overlay::UpdateHistoryFromLive() {
    int numkeys = 0;
    int numhistkeys = 0;
    const int *grid3dkeys = grid3d.GetKeys(&numkeys);
    const int *history3keys = history3d.GetKeys(&numhistkeys);

    if (fadehistory) {
        // reduce history on all cells by 1
        for (int i = 0; i < numhistkeys; i++) {
            int k = history3keys[i];
            history3d.DecrementTo1(k);
        }
    }

    // set history to maximum longevity for any live cells
    for (int i = 0; i < numkeys; i++) {
        int k = grid3dkeys[i];
        history3d.SetValue(k, showhistory);
    }
}

// -----------------------------------------------------------------------------

void Overlay::UpdateBoundingBoxFromHistory() {
    const int N = gridsize;
    int numhistkeys = 0;
    const int *history3keys = history3d.GetKeys(&numhistkeys);

    // do nothing if no history cells
    if (numhistkeys == 0) return;

    // set history min, max defaults
    int hminx = N;
    int hmaxx = -1;
    int hminy = N;
    int hmaxy = -1;
    int hminz = N;
    int hmaxz = -1;
    // compute bounding box for history cells
    for (int i = 0; i < numhistkeys; i++) {
        const int k = history3keys[i];
        const unsigned int loc = xyz[k];
        const int x = loc >> 16;
        const int y = (loc >> 8) & 0xff;
        const int z = loc & 0xff;
        if (x < hminx) hminx = x;
        if (x > hmaxx) hmaxx = x;
        if (y < hminy) hminy = y;
        if (y > hmaxy) hmaxy = y;
        if (z < hminz) hminz = z;
        if (z > hmaxz) hmaxz = z;
    }

    // adjust drawing range to include history bounding box
    if (stepx < 0) {
        if (hminx < tox) tox = hminx;
        if (hmaxx > fromx) fromx = hmaxx;
    } else {
        if (hminx < fromx) fromx = hminx;
        if (hmaxx > tox) tox = hmaxx;
    }
    if (stepy < 0) {
        if (hminy < toy) toy = hminy;
        if (hmaxy > fromy) fromy = hmaxy;
    } else {
        if (hminy < fromy) fromy = hminy;
        if (hmaxy > toy) toy = hmaxy;
    }
    if (stepz < 0) {
        if (hminz < toz) toz = hminz;
        if (hmaxz > fromz) fromz = hmaxz;
    } else {
        if (hminz < fromz) fromz = hminz;
        if (hmaxz > toz) toz = hmaxz;
    }
}

// -----------------------------------------------------------------------------
// Arguments:
// showhistory      integer     0..255
// fadehistory      boolean

const char *Overlay::Do3DSetCellHistory(lua_State *L, const int n, int *nresults) {
    const char *error = NULL;

    // read the show history longevity
    int idx = 2;
    int oldshow = showhistory;
    int value = 0;
    if ((error = ReadLuaInteger(L, n, idx++, &value, "showhistory")) != NULL) return error;
    if (value < 0 || value > 255) return OverlayError("showhistory must be from 0 to 255");
    showhistory = value;

    // read the fade history flag
    if ((error = ReadLuaBoolean(L, n, idx++, &fadehistory, "fadehistory")) != NULL) return error;

    // clear history if mode changed (but not if only fading changed)
    if (oldshow != showhistory) {
        history3d.Clear();
    }

    return NULL;
}

// -----------------------------------------------------------------------------
// Arguments:
// gencount         integer
// liveedge         boolean     (if algo is not BusyBoxes)

const char *Overlay::Do3DNextGen(lua_State *L, const int n, int *nresults) {
    const char *error = NULL;

    // get the grid size
    if (gridsize == 0) return OverlayError("grid size not set");

    // read gencount
    int idx = 2;
    int gencount = 0;
    if ((error = ReadLuaInteger(L, n, idx++, &gencount, "gencount")) != NULL) return error;

    // for non-BusyBoxes algos get liveedge flag
    liveedge = false;
    if (!(ruletype == bb || ruletype == bbw)) {
        if ((error = ReadLuaBoolean(L, n, idx++, &liveedge, "liveedge")) != NULL) return error;
    }

    // check div table exists
    if (modN == NULL) if (!CreateDivTable()) return OverlayError("could not allocate div table");

    // process each step
    int lastgen = gencount - (gencount % stepsize) + stepsize;
    int newpop = 0;
    bool laststep = false;

    while (gencount < lastgen) {
        // set the laststep flag
        if (gencount == lastgen - 1) laststep = true;

        // clear the intermediate counts
        count1.ClearKeys();
        if (!(ruletype == bb || ruletype == bbw)) {
            count2.ClearKeys();
        }

        // call the appropriate algorithm
        switch (ruletype) {
            case moore:
                Do3DNextGenMoore();
                newpop = CreateResultsFromC1G3(L, laststep);
                break;
            case face:
                Do3DNextGenFace();
                newpop = CreateResultsFromC1C2(L, laststep);
                break;
            case corner:
                Do3DNextGenCorner();
                newpop = CreateResultsFromC1C2(L, laststep);
                break;
            case edge:
                Do3DNextGenEdge();
                newpop = CreateResultsFromC1C2(L, laststep);
                break;
            case hexahedral:
                Do3DNextGenHexahedral();
                newpop = CreateResultsFromC1C2(L, laststep);
                break;
            case bb:
                if ((gridsize & 1) == 1) return OverlayError("grid size must be even for BusyBoxes");
                Do3DNextGenBB(true, gencount);
                newpop = CreateResultsFromC1(L, laststep);
                break;
            case bbw:
                if ((gridsize & 1) == 1) return OverlayError("grid size must be even for BusyBoxes");
                Do3DNextGenBB(false, gencount);
                newpop = CreateResultsFromC1(L, laststep);
                break;
            default:
                return OverlayError("illegal rule specified");
        }

        // update history if required
        if (showhistory > 0) UpdateHistoryFromLive();

        // next step
        gencount++;

        // exit if population is zero
        if (newpop == 0) break;
    }

    // return the population
    lua_pushinteger(L, newpop);

    // return the gencount
    lua_pushinteger(L, gencount);

    // return the grid bounding box
    lua_pushinteger(L, minx);
    lua_pushinteger(L, maxx);
    lua_pushinteger(L, miny);
    lua_pushinteger(L, maxy);
    lua_pushinteger(L, minz);
    lua_pushinteger(L, maxz);

    *nresults = 9;  // table, popcount, gencount, minx, maxx, miny, maxy, minz, maxz

    return NULL;
}

// -----------------------------------------------------------------------------

void Overlay::Do3DNextGenBB(const bool mirror, const int gencount) {
    // the algorithm used below is a slightly modified (and corrected!)
    // version of the kernel code in Ready's Salt 3D example
    // (see Patterns/CellularAutomata/Salt/salt3D_circular330.vti);
    // it uses a rule based on 28 cells in a 7x7 neighborhood for each live cell

    // swap site locations
    static const int swap1[] = { 1,  1};
    static const int swap2[] = {-1,  1};
    static const int swap3[] = {-1, -1};
    static const int swap4[] = { 1, -1};

    // activator locations
    static const int act5[]  = { 2, -1};
    static const int act6[]  = { 2,  1};
    static const int act7[]  = { 1,  2};
    static const int act8[]  = {-1,  2};
    static const int act9[]  = {-2,  1};
    static const int act10[] = {-2, -1};
    static const int act11[] = {-1, -2};
    static const int act12[] = { 1, -2};

    // inhibitor locations
    static const int inhib13[] = {-2, -3};
    static const int inhib14[] = { 0, -3};
    static const int inhib15[] = { 2, -3};
    static const int inhib16[] = {-3, -2};
    static const int inhib17[] = { 3, -2};
    static const int inhib18[] = { 0, -1};
    static const int inhib19[] = {-3,  0};
    static const int inhib20[] = {-1,  0};
    static const int inhib21[] = { 1,  0};
    static const int inhib22[] = { 3,  0};
    static const int inhib23[] = { 0,  1};
    static const int inhib24[] = {-3,  2};
    static const int inhib25[] = { 3,  2};
    static const int inhib26[] = {-2,  3};
    static const int inhib27[] = { 0,  3};
    static const int inhib28[] = { 2,  3};

    static const int *coords[] = {
        // 1 to 4 are the coordinates for the 4 potential swap sites:
        swap1, swap2, swap3, swap4,
        // 5 to 12 are activators:
        act5, act6, act7, act8, act9, act10, act11, act12,
        // 13 to 28 are inhibitors:
        inhib13, inhib14, inhib15, inhib16, inhib17, inhib18,
        inhib19, inhib20, inhib21, inhib22, inhib23, inhib24,
        inhib25, inhib26, inhib27, inhib28
    };

    // numbers are indices into the coords array
    static const int actidx1[] = {4,  7};
    static const int actidx2[] = {6,  9};
    static const int actidx3[] = {8, 11};
    static const int actidx4[] = {5, 10};
    static const int *activators[] = {actidx1, actidx2, actidx3, actidx4};

    static const int inhibidx1[] = {17, 24, 21, 26, 19, 27, 6,  9,  8, 11,  5, 10};
    static const int inhibidx2[] = {17, 23, 18, 26, 20, 25, 4,  7,  8, 11,  5, 10};
    static const int inhibidx3[] = {15, 22, 13, 18, 12, 20, 4,  7,  6,  9,  5, 10};
    static const int inhibidx4[] = {19, 14, 13, 21, 16, 22, 4,  7,  6,  9,  8, 11};
    static const int *inhibitors[] = {inhibidx1, inhibidx2, inhibidx3, inhibidx4};

    int numkeys;
    const int phase = gencount % 6;
    const int N = gridsize;
    const int NN = N * N;
    const int *grid3dkeys = grid3d.GetKeys(&numkeys);
    const unsigned char *grid3dvalues = grid3d.GetValues();

    // apply rule
    unsigned char val[28];
    for (int i = 0; i < numkeys; i++) {
        const int k = grid3dkeys[i];
        const unsigned int loc = xyz[k];
        int x = loc >> 16;
        int y = (loc >> 8) & 0xff;
        int z = loc & 0xff;
        if (((x + y + z) & 1) == (phase & 1)) {
            // this live cell has the right parity so get values for its 28 neighbors
            int sx, sy, sz;
            int j = 0;
            const int *coordsj;
            if (phase == 0 || phase == 3) {
                const int Nz = N * z;
                // use XY plane
                x += N;
                y += N;
                while (j < 28) {
                    // compute the next two values
                    coordsj = coords[j];
                    sx = modN[x + coordsj[0]];
                    sy = modN[y + coordsj[1]];
                    val[j++] = grid3dvalues[sx + N * (sy + Nz)];
                    coordsj = coords[j];
                    sx = modN[x + coordsj[0]];
                    sy = modN[y + coordsj[1]];
                    val[j++] = grid3dvalues[sx + N * (sy + Nz)];
                }
            } else {
                if (phase == 1 || phase == 4) {
                    // use YZ plane
                    y += N;
                    z += N;
                    while (j < 28) {
                        // compute the next two values
                        coordsj = coords[j];
                        sy = modN[y + coordsj[0]];
                        sz = modN[z + coordsj[1]];
                        val[j++] = grid3dvalues[x + N * (sy + N * sz)];
                        coordsj = coords[j];
                        sy = modN[y + coordsj[0]];
                        sz = modN[z + coordsj[1]];
                        val[j++] = grid3dvalues[x + N * (sy + N * sz)];
                    }
                } else {
                    // phase == 2 or 5 so use XZ plane
                    x += N;
                    z += N;
                    const int Ny = N * y;
                    while (j < 28) {
                        // compute the next two values
                        coordsj = coords[j];
                        sx = modN[x + coordsj[0]];
                        sz = modN[z + coordsj[1]];
                        val[j++] = grid3dvalues[sx + Ny + NN * sz];
                        coordsj = coords[j];
                        sx = modN[x + coordsj[0]];
                        sz = modN[z + coordsj[1]];
                        val[j++] = grid3dvalues[sx + Ny + NN * sz];
                    }
                }
            }

            // find the potential swaps
            int numswaps = 0;
            int swapi = 0;
            for (j = 0; j <= 3; j++) {
                const int *activatorsj = activators[j];
                const int *inhibitorsj = inhibitors[j];
                // if either activator is a live cell then the swap is possible,
                // but if any inhibitor is a live cell then the swap is forbidden
                if ((val[activatorsj[0]] || val[activatorsj[1]])
                    && ! (val[inhibitorsj[0]] || val[inhibitorsj[1]] ||
                             val[inhibitorsj[2]] || val[inhibitorsj[3]] ||
                             val[inhibitorsj[4]] || val[inhibitorsj[5]] ||
                             val[inhibitorsj[6]] || val[inhibitorsj[7]] ||
                             val[inhibitorsj[8]] || val[inhibitorsj[9]] ||
                             val[inhibitorsj[10]] || val[inhibitorsj[11]])) {
                    numswaps++;
                    if (numswaps > 1) break;
                    swapi = j;   // index of swap location in coords array (0..3)
                }
            }

            // if only one swap, and only to an empty cell, then do it
            if (numswaps == 1 && ! val[swapi]) {
                // calculate the swap position
                int newx, newy, newz;
                if (phase == 0 || phase == 3) {
                    // use XY plane
                    newx = x - N + coords[swapi][0];
                    newy = y - N + coords[swapi][1];
                    newz = z;
                } else {
                    if (phase == 1 || phase == 4) {
                        // use YZ plane
                        newx = x;
                        newy = y - N + coords[swapi][0];
                        newz = z - N + coords[swapi][1];
                    } else {
                        // phase == 2 or 5 so use XZ plane
                        newx = x - N + coords[swapi][0];
                        newy = y;
                        newz = z - N + coords[swapi][1];
                    }
                }
                // if using mirror mode then don't wrap
                if (mirror &&
                    (newx < 0 || newx >= N ||
                     newy < 0 || newy >= N ||
                     newz < 0 || newz >= N)) {
                    // swap position is outside grid so don't do it
                    count1.SetTo1(k);
                } else {
                    // do the swap, wrapping if necessary
                    newx = modN[newx + N];
                    newy = modN[newy + N];
                    newz = modN[newz + N];
                    count1.SetTo1(newx + N * (newy + N * newz));
                }
            } else {
                // don't swap this live cell
                count1.SetTo1(k);
            }
        } else {
            // live cell with wrong parity
            count1.SetTo1(k);
        }
    }
}

// -----------------------------------------------------------------------------

void Overlay::Do3DNextGenFace() {
    int numkeys;
    const int N = gridsize;
    const int NN = N * N;

    // check whether to use wrap
    if (liveedge) {
        // use wrap version
        const int *grid3dkeys = grid3d.GetKeys(&numkeys);
        const unsigned char *grid3dvalues = grid3d.GetValues();
        for (int i = 0; i < numkeys; i++) {
            const int k = grid3dkeys[i];
            const unsigned int loc = xyz[k];
            const int x = loc >> 16;
            const int y = (loc >> 8) & 0xff;
            const int z = loc & 0xff;
            count1.SetValue(k, 0);
            const int Ny = N * y;
            const int NNz = NN * z;
            const int NypNNz = Ny + NNz;
            const int xpNNz = x + NNz;
            const int xpNy = x + Ny;

            // calculate the positions of the 6 cells next to each face of this cell
            const int xp1 = modN[x + 1] + NypNNz;
            const int xm1 = modN[x - 1 + N] + NypNNz;
            const int yp1 = N * modN[y + 1] + xpNNz;
            const int ym1 = N * modN[y - 1 + N] + xpNNz;
            const int zp1 = NN * modN[z + 1] + xpNy;
            const int zm1 = NN * modN[z - 1 + N] + xpNy;

            if (grid3dvalues[xp1]) { count1.AddToValue(k, 1); } else { count2.AddToValue(xp1, 1); }
            if (grid3dvalues[xm1]) { count1.AddToValue(k, 1); } else { count2.AddToValue(xm1, 1); }
            if (grid3dvalues[yp1]) { count1.AddToValue(k, 1); } else { count2.AddToValue(yp1, 1); }
            if (grid3dvalues[ym1]) { count1.AddToValue(k, 1); } else { count2.AddToValue(ym1, 1); }
            if (grid3dvalues[zp1]) { count1.AddToValue(k, 1); } else { count2.AddToValue(zp1, 1); }
            if (grid3dvalues[zm1]) { count1.AddToValue(k, 1); } else { count2.AddToValue(zm1, 1); }
        }
    } else {
        // use no wrap version
        const int *grid3dkeys = grid3d.GetKeys(&numkeys);
        const unsigned char *grid3dvalues = grid3d.GetValues();
        for (int i = 0; i < numkeys; i++) {
            const int k = grid3dkeys[i];
            count1.SetValue(k, 0);

            // calculate the positions of the 6 cells next to each face of this cell
            const int xp1 = k + 1;
            const int xm1 = k - 1;
            const int yp1 = k + N;
            const int ym1 = k - N;
            const int zp1 = k + NN;
            const int zm1 = k - NN;

            if (grid3dvalues[xp1]) { count1.AddToValue(k, 1); } else { count2.AddToValue(xp1, 1); }
            if (grid3dvalues[xm1]) { count1.AddToValue(k, 1); } else { count2.AddToValue(xm1, 1); }
            if (grid3dvalues[yp1]) { count1.AddToValue(k, 1); } else { count2.AddToValue(yp1, 1); }
            if (grid3dvalues[ym1]) { count1.AddToValue(k, 1); } else { count2.AddToValue(ym1, 1); }
            if (grid3dvalues[zp1]) { count1.AddToValue(k, 1); } else { count2.AddToValue(zp1, 1); }
            if (grid3dvalues[zm1]) { count1.AddToValue(k, 1); } else { count2.AddToValue(zm1, 1); }
        }
    }
}

// -----------------------------------------------------------------------------

void Overlay::Do3DNextGenCorner() {
    int numkeys;
    const int N = gridsize;
    const int NN = N * N;

    // check whether to use wrap
    if (liveedge) {
        // use wrap version
        const int *grid3dkeys = grid3d.GetKeys(&numkeys);
        const unsigned char *grid3dvalues = grid3d.GetValues();
        for (int i = 0; i < numkeys; i++) {
            const int k = grid3dkeys[i];
            const unsigned int loc = xyz[k];
            const int x = loc >> 16;
            const int y = (loc >> 8) & 0xff;
            const int z = loc & 0xff;
            count1.SetValue(k, 0);

            const int xp1 = modN[x + 1];
            const int xm1 = modN[x - 1 + N];
            const int yp1 = N * modN[y + 1];
            const int ym1 = N * modN[y - 1 + N];
            const int zp1 = NN * modN[z + 1];
            const int zm1 = NN * modN[z - 1 + N];

            // calculate the positions of the 8 cells cells touching each corner of this cell
            const int ppp = xp1 + yp1 + zp1;
            const int mmm = xm1 + ym1 + zm1;
            const int ppm = xp1 + yp1 + zm1;
            const int mmp = xm1 + ym1 + zp1;
            const int mpp = xm1 + yp1 + zp1;
            const int pmm = xp1 + ym1 + zm1;
            const int pmp = xp1 + ym1 + zp1;
            const int mpm = xm1 + yp1 + zm1;

            if (grid3dvalues[ppp]) { count1.AddToValue(k, 1); } else { count2.AddToValue(ppp, 1); }
            if (grid3dvalues[mmm]) { count1.AddToValue(k, 1); } else { count2.AddToValue(mmm, 1); }
            if (grid3dvalues[ppm]) { count1.AddToValue(k, 1); } else { count2.AddToValue(ppm, 1); }
            if (grid3dvalues[mmp]) { count1.AddToValue(k, 1); } else { count2.AddToValue(mmp, 1); }
            if (grid3dvalues[mpp]) { count1.AddToValue(k, 1); } else { count2.AddToValue(mpp, 1); }
            if (grid3dvalues[pmm]) { count1.AddToValue(k, 1); } else { count2.AddToValue(pmm, 1); }
            if (grid3dvalues[pmp]) { count1.AddToValue(k, 1); } else { count2.AddToValue(pmp, 1); }
            if (grid3dvalues[mpm]) { count1.AddToValue(k, 1); } else { count2.AddToValue(mpm, 1); }
        }
    } else {
        // use no wrap version
        const int *grid3dkeys = grid3d.GetKeys(&numkeys);
        const unsigned char *grid3dvalues = grid3d.GetValues();
        for (int i = 0; i < numkeys; i++) {
            const int k = grid3dkeys[i];
            count1.SetValue(k, 0);

            // calculate the positions of the 8 cells cells touching each corner of this cell
            const int ppp = k + 1 + N + NN;
            const int mmm = k - 1 - N - NN;
            const int ppm = k + 1 + N - NN;
            const int mmp = k - 1 - N + NN;
            const int mpp = k - 1 + N + NN;
            const int pmm = k + 1 - N - NN;
            const int pmp = k + 1 - N + NN;
            const int mpm = k - 1 + N - NN;

            if (grid3dvalues[ppp]) { count1.AddToValue(k, 1); } else { count2.AddToValue(ppp, 1); }
            if (grid3dvalues[mmm]) { count1.AddToValue(k, 1); } else { count2.AddToValue(mmm, 1); }
            if (grid3dvalues[ppm]) { count1.AddToValue(k, 1); } else { count2.AddToValue(ppm, 1); }
            if (grid3dvalues[mmp]) { count1.AddToValue(k, 1); } else { count2.AddToValue(mmp, 1); }
            if (grid3dvalues[mpp]) { count1.AddToValue(k, 1); } else { count2.AddToValue(mpp, 1); }
            if (grid3dvalues[pmm]) { count1.AddToValue(k, 1); } else { count2.AddToValue(pmm, 1); }
            if (grid3dvalues[pmp]) { count1.AddToValue(k, 1); } else { count2.AddToValue(pmp, 1); }
            if (grid3dvalues[mpm]) { count1.AddToValue(k, 1); } else { count2.AddToValue(mpm, 1); }
        }
    }
}

// -----------------------------------------------------------------------------

void Overlay::Do3DNextGenEdge() {
    int numkeys;
    const int N = gridsize;
    const int NN = N * N;

    // check whether to use wrap
    if (liveedge) {
        // use wrap version
        const int *grid3dkeys = grid3d.GetKeys(&numkeys);
        const unsigned char *grid3dvalues = grid3d.GetValues();
        for (int i = 0; i < numkeys; i++) {
            const int k = grid3dkeys[i];
            const unsigned int loc = xyz[k];
            const int x = loc >> 16;
            const int y = (loc >> 8) & 0xff;
            const int z = loc & 0xff;
            count1.SetValue(k, 0);

            const int xp1 = modN[x + 1];
            const int xm1 = modN[x - 1 + N];
            const int yp1 = N * modN[y + 1];
            const int ym1 = N * modN[y - 1 + N];
            const int zp1 = NN * modN[z + 1];
            const int zm1 = NN * modN[z - 1 + N];
            const int Ny = N * y;
            const int NNz = NN * z;

            // calculate the positions of the 12 cells next to each edge of this cell
            const int xpp = x + yp1 + zp1;
            const int xmm = x + ym1 + zm1;
            const int xpm = x + yp1 + zm1;
            const int xmp = x + ym1 + zp1;

            const int pyp = xp1 + Ny + zp1;
            const int mym = xm1 + Ny + zm1;
            const int pym = xp1 + Ny + zm1;
            const int myp = xm1 + Ny + zp1;

            const int ppz = xp1 + yp1 + NNz;
            const int mmz = xm1 + ym1 + NNz;
            const int pmz = xp1 + ym1 + NNz;
            const int mpz = xm1 + yp1 + NNz;

            if (grid3dvalues[xpp]) { count1.AddToValue(k, 1); } else { count2.AddToValue(xpp, 1); }
            if (grid3dvalues[xmm]) { count1.AddToValue(k, 1); } else { count2.AddToValue(xmm, 1); }
            if (grid3dvalues[xpm]) { count1.AddToValue(k, 1); } else { count2.AddToValue(xpm, 1); }
            if (grid3dvalues[xmp]) { count1.AddToValue(k, 1); } else { count2.AddToValue(xmp, 1); }

            if (grid3dvalues[pyp]) { count1.AddToValue(k, 1); } else { count2.AddToValue(pyp, 1); }
            if (grid3dvalues[mym]) { count1.AddToValue(k, 1); } else { count2.AddToValue(mym, 1); }
            if (grid3dvalues[pym]) { count1.AddToValue(k, 1); } else { count2.AddToValue(pym, 1); }
            if (grid3dvalues[myp]) { count1.AddToValue(k, 1); } else { count2.AddToValue(myp, 1); }

            if (grid3dvalues[ppz]) { count1.AddToValue(k, 1); } else { count2.AddToValue(ppz, 1); }
            if (grid3dvalues[mmz]) { count1.AddToValue(k, 1); } else { count2.AddToValue(mmz, 1); }
            if (grid3dvalues[pmz]) { count1.AddToValue(k, 1); } else { count2.AddToValue(pmz, 1); }
            if (grid3dvalues[mpz]) { count1.AddToValue(k, 1); } else { count2.AddToValue(mpz, 1); }
        }
    } else {
        // use no wrap version
        const int *grid3dkeys = grid3d.GetKeys(&numkeys);
        const unsigned char *grid3dvalues = grid3d.GetValues();
        for (int i = 0; i < numkeys; i++) {
            const int k = grid3dkeys[i];
            count1.SetValue(k, 0);

            // calculate the positions of the 12 cells next to each edge of this cell
            const int xpp = k + N + NN;
            const int xmm = k - N - NN;
            const int xpm = k + N - NN;
            const int xmp = k - N + NN;

            const int pyp = k + 1 + NN;
            const int mym = k - 1 - NN;
            const int pym = k + 1 - NN;
            const int myp = k - 1 + NN;

            const int ppz = k + 1 + N;
            const int mmz = k - 1 - N;
            const int pmz = k + 1 - N;
            const int mpz = k - 1 + N;

            if (grid3dvalues[xpp]) { count1.AddToValue(k, 1); } else { count2.AddToValue(xpp, 1); }
            if (grid3dvalues[xmm]) { count1.AddToValue(k, 1); } else { count2.AddToValue(xmm, 1); }
            if (grid3dvalues[xpm]) { count1.AddToValue(k, 1); } else { count2.AddToValue(xpm, 1); }
            if (grid3dvalues[xmp]) { count1.AddToValue(k, 1); } else { count2.AddToValue(xmp, 1); }

            if (grid3dvalues[pyp]) { count1.AddToValue(k, 1); } else { count2.AddToValue(pyp, 1); }
            if (grid3dvalues[mym]) { count1.AddToValue(k, 1); } else { count2.AddToValue(mym, 1); }
            if (grid3dvalues[pym]) { count1.AddToValue(k, 1); } else { count2.AddToValue(pym, 1); }
            if (grid3dvalues[myp]) { count1.AddToValue(k, 1); } else { count2.AddToValue(myp, 1); }

            if (grid3dvalues[ppz]) { count1.AddToValue(k, 1); } else { count2.AddToValue(ppz, 1); }
            if (grid3dvalues[mmz]) { count1.AddToValue(k, 1); } else { count2.AddToValue(mmz, 1); }
            if (grid3dvalues[pmz]) { count1.AddToValue(k, 1); } else { count2.AddToValue(pmz, 1); }
            if (grid3dvalues[mpz]) { count1.AddToValue(k, 1); } else { count2.AddToValue(mpz, 1); }
        }
    }
}

// -----------------------------------------------------------------------------

void Overlay::Do3DNextGenHexahedral() {
    int numkeys;
    const int N = gridsize;
    const int NN = N * N;

    // check whether to use wrap
    if (liveedge) {
        // use wrap version
        const int *grid3dkeys = grid3d.GetKeys(&numkeys);
        const unsigned char *grid3dvalues = grid3d.GetValues();
        for (int i = 0; i < numkeys; i++) {
            const int k = grid3dkeys[i];
            const unsigned int loc = xyz[k];
            const int x = loc >> 16;
            const int y = (loc >> 8) & 0xff;
            const int z = loc & 0xff;
            count1.SetValue(k, 0);

            const int xp1 = modN[x + 1];
            const int xm1 = modN[x - 1 + N];
            const int yp1 = N * modN[y + 1];
            const int ym1 = N * modN[y - 1 + N];
            const int zp1 = NN * modN[z + 1];
            const int zm1 = NN * modN[z - 1 + N];
            const int Ny = N * y;
            const int NNz = NN * z;

            // calculate the positions of the 12 neighboring cells (using the top offsets given
            // on page 872 in http://www.complex-systems.com/pdf/01-5-1.pdf)
            const int xym = x + Ny + zm1;
            const int xyp = x + Ny + zp1;
            const int xpm = x + yp1 + zm1;
            const int xpz = x + yp1 + NNz;
            const int xmp = x + ym1 + zp1;
            const int xmz = x + ym1 + NNz;
            const int pym = xp1 + Ny + zm1;
            const int pyz = xp1 + Ny + NNz;
            const int myp = xm1 + Ny + zp1;
            const int myz = xm1 + Ny + NNz;
            const int pmz = xp1 + ym1 + NNz;
            const int mpz = xm1 + yp1 + NNz;

            if (grid3dvalues[xym]) { count1.AddToValue(k, 1); } else { count2.AddToValue(xym, 1); }
            if (grid3dvalues[xyp]) { count1.AddToValue(k, 1); } else { count2.AddToValue(xyp, 1); }
            if (grid3dvalues[xpm]) { count1.AddToValue(k, 1); } else { count2.AddToValue(xpm, 1); }
            if (grid3dvalues[xpz]) { count1.AddToValue(k, 1); } else { count2.AddToValue(xpz, 1); }

            if (grid3dvalues[xmp]) { count1.AddToValue(k, 1); } else { count2.AddToValue(xmp, 1); }
            if (grid3dvalues[xmz]) { count1.AddToValue(k, 1); } else { count2.AddToValue(xmz, 1); }
            if (grid3dvalues[pym]) { count1.AddToValue(k, 1); } else { count2.AddToValue(pym, 1); }
            if (grid3dvalues[pyz]) { count1.AddToValue(k, 1); } else { count2.AddToValue(pyz, 1); }

            if (grid3dvalues[myp]) { count1.AddToValue(k, 1); } else { count2.AddToValue(myp, 1); }
            if (grid3dvalues[myz]) { count1.AddToValue(k, 1); } else { count2.AddToValue(myz, 1); }
            if (grid3dvalues[pmz]) { count1.AddToValue(k, 1); } else { count2.AddToValue(pmz, 1); }
            if (grid3dvalues[mpz]) { count1.AddToValue(k, 1); } else { count2.AddToValue(mpz, 1); }
        }
    } else {
        // use no wrap version
        const int *grid3dkeys = grid3d.GetKeys(&numkeys);
        const unsigned char *grid3dvalues = grid3d.GetValues();
        for (int i = 0; i < numkeys; i++) {
            const int k = grid3dkeys[i];
            count1.SetValue(k, 0);

            // calculate the positions of the 12 neighboring cells (using the top offsets given
            // on page 872 in http://www.complex-systems.com/pdf/01-5-1.pdf)
            const int xym = k - NN;
            const int xyp = k + NN;
            const int xpm = k + N - NN;
            const int xpz = k + N;
            const int xmp = k - N + NN;
            const int xmz = k - N;
            const int pym = k + 1 - NN;
            const int pyz = k + 1;
            const int myp = k - 1 + NN;
            const int myz = k - 1;
            const int pmz = k + 1 - N;
            const int mpz = k - 1 + N;

            if (grid3dvalues[xym]) { count1.AddToValue(k, 1); } else { count2.AddToValue(xym, 1); }
            if (grid3dvalues[xyp]) { count1.AddToValue(k, 1); } else { count2.AddToValue(xyp, 1); }
            if (grid3dvalues[xpm]) { count1.AddToValue(k, 1); } else { count2.AddToValue(xpm, 1); }
            if (grid3dvalues[xpz]) { count1.AddToValue(k, 1); } else { count2.AddToValue(xpz, 1); }

            if (grid3dvalues[xmp]) { count1.AddToValue(k, 1); } else { count2.AddToValue(xmp, 1); }
            if (grid3dvalues[xmz]) { count1.AddToValue(k, 1); } else { count2.AddToValue(xmz, 1); }
            if (grid3dvalues[pym]) { count1.AddToValue(k, 1); } else { count2.AddToValue(pym, 1); }
            if (grid3dvalues[pyz]) { count1.AddToValue(k, 1); } else { count2.AddToValue(pyz, 1); }

            if (grid3dvalues[myp]) { count1.AddToValue(k, 1); } else { count2.AddToValue(myp, 1); }
            if (grid3dvalues[myz]) { count1.AddToValue(k, 1); } else { count2.AddToValue(myz, 1); }
            if (grid3dvalues[pmz]) { count1.AddToValue(k, 1); } else { count2.AddToValue(pmz, 1); }
            if (grid3dvalues[mpz]) { count1.AddToValue(k, 1); } else { count2.AddToValue(mpz, 1); }
        }
    }
}

// -----------------------------------------------------------------------------

void Overlay::Do3DNextGenMoore() {
    int numkeys;
    const int *count1keys = NULL;
    const unsigned char *count1values = NULL;
    const int N = gridsize;
    const int NN = N * N;
    const int NNN = NN * N;

    // check whether to use wrap
    if (liveedge) {
        // use wrap version
        const int *grid3dkeys = grid3d.GetKeys(&numkeys);
        const int NNmN = NN - N;
        for (int i = 0; i < numkeys; i++) {
            const int k = grid3dkeys[i];
            const int y = modNN[k];
            count1.AddToValue(k, 1);
            count1.AddToValue(k + (y >= NNmN ? -NNmN: N), 1);
            count1.AddToValue(k + (y < N ? NNmN : -N), 1);
        }

        // get keys and values in count1
        count1keys = count1.GetKeys(&numkeys);
        count1values = count1.GetValues();
        const int Nm1 = N - 1;
        for (int i = 0; i < numkeys; i++) {
            const int k = count1keys[i];
            const unsigned char v = count1values[k];
            const int x = modN[k];
            count2.AddToValue(k, v);
            count2.AddToValue(k + (x == Nm1 ? -Nm1 : 1), v);
            count2.AddToValue(k + (x == 0 ? Nm1 : -1), v);
        }

        // get keys and values in count2
        const int *count2keys = count2.GetKeys(&numkeys);
        const unsigned char *count2values = count2.GetValues();
        const int NNNmNN = NNN - NN;
        count1.ClearKeys();
        for (int i = 0; i < numkeys; i++) {
            const int k = count2keys[i];
            const unsigned char v = count2values[k];
            count1.AddToValue(k, v);
            count1.AddToValue(k >= NNNmNN ? k - NNNmNN : k + NN, v);
            count1.AddToValue(k < NN ? k + NNNmNN : k - NN, v);
        }
    } else {
        // use nowrap version
        const int *grid3dkeys = grid3d.GetKeys(&numkeys);
        for (int i = 0; i < numkeys; i++) {
            const int k = grid3dkeys[i];
            count1.AddToValue(k, 1);
            count1.AddToValue(k + N, 1);
            count1.AddToValue(k - N, 1);
        }

        // get keys and values in count1
        count1keys = count1.GetKeys(&numkeys);
        count1values = count1.GetValues();
        for (int i = 0; i < numkeys; i++) {
            const int k = count1keys[i];
            const unsigned char v = count1values[k];
            count2.AddToValue(k, v);
            count2.AddToValue(k + 1, v);
            count2.AddToValue(k - 1, v);
        }

        // get keys and values in count2
        const int *count2keys = count2.GetKeys(&numkeys);
        const unsigned char *count2values = count2.GetValues();
        count1.ClearKeys();
        for (int i = 0; i < numkeys; i++) {
            const int k = count2keys[i];
            const unsigned char v = count2values[k];
            count1.AddToValue(k, v);
            count1.AddToValue(k + NN, v);
            count1.AddToValue(k - NN, v);
        }
    }
}

// -----------------------------------------------------------------------------

const char *Overlay::DoOverlayCommand(const char *cmd)
{
    // determine which command to run
    if (strncmp(cmd, "set ", 4) == 0)          return DoSetPixel(cmd+4);
    if (strncmp(cmd, "get ", 4) == 0)          return DoGetPixel(cmd+4);
    if (strcmp(cmd,  "xy") == 0)               return DoGetXY();
    if (strncmp(cmd, "paste", 5) == 0)         return DoPaste(cmd+5);
    if (strncmp(cmd, "rgba", 4) == 0)          return DoSetRGBA(cmd+4);
    if (strncmp(cmd, "blend", 5) == 0)         return DoBlend(cmd+5);
    if (strncmp(cmd, "fill", 4) == 0)          return DoFill(cmd+4);
    if (strncmp(cmd, "copy", 4) == 0)          return DoCopy(cmd+4);
    if (strncmp(cmd, "optimize", 8) == 0)      return DoOptimize(cmd+8);
    if (strncmp(cmd, "lineoption ", 11) == 0)  return DoLineOption(cmd+11);
    if (strncmp(cmd, "lines", 5) == 0)         return DoLine(cmd+5, false);
    if (strncmp(cmd, "line", 4) == 0)          return DoLine(cmd+4, true);
    if (strncmp(cmd, "ellipse", 7) == 0)       return DoEllipse(cmd+7);
    if (strncmp(cmd, "flood", 5) == 0)         return DoFlood(cmd+5);
    if (strncmp(cmd, "textoption ", 11) == 0)  return DoTextOption(cmd+11);
    if (strncmp(cmd, "text", 4) == 0)          return DoText(cmd+4);
    if (strncmp(cmd, "font", 4) == 0)          return DoFont(cmd+4);
    if (strncmp(cmd, "transform", 9) == 0)     return DoTransform(cmd+9);
    if (strncmp(cmd, "position", 8) == 0)      return DoPosition(cmd+8);
    if (strncmp(cmd, "load", 4) == 0)          return DoLoad(cmd+4);
    if (strncmp(cmd, "save", 4) == 0)          return DoSave(cmd+4);
    if (strncmp(cmd, "scale", 5) == 0)         return DoScale(cmd+5);
    if (strncmp(cmd, "cursor", 6) == 0)        return DoCursor(cmd+6);
    if (strcmp(cmd,  "update") == 0)           return DoUpdate();
    if (strncmp(cmd, "create", 6) == 0)        return DoCreate(cmd+6);
    if (strncmp(cmd, "resize", 6) == 0)        return DoResize(cmd+6);
    if (strncmp(cmd, "cellview ", 9) == 0)     return DoCellView(cmd+9);
    if (strncmp(cmd, "celloption ", 11) == 0)  return DoCellOption(cmd+11);
    if (strncmp(cmd, "camera ", 7) == 0)       return DoCamera(cmd+7);
    if (strncmp(cmd, "theme ", 6) == 0)        return DoTheme(cmd+6);
    if (strncmp(cmd, "target", 6) == 0)        return DoTarget(cmd+6);
    if (strncmp(cmd, "replace ", 8) == 0)      return DoReplace(cmd+8);
    if (strncmp(cmd, "sound", 5) == 0)         return DoSound(cmd+5);
    if (strcmp(cmd,  "updatecells") == 0)      return DoUpdateCells();
    if (strcmp(cmd,  "drawcells") == 0)        return DoDrawCells();
    if (strncmp(cmd, "delete", 6) == 0)        return DoDelete(cmd+6);
    return OverlayError("unknown command");
}

// -----------------------------------------------------------------------------

const char *Overlay::DoOverlayTable(const char *cmd, lua_State *L, int n, int *nresults)
{
    // determine which command to run
    if ((strcmp(cmd, "set")) == 0)               return DoSetPixel(L, n, nresults);
    if ((strcmp(cmd, "get")) == 0)               return DoGet(L, n, nresults);
    if ((strcmp(cmd, "paste")) == 0)             return DoPaste(L, n, nresults);
    if ((strcmp(cmd, "rgba")) == 0)              return DoSetRGBA(cmd, L, n, nresults);
    if ((strcmp(cmd, "line")) == 0)              return DoLine(L, n, true, nresults);
    if ((strcmp(cmd, "lines")) == 0)             return DoLine(L, n, false, nresults);
    if ((strcmp(cmd, "fill")) == 0)              return DoFill(L, n, nresults);

    // customized commands to speed up 3D.lua
    if ((strcmp(cmd, "nextgen3d")) == 0)         return Do3DNextGen(L, n, nresults);
    if ((strcmp(cmd, "setrule3d")) == 0)         return Do3DSetRule(L, n, nresults);
    if ((strcmp(cmd, "setsize3d")) == 0)         return Do3DSetGridSize(L, n, nresults);
    if ((strcmp(cmd, "setstep3d")) == 0)         return Do3DSetStepSize(L, n, nresults);
    if ((strcmp(cmd, "settrans3d")) == 0)        return Do3DSetTransform(L, n, nresults);
    if ((strcmp(cmd, "displaycells3d")) == 0)    return Do3DDisplayCells(L, n, nresults);
    if ((strcmp(cmd, "setcelltype3d")) == 0)     return Do3DSetCellType(L, n, nresults);
    if ((strcmp(cmd, "setdepthshading3d")) == 0) return Do3DSetDepthShading(L, n, nresults);
    if ((strcmp(cmd, "setpattern3d")) == 0)      return Do3DSetPattern(L, n, nresults);
    if ((strcmp(cmd, "setselpasact3d")) == 0)    return Do3DSetSelectPasteActive(L, n, nresults);
    if ((strcmp(cmd, "sethistory3d")) == 0)      return Do3DSetCellHistory(L, n, nresults);

    return OverlayError("unknown command");
}
