                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2007 Andrew Trevorrow and Tomas Rokicki.

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

#include "wx/numdlg.h"     // for wxGetNumberFromUser

#include "bigint.h"
#include "lifealgo.h"
#include "qlifealgo.h"
#include "hlifealgo.h"
#include "viewport.h"

#include "wxgolly.h"       // for wxGetApp, mainptr, viewptr, bigview, statusptr
#include "wxmain.h"        // for mainptr->...
#include "wxview.h"        // for viewptr->...
#include "wxstatus.h"      // for statusptr->...
#include "wxutils.h"       // for Warning, FillRect, CreatePaleBitmap
#include "wxprefs.h"       // for gollydir, inithash, initrule, etc
#include "wxscript.h"      // for inscript
#include "wxlayer.h"

// -----------------------------------------------------------------------------

int numlayers = 0;               // number of existing layers
int numclones = 0;               // number of cloned layers
int currindex = -1;              // index of current layer

Layer* currlayer;                // pointer to current layer
Layer* layer[maxlayers];         // array of layers

bool suffavail[maxlayers];       // for setting unique tempstart suffix
bool cloneavail[maxlayers];      // for setting unique cloneid

bool cloning = false;            // adding a cloned layer?
bool duplicating = false;        // adding a duplicated layer?

bool oldhash;                    // hash setting in old layer
wxString oldrule;                // rule string in old layer
int oldmag;                      // scale in old layer
bigint oldx;                     // X position in old layer
bigint oldy;                     // Y position in old layer
wxCursor* oldcurs;               // cursor mode in old layer

// ids for bitmap buttons in layer bar
enum {
   LAYER_0 = 0,                  // LAYER_0 must be first id
   LAYER_LAST = LAYER_0 + maxlayers - 1,
   ADD_LAYER,
   CLONE_LAYER,
   DELETE_LAYER,
   STACK_LAYERS,
   TILE_LAYERS                   // if moved then change NUM_BUTTONS
};

const int NUM_BUTTONS = TILE_LAYERS + 1;

#ifdef __WXMSW__
   // bitmaps are loaded via .rc file
#else
   // bitmaps for some layer bar buttons; note that bitmaps for
   // LAYER_0..LAYER_LAST buttons are created in LayerBar::AddButton
   #include "bitmaps/add.xpm"
   #include "bitmaps/clone.xpm"
   #include "bitmaps/delete.xpm"
   #include "bitmaps/stack.xpm"
   #include "bitmaps/stack_down.xpm"
   #include "bitmaps/tile.xpm"
   #include "bitmaps/tile_down.xpm"
#endif

// -----------------------------------------------------------------------------

// Define layer bar window:

// derive from wxPanel so we get current theme's background color on Windows
class LayerBar : public wxPanel
{
public:
   LayerBar(wxWindow* parent, wxCoord xorg, wxCoord yorg, int wd, int ht);
   ~LayerBar() {}

   // add a bitmap button to layer bar
   void AddButton(int id, char label, const wxString& tip);

   // add a horizontal gap between buttons
   void AddSeparator();
   
   // enable/disable button
   void EnableButton(int id, bool enable);
   
   // set state of a toggle button
   void SelectButton(int id, bool select);

   // detect press and release of a bitmap button
   void OnButtonDown(wxMouseEvent& event);
   void OnButtonUp(wxMouseEvent& event);
   void OnMouseMotion(wxMouseEvent& event);
   void OnKillFocus(wxFocusEvent& event);

private:
   // any class wishing to process wxWidgets events must use this macro
   DECLARE_EVENT_TABLE()

   // event handlers
   void OnPaint(wxPaintEvent& event);
   void OnMouseDown(wxMouseEvent& event);
   void OnButton(wxCommandEvent& event);
   
   // bitmaps for normal or down state
   wxBitmap normbutt[NUM_BUTTONS];
   wxBitmap downbutt[NUM_BUTTONS];

   #ifdef __WXMSW__
      // on Windows we need bitmaps for disabled buttons
      wxBitmap disnormbutt[NUM_BUTTONS];
      wxBitmap disdownbutt[NUM_BUTTONS];
   #endif
   
   // positioning data used by AddButton and AddSeparator
   int ypos, xpos, smallgap, biggap;

   // id of currently pressed layer button
   int downid;
};

BEGIN_EVENT_TABLE(LayerBar, wxPanel)
   EVT_PAINT            (           LayerBar::OnPaint)
   EVT_LEFT_DOWN        (           LayerBar::OnMouseDown)
   EVT_BUTTON           (wxID_ANY,  LayerBar::OnButton)
END_EVENT_TABLE()

LayerBar* layerbarptr = NULL;      // global pointer to layer bar

// layer bar buttons (must be global to use Connect/Disconect on Windows)
wxBitmapButton* lbbutt[NUM_BUTTONS];

// -----------------------------------------------------------------------------

