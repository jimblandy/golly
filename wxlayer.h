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

// Golly supports multiple layers.  Each layer is a separate universe
// with its own algorithm, viewport, rule, window title, etc.

class bigint;
class lifealgo;
class viewport;

class Layer {
public:
   Layer();
   ~Layer();

   lifealgo* lalgo;           // this layer's universe
   bool lhash;                // does it use hlifealgo?
   viewport* lview;           // this layer's viewport
   wxString lrule;            // this layer's rule
   wxCursor* lcurs;           // this layer's cursor
   int lwarp;                 // this layer's speed setting
   
   // this layer's selection
   bigint ltop, lleft, lbottom, lright;
   
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
   
   // temporary file used to restore starting pattern or to show comments
   wxString tempstart;
};

const int maxlayers = 10;     // maximum number of layers
extern int numlayers;         // number of existing layers
extern int currindex;         // index of current layer (0..numlayers-1)
extern Layer* currlayer;      // pointer to current layer

//!!! document these:
void AddLayer();
void DeleteLayer();
void DeleteOtherLayers();
void MoveLayerDialog();
void SetLayer(int index);
void ToggleDrawLayers();
void ToggleGenLayers();
void ResizeLayers(int wd, int ht);
Layer* GetLayer(int index);

#endif
