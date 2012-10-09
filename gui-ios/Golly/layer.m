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

#include "bigint.h"
#include "lifealgo.h"
#include "qlifealgo.h"
#include "hlifealgo.h"
#include "viewport.h"
#include "util.h"          // for linereader

#include "utils.h"         // for gRect, Warning, etc
#include "prefs.h"         // for initrule, swapcolors, userrules, rulesdir, etc
#include "algos.h"         // for algo_type, initalgo, algoinfo, CreateNewUniverse, etc
#include "control.h"       // for generating
#include "select.h"        // for Selection
#include "file.h"          // for SetPatternTitle
#include "view.h"          // for OutsideLimits, CopyRect
#include "undo.h"          // for UndoRedo
#include "layer.h"

// -----------------------------------------------------------------------------

bool inscript = false;          // move to script.m if we ever support scripting!!!

int numlayers = 0;              // number of existing layers
int numclones = 0;              // number of cloned layers
int currindex = -1;             // index of current layer

Layer* currlayer = NULL;        // pointer to current layer
Layer* layer[MAX_LAYERS];       // array of layers

bool cloneavail[MAX_LAYERS];    // for setting unique cloneid
bool cloning = false;           // adding a cloned layer?
bool duplicating = false;       // adding a duplicated layer?

algo_type oldalgo;              // algorithm in old layer
std::string oldrule;            // rule in old layer
int oldmag;                     // scale in old layer
bigint oldx;                    // X position in old layer
bigint oldy;                    // Y position in old layer
TouchModes oldmode;             // touch mode in old layer

// -----------------------------------------------------------------------------

void CalculateTileRects(int bigwd, int bight)
{
    // set tilerect in each layer
    gRect r;
    bool portrait = (bigwd <= bight);
    int rows, cols;
    
    // try to avoid the aspect ratio of each tile becoming too large
    switch (numlayers) {
        case 4: rows = 2; cols = 2; break;
        case 9: rows = 3; cols = 3; break;
            
        case 3: case 5: case 7:
            rows = portrait ? numlayers / 2 + 1 : 2;
            cols = portrait ? 2 : numlayers / 2 + 1;
            break;
            
        case 6: case 8: case 10:
            rows = portrait ? numlayers / 2 : 2;
            cols = portrait ? 2 : numlayers / 2;
            break;
            
        default:
            // numlayers == 2 or > 10
            rows = portrait ? numlayers : 1;
            cols = portrait ? 1 : numlayers;
    }
    
    int tilewd = bigwd / cols;
    int tileht = bight / rows;
    if ( float(tilewd) > float(tileht) * 2.5 ) {
        rows = 1;
        cols = numlayers;
        tileht = bight;
        tilewd = bigwd / numlayers;
    } else if ( float(tileht) > float(tilewd) * 2.5 ) {
        cols = 1;
        rows = numlayers;
        tilewd = bigwd;
        tileht = bight / numlayers;
    }
    
    for ( int i = 0; i < rows; i++ ) {
        for ( int j = 0; j < cols; j++ ) {
            r.x = j * tilewd;
            r.y = i * tileht;
            r.width = tilewd;
            r.height = tileht;
            if (i == rows - 1) {
                // may need to increase height of bottom-edge tile
                r.height += bight - (rows * tileht);
            }
            if (j == cols - 1) {
                // may need to increase width of right-edge tile
                r.width += bigwd - (cols * tilewd);
            }
            int index = i * cols + j;
            if (index == numlayers) {
                // numlayers == 3,5,7
                layer[index - 1]->tilerect.width += r.width;
            } else {
                layer[index]->tilerect = r;
            }
        }
    }
    
    if (tileborder > 0) {
        // make tilerects smaller to allow for equal-width tile borders
        for ( int i = 0; i < rows; i++ ) {
            for ( int j = 0; j < cols; j++ ) {
                int index = i * cols + j;
                if (index == numlayers) {
                    // numlayers == 3,5,7
                    layer[index - 1]->tilerect.width -= tileborder;
                } else {
                    layer[index]->tilerect.x += tileborder;
                    layer[index]->tilerect.y += tileborder;
                    layer[index]->tilerect.width -= tileborder;
                    layer[index]->tilerect.height -= tileborder;
                    if (j == cols - 1) layer[index]->tilerect.width -= tileborder;
                    if (i == rows - 1) layer[index]->tilerect.height -= tileborder;
                }
            }
        }
    }
}

// -----------------------------------------------------------------------------

void ResizeTiles(int bigwd, int bight)
{
    // set tilerect for each layer so they tile bigview's client area
    CalculateTileRects(bigwd, bight);
    
    /*!!!
    // set size of each tile window
    for ( int i = 0; i < numlayers; i++ )
        layer[i]->tilewin->SetSize( layer[i]->tilerect );
    
    // set viewport size for each tile; this is currently the same as the
    // tilerect size because tile windows are created with wxNO_BORDER
    for ( int i = 0; i < numlayers; i++ ) {
        int wd, ht;
        layer[i]->tilewin->GetClientSize(&wd, &ht);
        // wd or ht might be < 1 on Windows
        if (wd < 1) wd = 1;
        if (ht < 1) ht = 1;
        layer[i]->view->resize(wd, ht);
    }
    */
}

// -----------------------------------------------------------------------------

void ResizeLayers(int wd, int ht)
{
    // this is called whenever the size of the bigview window changes;
    // wd and ht are the dimensions of bigview's client area
    if (tilelayers && numlayers > 1) {
        ResizeTiles(wd, ht);
    } else {
        // resize viewport in each layer to bigview's client area
        for (int i = 0; i < numlayers; i++)
            layer[i]->view->resize(wd, ht);
    }
}

// -----------------------------------------------------------------------------

/*!!!

void CreateTiles()
{
    // create tile windows
    for ( int i = 0; i < numlayers; i++ ) {
        layer[i]->tilewin = new PatternView(bigview,
                                            // correct size will be set below by ResizeTiles
                                            0, 0, 0, 0,
                                            // we draw our own tile borders
                                            wxNO_BORDER |
                                            // needed for wxGTK
                                            wxFULL_REPAINT_ON_RESIZE |
                                            wxWANTS_CHARS);
        if (layer[i]->tilewin == NULL) Fatal(_("Failed to create tile window!"));
        
        // set tileindex >= 0; this must always match the layer index, so we'll need to
        // destroy and recreate all tiles whenever a tile is added, deleted or moved
        layer[i]->tilewin->tileindex = i;
        
#if wxUSE_DRAG_AND_DROP
        // let user drop file onto any tile (but file will be loaded into current tile)
        layer[i]->tilewin->SetDropTarget(mainptr->NewDropTarget());
#endif
    }
    
    // init tilerects, tile window sizes and their viewport sizes
    int wd, ht;
    bigview->GetClientSize(&wd, &ht);
    // wd or ht might be < 1 on Windows
    if (wd < 1) wd = 1;
    if (ht < 1) ht = 1;
    ResizeTiles(wd, ht);
    
    // change viewptr to tile window for current layer
    viewptr = currlayer->tilewin;
    if (mainptr->IsActive()) viewptr->SetFocus();
}

// -----------------------------------------------------------------------------

void DestroyTiles()
{
    // reset viewptr to main viewport window
    viewptr = bigview;
    if (mainptr->IsActive()) viewptr->SetFocus();
    
    // destroy all tile windows
    for ( int i = 0; i < numlayers; i++ )
        delete layer[i]->tilewin;
    
    // resize viewport in each layer to bigview's client area
    int wd, ht;
    bigview->GetClientSize(&wd, &ht);
    // wd or ht might be < 1 on Windows
    if (wd < 1) wd = 1;
    if (ht < 1) ht = 1;
    for ( int i = 0; i < numlayers; i++ )
        layer[i]->view->resize(wd, ht);
}
!!!*/