LayerBar::LayerBar(wxWindow* parent, wxCoord xorg, wxCoord yorg, int wd, int ht)
   : wxPanel(parent, wxID_ANY, wxPoint(xorg,yorg), wxSize(wd,ht),
             wxNO_FULL_REPAINT_ON_RESIZE)
{
   #ifdef __WXGTK__
      // avoid erasing background on GTK+
      SetBackgroundStyle(wxBG_STYLE_CUSTOM);
   #endif

   // init bitmaps for normal state;
   // note that bitmaps for layer buttons are created in AddButton
   normbutt[ADD_LAYER] =    wxBITMAP(add);
   normbutt[CLONE_LAYER] =  wxBITMAP(clone);
   normbutt[DELETE_LAYER] = wxBITMAP(delete);
   normbutt[STACK_LAYERS] = wxBITMAP(stack);
   normbutt[TILE_LAYERS] =  wxBITMAP(tile);
   
   // toggle buttons also have a down state
   downbutt[STACK_LAYERS] = wxBITMAP(stack_down);
   downbutt[TILE_LAYERS] =  wxBITMAP(tile_down);

   #ifdef __WXMSW__
      // create bitmaps for disabled buttons
      CreatePaleBitmap(normbutt[ADD_LAYER],     disnormbutt[ADD_LAYER]);
      CreatePaleBitmap(normbutt[CLONE_LAYER],   disnormbutt[CLONE_LAYER]);
      CreatePaleBitmap(normbutt[DELETE_LAYER],  disnormbutt[DELETE_LAYER]);
      CreatePaleBitmap(normbutt[STACK_LAYERS],  disnormbutt[STACK_LAYERS]);
      CreatePaleBitmap(normbutt[TILE_LAYERS],   disnormbutt[TILE_LAYERS]);
      
      // create bitmaps for disabled buttons in down state
      CreatePaleBitmap(downbutt[STACK_LAYERS],  disdownbutt[STACK_LAYERS]);
      CreatePaleBitmap(downbutt[TILE_LAYERS],   disdownbutt[TILE_LAYERS]);
   #endif

   // init position variables used by AddButton and AddSeparator
   biggap = 16;
   xpos = biggap;       // nicer than 4 when no pattern/script window
   #ifdef __WXGTK__
      ypos = 3;
      smallgap = 6;
   #else
      ypos = 4;
      smallgap = 4;
   #endif
   
   downid = -1;         // no layer button down as yet
}

// -----------------------------------------------------------------------------

void LayerBar::OnPaint(wxPaintEvent& WXUNUSED(event))
{
   wxPaintDC dc(this);

   int wd, ht;
   GetClientSize(&wd, &ht);
   if (wd < 1 || ht < 1 || !showlayer) return;
      
   #ifdef __WXMSW__
      // needed on Windows
      dc.Clear();
   #endif
   
   wxRect r = wxRect(0, 0, wd, ht);
   
   #ifdef __WXMAC__
      wxBrush brush(wxColor(202,202,202));
      FillRect(dc, r, brush);
   #endif
   
   // draw gray border line at bottom edge
   #if defined(__WXMSW__)
      dc.SetPen(*wxGREY_PEN);
   #elif defined(__WXMAC__)
      wxPen linepen(wxColor(140,140,140));
      dc.SetPen(linepen);
   #else
      dc.SetPen(*wxLIGHT_GREY_PEN);
   #endif
   dc.DrawLine(0, r.GetBottom(), r.width, r.GetBottom());
   dc.SetPen(wxNullPen);
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
   
   #ifdef __WXMSW__
      // disconnect focus handler and reset focus to viewptr;
      // we must do latter before button becomes disabled
      lbbutt[id]->Disconnect(id, wxEVT_KILL_FOCUS,
                              wxFocusEventHandler(LayerBar::OnKillFocus));
      viewptr->SetFocus();
   #endif

   switch (id) {
      case ADD_LAYER:      AddLayer(); break;
      case CLONE_LAYER:    CloneLayer(); break;
      case DELETE_LAYER:   DeleteLayer(); break;
      case STACK_LAYERS:   ToggleStackLayers(); break;
      case TILE_LAYERS:    ToggleTileLayers(); break;
      default:
         SetLayer(id);
         if (inscript) {
            // update window title, viewport and status bar
            inscript = false;
            mainptr->SetWindowTitle(wxEmptyString);
            mainptr->UpdatePatternAndStatus();
            inscript = true;
         }
   }
}

// -----------------------------------------------------------------------------

void LayerBar::OnKillFocus(wxFocusEvent& event)
{
   int id = event.GetId();
   lbbutt[id]->SetFocus();   // don't let button lose focus
}

// -----------------------------------------------------------------------------

//!!! flag is not used at the moment (probably need later for dragging button)
bool layerbuttdown = false;

void LayerBar::OnButtonDown(wxMouseEvent& event)
{
   // a layer bar button has been pressed
   layerbuttdown = true;
   
   int id = event.GetId();
   
   // connect a handler that keeps focus with the pressed button
   lbbutt[id]->Connect(id, wxEVT_KILL_FOCUS,
                       wxFocusEventHandler(LayerBar::OnKillFocus));
        
   event.Skip();
}

// -----------------------------------------------------------------------------

