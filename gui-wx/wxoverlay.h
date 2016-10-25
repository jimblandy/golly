/*** /
 
 This file is part of Golly, a Game of Life Simulator.
 Copyright (C) 2013 Andrew Trevorrow and Tomas Rokicki.
 
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

#ifndef _WXOVERLAY_H_
#define _WXOVERLAY_H_

#include <string>                   // for std::string
#ifdef _MSC_VER
    #pragma warning(disable:4702)   // disable "unreachable code" warnings from MSVC
#endif
#include <map>                      // for std::map
#ifdef _MSC_VER
    #pragma warning(default:4702)   // enable "unreachable code" warnings
#endif

// maximum columns
const int maxcols = 10;

// maximum length delimiter string
const int maxdelim = 10;

// The overlay is a scriptable graphics layer that is (optionally) drawn
// on top of Golly's current layer.

// The overlay can be displayed at any corner or in the middle:
typedef enum {
    topleft, topright, bottomright, bottomleft, middle
} overlay_position;

// Multiline text can be left or right justified or centered.
typedef enum {
    left, right, center
} text_alignment;

// The Clip class is used by the copy and text commands to store pixel data
// in a named "clipboard" for later use by the paste command:
class Clip {
public:
    Clip(int w, int h) {
        cwd = w;
        cht = h;
        cdata = (unsigned char*) malloc(cwd * cht * 4);
    }
    ~Clip() {
        if (cdata) free(cdata);
    }
    unsigned char* cdata;   // RGBA data (cwd * cht * 4 bytes)
    int cwd, cht;
};

class Overlay {
public:
    Overlay();
    ~Overlay();
    
    const char* DoOverlayCommand(const char* cmd);
    // Parse and execute the given command.
    // If an error is detected then return a string starting with "ERR:"
    // and immediately followed by an appropriate error message.
    // If no error then return either a string containing the results
    // of the command, or NULL if there are no results.
    
    void DeleteOverlay();
    // Deallocate all memory used by the overlay and reset pixmap to NULL.

    bool PointInOverlay(int vx, int vy, int* ox, int* oy);
    // Return true if the given pixel location within the current view
    // is within the overlay.  If so then return the overlay location.

    bool TransparentPixel(int x, int y);
    // Return true if the given pixel is within the overlay and completely
    // transparent (ie. its alpha value is 0).

    void SetOverlayCursor();
    // Set appropriate cursor for when the mouse is over a non-transparent
    // pixel in the overlay.

    void CheckCursor();
    // If the pixmap has changed then the cursor might need to be changed.
    
    unsigned char* GetOverlayData() { return pixmap; }
    // Return a pointer to the overlay's RGBA data (possibly NULL).
    // This data is used to render the overlay (see DrawOverlay in
    // wxrender.cpp).
    
    int GetOverlayWidth() { return wd; }
    // Return the current pixel width of the overlay.
    
    int GetOverlayHeight() { return ht; }
    // Return the current pixel height of the overlay.
    
    overlay_position GetOverlayPosition() { return pos; }
    // Return the current position of the overlay.
    
    bool OnlyDrawOverlay();
    // If true then DrawView (in wxrender.cpp) will only draw the overlay.

private:
    const char* DoCreate(const char* args);
    // Create a pixmap with the given width and height.
    // All bytes are initialized to 0 (ie. all pixels are transparent).
    // The current RGBA values are all set to 255 (opaque white).
    // The current position is set to topleft.
    // The current cursor is set to the standard arrow.
    // Alpha blending is turned off.
    // The transformation values are set to 1,0,0,1 (identity).
    // The current font is set to the default font at 10pt.
    // No cell view is allocated.
    
    const char* DoResize(const char* args);
    // Resizes a pixmap with the given width and height
    // Does not change any settings and keeps any existing cell view.

    const char* DoPosition(const char* args);
    // Specify where to display the overlay within the current view.

    const char* DoCursor(const char* args);
    // Specify which cursor to use when the mouse moves over a
    // non-transparent pixel and return the old cursor name.
    
    const char* DoSetRGBA(const char* args);
    // Set the current RGBA values and return the old values as a string
    // of the form "r g b a".
    
    void DrawPixel(int x, int y);
    // Called by DoSetPixel, DoLine, etc to set a given pixel
    // that is within the overlay.
    
    const char* DoSetPixel(const char* args);
    // Set the given pixel to the current RGBA values.
    // Does nothing if the pixel is outside the edges of the overlay.
    
    const char* DoGetPixel(const char* args);
    // Return the RGBA values of the given pixel as a string of the
    // form "r g b a", or "" if the pixel is outside the overlay.
    
    const char* DoGetXY();
    // Return the current mouse position within the overlay as a string
    // of the form "x y", or "" if the mouse is outside the overlay.
    
    const char* DoLine(const char* args);
    // Draw a line using the current RGBA values.
    // Automatically clips any parts of the line outside the overlay.
    
    void FillRect(int x, int y, int w, int h);
    // Called by DoFill to set pixels in given rectangle.
    
    const char* DoFill(const char* args);
    // Fill a given rectangle with the current RGBA values.
    // Automatically clips any parts of the rectangle outside the overlay.
    
    const char* DoCopy(const char* args);
    // Copy the pixels in a given rectangle into Clip data with a given name.
    // The rectangle must be within the overlay.
    
    const char* DoPaste(const char* args);
    // Paste the named Clip data into the overlay at the given location.
    // Automatically clips any pixels outside the overlay.
    
    const char* DoFreeClip(const char* args);
    // Free the memory used by the named Clip.
    
    const char* DoLoad(const char* args);
    // Load the image from a given .bmp/gif/png/tiff file into the overlay
    // at a given location, clipping if necessary.  If successful, return the
    // total dimensions of the image as a string of the form "width height".
    
    const char* DoSave(const char* args);
    // Save the pixels in a given rectangle into a given .png file.
    // The rectangle must be within the overlay.
    
    const char* DoFlood(const char* args);
    // Do a flood fill starting from the given pixel and using the
    // current RGBA values.
    
    const char* DoBlend(const char* args);
    // Turn alpha blending on or off and return the old setting as a string
    // of the form "1" or "0".
    
    const char* DoFont(const char* args);
    // Set the current font according to the given point size and font name
    // and return the old font as a string of the form "fontsize fontname".
    
    void SetTextColumnDefaults();
    // Set text default widths and alignments.

    const char* TextOptionAlign(const char* args);
    // Set text alignment per column.

    const char* TextOptionBackground(const char* args);
    // Set text background r, g, b, a color.

    const char* TextOptionColumns(const char* args);
    // Set number of text columns.

    const char* TextOptionDelimiter(const char* args);
    // Set text column delimiter string.

    const char* TextOptionShadow(const char* args);
    // Set text shadow r, g, b, a color.

    const char* TextOptionWidth(const char* args);
    // Set text width per column.

    const char* DoTextOption(const char* args);
    // Set a text option.

    const char* DoText(const char* args);
    // Create Clip data with the given name containing the given text.
    // Return the dimensions of the text as a string of the form
    // "width height descent leading".
    
    const char* DoTransform(const char* args);
    // Set the affine transformation values used by later paste commands
    // and return the old values as a string of the form "axx axy ayx ayy".
    
    const char* DoUpdate();
    // Set only_draw_overlay to true and then update the current layer
    // so DrawView will only draw the overlay if OnlyDrawOverlay() is true.

    const char* OverlayError(const char* msg);
    // Return a string starting with "ERR:" followed by the given message.

    void SetRGBA(unsigned char r, unsigned char b, unsigned char g, unsigned char a, unsigned int *rgba);
    // Set the rgba value from the r, b, g, a components.

    void GetRGBA(unsigned char *r, unsigned char *g, unsigned char *b, unsigned char *a, unsigned int rgba);
    // Get the r, g, b, a components from the rgba value.

    // cell view

    const char* DoCellView(const char* args);
    // Create a cell view that tracks a rectangle of cells and can be rapidly
    // drawn onto the overlay at a particular scale and angle.

    void DeleteCellView();
    // Deallocate all memory used by the cell view.

    void RefreshCellView();
    // Refresh the cell view from the universe.

    void RefreshCellViewWithTheme();
    // Refresh the cell view from the universe using the theme.

    void GetPatternColors();
    // Get the state colors from the pattern.

    void GetThemeColors(double brightness);
    // Get the state colors from the theme.

    const char* DoUpdateCells();
    // Update the cell view from the universe.

    void UpdateZoomView(unsigned char* source, unsigned char *dest, int step);
    // Update the zoom view from the cell view at the given step size.

    void DrawCellsRotate(unsigned char* cells, int mask, double angle);
    // Draw cells onto the overlay with the given location mask and rotation.

    void DrawCellsNoRotate(unsigned char* cells, int mask);
    // Draw cells onto the overlay with the given location mask but no rotation.

    const char* DoDrawCells();
    // Draw the cells onto the overlay.

    // camera

    const char* CamAngle(const char* args);
    // Set the camera angle.

    const char* CamZoom(const char* args);
    // Set the camera zoom.

    const char* CamXY(const char* args);
    // Set the camera position.

    const char* DoCamera(const char* args);
    // Set a camera parameter.

    // celloption

    const char* CellOptionDepth(const char* args);
    // Set the cellview layer depth.

    const char* CellOptionLayers(const char* args);
    // Set the cellview number of layers.

    const char* CellOptionHex(const char* args);
    // Set hex display mode on or off for the cellview.

    const char* CellOptionGrid(const char* args);
    // Turn grid display on or off.

    const char* CellOptionGridMajor(const char* args);
    // Set the grid major interval.

    const char* CellOptionStars(const char* args);
    // Set the stars display on or off.

    const char* DoCellOption(const char* args);
    // Set a cellview option.
    
    // theme

    const char* DoTheme(const char* args);
    // Set the color theme RGB values in the form
    // "r1 g1 b1 r2 g2 b2 r3 g3 b3 r4 g4 b4 r5 g5 b5" representing RGB values for cells:
    // r1 g1 b1  just born
    // r2 g2 b2  alive at least 63 generations
    // r3 g3 b3  just died
    // r4 g4 b4  dead at least 63 generations
    // r5 g5 b5  never occupied
    // If a single parameter "-1" is supplied then disable the theme and
    // use the patterns default colors.

    // grid

    void DrawVLine(int x, int y1, int y2, unsigned int color);
    // Draw vertical line.

    void DrawHLine(int x1, int x2, int y, unsigned int color);
    // Draw horizontal line.

    void DrawGridLines();
    // Draw grid lines.

    // stars

    void DrawStars(double angle);
    // Draw the stars.

    void CreateStars();
    // Allocate and position the stars.

    void DeleteStars();
    // Free the memory used by the stars.

    unsigned char* pixmap;         // RGBA data (wd * ht * 4 bytes)
    int wd, ht;                    // width and height of pixmap
    unsigned char r, g, b, a;      // current RGBA values for drawing pixels
    bool alphablend;               // do alpha blending when drawing translucent pixels?
    bool only_draw_overlay;        // set by DoUpdate, reset by OnlyDrawOverlay
    overlay_position pos;          // where to display overlay
    const wxCursor* ovcursor;      // cursor to use when mouse is in overlay
    std::string cursname;          // remember cursor name
    int axx, axy, ayx, ayy;        // affine transformation values
    bool identity;                 // true if transformation values are 1,0,0,1

    // text

    wxFont currfont;               // current font used by text command
    std::string fontname;          // name of current font
    int fontsize;                  // size of current font
    text_alignment align[maxcols]; // text alignment (per column)
    int width[maxcols];            // text width (per column)
    int columns;                   // number of text columns
    unsigned int textbgRGBA;       // text background color
    unsigned int textshadowRGBA;   // text shadow color
    int textshadowx;               // text shadow x offset
    int textshadowy;               // text shadow y offset
    bool textshadow;               // whether shadow enabled
    char textdelim[maxdelim];      // text column delimiter
    
    std::map<std::string,Clip*> clips;
    // named Clip data created by DoCopy or DoText and used by DoPaste

    // cell view

    unsigned int cellRGBA[256]; // cell RGBA values
    unsigned char* cellview;    // cell state data (cellwd * cellht bytes)
    unsigned char* zoomview;    // cell state data (cellwd * cellht bytes) for zoom out
    int cellwd, cellht;         // width and height of cell view
    int cellx, celly;           // x and y position of bottom left cell
    bool ishex;                 // whether to display in hex mode

    // camera

    double camx;                 // camera x position
    double camy;                 // camera y position
    double camzoom;              // camera zoom
    double camangle;             // camera angle
    int camlayers;               // camera layers
    double camlayerdepth;        // camera layer depth

    // theme

    bool theme;                  // whether a theme is active
    unsigned int aliveStartRGBA; // new cell RGBA
    unsigned int aliveEndRGBA;   // cell alive longest RGBA
    unsigned int deadStartRGBA;  // cell just died RGBA
    unsigned int deadEndRGBA;    // cell dead longest RGBA
    unsigned int unoccupiedRGBA; // cell never occupied RGBA

    // grid
    bool grid;                   // whether to display grid lines
    int gridmajor;               // major grid line interval
    unsigned int gridRGBA;       // grid line color
    unsigned int gridmajorRGBA;  // major grid line color
    bool customgridcolor;        // whether grid line color is custom
    bool customgridmajorcolor;   // whether major grid line color is custom

    // stars
    double* starx;               // star x coordinates
    double* stary;               // star y coordinates
    double* starz;               // star z coordinates
    unsigned int starRGBA;       // star color
    bool stars;                  // whether to display stars
};

extern Overlay* curroverlay;    // pointer to current overlay (set by client)

#endif
