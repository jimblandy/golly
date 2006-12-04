                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2006 Andrew Trevorrow and Tomas Rokicki.

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
#ifndef _WXLAYER_H_
#define _WXLAYER_H_

class bigint;
class lifealgo;
class viewport;

// Golly supports multiple layers.  Each layer is a separate universe
// with its own algorithm, rule, viewport, window title, selection, etc.

class Layer {
public:
   Layer();
   ~Layer();

   lifealgo* algo;            // this layer's universe
   bool hash;                 // does it use hlife?
   viewport* view;            // viewport for displaying patterns
   wxCursor* curs;            // cursor mode
   int warp;                  // speed setting

   // WARNING: this layer's rule is only guaranteed to be correct AFTER
   // switching to another layer, so use global_liferules.getrule() or
   // currlayer->algo->getrule() rather than currlayer->rule
   wxString rule;
   
   // selection edges
   bigint seltop, selbottom, selleft, selright;

   bigint originx;            // X origin offset
   bigint originy;            // Y origin offset
   
   wxString currfile;         // full path of current pattern file
   wxString currname;         // name seen in window title and Layer menu

   // for saving and restoring starting pattern
   bool savestart;            // need to save starting pattern?
   bool starthash;            // hashing was on at start?
   wxString startfile;        // file for saving starting pattern
   wxString startrule;        // starting rule
   bigint startgen;           // starting generation (>= 0)
   bigint startx, starty;     // starting location
   int startwarp;             // starting speed
   int startmag;              // starting scale
   
   // temporary file used to restore starting pattern or to show comments;
   // each layer uses a different file name
   wxString tempstart;
};

const int maxlayers = 10;     // maximum number of layers
extern int numlayers;         // number of existing layers
extern int currindex;         // index of current layer (0..numlayers-1)
extern Layer* currlayer;      // pointer to current layer

void AddLayer();
// Add a new layer (with an empty universe) and make it the current layer.
// The first call creates the initial layer.  Later calls insert the
// new layer immediately after the old current layer.  The new layer
// inherits the same type of universe, the same rule, the same scale
// and location, and the same cursor.

void DeleteLayer();
// Delete the current layer.  The current layer changes to the previous
// index, unless layer 0 was deleted, in which case the current layer
// is the new layer at index 0.

void DeleteOtherLayers();
// Delete all layers except the current layer.

void MoveLayerDialog();
// Bring up a dialog that allows the user to move a specified layer
// to another index which then becomes the current layer.

void SetLayer(int index);
// Set the current layer using the given index.

void ToggleLayerBar();
// Show/hide the layer bar.

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

void ResizeLayers(int wd, int ht);
// Resize the viewport in all layers.

Layer* GetLayer(int index);
// Return a pointer to the layer specified by the given index.

#endif
