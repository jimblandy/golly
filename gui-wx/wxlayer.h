// This file is part of Golly.
// See docs/License.html for the copyright notice.

#ifndef _WXLAYER_H_
#define _WXLAYER_H_

#include "bigint.h"           // for bigint class
#include "viewport.h"         // for viewport class
#include "wxselect.h"         // for Selection class
#include "wxundo.h"           // for UndoRedo class
#include "wxalgos.h"          // for algo_type

// Golly supports multiple layers.  Each layer is a separate universe
// (unless cloned) with its own algorithm, rule, viewport, window title,
// selection, undo/redo history, etc.

class Layer {
public:
    Layer();
    ~Layer();
    
    // if this is a cloned layer then cloneid is > 0 and all the other clones
    // have the same cloneid
    int cloneid;
    
    lifealgo* algo;               // this layer's universe (shared by clones)
    algo_type algtype;            // type of universe (index into algoinfo)
    bool hyperspeed;              // use acceleration while generating?
    bool showhashinfo;            // show hashing info?
    bool autofit;                 // auto fit pattern while generating?
    bool dirty;                   // user has modified pattern?
    bool savedirty;               // state of dirty flag before drawing/script change
    bool stayclean;               // script has reset dirty flag?
    int currbase;                 // current base step
    int currexpo;                 // current step exponent
    int drawingstate;             // current drawing state
    wxCursor* curs;               // current cursor
    UndoRedo* undoredo;           // undo/redo history (shared by clones)
    
    // each layer (cloned or not) has its own viewport for displaying patterns;
    // note that we use a pointer to the viewport to allow temporary switching
    // in wxrender.cpp (eg. in DrawStackedLayers)
    viewport* view;
    
    // WARNING: this string is used to remember the current rule when
    // switching to another layer; to determine the current rule at any
    // time, use currlayer->algo->getrule()
    wxString rule;
    
    Selection currsel;            // current selection
    Selection savesel;            // for saving/restoring selection
    
    bigint originx;               // X origin offset
    bigint originy;               // Y origin offset
    
    wxString currfile;            // full path of current pattern file
    wxString currname;            // name seen in window title and Layer menu
    
    // for saving and restoring starting pattern
    algo_type startalgo;          // starting algorithm
    bool savestart;               // need to save starting pattern?
    bool startdirty;              // starting state of dirty flag
    wxString startname;           // starting currname
    wxString startrule;           // starting rule
    bigint startgen;              // starting generation (>= 0)
    bigint startx, starty;        // starting location
    int startbase;                // starting base step
    int startexpo;                // starting step exponent
    int startmag;                 // starting scale
    Selection startsel;           // starting selection
    
    // temporary file used to restore starting pattern or to show comments;
    // each non-cloned layer uses a different temporary file
    wxString tempstart;
    
    // used when tilelayers is true
    PatternView* tilewin;         // tile window
    wxRect tilerect;              // tile window's size and position
    
    // color scheme for this layer
    wxColor fromrgb;              // start of gradient
    wxColor torgb;                // end of gradient
    unsigned char cellr[256];     // red components for states 0..255
    unsigned char cellg[256];     // green components for states 0..255
    unsigned char cellb[256];     // blue components for states 0..255
    
    // icons for this layer
    wxBitmap** icons7x7;          // icons for scale 1:8
    wxBitmap** icons15x15;        // icons for scale 1:16
    wxBitmap** icons31x31;        // icons for scale 1:32

    // texture atlases for rendering icons
    unsigned char* atlas7x7;      // atlas for 7x7 icons
    unsigned char* atlas15x15;    // atlas for 15x15 icons
    unsigned char* atlas31x31;    // atlas for 31x31 icons
    
    int numicons;                 // number of icons (= number of live states)
    bool multicoloricons;         // are icons multi-colored? (grayscale if not)
    
    // used if the layer has a timeline (see wxtimeline.cpp)
    int currframe;                // current frame in timeline
    int autoplay;                 // +ve = play forwards, -ve = play backwards, 0 = stop
    int tlspeed;                  // controls speed at which frames are played
};

const int MAX_LAYERS = 10;  // maximum number of layers
extern int numlayers;       // number of existing layers
extern int numclones;       // number of cloned layers
extern int currindex;       // index of current layer (0..numlayers-1)
extern Layer* currlayer;    // pointer to current layer

Layer* GetLayer(int index);
// Return a pointer to the layer specified by the given index.

void AddLayer();
// Add a new layer (with an empty universe) and make it the current layer.
// The first call creates the initial layer.  Later calls insert the
// new layer immediately after the old current layer.  The new layer
// inherits the same type of universe, the same rule, the same scale
// and location, and the same cursor.

