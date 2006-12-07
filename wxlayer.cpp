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

#include "wx/wxprec.h"     // for compilers that support precompilation
#ifndef WX_PRECOMP
   #include "wx/wx.h"      // for all others include the necessary headers
#endif

#include "bigint.h"
#include "lifealgo.h"
#include "qlifealgo.h"
#include "hlifealgo.h"
#include "viewport.h"

#include "wxgolly.h"       // for wxGetApp, mainptr, viewptr, statusptr
#include "wxmain.h"        // for mainptr->...
#include "wxview.h"        // for viewptr->...
#include "wxstatus.h"      // for statusptr->...
#include "wxutils.h"       // for Warning, FillRect
#include "wxprefs.h"       // for gollydir, inithash, initrule, etc
#include "wxscript.h"      // for inscript
#include "wxlayer.h"

// -----------------------------------------------------------------------------

int numlayers = 0;            // number of existing layers
int currindex = -1;           // index of current layer

Layer* currlayer;             // pointer to current layer
Layer* layer[maxlayers];      // array of layers

bool available[maxlayers];    // for setting tempstart suffix

bool oldhash;                 // hash setting in old layer
wxString oldrule;             // rule string in old layer
int oldmag;                   // scale in old layer
bigint oldx;                  // X position in old layer
bigint oldy;                  // Y position in old layer
wxCursor* oldcurs;            // cursor mode in old layer

// ids for bitmap buttons in layer bar;
// also used as indices for bitbutt/normbitmap/toggbitmap arrays
enum {
   ADD_LAYER = 0,
   DELETE_LAYER,
   STACK_LAYERS,
   TILE_LAYERS,
   LAYER_0,
   LAYER_LAST = LAYER_0 + maxlayers - 1
};

// array of bitmap buttons (set in LayerBar::AddButton)
wxBitmapButton* bitbutt[LAYER_LAST + 1];

// array of bitmaps for normal or toggled state (set in LayerBar::AddButton)
wxBitmap* normbitmap[LAYER_LAST + 1];
wxBitmap* toggbitmap[LAYER_LAST + 1];

int toggid = -1;              // id of currently toggled layer button

const int BUTTON_WD = 24;     // nominal width of bitmap buttons
const int BITMAP_WD = 16;     // width of bitmaps
const int BITMAP_HT = 16;     // height of bitmaps

// -----------------------------------------------------------------------------

void SelectButton(int id, bool select)
{
   if (select && id >= LAYER_0 && id <= LAYER_LAST) {
      if (toggid >= LAYER_0) {
         // deselect old layer button
         bitbutt[toggid]->SetBitmapLabel(*normbitmap[toggid]);
         if (showlayer) bitbutt[toggid]->Refresh(false);
      }
      toggid = id;
   }
   
   if (select) {
      bitbutt[id]->SetBitmapLabel(*toggbitmap[id]);
   } else {
      bitbutt[id]->SetBitmapLabel(*normbitmap[id]);
   }
   if (showlayer) bitbutt[id]->Refresh(false);
}

// -----------------------------------------------------------------------------

void SaveLayerSettings()
{
   // set oldhash and oldrule for use in CurrentLayerChanged
   oldhash = currlayer->hash;
   oldrule = wxString(global_liferules.getrule(), wxConvLocal);
   
   if (syncviews) {
      // save scale and location for use in CurrentLayerChanged
      oldmag = currlayer->view->getmag();
      oldx = currlayer->view->x;
      oldy = currlayer->view->y;
   }
   
   if (synccursors) {
      // save cursor mode for use in CurrentLayerChanged
      oldcurs = currlayer->curs;
   }
   
   // we're about to change layer so remember current rule
   currlayer->rule = oldrule;
}

// -----------------------------------------------------------------------------

void CurrentLayerChanged()
{
   // need to update global rule table if the hash setting has changed
   // or if the current rule has changed
   wxString currentrule = wxString(global_liferules.getrule(), wxConvLocal);
   if ( oldhash != currlayer->hash || !oldrule.IsSameAs(currentrule,false) ) {
      currlayer->algo->setrule( currentrule.mb_str(wxConvLocal) );
   }
   
   if (syncviews) currlayer->view->setpositionmag(oldx, oldy, oldmag);
   if (synccursors) currlayer->curs = oldcurs;

   mainptr->SetWarp(currlayer->warp);
   mainptr->SetWindowTitle(currlayer->currname);

   // select current layer button (also deselects old button)
   SelectButton(LAYER_0 + currindex, true);

   mainptr->UpdateUserInterface(mainptr->IsActive());
   mainptr->UpdatePatternAndStatus();
}

