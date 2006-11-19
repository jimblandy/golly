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

   lifealgo* lalgo;     // this layer's universe
   bool lhash;          // this layer uses hlifealgo?
   viewport* lview;     // this layer's viewport
   wxString lrule;      // this layer's rule
   wxString ltitle;     // this layer's window title
   
   // this layer's selection
   bigint ltop, lleft, lbottom, lright;
   
   //!!! startgen, startfile, warp, etc???
};

const int maxlayers = 10;     // maximum number of layers
extern int currlayer;         // index of current layer
extern int numlayers;         // number of existing layers

//!!! document these:
void AddLayer();
void DeleteLayer();
void DeleteOtherLayers();
void MoveLayerDialog();
void SetLayer(int index);
void ToggleDrawLayers();
void ToggleGenLayers();
Layer* GetLayer(int index);

#endif