void CloneLayer();
// Like AddLayer but shares the same universe and undo/redo history
// as the current layer.  All the current layer's settings are duplicated
// and most will be kept synchronized; ie. changing one clone changes all
// the others, but each cloned layer has a separate viewport so the same
// pattern can be viewed at different scales and locations.

void SyncClones();
// If the current layer is a clone then this call ensures the universe
// and settings in its other clones are correctly synchronized.

void DuplicateLayer();
// Like AddLayer but makes a copy of the current layer's pattern.
// Also duplicates all the current settings but, unlike a cloned layer,
// the settings are not kept synchronized.

void DeleteLayer();
// Delete the current layer.  The current layer changes to the previous
// index, unless layer 0 was deleted, in which case the current layer
// is the new layer at index 0.

void DeleteOtherLayers();
// Delete all layers except the current layer.

void SetLayer(int index);
// Set the current layer using the given index (0..numlayers-1).

void MoveLayer(int fromindex, int toindex);
// Move the layer at fromindex to toindex and make it the current layer.
// This is called by MoveLayerDialog and the movelayer script command.

void MoveLayerDialog();
// Bring up a dialog that allows the user to move the current layer
// to a new index (which becomes the current layer).

void NameLayerDialog();
// Bring up a dialog that allows the user to change the name of the
// current layer.

void MarkLayerDirty();
// Set dirty flag in current layer and any of its clones.
// Also prefix window title and layer name(s) with an asterisk.

void MarkLayerClean(const wxString& title);
// Reset dirty flag in current layer and any of its clones.
// Also set window title, removing asterisk from it and layer name(s).

void ToggleSyncViews();
// Toggle the syncviews flag.  When true, every layer uses the same
// scale and location as the current layer.

void ToggleSyncCursors();
// Toggle the synccursors flag.  When true, every layer uses the same
// cursor as the current layer.

void ToggleStackLayers();
// Toggle the stacklayers flag.  When true, the rendering code displays
// all layers using the same scale and location as the current layer.
// Layer 0 is drawn first (100% opaque), then all the other layers
// are drawn as overlays using an opacity set by the user.  The colors
// for the live cells in each layer can be set via the Prefs dialog.

void ToggleTileLayers();
// Toggle the tilelayers flag.  When true, the rendering code displays
// all layers in tiled sub-windows.

bool CanSwitchLayer(int index);
// Return true if the user can switch to the given layer.

void SwitchToClickedTile(int index);
// If allowed, change current layer to clicked tile.

void ResizeLayers(int wd, int ht);
// Resize the viewport in all layers.

bool RestoreRule(const wxString& rule);
// Try to set the current layer's rule to a previously known rule.
// If this succeeds return true, but if it fails then warn the user,
// switch to the current algorithm's default rule, and return false.
// The latter can happen if the given rule's table/tree file was
// deleted or was edited and some sort of error introduced.

Layer* CreateTemporaryLayer();
// Create a temporary layer with the same algo type as currlayer.


// Layer bar routines:

void CreateLayerBar(wxWindow* parent);
// Create layer bar window at top of given parent window.

int LayerBarHeight();
// Return height of layer bar.

void ResizeLayerBar(int wd);
// Change width of layer bar.

void UpdateLayerBar();
// Update state of buttons in layer bar.

void UpdateLayerButton(int index, const wxString& name);
// Update the name in the specified layer button.

void RedrawLayerBar();
// Redraw layer bar.  This is needed because UpdateLayerBar doesn't
// refresh the layer bar (to avoid unwanted flashing).

void ToggleLayerBar();
// Show/hide the layer bar.


// Color handling routines:

void CreateColorGradient();
// Create a color gradient for the current layer using
// currlayer->fromrgb and currlayer->torgb.

void UpdateIconColors();
// Update the icon texture atlases for the current layer.
// Must be called BEFORE calling UpdateCloneColors.

void UpdateCloneColors();
// If current layer has clones then update their colors.

void UpdateLayerColors();
// Update the cell colors and icons for the current layer (and its clones)
// according to the current algo and rule.  Must be called very soon
// after any algo/rule change, and before the viewport is updated.

void InvertCellColors();
// Invert the cell colors in all layers, including the dead cell color,
// grid lines, and the colors in all icons.

void InvertIconColors(unsigned char* atlasptr, int iconsize, int numicons);
// Invert the icon colors in the given texture atlas.

void SetLayerColors();
// Open a dialog to change the current layer's colors.

#endif