void LayerBar::OnButtonUp(wxMouseEvent& event)
{
   // a layer bar button has been released
   layerbuttdown = false;

   int id = event.GetId();
   wxPoint pt = lbbutt[id]->ScreenToClient( wxGetMousePosition() );

   int wd, ht;
   lbbutt[id]->GetClientSize(&wd, &ht);
   wxRect r(0, 0, wd, ht);

   // diconnect kill-focus handler
   lbbutt[id]->Disconnect(id, wxEVT_KILL_FOCUS,
                          wxFocusEventHandler(LayerBar::OnKillFocus));
   viewptr->SetFocus();

#if wxCHECK_VERSION(2, 7, 0)
// Inside is deprecated
if ( r.Contains(pt) ) {
#else
if ( r.Inside(pt) ) {
#endif
      // call OnButton
      wxCommandEvent buttevt(wxEVT_COMMAND_BUTTON_CLICKED, id);
      buttevt.SetEventObject(lbbutt[id]);
      lbbutt[id]->ProcessEvent(buttevt);
   }
}

// -----------------------------------------------------------------------------

//!!! not used at the moment (probably need later for button dragging)
void LayerBar::OnMouseMotion(wxMouseEvent& event)
{   
   if (layerbuttdown) {
      //!!!
   }
   event.Skip();
}

// -----------------------------------------------------------------------------

void LayerBar::AddButton(int id, char label, const wxString& tip)
{
   if (id >= LAYER_0 && id <= LAYER_LAST) {
      // create bitmaps for given layer button
      const int BITMAP_WD = 16;
      const int BITMAP_HT = 16;

      wxMemoryDC dc;
      #ifdef __WXMAC__
         wxFont* font = wxFont::New(11, wxMODERN, wxNORMAL, wxBOLD);
      #else
         wxFont* font = wxFont::New(10, wxMODERN, wxNORMAL, wxBOLD);
      #endif
      wxString str;
      str.Printf(_("%c"), label);
      
      wxColor darkblue(0,0,128);       // matches blue in above buttons

      // create bitmap for normal state
      normbutt[id] = wxBitmap(BITMAP_WD, BITMAP_HT);
      dc.SelectObject(normbutt[id]);
      dc.SetFont(*font);
      dc.SetTextForeground(darkblue);
      dc.SetBrush(*wxBLACK_BRUSH);
      #ifndef __WXMAC__
         dc.Clear();   // needed on Windows and Linux
      #endif
      dc.SetBackgroundMode(wxTRANSPARENT);
      #if defined(__WXMAC__)
         dc.DrawText(str, 3, 2);
      #elif defined(__WXX11__)
         dc.DrawText(str, 4, 2);
      #else
         dc.DrawText(str, 4, 0);
      #endif
      dc.SelectObject(wxNullBitmap);
      #if defined(__WXMSW__) || defined(__WXGTK__) || defined(__WXX11__)
         // prevent white background
         normbutt[id].SetMask( new wxMask(normbutt[id],*wxWHITE) );
      #endif

      // create bitmap for down state
      downbutt[id] = wxBitmap(BITMAP_WD, BITMAP_HT);
      dc.SelectObject(downbutt[id]);
      wxRect r = wxRect(0, 0, BITMAP_WD, BITMAP_HT);
      wxBrush brush(wxColor(140,150,166));
      FillRect(dc, r, brush);
      dc.SetFont(*font);
      dc.SetTextForeground(wxColor(0,0,48));
      dc.SetBrush(*wxBLACK_BRUSH);
      dc.SetBackgroundMode(wxTRANSPARENT);
      #if defined(__WXMAC__)
         dc.DrawText(str, 5, 1);             // why diff to above???
      #elif defined(__WXX11__)
         dc.DrawText(str, 4, 2);
      #else
         dc.DrawText(str, 4, 0);
      #endif
      dc.SelectObject(wxNullBitmap);

      #ifdef __WXMSW__
         CreatePaleBitmap(normbutt[id], disnormbutt[id]);
         CreatePaleBitmap(downbutt[id], disdownbutt[id]);
      #endif
   }
   
   lbbutt[id] = new wxBitmapButton(this, id, normbutt[id], wxPoint(xpos,ypos));
   if (lbbutt[id] == NULL) {
      Fatal(_("Failed to create layer bar button!"));
   } else {
      const int BUTTON_WD = 24;        // nominal width of bitmap buttons
      xpos += BUTTON_WD + smallgap;
      lbbutt[id]->SetToolTip(tip);
      #ifdef __WXMSW__
         // fix problem with layer bar buttons when generating/inscript
         // due to focus being changed to viewptr
         lbbutt[id]->Connect(id, wxEVT_LEFT_DOWN,
                             wxMouseEventHandler(LayerBar::OnButtonDown));
         lbbutt[id]->Connect(id, wxEVT_LEFT_UP,
                             wxMouseEventHandler(LayerBar::OnButtonUp));
         /* don't need this handler at the moment
         lbbutt[id]->Connect(id, wxEVT_MOTION,
                             wxMouseEventHandler(LayerBar::OnMouseMotion));
         */
      #endif
   }
}

// -----------------------------------------------------------------------------

void LayerBar::AddSeparator()
{
   xpos += biggap - smallgap;
}

// -----------------------------------------------------------------------------

void LayerBar::EnableButton(int id, bool enable)
{
   if (enable == lbbutt[id]->IsEnabled()) return;

   #ifdef __WXMSW__
      if (id >= LAYER_0 && id <= LAYER_LAST && id == downid) {
         lbbutt[id]->SetBitmapDisabled(disdownbutt[id]);
         
      } else if (id == STACK_LAYERS && stacklayers) {
         lbbutt[id]->SetBitmapDisabled(disdownbutt[id]);
         
      } else if (id == TILE_LAYERS && tilelayers) {
         lbbutt[id]->SetBitmapDisabled(disdownbutt[id]);
         
      } else {
         lbbutt[id]->SetBitmapDisabled(disnormbutt[id]);
      }
   #endif

   lbbutt[id]->Enable(enable);
}

// -----------------------------------------------------------------------------

void LayerBar::SelectButton(int id, bool select)
{
   if (select && id >= LAYER_0 && id <= LAYER_LAST) {
      if (downid >= LAYER_0) {
         // deselect old layer button
         lbbutt[downid]->SetBitmapLabel(normbutt[downid]);         
         if (showlayer) {
            #ifdef __WXX11__
               lbbutt[downid]->ClearBackground();    // fix wxX11 problem
            #endif
            lbbutt[downid]->Refresh(false);
         }
      }
      downid = id;
   }
   
   if (select) {
      lbbutt[id]->SetBitmapLabel(downbutt[id]);
   } else {
      lbbutt[id]->SetBitmapLabel(normbutt[id]);
   }

   if (showlayer) {
      #ifdef __WXX11__
         lbbutt[id]->ClearBackground();    // fix wxX11 problem
      #endif
      lbbutt[id]->Refresh(false);
   }
}

// -----------------------------------------------------------------------------

void CreateLayerBar(wxWindow* parent)
{
   int wd, ht;
   parent->GetClientSize(&wd, &ht);

   layerbarptr = new LayerBar(parent, 0, 0, wd, layerbarht);
   if (layerbarptr == NULL) Fatal(_("Failed to create layer bar!"));

   // add buttons to layer bar
   layerbarptr->AddButton(ADD_LAYER,    0, _("Add new layer"));
   layerbarptr->AddButton(CLONE_LAYER,  0, _("Clone current layer"));
   layerbarptr->AddButton(DELETE_LAYER, 0, _("Delete current layer"));
   layerbarptr->AddSeparator();
   layerbarptr->AddButton(STACK_LAYERS, 0, _("Toggle stacked layers"));
   layerbarptr->AddButton(TILE_LAYERS,  0, _("Toggle tiled layers"));
   layerbarptr->AddSeparator();
   for (int i = 0; i < maxlayers; i++) {
      wxString tip;
      tip.Printf(_("Switch to layer %d"), i);
      layerbarptr->AddButton(i, '0' + i, tip);
   }
  
   // hide all layer buttons except layer 0
   for (int i = 1; i < maxlayers; i++) lbbutt[i]->Show(false);
   
   // select STACK_LAYERS or TILE_LAYERS if necessary
   if (stacklayers) layerbarptr->SelectButton(STACK_LAYERS, true);
   if (tilelayers) layerbarptr->SelectButton(TILE_LAYERS, true);
   
   // select LAYER_0 button
   layerbarptr->SelectButton(LAYER_0, true);
      
   layerbarptr->Show(showlayer);
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

      layerbarptr->EnableButton(ADD_LAYER,      active && !busy && numlayers < maxlayers);
      layerbarptr->EnableButton(CLONE_LAYER,    active && !busy && numlayers < maxlayers);
      layerbarptr->EnableButton(DELETE_LAYER,   active && !busy && numlayers > 1);
      layerbarptr->EnableButton(STACK_LAYERS,   active);
      layerbarptr->EnableButton(TILE_LAYERS,    active);
      for (int i = 0; i < numlayers; i++)
         layerbarptr->EnableButton(i, active && CanSwitchLayer(i));
   }
}