// -----------------------------------------------------------------------------

void SyncClones()
{
    if (numclones == 0) return;
    
    if (currlayer->cloneid > 0) {
        // make sure clone algo and most other settings are synchronized
        for ( int i = 0; i < numlayers; i++ ) {
            Layer* cloneptr = layer[i];
            if (cloneptr != currlayer && cloneptr->cloneid == currlayer->cloneid) {
                // universe might have been re-created, or algorithm changed
                cloneptr->algo = currlayer->algo;
                cloneptr->algtype = currlayer->algtype;
                cloneptr->rule = currlayer->rule;
                
                // no need to sync undo/redo history
                // cloneptr->undoredo = currlayer->undoredo;
                
                // along with view, don't sync these settings
                // cloneptr->autofit = currlayer->autofit;
                // cloneptr->hyperspeed = currlayer->hyperspeed;
                // cloneptr->showhashinfo = currlayer->showhashinfo;
                // cloneptr->drawingstate = currlayer->drawingstate;
                // cloneptr->touchmode = currlayer->touchmode;
                // cloneptr->originx = currlayer->originx;
                // cloneptr->originy = currlayer->originy;
                // cloneptr->currname = currlayer->currname;
                
                // sync various flags
                cloneptr->dirty = currlayer->dirty;
                cloneptr->savedirty = currlayer->savedirty;
                cloneptr->stayclean = currlayer->stayclean;
                
                // sync step size
                cloneptr->currbase = currlayer->currbase;
                cloneptr->currexpo = currlayer->currexpo;
                
                // sync selection info
                cloneptr->currsel = currlayer->currsel;
                cloneptr->savesel = currlayer->savesel;
                
                // sync the stuff needed to reset pattern
                cloneptr->startalgo = currlayer->startalgo;
                cloneptr->savestart = currlayer->savestart;
                cloneptr->startdirty = currlayer->startdirty;
                cloneptr->startrule = currlayer->startrule;
                cloneptr->startfile = currlayer->startfile;
                cloneptr->startgen = currlayer->startgen;
                cloneptr->currfile = currlayer->currfile;
                cloneptr->startsel = currlayer->startsel;
                // clone can have different starting name, pos, scale, step
                // cloneptr->startname = currlayer->startname;
                // cloneptr->startx = currlayer->startx;
                // cloneptr->starty = currlayer->starty;
                // cloneptr->startmag = currlayer->startmag;
                // cloneptr->startbase = currlayer->startbase;
                // cloneptr->startexpo = currlayer->startexpo;
                
                // sync timeline settings
                cloneptr->currframe = currlayer->currframe;
                cloneptr->autoplay = currlayer->autoplay;
                cloneptr->tlspeed = currlayer->tlspeed;
                cloneptr->lastframe = currlayer->lastframe;
            }
        }
    }
}

// -----------------------------------------------------------------------------

void SaveLayerSettings()
{
    // set oldalgo and oldrule for use in CurrentLayerChanged
    oldalgo = currlayer->algtype;
    oldrule = currlayer->algo->getrule();
    
    // we're about to change layer so remember current rule
    // in case we switch back to this layer
    currlayer->rule = oldrule;
    
    // synchronize clone info (do AFTER setting currlayer->rule)
    SyncClones();
    
    if (syncviews) {
        // save scale and location for use in CurrentLayerChanged
        oldmag = currlayer->view->getmag();
        oldx = currlayer->view->x;
        oldy = currlayer->view->y;
    }
    
    if (syncmodes) {
        // save touch mode for use in CurrentLayerChanged
        oldmode = currlayer->touchmode;
    }
}

// -----------------------------------------------------------------------------

bool RestoreRule(const char* rule)
{
    const char* err = currlayer->algo->setrule(rule);
    if (err) {
        // this can happen if the given rule's table/tree file was deleted
        // or it was edited and some sort of error introduced, so best to
        // use algo's default rule (which should never fail)
        currlayer->algo->setrule( currlayer->algo->DefaultRule() );
        std::string msg = "The rule \"";
        msg += rule;
        msg += "\" is no longer valid!\nUsing the default rule instead.";
        Warning(msg.c_str());
        return false;
    }
    return true;
}

// -----------------------------------------------------------------------------

/*!!!

void CurrentLayerChanged()
{
    // currlayer has changed since SaveLayerSettings was called;
    // update rule if the new currlayer uses a different algorithm or rule
    if ( currlayer->algtype != oldalgo || !currlayer->rule.IsSameAs(oldrule,false) ) {
        RestoreRule(currlayer->rule);
    }
    
    if (syncviews) currlayer->view->setpositionmag(oldx, oldy, oldmag);
    if (syncmodes) currlayer->touchmode = oldmode;
    
    // select current layer button (also deselects old button)
    layerbarptr->SelectButton(currindex, true);
    
    if (tilelayers && numlayers > 1) {
        // switch to new tile
        viewptr = currlayer->tilewin;
        if (mainptr->IsActive()) viewptr->SetFocus();
    }
    
    if (allowundo) {
        // update Undo/Redo items so they show the correct action
        currlayer->undoredo->UpdateUndoRedoItems();
    } else {
        // undo/redo is disabled so clear history;
        // this also removes action from Undo/Redo items
        currlayer->undoredo->ClearUndoRedo();
    }
    
    mainptr->SetStepExponent(currlayer->currexpo);
    // SetStepExponent calls SetGenIncrement
    mainptr->SetPatternTitle(currlayer->currname);
    
    mainptr->UpdateUserInterface(mainptr->IsActive());
    mainptr->UpdatePatternAndStatus();
    bigview->UpdateScrollBars();
}

!!!*/

// -----------------------------------------------------------------------------

static CGImageRef* CopyIcons(CGImageRef* srcicons, int maxstate)
{
    CGImageRef* iconptr = (CGImageRef*) malloc(256 * sizeof(CGImageRef));
    if (iconptr) {
        for (int i = 0; i < 256; i++) iconptr[i] = NULL;
        for (int i = 0; i <= maxstate; i++) {
            if (srcicons && srcicons[i]) {
                iconptr[i] = CGImageCreateCopy(srcicons[i]);
            }
        }
    }
    return iconptr;
}