// -----------------------------------------------------------------------------

void AddLayer()
{
   if (mainptr && mainptr->generating) return;
   if (numlayers >= maxlayers) return;
   
   if (numlayers == 0) {
      // creating the very first layer
      currindex = 0;
   } else {
      SaveLayerSettings();
      
      // insert new layer after currindex
      currindex++;
      if (currindex < numlayers) {
         // shift right one or more layers
         for (int i = numlayers; i > currindex; i--)
            layer[i] = layer[i-1];
      }
   }

   currlayer = new Layer();
   layer[currindex] = currlayer;
   
   numlayers++;

   if (numlayers > 1) {
      // add bitmap button at end of layer bar
      bitbutt[LAYER_0 + numlayers - 1]->Show(true);
      
      // add another item at end of Layer menu
      mainptr->AppendLayerItem();

      // update names in any items after currindex
      for (int i = currindex + 1; i < numlayers; i++)
         mainptr->UpdateLayerItem(i);
      
      CurrentLayerChanged();
   }
}

// -----------------------------------------------------------------------------

void DeleteLayer()
{
   if (mainptr->generating || numlayers <= 1) return;
   
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

   // remove bitmap button at end of layer bar
   bitbutt[LAYER_0 + numlayers]->Show(false);
      
   // remove item from end of Layer menu
   mainptr->RemoveLayerItem();

   // update names in any items after currindex
   for (int i = currindex + 1; i < numlayers; i++)
      mainptr->UpdateLayerItem(i);
   
   CurrentLayerChanged();
}

// -----------------------------------------------------------------------------

void DeleteOtherLayers()
{
   if (numlayers <= 1) return;
   
   // delete all layers except current layer
   for (int i = 0; i < numlayers; i++)
      if (i != currindex) delete layer[i];

   layer[0] = layer[currindex];
   currindex = 0;
   // currlayer doesn't change

   // remove all items except layer 0
   while (numlayers > 1) {
      numlayers--;

      // remove bitmap button at end of layer bar
      bitbutt[LAYER_0 + numlayers]->Show(false);
      
      // remove item from end of Layer menu
      mainptr->RemoveLayerItem();
   }

   // update name in currindex item
   mainptr->UpdateLayerItem(currindex);

   // select LAYER_0 button (also deselects old button)
   SelectButton(LAYER_0, true);

   mainptr->UpdateMenuItems(mainptr->IsActive());
   mainptr->UpdatePatternAndStatus();
}

// -----------------------------------------------------------------------------

void MoveLayerDialog()
{
   Warning(_("Not yet implemented."));    //!!!
}

// -----------------------------------------------------------------------------

void SetLayer(int index)
{
   if (mainptr->generating || index < 0 || index >= numlayers) return;
   if (currindex == index) return;
   
   SaveLayerSettings();
   currindex = index;
   currlayer = layer[currindex];
   CurrentLayerChanged();
}

// -----------------------------------------------------------------------------

void ToggleSyncViews()
{
   syncviews = !syncviews;

   mainptr->UpdateMenuItems(mainptr->IsActive());
}

// -----------------------------------------------------------------------------

void ToggleSyncCursors()
{
   synccursors = !synccursors;

   mainptr->UpdateMenuItems(mainptr->IsActive());
}

// -----------------------------------------------------------------------------

void ToggleStackLayers()
{
   stacklayers = !stacklayers;
   if (stacklayers && tilelayers) {
      tilelayers = false;
      SelectButton(TILE_LAYERS, false);
   }
   SelectButton(STACK_LAYERS, stacklayers);

   //!!! need??? UpdateLayerBar(mainptr->IsActive());
   mainptr->UpdateMenuItems(mainptr->IsActive());
   mainptr->UpdatePatternAndStatus();
}

// -----------------------------------------------------------------------------