// -----------------------------------------------------------------------------

void ToggleLayerBar()
{
   showlayer = !showlayer;
   wxRect r = bigview->GetRect();
   
   if (showlayer) {
      // show layer bar at top of viewport window
      r.y += layerbarht;
      r.height -= layerbarht;
   } else {
      // hide layer bar
      r.y -= layerbarht;
      r.height += layerbarht;
   }
   
   bigview->SetSize(r);
   layerbarptr->Show(showlayer);    // needed on Windows
}

// -----------------------------------------------------------------------------

void CalculateTileRects(int bigwd, int bight)
{
   // set tilerect in each layer
   wxRect r;
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
   
   // set size of each tile window
   for ( int i = 0; i < numlayers; i++ )
      layer[i]->tilewin->SetSize( layer[i]->tilerect );
   
   // set viewport size for each tile; this is currently the same as the
   // tilerect size because tile windows are created with wxNO_BORDER
   for ( int i = 0; i < numlayers; i++ ) {
      int wd, ht;
      layer[i]->tilewin->GetClientSize(&wd, &ht);
      // wd or ht might be < 1 on Win/X11 platforms
      if (wd < 1) wd = 1;
      if (ht < 1) ht = 1;
      layer[i]->view->resize(wd, ht);
   }
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

void CreateTiles()
{
   //!!! debug
   if (viewptr != bigview) Fatal(_("Bug in CreateTiles!"));
   
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
   // wd or ht might be < 1 on Win/X11 platforms
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
   //!!! debug
   if (viewptr == bigview) Fatal(_("Bug in DestroyTiles!"));

   // reset viewptr to main viewport window
   viewptr = bigview;
   if (mainptr->IsActive()) viewptr->SetFocus();

   // destroy all tile windows
   for ( int i = 0; i < numlayers; i++ )
      delete layer[i]->tilewin;

   // resize viewport in each layer to bigview's client area
   int wd, ht;
   bigview->GetClientSize(&wd, &ht);
   // wd or ht might be < 1 on Win/X11 platforms
   if (wd < 1) wd = 1;
   if (ht < 1) ht = 1;
   for ( int i = 0; i < numlayers; i++ )
      layer[i]->view->resize(wd, ht);
}

// -----------------------------------------------------------------------------

void UpdateView()
{
   // update main viewport window, including all tile windows if they exist
   // (tile windows are children of bigview)
   bigview->Refresh(false);
   bigview->Update();
}

// -----------------------------------------------------------------------------

void RefreshView()
{
   // refresh main viewport window, including all tile windows if they exist
   // (tile windows are children of bigview)
   bigview->Refresh(false);
}

// -----------------------------------------------------------------------------

void SyncClones()
{
   //!!! debug
   if (numclones < 0) Fatal(_("Bug in SyncClones!"));
   
   if (numclones == 0) return;
   
   if (currlayer->cloneid > 0) {
      // make sure clone algo and most other settings are synchronized
      for ( int i = 0; i < numlayers; i++ ) {
         Layer* cloneptr = layer[i];
         if (cloneptr != currlayer && cloneptr->cloneid == currlayer->cloneid) {
            // universe might have been re-created, or hashing changed
            cloneptr->algo = currlayer->algo;
            cloneptr->hash = currlayer->hash;
            cloneptr->rule = currlayer->rule;

            // along with view, don't sync these settings
            // cloneptr->autofit = currlayer->autofit;
            // cloneptr->hyperspeed = currlayer->hyperspeed;
            // cloneptr->showhashinfo = currlayer->showhashinfo;
            // cloneptr->curs = currlayer->curs;
            // cloneptr->currname = currlayer->currname;
            // cloneptr->originx = currlayer->originx;
            // cloneptr->originy = currlayer->originy;
            
            // sync dirty flag
            cloneptr->dirty = currlayer->dirty;
            cloneptr->stayclean = currlayer->stayclean;
            
            // sync speed
            cloneptr->warp = currlayer->warp;
            
            // sync selection
            cloneptr->seltop = currlayer->seltop;
            cloneptr->selbottom = currlayer->selbottom;
            cloneptr->selleft = currlayer->selleft;
            cloneptr->selright = currlayer->selright;
            
            // sync the stuff needed to reset pattern
            cloneptr->savestart = currlayer->savestart;
            cloneptr->starthash = currlayer->starthash;
            cloneptr->startrule = currlayer->startrule;
            cloneptr->startx = currlayer->startx;
            cloneptr->starty = currlayer->starty;
            cloneptr->startwarp = currlayer->startwarp;
            cloneptr->startmag = currlayer->startmag;
            cloneptr->startfile = currlayer->startfile;
            cloneptr->startgen = currlayer->startgen;
            cloneptr->currfile = currlayer->currfile;
         }
      }
   }
}

// -----------------------------------------------------------------------------

void SaveLayerSettings()
{
   // a good place to synchronize clone info
   SyncClones();

   // set oldhash and oldrule for use in CurrentLayerChanged
   oldhash = currlayer->hash;
   oldrule = wxString(global_liferules.getrule(), wxConvLocal);
   
   // we're about to change layer so remember current rule
   // in case we switch back to this layer
   currlayer->rule = oldrule;
   
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
}

// -----------------------------------------------------------------------------

void CurrentLayerChanged()
{
   // currlayer has changed since SaveLayerSettings was called;
   // need to update global rule table if the new currlayer has a
   // different hash setting or different rule
   if ( currlayer->hash != oldhash || !currlayer->rule.IsSameAs(oldrule,false) ) {
      currlayer->algo->setrule( currlayer->rule.mb_str(wxConvLocal) );
   }
   
   hlifealgo::setVerbose(currlayer->showhashinfo);
   
   if (syncviews) currlayer->view->setpositionmag(oldx, oldy, oldmag);
   if (synccursors) currlayer->curs = oldcurs;

   // select current layer button (also deselects old button)
   layerbarptr->SelectButton(currindex, true);
   
   if (tilelayers && numlayers > 1) {
      // switch to new tile
      viewptr = currlayer->tilewin;
      if (mainptr->IsActive()) viewptr->SetFocus();
   }

   mainptr->SetWarp(currlayer->warp);
   mainptr->SetWindowTitle(currlayer->currname);

   mainptr->UpdateUserInterface(mainptr->IsActive());
   mainptr->UpdatePatternAndStatus();
   bigview->UpdateScrollBars();
}

// -----------------------------------------------------------------------------

void UpdateLayerNames()
{
   // update names in all layer items at end of Layer menu
   for (int i = 0; i < numlayers; i++)
      mainptr->UpdateLayerItem(i);
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
      if (tilelayers && numlayers > 1) DestroyTiles();

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
      lbbutt[numlayers - 1]->Show(true);
      
      // add another item at end of Layer menu
      mainptr->AppendLayerItem();

      UpdateLayerNames();
      
      if (tilelayers && numlayers > 1) CreateTiles();
      
      CurrentLayerChanged();
   }
}