// -----------------------------------------------------------------------------

static unsigned char** GetIconPixels(CGImageRef* srcicons, int maxstate)
{
    unsigned char** iconpixelsptr = (unsigned char**) malloc(256 * sizeof(unsigned char*));
    if (iconpixelsptr) {
        for (int i = 0; i < 256; i++) iconpixelsptr[i] = NULL;
        for (int i = 0; i <= maxstate; i++) {
            if (srcicons && srcicons[i]) {
                int wd = CGImageGetWidth(srcicons[i]);
                int ht = CGImageGetHeight(srcicons[i]);
                int bytesPerPixel = 4;
                int bytesPerRow = bytesPerPixel * wd;
                int bitsPerComponent = 8;

                // allocate enough memory to store icon's RGBA pixel data
                iconpixelsptr[i] = (unsigned char*) calloc(wd * ht * 4, 1);
                if (iconpixelsptr[i]) {
                    CGColorSpaceRef colorspace = CGColorSpaceCreateDeviceRGB();
                    CGContextRef ctx = CGBitmapContextCreate(iconpixelsptr[i], wd, ht,
                        bitsPerComponent, bytesPerRow, colorspace,
                        kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
                    CGContextDrawImage(ctx, CGRectMake(0, 0, wd, ht), srcicons[i]);
                    CGContextRelease(ctx);
                    CGColorSpaceRelease(colorspace);
                    // iconpixelsptr[i] now points to the icon's pixels in RGBA format
                }
            }
        }
    }
    return iconpixelsptr;
}

// -----------------------------------------------------------------------------

static void UpdateColorReferences(Layer* layerptr, int numstates)
{
    // release any old color refs
    for (int n = 0; n < 256; n++) {
        CGColorRelease(layerptr->colorref[n]);
        layerptr->colorref[n] = NULL;
    }
    
    CGColorSpaceRef colorspace = CGColorSpaceCreateDeviceRGB();
    CGFloat components[4];
    components[3] = 1.0;    // alpha

    // create new color refs
    for (int n = 0; n < numstates; n++) {
        components[0] = layerptr->cellr[n] / 255.0;
        components[1] = layerptr->cellg[n] / 255.0;
        components[2] = layerptr->cellb[n] / 255.0;
        layerptr->colorref[n] = CGColorCreate(colorspace, components);
    }
    
    CGColorSpaceRelease(colorspace);
}

// -----------------------------------------------------------------------------

void AddLayer()
{
    if (numlayers >= MAX_LAYERS) return;
    
    if (generating) Warning("Bug detected in AddLayer!");
    
    if (numlayers == 0) {
        // creating the very first layer
        currindex = 0;
    } else {
        //!!! if (tilelayers && numlayers > 1) DestroyTiles();
        
        SaveLayerSettings();
        
        // insert new layer after currindex
        currindex++;
        if (currindex < numlayers) {
            // shift right one or more layers
            for (int i = numlayers; i > currindex; i--)
                layer[i] = layer[i-1];
        }
    }
    
    Layer* oldlayer = NULL;
    if (cloning || duplicating) oldlayer = currlayer;
    
    currlayer = new Layer();
    if (currlayer == NULL) Fatal("Failed to create new layer!");
    layer[currindex] = currlayer;
    
    if (cloning || duplicating) {
        // copy old layer's colors to new layer
        currlayer->fromrgb = oldlayer->fromrgb;
        currlayer->torgb = oldlayer->torgb;
        for (int n = 0; n < currlayer->algo->NumCellStates(); n++) {
            currlayer->cellr[n] = oldlayer->cellr[n];
            currlayer->cellg[n] = oldlayer->cellg[n];
            currlayer->cellb[n] = oldlayer->cellb[n];
        }
        UpdateColorReferences(currlayer, currlayer->algo->NumCellStates());
        if (cloning) {
            // use same icon pointers
            currlayer->icons7x7 = oldlayer->icons7x7;
            currlayer->icons15x15 = oldlayer->icons15x15;
            currlayer->icons31x31 = oldlayer->icons31x31;
            // use same pixel data
            currlayer->iconpixels7x7 = oldlayer->iconpixels7x7;
            currlayer->iconpixels15x15 = oldlayer->iconpixels15x15;
            currlayer->iconpixels31x31 = oldlayer->iconpixels31x31;
        } else {
            // duplicate icons from old layer
            int maxstate = currlayer->algo->NumCellStates() - 1;
            currlayer->icons7x7 = CopyIcons(oldlayer->icons7x7, maxstate);
            currlayer->icons15x15 = CopyIcons(oldlayer->icons15x15, maxstate);
            currlayer->icons31x31 = CopyIcons(oldlayer->icons31x31, maxstate);
            // get icon pixel data
            currlayer->iconpixels7x7 = GetIconPixels(oldlayer->icons7x7, maxstate);
            currlayer->iconpixels15x15 = GetIconPixels(oldlayer->icons15x15, maxstate);
            currlayer->iconpixels31x31 = GetIconPixels(oldlayer->icons31x31, maxstate);
        }
    } else {
        // set new layer's colors+icons to default colors+icons for current algo+rule
        UpdateLayerColors();
    }
    
    numlayers++;
    
    if (numlayers > 1) {
        //!!! if (tilelayers && numlayers > 1) CreateTiles();
        //!!! CurrentLayerChanged();
    }
}

// -----------------------------------------------------------------------------

void CloneLayer()
{
    if (numlayers >= MAX_LAYERS) return;
    
    if (generating) Warning("Bug detected in CloneLayer!");
    
    cloning = true;
    AddLayer();
    cloning = false;
}

// -----------------------------------------------------------------------------

void DuplicateLayer()
{
    if (numlayers >= MAX_LAYERS) return;
    
    if (generating) Warning("Bug detected in DuplicateLayer!");
    
    duplicating = true;
    AddLayer();
    duplicating = false;
}

// -----------------------------------------------------------------------------

/*!!!

void DeleteLayer()
{
    if (numlayers <= 1) return;
    
    if (generating) Warning("Bug detected in DeleteLayer!");
    
    // note that we don't need to ask to delete a clone
    if (currlayer->dirty && currlayer->cloneid == 0 &&
        askondelete && !SaveCurrentLayer()) return;
    
    // numlayers > 1
    if (tilelayers) DestroyTiles();
    
    SaveLayerSettings();
    
    delete currlayer;
    numlayers--;
    
    if (currindex < numlayers) {
        // shift left one or more layers
        for (int i = currindex; i < numlayers; i++)
            layer[i] = layer[i+1];
    }
    if (currindex > 0) currindex--;
    currlayer = layer[currindex];
    
    // remove toggle button at end of layer bar
    togglebutt[numlayers]->Show(false);
    layerbarptr->ResizeLayerButtons();
    
    // remove item from end of Layer menu
    mainptr->RemoveLayerItem();
    
    if (tilelayers && numlayers > 1) CreateTiles();
    
    CurrentLayerChanged();
}

// -----------------------------------------------------------------------------

void SetLayer(int index)
{
    if (currindex == index) return;
    if (index < 0 || index >= numlayers) return;
    
    SaveLayerSettings();
    currindex = index;
    currlayer = layer[currindex];
    CurrentLayerChanged();
}

// -----------------------------------------------------------------------------

bool CanSwitchLayer(int WXUNUSED(index))
{
    if (inscript) {
        // user can only switch layers if script has set the appropriate option
        return canswitch;
    } else {
        // user can switch to any layer
        return true;
    }
}

// -----------------------------------------------------------------------------

void SwitchToClickedTile(int index)
{
    if (inscript && !CanSwitchLayer(index)) {
        Warning("You cannot switch to another layer while this script is running.");
        return;
    }
    
    // switch current layer to clicked tile
    SetLayer(index);
    
    if (inscript) {
        // update window title, viewport and status bar
        inscript = false;
        mainptr->SetPatternTitle(wxEmptyString);
        mainptr->UpdatePatternAndStatus();
        inscript = true;
    }
}

// -----------------------------------------------------------------------------

void MoveLayer(int fromindex, int toindex)
{
    if (fromindex == toindex) return;
    if (fromindex < 0 || fromindex >= numlayers) return;
    if (toindex < 0 || toindex >= numlayers) return;
    
    SaveLayerSettings();
    
    if (fromindex > toindex) {
        Layer* savelayer = layer[fromindex];
        for (int i = fromindex; i > toindex; i--) layer[i] = layer[i - 1];
        layer[toindex] = savelayer;
    } else {
        // fromindex < toindex
        Layer* savelayer = layer[fromindex];
        for (int i = fromindex; i < toindex; i++) layer[i] = layer[i + 1];
        layer[toindex] = savelayer;
    }
    
    currindex = toindex;
    currlayer = layer[currindex];
    
    if (tilelayers && numlayers > 1) {
        DestroyTiles();
        CreateTiles();
    }
    
    CurrentLayerChanged();
}

// -----------------------------------------------------------------------------

void MoveLayerDialog()
{
    if (numlayers <= 1) return;
    
    wxString msg = _("Move the current layer to a new position:");
    if (currindex > 0) {
        msg += _("\n(enter 0 to make it the first layer)");
    }
    
    int newindex;
    if ( GetInteger(_("Move Layer"), msg,
                    currindex, 0, numlayers - 1, &newindex) ) {
        MoveLayer(currindex, newindex);
    }
}

// -----------------------------------------------------------------------------

void NameLayerDialog()
{    
    wxString oldname = currlayer->currname;
    wxString newname;
    if ( GetString(_("Name Layer"), _("Enter a new name for the current layer:"),
                   oldname, newname) &&
        !newname.IsEmpty() && oldname != newname ) {
        
        // inscript is false so no need to call SavePendingChanges
        // if (allowundo) SavePendingChanges();
        
        // show new name in main window's title bar;
        // also sets currlayer->currname and updates menu item
        mainptr->SetPatternTitle(newname);
        
        if (allowundo) {
            // note that currfile and savestart/dirty flags don't change here
            currlayer->undoredo->RememberNameChange(oldname, currlayer->currfile,
                                                    currlayer->savestart, currlayer->dirty);
        }
    }
}

!!!*/

// -----------------------------------------------------------------------------

void DeleteOtherLayers()
{
    if (inscript || numlayers <= 1) return;
    
    /*!!!
    if (askondelete) {
        // keep track of which unique clones have been seen;
        // we add 1 below to allow for cloneseen[0] (always false)
        const int maxseen = MAX_LAYERS/2 + 1;
        bool cloneseen[maxseen];
        for (int i = 0; i < maxseen; i++) cloneseen[i] = false;
        
        // for each dirty layer, except current layer and all of its clones,
        // ask user if they want to save changes
        int cid = layer[currindex]->cloneid;
        if (cid > 0) cloneseen[cid] = true;
        int oldindex = currindex;
        for (int i = 0; i < numlayers; i++) {
            // only ask once for each unique clone (cloneid == 0 for non-clone)
            cid = layer[i]->cloneid;
            if (i != oldindex && !cloneseen[cid]) {
                if (cid > 0) cloneseen[cid] = true;
                if (layer[i]->dirty) {
                    // temporarily turn off generating flag for SetLayer
                    bool oldgen = mainptr->generating;
                    mainptr->generating = false;
                    SetLayer(i);
                    if (!mainptr->SaveCurrentLayer()) {
                        // user hit Cancel so restore current layer and generating flag
                        SetLayer(oldindex);
                        mainptr->generating = oldgen;
                        mainptr->UpdateUserInterface(mainptr->IsActive());
                        return;
                    }
                    SetLayer(oldindex);
                    mainptr->generating = oldgen;
                }
            }
        }
    }
    
    // numlayers > 1
    if (tilelayers) DestroyTiles();
    
    SyncClones();
    
    // delete all layers except current layer;
    // we need to do this carefully because ~Layer() requires numlayers
    // and the layer array to be correct when deleting a cloned layer
    int i = numlayers;
    while (numlayers > 1) {
        i--;
        if (i != currindex) {
            delete layer[i];     // ~Layer() is called
            numlayers--;
            
            // may need to shift the current layer left one place
            if (i < numlayers) layer[i] = layer[i+1];
            
            // remove toggle button at end of layer bar
            togglebutt[numlayers]->Show(false);
            
            // remove item from end of Layer menu
            mainptr->RemoveLayerItem();
        }
    }
    
    layerbarptr->ResizeLayerButtons();
    
    currindex = 0;
    // currlayer doesn't change
    
    // update the only layer item
    mainptr->UpdateLayerItem(0);
    
    // update window title (may need to remove "=" prefix)
    mainptr->SetPatternTitle(wxEmptyString);
    
    // select LAYER_0 button (also deselects old button)
    layerbarptr->SelectButton(LAYER_0, true);
    
    mainptr->UpdateMenuItems(mainptr->IsActive());
    mainptr->UpdatePatternAndStatus();
    */
}

// -----------------------------------------------------------------------------

void MarkLayerDirty()
{
    // need to save starting pattern
    currlayer->savestart = true;
    
    // if script has reset dirty flag then don't change it; this makes sense
    // for scripts that call new() and then construct a pattern
    if (currlayer->stayclean) return;
    
    if (!currlayer->dirty) {
        currlayer->dirty = true;
        
        // pass in currname so UpdateLayerItem(currindex) gets called
        SetPatternTitle(currlayer->currname.c_str());
        
        if (currlayer->cloneid > 0) {
            // synchronize other clones
            for ( int i = 0; i < numlayers; i++ ) {
                Layer* cloneptr = layer[i];
                if (cloneptr != currlayer && cloneptr->cloneid == currlayer->cloneid) {
                    // set dirty flag and display asterisk in layer item
                    cloneptr->dirty = true;
                    //!!! UpdateLayerItem(i);
                }
            }
        }
    }
}

// -----------------------------------------------------------------------------

void MarkLayerClean(const char* title)
{
    currlayer->dirty = false;
    
    // if script is resetting dirty flag -- eg. via new() -- then don't allow
    // dirty flag to be set true for the remainder of the script; this is
    // nicer for scripts that construct a pattern (ie. running such a script
    // is equivalent to loading a pattern file)
    if (inscript) currlayer->stayclean = true;
    
    if (title[0] == 0) {
        // pass in currname so UpdateLayerItem(currindex) gets called
        SetPatternTitle(currlayer->currname.c_str());
    } else {
        // set currlayer->currname to title and call UpdateLayerItem(currindex)
        SetPatternTitle(title);
    }
    
    if (currlayer->cloneid > 0) {
        // synchronize other clones
        for ( int i = 0; i < numlayers; i++ ) {
            Layer* cloneptr = layer[i];
            if (cloneptr != currlayer && cloneptr->cloneid == currlayer->cloneid) {
                // reset dirty flag
                cloneptr->dirty = false;
                if (inscript) cloneptr->stayclean = true;
                
                // always allow clones to have different names
                // cloneptr->currname = currlayer->currname;
                
                // remove asterisk from layer name
                //!!! UpdateLayerItem(i);
            }
        }
    }
}

// -----------------------------------------------------------------------------

/*!!!

void ToggleSyncViews()
{
    syncviews = !syncviews;
    
    mainptr->UpdateUserInterface(mainptr->IsActive());
    mainptr->UpdatePatternAndStatus();
}
 
// -----------------------------------------------------------------------------
 
void ToggleSyncModes()
{
    syncmodes = !syncmodes;
 
    mainptr->UpdateUserInterface(mainptr->IsActive());
    mainptr->UpdatePatternAndStatus();
}

// -----------------------------------------------------------------------------

void ToggleStackLayers()
{
    stacklayers = !stacklayers;
    if (stacklayers && tilelayers) {
        tilelayers = false;
        layerbarptr->SelectButton(TILE_LAYERS, false);
        if (numlayers > 1) DestroyTiles();
    }
    layerbarptr->SelectButton(STACK_LAYERS, stacklayers);
    
    mainptr->UpdateUserInterface(mainptr->IsActive());
    if (inscript) {
        // always update viewport and status bar
        inscript = false;
        mainptr->UpdatePatternAndStatus();
        inscript = true;
    } else {
        mainptr->UpdatePatternAndStatus();
    }
}

// -----------------------------------------------------------------------------

void ToggleTileLayers()
{
    tilelayers = !tilelayers;
    if (tilelayers && stacklayers) {
        stacklayers = false;
        layerbarptr->SelectButton(STACK_LAYERS, false);
    }
    layerbarptr->SelectButton(TILE_LAYERS, tilelayers);
    
    if (tilelayers) {
        if (numlayers > 1) CreateTiles();
    } else {
        if (numlayers > 1) DestroyTiles();
    }
    
    mainptr->UpdateUserInterface(mainptr->IsActive());
    if (inscript) {
        // always update viewport and status bar
        inscript = false;
        mainptr->UpdatePatternAndStatus();
        inscript = true;
    } else {
        mainptr->UpdatePatternAndStatus();
    }
}

// -----------------------------------------------------------------------------

void UpdateCloneColors()
{
    if (currlayer->cloneid > 0) {
        int maxstate = currlayer->algo->NumCellStates() - 1;
        for (int i = 0; i < numlayers; i++) {
            Layer* cloneptr = layer[i];
            if (cloneptr != currlayer && cloneptr->cloneid == currlayer->cloneid) {
                cloneptr->fromrgb = currlayer->fromrgb;
                cloneptr->torgb = currlayer->torgb;
                for (int n = 0; n <= maxstate; n++) {
                    cloneptr->cellr[n] = currlayer->cellr[n];
                    cloneptr->cellg[n] = currlayer->cellg[n];
                    cloneptr->cellb[n] = currlayer->cellb[n];
                    cloneptr->colorref[n] = currlayer->colorref[n];
                }
                // use same icon pointers
                cloneptr->icons7x7 = currlayer->icons7x7;
                cloneptr->icons15x15 = currlayer->icons15x15;
                cloneptr->icons31x31 = currlayer->icons31x31;
                // use same pixel data
                cloneptr->iconpixels7x7 = currlayer->iconpixels7x7;
                cloneptr->iconpixels15x15 = currlayer->iconpixels15x15;
                cloneptr->iconpixels31x31 = currlayer->iconpixels31x31;
            }
        }
    }
}

!!!*/

// -----------------------------------------------------------------------------

static void DeleteIcons(Layer* layer)
{
    // delete given layer's existing icons
    if (layer->icons7x7) {
        for (int i = 0; i < 256; i++) CGImageRelease(layer->icons7x7[i]);
        free(layer->icons7x7);
        layer->icons7x7 = NULL;
    }
    if (layer->icons15x15) {
        for (int i = 0; i < 256; i++) CGImageRelease(layer->icons15x15[i]);
        free(layer->icons15x15);
        layer->icons15x15 = NULL;
    }
    if (layer->icons31x31) {
        for (int i = 0; i < 256; i++) CGImageRelease(layer->icons31x31[i]);
        free(layer->icons31x31);
        layer->icons31x31 = NULL;
    }
    
    // also delete icon pixel data
    if (layer->iconpixels7x7) {
        for (int i = 0; i < 256; i++) if (layer->iconpixels7x7[i]) free(layer->iconpixels7x7[i]);
        free(layer->iconpixels7x7);
        layer->iconpixels7x7 = NULL;
    }
    if (layer->iconpixels15x15) {
        for (int i = 0; i < 256; i++) if (layer->iconpixels15x15[i]) free(layer->iconpixels15x15[i]);
        free(layer->iconpixels15x15);
        layer->iconpixels15x15 = NULL;
    }
    if (layer->iconpixels31x31) {
        for (int i = 0; i < 256; i++) if (layer->iconpixels31x31[i]) free(layer->iconpixels31x31[i]);
        free(layer->iconpixels31x31);
        layer->iconpixels31x31 = NULL;
    }
}

// -----------------------------------------------------------------------------

static bool FindIconFile(const std::string& rule, const std::string& dir, std::string& path)
{
    const std::string extn = ".icons";
    
    // first look for rule.icons in given directory
    path = dir + rule;
    path += extn;
    if (FileExists(path)) return true;
    
    // if rule has the form foo-* then look for foo.icons in dir;
    // this allows related rules to share a single .icons file
    size_t hyphenpos = rule.rfind('-');
    if (hyphenpos != std::string::npos) {
        std::string prefix = rule.substr(0, hyphenpos);
        path = dir + prefix;
        path += extn;
        if (FileExists(path)) return true;
    }
    
    return false;
}

// -----------------------------------------------------------------------------

static bool LoadRuleIcons(const std::string& rule, int maxstate)
{
    // delete current layer's existing icons
    DeleteIcons(currlayer);
    
    // all of Golly's default icons are monochrome, but LoadIconFile might load
    // a multi-colored icon and set this flag to true
    currlayer->multicoloricons = false;
    
    // if rule.icons file exists in userrules or rulesdir then load icons for current layer
    std::string path;
    return (FindIconFile(rule, userrules, path) ||
            FindIconFile(rule, rulesdir, path)) &&
                LoadIconFile(path, maxstate, &currlayer->icons7x7,
                                             &currlayer->icons15x15,
                                             &currlayer->icons31x31);
}

// -----------------------------------------------------------------------------

static void SetAverageColor(int state, int numpixels, unsigned char* pxldata)
{
    // set non-icon color to average color of non-black pixels in given pixel data
    // which contains the icon bitmap in RGBA pixel format
    int byte = 0;
    int nbcount = 0;  // # of non-black pixels
    int totalr = 0;
    int totalg = 0;
    int totalb = 0;
    for (int i = 0; i < numpixels; i++) {
        unsigned char r = pxldata[byte];
        unsigned char g = pxldata[byte+1];
        unsigned char b = pxldata[byte+2];
        if (r || g || b) {
            // non-black pixel
            totalr += r;
            totalg += g;
            totalb += b;
            nbcount++;
        }
        byte += 4;
    }
    
    if (nbcount > 0) {
        currlayer->cellr[state] = int(totalr / nbcount);
        currlayer->cellg[state] = int(totalg / nbcount);
        currlayer->cellb[state] = int(totalb / nbcount);
    } else {
        // avoid div0
        currlayer->cellr[state] = 0;
        currlayer->cellg[state] = 0;
        currlayer->cellb[state] = 0;
    }
}

// -----------------------------------------------------------------------------

static FILE* FindColorFile(const std::string& rule, const std::string& dir)
{
    const std::string extn = ".colors";
    std::string path;
    
    // first look for rule.colors in given directory
    path = dir + rule;
    path += extn;
    FILE* f = fopen(path.c_str(), "r");
    if (f) return f;
    
    // if rule has the form foo-* then look for foo.colors in dir;
    // this allows related rules to share a single .colors file
    size_t hyphenpos = rule.rfind('-');
    if (hyphenpos != std::string::npos) {
        std::string prefix = rule.substr(0, hyphenpos);
        path = dir + prefix;
        path += extn;
        f = fopen(path.c_str(), "r");
        if (f) return f;
    }
    
    return NULL;
}

// -----------------------------------------------------------------------------

static bool LoadRuleColors(const std::string& rule, int maxstate)
{
    // if rule.colors file exists in userrules or rulesdir then
    // change colors according to info in file
    FILE* f = FindColorFile(rule, userrules);
    if (!f) f = FindColorFile(rule, rulesdir);
    if (f) {
        // the linereader class handles all line endings (CR, CR+LF, LF)
        linereader reader(f);
        // not needed here, but useful if we ever return early due to error
        // reader.setcloseonfree();
        const int MAXLINELEN = 512;
        char buf[MAXLINELEN + 1];
        while (reader.fgets(buf, MAXLINELEN) != 0) {
            if (buf[0] == '#' || buf[0] == 0) {
                // skip comment or empty line
            } else {
                // look for "color" or "gradient" keyword at start of line
                char* keyword = buf;
                char* value;
                while (*keyword == ' ') keyword++;
                value = keyword;
                while (*value >= 'a' && *value <= 'z') value++;
                while (*value == ' ' || *value == '=') value++;
                if (strncmp(keyword, "color", 5) == 0) {
                    int state, r, g, b;
                    if (sscanf(value, "%d%d%d%d", &state, &r, &g, &b) == 4) {
                        if (state >= 0 && state <= maxstate) {
                            currlayer->cellr[state] = r;
                            currlayer->cellg[state] = g;
                            currlayer->cellb[state] = b;
                        }
                    };
                } else if (strncmp(keyword, "gradient", 8) == 0) {
                    int r1, g1, b1, r2, g2, b2;
                    if (sscanf(value, "%d%d%d%d%d%d", &r1, &g1, &b1, &r2, &g2, &b2) == 6) {
                        SetColor(currlayer->fromrgb, r1, g1, b1);
                        SetColor(currlayer->torgb, r2, g2, b2);
                        CreateColorGradient();
                    };
                }
            }
        }
        reader.close();
        return true;
    }
    return false;
}

// -----------------------------------------------------------------------------

void CreateColorGradient()
{
    int maxstate = currlayer->algo->NumCellStates() - 1;
    unsigned char r1 = currlayer->fromrgb.r;
    unsigned char g1 = currlayer->fromrgb.g;
    unsigned char b1 = currlayer->fromrgb.b;
    unsigned char r2 = currlayer->torgb.r;
    unsigned char g2 = currlayer->torgb.g;
    unsigned char b2 = currlayer->torgb.b;
    
    // set cell colors for states 1..maxstate using a color gradient
    // starting with r1,g1,b1 and ending with r2,g2,b2
    currlayer->cellr[1] = r1;
    currlayer->cellg[1] = g1;
    currlayer->cellb[1] = b1;
    if (maxstate > 2) {
        int N = maxstate - 1;
        double rfrac = (double)(r2 - r1) / (double)N;
        double gfrac = (double)(g2 - g1) / (double)N;
        double bfrac = (double)(b2 - b1) / (double)N;
        for (int n = 1; n < N; n++) {
            currlayer->cellr[n+1] = (int)(r1 + n * rfrac + 0.5);
            currlayer->cellg[n+1] = (int)(g1 + n * gfrac + 0.5);
            currlayer->cellb[n+1] = (int)(b1 + n * bfrac + 0.5);
        }
    }
    if (maxstate > 1) {
        currlayer->cellr[maxstate] = r2;
        currlayer->cellg[maxstate] = g2;
        currlayer->cellb[maxstate] = b2;
    }
}

// -----------------------------------------------------------------------------

void UpdateCurrentColors()
{
    // set current layer's colors and icons according to current algo and rule
    AlgoData* ad = algoinfo[currlayer->algtype];
    int maxstate = currlayer->algo->NumCellStates() - 1;
    
    // copy default colors from current algo
    currlayer->fromrgb = ad->fromrgb;
    currlayer->torgb = ad->torgb;
    if (ad->gradient) {
        CreateColorGradient();
        // state 0 is not part of the gradient
        currlayer->cellr[0] = ad->algor[0];
        currlayer->cellg[0] = ad->algog[0];
        currlayer->cellb[0] = ad->algob[0];
    } else {
        for (int n = 0; n <= maxstate; n++) {
            currlayer->cellr[n] = ad->algor[n];
            currlayer->cellg[n] = ad->algog[n];
            currlayer->cellb[n] = ad->algob[n];
        }
    }
    
    std::string rule = currlayer->algo->getrule();
    // replace any '\' and '/' chars with underscores;
    // ie. given 12/34/6 we look for 12_34_6.{colors|icons}
    std::replace(rule.begin(), rule.end(), '\\', '_');
    std::replace(rule.begin(), rule.end(), '/', '_');
    
    // strip off any suffix like ":T100,200" used to specify a bounded grid
    size_t colonpos = rule.find(':');
    if (colonpos != std::string::npos) rule = rule.substr(0, colonpos);
    
    // if rule.colors file exists then override default colors
    bool loadedcolors = LoadRuleColors(rule, maxstate);
    
    // if rule.icons file exists then use those icons
    if ( !LoadRuleIcons(rule, maxstate) ) {
        if (currlayer->algo->getgridtype() == lifealgo::HEX_GRID) {
            // use hexagonal icons
            currlayer->icons7x7 = CopyIcons(hexicons7x7, maxstate);
            currlayer->icons15x15 = CopyIcons(hexicons15x15, maxstate);
            currlayer->icons31x31 = CopyIcons(hexicons31x31, maxstate);
        } else if (currlayer->algo->getgridtype() == lifealgo::VN_GRID) {
            // use diamond-shaped icons for 4-neighbor von Neumann neighborhood
            currlayer->icons7x7 = CopyIcons(vnicons7x7, maxstate);
            currlayer->icons15x15 = CopyIcons(vnicons15x15, maxstate);
            currlayer->icons31x31 = CopyIcons(vnicons31x31, maxstate);
        } else {
            // otherwise copy default icons from current algo
            currlayer->icons7x7 = CopyIcons(ad->icons7x7, maxstate);
            currlayer->icons15x15 = CopyIcons(ad->icons15x15, maxstate);
            currlayer->icons31x31 = CopyIcons(ad->icons31x31, maxstate);
        }
    }

    // get icon pixel data (mainly used for rendering, but also useful below)
    currlayer->iconpixels7x7 = GetIconPixels(currlayer->icons7x7, maxstate);
    currlayer->iconpixels15x15 = GetIconPixels(currlayer->icons15x15, maxstate);
    currlayer->iconpixels31x31 = GetIconPixels(currlayer->icons31x31, maxstate);
    
    // if rule.colors file wasn't loaded and icons are multi-color then we
    // set non-icon colors to the average of the non-black pixels in each icon
    // (note that we use the 7x7 icons because they are faster to scan)
    unsigned char** iconpixels = currlayer->iconpixels7x7;
    if (!loadedcolors && iconpixels && currlayer->multicoloricons) {
        for (int n = 1; n <= maxstate; n++) {
            if (iconpixels[n]) SetAverageColor(n, 7*7, iconpixels[n]);
        }
        // if a 15x15 icon is supplied in the 0th position then use
        // its top left pixel to set the state 0 color
        iconpixels = currlayer->iconpixels15x15;
        if (iconpixels && iconpixels[0]) {
            unsigned char* pxldata = iconpixels[0];
            // pxldata contains the icon bitmap in RGBA pixel format
            currlayer->cellr[0] = pxldata[0];
            currlayer->cellg[0] = pxldata[1];
            currlayer->cellb[0] = pxldata[2];
        }
    }
    
    if (swapcolors) {
        // invert cell colors in current layer
        for (int n = 0; n <= maxstate; n++) {
            currlayer->cellr[n] = 255 - currlayer->cellr[n];
            currlayer->cellg[n] = 255 - currlayer->cellg[n];
            currlayer->cellb[n] = 255 - currlayer->cellb[n];
        }
    }
    
    UpdateColorReferences(currlayer, currlayer->algo->NumCellStates());
}

// -----------------------------------------------------------------------------

void UpdateLayerColors()
{
    UpdateCurrentColors();
    
    // if current layer has clones then update their colors
    //!!! UpdateCloneColors();
}

// -----------------------------------------------------------------------------

void InvertCellColors()
{
    // invert cell colors in all layers
    for (int i = 0; i < numlayers; i++) {
        Layer* layerptr = layer[i];
        // do NOT use layerptr->algo->... here -- it might not be correct
        // for a non-current layer (but we can use layerptr->algtype)
        for (int n = 0; n < algoinfo[layerptr->algtype]->maxstates; n++) {
            layerptr->cellr[n] = 255 - layerptr->cellr[n];
            layerptr->cellg[n] = 255 - layerptr->cellg[n];
            layerptr->cellb[n] = 255 - layerptr->cellb[n];
        }
        UpdateColorReferences(layerptr, algoinfo[layerptr->algtype]->maxstates);
    }
}

// -----------------------------------------------------------------------------

Layer* GetLayer(int index)
{
    if (index < 0 || index >= numlayers) {
        Warning("Bad index in GetLayer!");
        return NULL;
    } else {
        return layer[index];
    }
}

// -----------------------------------------------------------------------------

int GetUniqueCloneID()
{
    // find first available index (> 0) to use as cloneid
    for (int i = 1; i < MAX_LAYERS; i++) {
        if (cloneavail[i]) {
            cloneavail[i] = false;
            return i;
        }
    }
    // bug if we get here
    Warning("Bug in GetUniqueCloneID!");
    return 1;
}

// -----------------------------------------------------------------------------

Layer::Layer()
{
    if (!cloning) {
        // use a unique temporary file for saving starting patterns
        tempstart = CreateTempFileName("golly_start_");
    }
    
    dirty = false;                // user has not modified pattern
    savedirty = false;            // in case script created layer
    stayclean = inscript;         // if true then keep the dirty flag false
                                  // for the duration of the script
    savestart = false;            // no need to save starting pattern
    startgen = 0;                 // initial starting generation
    startfile.clear();            // no starting pattern
    currname = "untitled";        // initial window title
    currfile.clear();             // no pattern file has been loaded
    originx = 0;                  // no X origin offset
    originy = 0;                  // no Y origin offset
    
    icons7x7 = NULL;              // no 7x7 icons
    icons15x15 = NULL;            // no 15x15 icons
    icons31x31 = NULL;            // no 31x31 icons

    iconpixels7x7 = NULL;         // no pixel data for 7x7 icons
    iconpixels15x15 = NULL;       // no pixel data for 15x15 icons
    iconpixels31x31 = NULL;       // no pixel data for 31x31 icons
    
    // init color references
    for (int n = 0; n < 256; n++) {
        colorref[n] = NULL;
    }
    
    currframe = 0;                // first frame in timeline
    autoplay = 0;                 // not playing
    tlspeed = 0;                  // default speed for autoplay
    lastframe = 0;                // no frame displayed
        
    // create viewport; the initial size is not important because it will soon change
    view = new viewport(100,100);
    if (view == NULL) Fatal("Failed to create viewport!");
    
    if (numlayers == 0) {
        // creating very first layer (can't be a clone)
        cloneid = 0;
        
        // initialize cloneavail array (cloneavail[0] is never used)
        cloneavail[0] = false;
        for (int i = 1; i < MAX_LAYERS; i++) cloneavail[i] = true;
        
        // set some options using initial values stored in prefs file
        algtype = initalgo;
        hyperspeed = inithyperspeed;
        showhashinfo = initshowhashinfo;
        autofit = initautofit;
        
        // initial base step and exponent
        currbase = algoinfo[algtype]->defbase;
        currexpo = 0;
        
        // create empty universe
        algo = CreateNewUniverse(algtype);
        
        // set rule using initrule stored in prefs file
        const char* err = algo->setrule(initrule);
        if (err) {
            // user might have edited rule in prefs file, or deleted table/tree file
            algo->setrule( algo->DefaultRule() );
        }
        
        // don't need to remember rule here (SaveLayerSettings will do it)
        rule.clear();
        
        // initialize undo/redo history
        undoredo = new UndoRedo();
        if (undoredo == NULL) Fatal("Failed to create new undo/redo object!");
        
        touchmode = drawmode;
        drawingstate = 1;
        
    } else {
        // adding a new layer after currlayer (see AddLayer)
        
        // inherit current universe type and other settings
        algtype = currlayer->algtype;
        hyperspeed = currlayer->hyperspeed;
        showhashinfo = currlayer->showhashinfo;
        autofit = currlayer->autofit;
        
        // initial base step and exponent
        currbase = algoinfo[algtype]->defbase;
        currexpo = 0;
        
        if (cloning) {
            if (currlayer->cloneid == 0) {
                // first time this universe is being cloned so need a unique cloneid
                cloneid = GetUniqueCloneID();
                currlayer->cloneid = cloneid;    // current layer also becomes a clone
                numclones += 2;
            } else {
                // we're cloning an existing clone
                cloneid = currlayer->cloneid;
                numclones++;
            }
            
            // clones share the same universe and undo/redo history
            algo = currlayer->algo;
            undoredo = currlayer->undoredo;
            
            // clones also share the same timeline
            currframe = currlayer->currframe;
            autoplay = currlayer->autoplay;
            tlspeed = currlayer->tlspeed;
            lastframe = currlayer->lastframe;
            
            // clones use same name for starting file
            tempstart = currlayer->tempstart;
            
        } else {
            // this layer isn't a clone
            cloneid = 0;
            
            // create empty universe
            algo = CreateNewUniverse(algtype);
            
            // use current rule
            const char* err = algo->setrule(currlayer->algo->getrule());
            if (err) {
                // table/tree file might have been deleted
                algo->setrule( algo->DefaultRule() );
            }
            
            // initialize undo/redo history
            undoredo = new UndoRedo();
            if (undoredo == NULL) Fatal("Failed to create new undo/redo object!");
        }
        
        // inherit current rule
        rule = currlayer->algo->getrule();
        
        // inherit current viewport's size, scale and location
        view->resize( currlayer->view->getwidth(), currlayer->view->getheight() );
        view->setpositionmag( currlayer->view->x, currlayer->view->y,
                             currlayer->view->getmag() );
        
        // inherit current touch mode and drawing state
        touchmode = currlayer->touchmode;
        drawingstate = currlayer->drawingstate;
        
        if (cloning || duplicating) {
            // duplicate all the other current settings
            currname = currlayer->currname;
            dirty = currlayer->dirty;
            savedirty = currlayer->savedirty;
            stayclean = currlayer->stayclean;
            currbase = currlayer->currbase;
            currexpo = currlayer->currexpo;
            autofit = currlayer->autofit;
            hyperspeed = currlayer->hyperspeed;
            showhashinfo = currlayer->showhashinfo;
            originx = currlayer->originx;
            originy = currlayer->originy;
            
            // duplicate selection info
            currsel = currlayer->currsel;
            savesel = currlayer->savesel;
            
            // duplicate the stuff needed to reset pattern
            savestart = currlayer->savestart;
            startalgo = currlayer->startalgo;
            startdirty = currlayer->startdirty;
            startname = currlayer->startname;
            startrule = currlayer->startrule;
            startx = currlayer->startx;
            starty = currlayer->starty;
            startbase = currlayer->startbase;
            startexpo = currlayer->startexpo;
            startmag = currlayer->startmag;
            startgen = currlayer->startgen;
            startfile = currlayer->startfile;
            currfile = currlayer->currfile;
            startsel = currlayer->startsel;
        }
        
        if (duplicating) {
            // first set same gen count
            algo->setGeneration( currlayer->algo->getGeneration() );
            
            // duplicate pattern
            if ( !currlayer->algo->isEmpty() ) {
                bigint top, left, bottom, right;
                currlayer->algo->findedges(&top, &left, &bottom, &right);
                if ( OutsideLimits(top, left, bottom, right) ) {
                    Warning("Pattern is too big to duplicate.");
                } else {
                    CopyRect(top.toint(), left.toint(), bottom.toint(), right.toint(),
                             currlayer->algo, algo, false, "Duplicating layer");
                }
            }
            
            // tempstart file must remain unique in duplicate layer
            if ( FileExists(currlayer->tempstart) ) {
                if ( !CopyFile(currlayer->tempstart, tempstart) ) {
                    Warning("Could not copy tempstart file!");
                }
            }
            
            if (currlayer->startfile == currlayer->tempstart) {
                startfile = tempstart;
            }
            if (currlayer->currfile == currlayer->tempstart) {
                // starting pattern came from clipboard or lexicon pattern
                currfile = tempstart;
            }
            
            if (allowundo) {
                // duplicate current undo/redo history in new layer
                undoredo->DuplicateHistory(currlayer, this);
            }
        }
    }
}

// -----------------------------------------------------------------------------

Layer::~Layer()
{
    // delete stuff allocated in ctor
    delete view;
    
    if (cloneid > 0) {
        // this layer is a clone, so count how many layers have the same cloneid
        int clonecount = 0;
        for (int i = 0; i < numlayers; i++) {
            if (layer[i]->cloneid == cloneid) clonecount++;
            
            // tell undo/redo which clone is being deleted
            if (this == layer[i]) undoredo->DeletingClone(i);
        }     
        if (clonecount > 2) {
            // only delete this clone
            numclones--;
        } else {
            // first make this cloneid available for the next clone
            cloneavail[cloneid] = true;
            // reset the other cloneid to 0 (should only be one such clone)
            for (int i = 0; i < numlayers; i++) {
                // careful -- layer[i] might be this layer
                if (this != layer[i] && layer[i]->cloneid == cloneid)
                    layer[i]->cloneid = 0;
            }
            numclones -= 2;
        }
        
    } else {
        // this layer is not a clone, so delete universe and undo/redo history
        delete algo;
        delete undoredo;
        
        // delete tempstart file if it exists
        if (FileExists(tempstart)) RemoveFile(tempstart);

        // release color references
        for (int i = 0; i < 256; i++) CGColorRelease(colorref[i]);

        // delete any icons
        DeleteIcons(this);
    }
}