void ToggleTileLayers()
{
   tilelayers = !tilelayers;
   if (tilelayers && stacklayers) {
      stacklayers = false;
      SelectButton(STACK_LAYERS, false);
   }
   SelectButton(TILE_LAYERS, tilelayers);

   //!!! need??? UpdateLayerBar(mainptr->IsActive());
   mainptr->UpdateMenuItems(mainptr->IsActive());
   mainptr->UpdatePatternAndStatus();
}

// -----------------------------------------------------------------------------

void ResizeLayers(int wd, int ht)
{
   // resize viewport in each layer
   for (int i = 0; i < numlayers; i++)
      layer[i]->view->resize(wd, ht);
}

// -----------------------------------------------------------------------------

Layer* GetLayer(int index)
{
   if (index < 0 || index >= numlayers) {
      Warning(_("Bad index in GetLayer!"));
      return NULL;
   } else {
      return layer[index];
   }
}

// -----------------------------------------------------------------------------

int FindAvailableSuffix()
{
   // find first available index to use as tempstart suffix
   for (int i = 0; i < maxlayers; i++) {
      if (available[i]) {
         available[i] = false;
         return i;
      }
   }
   // bug if we get here
   Warning(_("Bug in FindAvailableSuffix!"));
   return 0;
}

// -----------------------------------------------------------------------------

Layer::Layer()
{
   // set tempstart prefix (unique suffix will be added below);
   // WARNING: ~Layer() assumes prefix ends with '_'
   tempstart = gollydir + wxT(".golly_start_");

   savestart = false;            // no need to save starting pattern just yet
   startfile.Clear();            // no starting pattern
   startgen = 0;                 // initial starting generation
   currname = _("untitled");
   currfile = wxEmptyString;
   warp = 0;                     // initial speed setting
   originx = 0;                  // no X origin offset
   originy = 0;                  // no Y origin offset
   
   // no selection
   seltop = 1;
   selbottom = 0;
   selleft = 0;
   selright = 0;

   // rule is only set later in SaveLayerSettings, so here we just assign
   // a string to help catch caller accessing rule when it shouldn't
   rule = _("bug!");

   if (numlayers == 0) {
      // creating very first layer
      
      // set hash using inithash stored in prefs file
      hash = inithash;
      
      // create empty universe
      if (hash) {
         algo = new hlifealgo();
         algo->setMaxMemory(maxhashmem);
      } else {
         algo = new qlifealgo();
      }
      algo->setpoll(wxGetApp().Poller());

      // set rule using initrule stored in prefs file;
      // errors can only occur if someone has edited the prefs file
      const char *err = algo->setrule(initrule);
      if (err) {
         Warning(wxString(err,wxConvLocal));
         // user will see offending rule string in window title
      } else if (global_liferules.hasB0notS8 && hash) {
         // silently turn off hashing
         hash = false;
         delete algo;
         algo = new qlifealgo();
         algo->setpoll(wxGetApp().Poller());
         algo->setrule(initrule);
      }
      
      // create viewport; the initial size is not important because
      // ResizeLayers will soon be called
      view = new viewport(100,100);
      
      // set cursor in case newcurs/opencurs are set to "No Change"
      curs = curs_pencil;
      
      // add suffix to tempstart and initialize available array
      tempstart += wxT("0");
      available[0] = false;
      for (int i = 1; i < maxlayers; i++) available[i] = true;

   } else {
      // adding a new layer after currlayer (see AddLayer)

      // inherit current universe type and create empty universe
      hash = currlayer->hash;
      if (hash) {
         algo = new hlifealgo();
         algo->setMaxMemory(maxhashmem);
      } else {
         algo = new qlifealgo();
      }
      algo->setpoll(wxGetApp().Poller());
      
      // inherit current rule (already in global_liferules)
      
      // inherit current viewport's size, scale and location
      view = new viewport(100,100);
      view->resize( currlayer->view->getwidth(),
                    currlayer->view->getheight() );
      view->setpositionmag( currlayer->view->x, currlayer->view->y,
                            currlayer->view->getmag() );
      
      // inherit current cursor
      curs = currlayer->curs;

      // add unique suffix to tempstart
      tempstart += wxString::Format("%d", FindAvailableSuffix());
   }
}

// -----------------------------------------------------------------------------