// -----------------------------------------------------------------------------

void CloneLayer()
{
   cloning = true;
   AddLayer();
   cloning = false;
}

// -----------------------------------------------------------------------------

void DuplicateLayer()
{
   duplicating = true;
   AddLayer();
   duplicating = false;
}

// -----------------------------------------------------------------------------

void DeleteLayer()
{
   if (mainptr->generating || numlayers <= 1) return;

   if (!inscript && currlayer->dirty && askondelete && !mainptr->SaveCurrentLayer()) return;

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

   // remove bitmap button at end of layer bar
   lbbutt[numlayers]->Show(false);
      
   // remove item from end of Layer menu
   mainptr->RemoveLayerItem();

   UpdateLayerNames();
      
   if (tilelayers && numlayers > 1) CreateTiles();
   
   CurrentLayerChanged();
}

// -----------------------------------------------------------------------------

void DeleteOtherLayers()
{
   if (inscript || numlayers <= 1) return;

   if (askondelete) {
      // keep track of which unique clones have been seen;
      // we add 1 below to allow for cloneseen[0] (always false)
      const int maxseen = maxlayers/2 + 1;
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
   
         // remove bitmap button at end of layer bar
         lbbutt[numlayers]->Show(false);
         
         // remove item from end of Layer menu
         mainptr->RemoveLayerItem();
      }
   }

   currindex = 0;
   // currlayer doesn't change

   // update the only layer item
   mainptr->UpdateLayerItem(0);
   
   // update window title (may need to remove "=" prefix)
   mainptr->SetWindowTitle(wxEmptyString);

   // select LAYER_0 button (also deselects old button)
   layerbarptr->SelectButton(LAYER_0, true);

   mainptr->UpdateMenuItems(mainptr->IsActive());
   mainptr->UpdatePatternAndStatus();
}

// -----------------------------------------------------------------------------

void SetLayer(int index)
{
   if (currindex == index) return;
   if (index < 0 || index >= numlayers) return;

   if (inscript) {
      // always allow a script to switch layers
   } else if (mainptr->generating && !CanSwitchLayer(index)) {
      // status bar error is nicer than Warning dialog
      statusptr->ErrorMessage(
         _("You cannot switch to another universe while a pattern is generating."));
      return;
   }
   
   SaveLayerSettings();
   currindex = index;
   currlayer = layer[currindex];
   CurrentLayerChanged();
}

// -----------------------------------------------------------------------------

bool CanSwitchLayer(int index)
{
   if (inscript) {
      // user can only switch layers if script has set the appropriate option
      return canswitch;

   } else if (mainptr->generating) {
      // user can only switch to a clone of the current universe
      // (mainly because all universes share a global rule table)
      return currlayer->cloneid > 0 && currlayer->cloneid == layer[index]->cloneid;

   } else {
      // user can switch to any layer
      return true;
   }
}

// -----------------------------------------------------------------------------

void SwitchToClickedTile(int index)
{
   if (inscript && !CanSwitchLayer(index)) {
      // statusptr->ErrorMessage does nothing if inscript is true
      Warning(_("You cannot switch to another layer while this script is running."));
      return;
   }

   // switch current layer to clicked tile
   SetLayer(index);

   if (inscript) {
      // update window title, viewport and status bar
      inscript = false;
      mainptr->SetWindowTitle(wxEmptyString);
      mainptr->UpdatePatternAndStatus();
      inscript = true;
   }
}

// -----------------------------------------------------------------------------

void MoveLayer(int fromindex, int toindex)
{
   if (mainptr->generating || fromindex == toindex) return;
   if (fromindex < 0 || fromindex >= numlayers) return;
   if (toindex < 0 || toindex >= numlayers) return;

   SaveLayerSettings();
   
   if (fromindex > toindex) {
      Layer* savelayer = layer[fromindex];
      for (int i = fromindex; i > toindex; i--)
         layer[i] = layer[i - 1];
      layer[toindex] = savelayer;
   } else {
      // fromindex < toindex
      Layer* savelayer = layer[fromindex];
      for (int i = fromindex; i < toindex; i++)
         layer[i] = layer[i + 1];
      layer[toindex] = savelayer;
   }

   currindex = toindex;
   currlayer = layer[currindex];

   UpdateLayerNames();

   if (tilelayers && numlayers > 1) {
      DestroyTiles();
      CreateTiles();
   }
   
   CurrentLayerChanged();
}

// -----------------------------------------------------------------------------

//!!! remove this eventually if user can drag layer buttons???
void MoveLayerDialog()
{
   if (mainptr->generating || inscript || numlayers <= 1) return;
   
   long n = wxGetNumberFromUser(_("Move current layer to new position."),
                                _("Enter new index:"),
                                _("Move Layer"),
                                currindex, 0, numlayers - 1,   // default, min, max
                                wxGetActiveWindow());
                                //!!!??? calc offset from main win top left
                                //!!! wxPoint(100,100));    ignored on Mac -- try 2.8???
   
   if (n >= 0 && n < numlayers) MoveLayer(currindex, n);
}