Layer::~Layer()
{
   if (algo) delete algo;
   if (view) delete view;
   
   // delete tempstart file if it exists
   if (wxFileExists(tempstart)) wxRemoveFile(tempstart);
   
   // make tempstart suffix available for new layers
   wxString suffix = tempstart.AfterLast('_');
   long val;
   if (suffix.ToLong(&val) && val >= 0 && val < maxlayers) {
      available[val] = true;
   } else {
      Warning(_("Problem in tempstart: ") + tempstart);
   }
}

// -----------------------------------------------------------------------------

// Define layer bar window:

class LayerBar : public wxWindow
{
public:
   LayerBar(wxWindow* parent, wxCoord xorg, wxCoord yorg, int wd, int ht)
      : wxWindow(parent, wxID_ANY, wxPoint(xorg,yorg), wxSize(wd,ht))
   {
      // avoid erasing background on GTK+
      SetBackgroundStyle(wxBG_STYLE_CUSTOM);
   }
   ~LayerBar() {}

   void AddButton(int id, char label, int x, int y);

   // event handlers
   void OnPaint(wxPaintEvent& event);
   void OnMouseDown(wxMouseEvent& event);
   void OnButton(wxCommandEvent& event);

   DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(LayerBar, wxWindow)
   EVT_PAINT       (           LayerBar::OnPaint)
   EVT_LEFT_DOWN   (           LayerBar::OnMouseDown)
   EVT_BUTTON      (wxID_ANY,  LayerBar::OnButton)
END_EVENT_TABLE()

LayerBar* layerbarptr = NULL;

// -----------------------------------------------------------------------------

void LayerBar::OnPaint(wxPaintEvent& WXUNUSED(event))
{
   int wd, ht;
   GetClientSize(&wd, &ht);
   if (wd > 0 && ht > 0 && showlayer) {
      wxPaintDC dc(this);
      
      #ifdef __WXMSW__
         dc.Clear();       // needed on Windows
      #endif
      
      //!!! need to draw some border lines???
   }
}

// -----------------------------------------------------------------------------

void LayerBar::OnMouseDown(wxMouseEvent& WXUNUSED(event))
{
   // this is NOT called if user clicks a layer bar button;
   // on Windows we need to reset keyboard focus to viewport window
   viewptr->SetFocus();
}

// -----------------------------------------------------------------------------

void LayerBar::OnButton(wxCommandEvent& event)
{
   mainptr->showbanner = false;
   statusptr->ClearMessage();

   int id = event.GetId();
   switch (id)
   {
      case ADD_LAYER:      AddLayer(); break;
      case DELETE_LAYER:   DeleteLayer(); break;
      case STACK_LAYERS:   ToggleStackLayers(); break;
      case TILE_LAYERS:    ToggleTileLayers(); break;
      default:             SetLayer(id - LAYER_0);
   }
   
   // needed on Windows to clear button focus
   viewptr->SetFocus();
}

// -----------------------------------------------------------------------------

void LayerBar::AddButton(int id, char label, int x, int y)
{
   wxMemoryDC dc;
   wxFont* font = wxFont::New(10, wxMODERN, wxFONTSTYLE_NORMAL, wxBOLD);
   wxString str;
   str.Printf(_("%c"), label);

   // create bitmap for normal button state
   normbitmap[id] = new wxBitmap(BITMAP_WD, BITMAP_HT);
   dc.SelectObject(*normbitmap[id]);
   dc.SetFont(*font);
   dc.SetTextForeground(*wxBLACK);
   dc.SetBrush(*wxBLACK_BRUSH);
   #ifndef __WXMAC__
      dc.Clear();   // needed on Windows and Linux
   #endif
   dc.SetBackgroundMode(wxTRANSPARENT);
   #ifdef __WXMAC__
      dc.DrawText(str, 3, 2);
   #else
      dc.DrawText(str, 4, 0);
   #endif
   dc.SelectObject(wxNullBitmap);
   
   // create bitmap for toggled state
   toggbitmap[id] = new wxBitmap(BITMAP_WD, BITMAP_HT);
   dc.SelectObject(*toggbitmap[id]);
   wxRect r = wxRect(0, 0, BITMAP_WD, BITMAP_HT);
   wxColor gray(64,64,64);
   wxBrush brush(gray);
   FillRect(dc, r, brush);
   dc.SetFont(*font);
   dc.SetTextForeground(*wxWHITE);
   dc.SetBrush(*wxWHITE_BRUSH);
   dc.SetBackgroundMode(wxTRANSPARENT);
   #ifdef __WXMAC__
      dc.DrawText(str, 5, 1);             //!!! why diff to above???
   #else
      dc.DrawText(str, 4, 0);
   #endif
   dc.SelectObject(wxNullBitmap);
   
   bitbutt[id] = new wxBitmapButton(this, id, *normbitmap[id], wxPoint(x,y));
   if (bitbutt[id] == NULL) Fatal(_("Failed to create layer button!"));
}

// -----------------------------------------------------------------------------

void CreateLayerBar(wxWindow* parent)
{
   int wd, ht;
   parent->GetClientSize(&wd, &ht);

   layerbarptr = new LayerBar(parent, 0, 0, wd, layerbarht);
   if (layerbarptr == NULL) Fatal(_("Failed to create layer bar!"));

   #ifdef __WXMSW__
      // use current theme's background colour
      layerbarptr->SetBackgroundColour(wxNullColour);
   #endif

   // create bitmap buttons
   int x = 4;
   int y = 4;
   int sgap = 4;
   int bgap = 16;
   layerbarptr->AddButton(ADD_LAYER,    '+', x, y);   x += BUTTON_WD + sgap;
   layerbarptr->AddButton(DELETE_LAYER, '-', x, y);   x += BUTTON_WD + bgap;
   layerbarptr->AddButton(STACK_LAYERS, 'S', x, y);   x += BUTTON_WD + sgap;
   layerbarptr->AddButton(TILE_LAYERS,  'T', x, y);   x += BUTTON_WD + bgap;
   for (int i = 0; i < maxlayers; i++) {
      layerbarptr->AddButton(LAYER_0 + i, '0' + i, x, y);
      x += BUTTON_WD + sgap;
   }

   // add tool tips to buttons
   bitbutt[ADD_LAYER]->SetToolTip(_("Add new layer"));
   bitbutt[DELETE_LAYER]->SetToolTip(_("Delete current layer"));
   bitbutt[STACK_LAYERS]->SetToolTip(_("Toggle stacked layers"));
   bitbutt[TILE_LAYERS]->SetToolTip(_("Toggle tiled layers"));
   for (int i = 0; i < maxlayers; i++) {
      wxString tip;
      tip.Printf(_("Switch to layer %d"), i);
      bitbutt[LAYER_0 + i]->SetToolTip(tip);
   }
   
   // hide all layer buttons except layer 0
   for (int i = 1; i < maxlayers; i++) {
      bitbutt[LAYER_0 + i]->Show(false);
   }
   
   // select STACK_LAYERS or TILE_LAYERS if necessary
   if (stacklayers) SelectButton(STACK_LAYERS, true);
   if (tilelayers) SelectButton(TILE_LAYERS, true);
   
   // select LAYER_0 button
   SelectButton(LAYER_0, true);
   
   // disable DELETE_LAYER button
   bitbutt[DELETE_LAYER]->Enable(false);
      
   layerbarptr->Show(showlayer);    // needed on Windows
}

// -----------------------------------------------------------------------------

void ResizeLayerBar(int wd)
{
   if (layerbarptr) {
      layerbarptr->SetSize(wd, layerbarht);
   }
}

// -----------------------------------------------------------------------------

void UpdateLayerBar(bool active)
{
   if (layerbarptr && showlayer) {
      if (viewptr->waitingforclick) active = false;
      bool busy = mainptr->generating || inscript;
      
      bitbutt[ADD_LAYER]->Enable(active && !busy && numlayers < maxlayers);
      bitbutt[DELETE_LAYER]->Enable(active && !busy && numlayers > 1);
      bitbutt[STACK_LAYERS]->Enable(active);
      bitbutt[TILE_LAYERS]->Enable(active);
      for (int i = LAYER_0; i < LAYER_0 + numlayers; i++)
         bitbutt[i]->Enable(active && !busy);
   }
}

// -----------------------------------------------------------------------------

void ToggleLayerBar()
{
   showlayer = !showlayer;
   wxRect r = viewptr->GetRect();
   if (showlayer) {
      // show layer bar at top of viewport window
      r.y += layerbarht;
      r.height -= layerbarht;
   } else {
      // hide layer bar
      r.y -= layerbarht;
      r.height += layerbarht;
   }
   viewptr->SetSize(r);
   layerbarptr->Show(showlayer);    // needed on Windows
}