// -----------------------------------------------------------------------------

void NameLayerDialog()
{
   if (inscript) return;

   wxTextEntryDialog dialog(wxGetActiveWindow(),
                            _("Enter a name for the current layer:"),
                            _("Name Layer"),
                            currlayer->currname,
                            #ifdef __WXMAC__
                               //!!! without this dlg appears in top left corner;
                               // ditto in dialogs sample
                               wxCENTRE |
                            #endif
                            wxOK | wxCANCEL);

   if (dialog.ShowModal() == wxID_OK) {
      wxString newname = dialog.GetValue();
      if ( !newname.IsEmpty() ) {
         // show new name in main window's title;
         // also sets currlayer->currname and updates menu item
         mainptr->SetWindowTitle(newname);
      }
   }
}

// -----------------------------------------------------------------------------

void MarkLayerDirty()
{
   // if script has reset dirty flag then don't change it; this makes sense
   // for scripts that call new() and then construct a pattern
   if (currlayer->stayclean) return;

   if (!currlayer->dirty) {
      currlayer->dirty = true;
      
      // pass in currname so UpdateLayerItem(currindex) gets called
      mainptr->SetWindowTitle(currlayer->currname);
      
      if (currlayer->cloneid > 0) {
         // synchronize other clones
         for ( int i = 0; i < numlayers; i++ ) {
            Layer* cloneptr = layer[i];
            if (cloneptr != currlayer && cloneptr->cloneid == currlayer->cloneid) {
               // set dirty flag and display asterisk in layer item
               cloneptr->dirty = true;
               mainptr->UpdateLayerItem(i);
            }
         }
      }
   }
}

// -----------------------------------------------------------------------------

void MarkLayerClean(const wxString& title)
{
   currlayer->dirty = false;
   
   // if script is resetting dirty flag -- eg. via new() -- then keep it that way
   if (inscript) currlayer->stayclean = true;
   
   // set currlayer->currname and call UpdateLayerItem(currindex)
   mainptr->SetWindowTitle(title);
   
   if (currlayer->cloneid > 0) {
      // synchronize other clones
      for ( int i = 0; i < numlayers; i++ ) {
         Layer* cloneptr = layer[i];
         if (cloneptr != currlayer && cloneptr->cloneid == currlayer->cloneid) {
            // reset dirty flag
            cloneptr->dirty = false;
            if (inscript) cloneptr->stayclean = true;
            
            // also best if clone uses same name at this stage
            cloneptr->currname = currlayer->currname;
            
            // remove asterisk from layer item
            mainptr->UpdateLayerItem(i);
         }
      }
   }
}

// -----------------------------------------------------------------------------

void ToggleSyncViews()
{
   syncviews = !syncviews;

   mainptr->UpdateUserInterface(mainptr->IsActive());
   mainptr->UpdatePatternAndStatus();
}

// -----------------------------------------------------------------------------

void ToggleSyncCursors()
{
   synccursors = !synccursors;

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

int GetUniqueCloneID()
{
   // find first available index (> 0) to use as cloneid
   for (int i = 1; i < maxlayers; i++) {
      if (cloneavail[i]) {
         cloneavail[i] = false;
         return i;
      }
   }
   // bug if we get here
   Warning(_("Bug in GetUniqueCloneID!"));
   return 1;
}

// -----------------------------------------------------------------------------

int GetUniqueSuffix()
{
   // find first available index to use as tempstart suffix
   for (int i = 0; i < maxlayers; i++) {
      if (suffavail[i]) {
         suffavail[i] = false;
         return i;
      }
   }
   // bug if we get here
   Warning(_("Bug in GetUniqueSuffix!"));
   return 0;
}

// -----------------------------------------------------------------------------

Layer::Layer()
{
   // set tempstart prefix (unique suffix will be added below);
   // WARNING: ~Layer() assumes prefix ends with '_'
   tempstart = gollydir + wxT(".golly_start_");

   dirty = false;                // user has not modified pattern
   if (inscript) {
      // script created this layer so best to keep dirty flag false
      // (but only for duration of this script)
      stayclean = true;
   } else {
      stayclean = false;         // script has not reset dirty flag
   }
   
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

   if (numlayers == 0) {
      // creating very first layer
      
      // set hash etc using initial values stored in prefs file
      hash = inithash;
      hyperspeed = inithyperspeed;
      showhashinfo = initshowhashinfo;
      autofit = initautofit;
      
      hlifealgo::setVerbose(initshowhashinfo);
      
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
   
      // don't need to remember rule here (SaveLayerSettings will do it)
      rule = wxEmptyString;
      
      // create viewport; the initial size is not important because
      // ResizeLayers will soon be called
      view = new viewport(100,100);
      
      // set cursor in case newcurs/opencurs are set to "No Change"
      curs = curs_pencil;
      
      // add suffix to tempstart and initialize suffavail array
      tempstart += wxT("0");
      suffavail[0] = false;
      for (int i = 1; i < maxlayers; i++) suffavail[i] = true;
      
      // first layer can't be a clone
      cloneid = 0;
      
      // initialize cloneavail array (cloneavail[0] is never used)
      cloneavail[0] = false;
      for (int i = 1; i < maxlayers; i++) cloneavail[i] = true;

   } else {
      // adding a new layer after currlayer (see AddLayer)

      // inherit current universe type and other settings
      hash = currlayer->hash;
      hyperspeed = currlayer->hyperspeed;
      showhashinfo = currlayer->showhashinfo;
      autofit = currlayer->autofit;
      
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

         // clones share the same universe
         algo = currlayer->algo;

         // clones use same name for starting file
         tempstart = currlayer->tempstart;

      } else {
         // this layer isn't a clone
         cloneid = 0;
         
         // create empty universe
         if (hash) {
            algo = new hlifealgo();
            algo->setMaxMemory(maxhashmem);
         } else {
            algo = new qlifealgo();
         }
         algo->setpoll(wxGetApp().Poller());

         // add unique suffix to tempstart
         tempstart += wxString::Format("%d", GetUniqueSuffix());
      }
      
      // inherit current rule in global_liferules (NOT in currlayer->rule)
      rule = wxString(global_liferules.getrule(), wxConvLocal);
      
      // inherit current viewport's size, scale and location
      view = new viewport(100,100);
      view->resize( currlayer->view->getwidth(),
                    currlayer->view->getheight() );
      view->setpositionmag( currlayer->view->x, currlayer->view->y,
                            currlayer->view->getmag() );
      
      // inherit current cursor
      curs = currlayer->curs;

      if (cloning || duplicating) {
         // duplicate all the other current settings
         currname = currlayer->currname;
         dirty = currlayer->dirty;
         stayclean = currlayer->stayclean;
         warp = currlayer->warp;
         autofit = currlayer->autofit;
         hyperspeed = currlayer->hyperspeed;
         showhashinfo = currlayer->showhashinfo;
         originx = currlayer->originx;
         originy = currlayer->originy;
         
         // duplicate selection
         seltop = currlayer->seltop;
         selbottom = currlayer->selbottom;
         selleft = currlayer->selleft;
         selright = currlayer->selright;

         // duplicate the stuff needed to reset pattern
         savestart = currlayer->savestart;
         starthash = currlayer->starthash;
         startrule = currlayer->startrule;
         startx = currlayer->startx;
         starty = currlayer->starty;
         startwarp = currlayer->startwarp;
         startmag = currlayer->startmag;
         startfile = currlayer->startfile;
         startgen = currlayer->startgen;
         currfile = currlayer->currfile;
      }
      
      if (duplicating) {
         // first set same gen count
         algo->setGeneration( currlayer->algo->getGeneration() );
         
         // duplicate pattern
         if ( !currlayer->algo->isEmpty() ) {
            bigint top, left, bottom, right;
            currlayer->algo->findedges(&top, &left, &bottom, &right);
            if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
               Warning(_("Pattern is too big to duplicate."));
            } else {
               viewptr->CopyRect(top.toint(), left.toint(), bottom.toint(), right.toint(),
                                 currlayer->algo, algo, false, _("Duplicating layer"));
            }
         }
         
         // tempstart must remain unique
         if (currlayer->startfile == currlayer->tempstart)
            startfile = tempstart;
         
         // if currlayer->tempstart exists then copy it to this layer's unique tempstart
         if ( wxFileExists(currlayer->tempstart) ) {
            if ( wxCopyFile(currlayer->tempstart, tempstart, true) ) {
               if (currlayer->currfile == currlayer->tempstart)
                  currfile = tempstart;   // starting pattern came from clipboard
            } else {
               Warning(_("Could not copy tempstart file!"));
            }
         }
      }
   }
}

// -----------------------------------------------------------------------------

Layer::~Layer()
{
   if (view) delete view;

   if (cloneid > 0) {
      // count how many layers have the same cloneid
      int clonecount = 0;
      for (int i = 0; i < numlayers; i++) {
         if (layer[i]->cloneid == cloneid) clonecount++;
      }     
      if (clonecount > 2) {
         // only delete this clone
         numclones--;
      } else {
         // first make this cloneid available for the next clone
         cloneavail[cloneid] = true;
         // reset other cloneid to 0 (should only be one such clone)
         for (int i = 0; i < numlayers; i++) {
            // careful -- layer[i] might be this layer
            if (this != layer[i] && layer[i]->cloneid == cloneid)
               layer[i]->cloneid = 0;
         }
         numclones -= 2;
         //!!! debug
         if (clonecount < 2 || numclones < 0) {
            Warning(_("Bug detected deleting clone!"));
         }
      }
      
   } else {
      // not a clone so delete algo and tempstart file
      if (algo) delete algo;
      
      // delete tempstart file if it exists
      if (wxFileExists(tempstart)) wxRemoveFile(tempstart);
      
      // make tempstart suffix available for new layers
      wxString suffix = tempstart.AfterLast('_');
      long val;
      if (suffix.ToLong(&val) && val >= 0 && val < maxlayers) {
         suffavail[val] = true;
      } else {
         Warning(_("Problem with tempstart suffix: ") + tempstart);
      }
   }
}
