// This file is part of Golly.
// See docs/License.html for the copyright notice.

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

#ifdef ENABLE_SOUND
#include <irrKlang.h>               // for sound
using namespace irrklang;
#endif

#include "lua.hpp"

// The overlay is a scriptable graphics layer that is (optionally) drawn
// on top of Golly's current layer.  See Help/overlay.html for details.

// The overlay can be displayed at any corner or in the middle:
typedef enum {
    topleft, topright, bottomright, bottomleft, middle
} overlay_position;

// Multi-line text can be left or right justified or centered:
typedef enum {
    left, right, center
} text_alignment;

// The Clip class is used by some commands (eg. copy, text) to store pixel data
// in a named "clipboard" for later use by other commands (eg. paste, replace):
class Clip;

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
    
    const char* DoOverlayTable(const char* cmd, lua_State* L, int n, int* nresults);
    // Parse and execute the given command.
    // Used for table API commands: fill, get, line, lines, paste, rgba and set

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
    
    unsigned char* GetOverlayData() { return ovpixmap; }
    // Return a pointer to the overlay's RGBA data (possibly NULL).
    // This data is used to render the overlay (see DrawOverlay in
    // wxrender.cpp).
    
    int GetOverlayWidth() { return ovwd; }
    // Return the current pixel width of the overlay.
    
    int GetOverlayHeight() { return ovht; }
    // Return the current pixel height of the overlay.
    
    overlay_position GetOverlayPosition() { return pos; }
    // Return the current position of the overlay.
    
    bool OnlyDrawOverlay();
    // If true then DrawView (in wxrender.cpp) will only draw the overlay.
    
    void SaveOverlay(const wxString& pngpath);
    // Save overlay in given PNG file.

private:
    const char* GetCoordinatePair(char* args, int* x, int* y);
    // Decode a pair of integers from the supplied string.
    // Returns a pointer to the first non-space character after
    // the coordinate pair or NULL if decode failed.

    void SetRenderTarget(unsigned char* pix, int pwd, int pht, Clip* clip);
    // Set the render target pixmap and size.

    const char* DoCreate(const char* args);
    // Create the current render target with the given width and height.
    // All bytes are initialized to 0 (ie. all pixels are transparent).
    // If the render target is the overlay then additionally:
    //   The current RGBA values are all set to 255 (opaque white).
    //   The current position is set to topleft.
    //   The current cursor is set to the standard arrow.
    //   Alpha blending is turned off.
    //   The transformation values are set to 1,0,0,1 (identity).
    //   The current font is set to the default font at 10pt.
    //   No cell view is allocated.

    const char* DoDelete(const char* args);
    // Delete the named clip or if no clip specified delete the overlay.
    // It is an error to attempt to delete a clip if it is the render target.
    
    const char* DoResize(const char* args);
    // Resize the named clip or if no clip specified resize the overlay.
    // Returns the previous size as a string.
    // Does not change any settings and keeps any existing cell view.

    const char* DoPosition(const char* args);
    // Specify where to display the overlay within the current view.

    const char* DoCursor(const char* args);
    // Specify which cursor to use when the mouse moves over a
    // non-transparent pixel and return the old cursor name.
    
    const char* DoTarget(const char* args);
    // Sets the render target to the named clip or the overlay
    // if no clip specified.
    // Returns the old render target clip name or "" for overlay.

    const char* DoSetRGBA(const char* args);
    // Set the current RGBA values and return the old values as a string
    // of the form "r g b a".
    
    void DrawPixel(int x, int y);
    // Called by DoSetPixel, DoLine, etc to set a given pixel
    // that is within the render target.
    
    void DrawAAPixel(int x, int y, double opacity);
    // Draw an antialiased pixel in the render target.

    const char* DoSetPixel(const char* args);
    // Set the given pixel to the current RGBA values.
    // Does nothing if the pixel is outside the edges of the render target.
    
    const char* DoGetPixel(const char* args);
    // Return the RGBA values of the given pixel as a string of the
    // form "r g b a", or "" if the pixel is outside the render target.
    
    const char* DoGetXY();
    // Return the current mouse position within the overlay as a string
    // of the form "x y", or "" if the mouse is outside the overlay.
    
    const char* LineOptionWidth(const char* args);
    // Set linewidth and return previous value as a string.
    
    const char* DoLineOption(const char* args);
    // Set a line option.
    
    void RenderLine(int x0, int y0, int x1, int y1);
    // Called by DoLine to render a line.

    const char* DoLine(const char* args, bool connected);
    // Draw a one or more optionally connected lines using the current RGBA values.
    // Automatically clips any parts of the line outside the render target.
    
    void DrawThickLine(int x0, int y0, int x1, int y1);
    // Called by RenderLine to draw a line when linewidth is > 1.
    // If alphablend is true then the edges will be antialiased.
    
    void PerpendicularX(int x0, int y0, int dx, int dy, int xstep, int ystep,
                        int einit, int winit, double w, double D2);
    // Called by DrawThickLine.
    
    void PerpendicularY(int x0, int y0, int dx, int dy, int xstep, int ystep,
                        int einit, int winit, double w, double D2);
    // Called by DrawThickLine.

    void DrawAntialiasedLine(int x0, int y0, int x1, int y1);
    // Called by DoLine to draw an antialiased line when alphablend is true
    // and linewidth is 1.
    
    const char* DoEllipse(const char* args);
    // Draw an ellipse within the given rectangle.

    void DrawEllipse(int x0, int y0, int x1, int y1);
    // Called by DoEllipse to draw a non-antialiased ellipse when linewidth is 1.
    
    void DrawThickEllipse(int x0, int y0, int x1, int y1);
    // Called by DoEllipse to draw an ellipse when linewidth is > 1.
    // If alphablend is true then the edges will be antialiased.
    
    void DrawAntialiasedEllipse(int x0, int y0, int x1, int y1);
    // Called by DoEllipse to draw an antialiased ellipse when alphablend is true
    // and linewidth is 1.
    
    const char* DoFill(const char* args);
    // Fill a given rectangle with the current RGBA values.
    // Automatically clips any parts of the rectangle outside the render target.

    void FillRect(int x, int y, int w, int h);
    // Called by DoFill to set pixels in given rectangle.
    
    const char* DoCopy(const char* args);
    // Copy the pixels in a given rectangle into Clip data with a given name.
    // The rectangle must be within the render target.
    
    const char* DoPaste(const char* args);
    // Paste the named Clip data into the render target at the given location.
    // Automatically clips any pixels outside the render target.
    
    const char* DoScale(const char* args);
    // Scale the named Clip data into the render target using the given rectangle.
    // Automatically clips any pixels outside the render target.

    void DisableTargetClipIndex();
    // Disable the index on the render target clip (used when it is written to which
    // invalidates the row index).

    const char* DoOptimize(const char* args);
    // Optimize the clip for alpha blended rendering by ignoring any blank rows
    // of pixels (alpha = 0).
    
    const char* DecodeReplaceArg(const char* arg, int* find, bool* negfind, int* replace,
                                 int* invreplace, int* delta, int component);
    // Decodes a single argument for the replace comand.

    const char* DoReplace(const char* args);
    // Replace pixels of a specified RGBA color in the render target with the
    // current RGBA values.
    // Return the number of pixels replaced.

    const char* DoLoad(const char* args);
    // Load the image from a given .bmp/gif/png/tiff file into the render target
    // at a given location, clipping if necessary.  If successful, return the
    // total dimensions of the image as a string of the form "width height".
    
    const char* DoSave(const char* args);
    // Save the pixels in a given rectangle into a given .png file.
    // The rectangle must be within the render target.
    
    const char* DoFlood(const char* args);
    // Do a flood fill starting from the given pixel and using the
    // current RGBA values.
    
    const char* DoBlend(const char* args);
    // Turn alpha blending on or off and return the old setting as a string
    // of the form "1" or "0".
    
    const char* DoFont(const char* args);
    // Set the current font according to the given point size and font name
    // and return the old font as a string of the form "fontsize fontname".
    
    const char* TextOptionAlign(const char* args);
    // Set text alignment per column.

    const char* TextOptionBackground(const char* args);
    // Set text background r, g, b, a color.

    const char* DoTextOption(const char* args);
    // Set a text option.

    const char* DoText(const char* args);
    // Create Clip data with the given name containing the given text.
    // Return the dimensions of the text (which can have one or more lines)
    // as a string of the form "width height descent".
    
    const char* DoTransform(const char* args);
    // Set the affine transformation values used by later paste commands
    // and return the old values as a string of the form "axx axy ayx ayy".
    
    const char* DoUpdate();
    // Set only_draw_overlay to true and then update the current layer
    // so DrawView will only draw the overlay if OnlyDrawOverlay() is true.

#ifdef ENABLE_SOUND
    const char* SoundStop(const char* args);
    // Stop sound playback for specified sound or all sounds if none specified.

    const char* SoundPlay(const char* args, bool loop);
    // Play the specified sound once or in a loop.

    const char* SoundState(const char* args);
    // Returns whether the specified sound or any sound (if none specified) is playing.
    // Can be "playing", "stopped", or "unknown" if the specified sound is not found.

    const char* SoundVolume(const char* args);
    // Sets the volume of the specified sound.

    const char* SoundPause(const char* args);
    // Pauses all or the specified sound.

    const char* SoundResume(const char* args);
    // Resumes all or the specified sound.
#endif

    const char* DoSound(const char* args);
    // Play, stop, pause or resume playback, get playback status, or set volume.

    const char* OverlayError(const char* msg);
    // Return a string starting with "ERR:" followed by the given message.

    void SetRGBA(unsigned char r, unsigned char b, unsigned char g, unsigned char a, unsigned int *rgba);
    // Set the rgba value from the r, b, g, a components.

    void GetRGBA(unsigned char *r, unsigned char *g, unsigned char *b, unsigned char *a, unsigned int rgba);
    // Get the r, g, b, a components from the rgba value.

    // cell view

    const char* DoCellView(const char* args);
    // Create a cell view that tracks a rectangle of cells and can be rapidly
    // drawn onto the the render target at a particular scale and angle.

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
    // Draw cells onto the render target with the given location mask and rotation.

    void DrawCellsNoRotate(unsigned char* cells, int mask);
    // Draw cells onto the render target with the given location mask but no rotation.

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

    // overlay table API calls

    const char* DoFillTable(lua_State* L, int n, int* nresults);
    const char* DoGetTable(lua_State* L, int n, int* nresults);
    const char* DoLineTable(lua_State* L, int n, bool connected, int* nresults);
    const char* DoPasteTable(lua_State* L, int n, int* nresults);
    const char* DoPasteTableInternal(const int* coords, int n, const char* clip);
    const char* DoSetTable(lua_State* L, int n, int* nresults);
    const char* DoSetRGBATable(const char* cmd, lua_State* L, int n, int* nresults);

    // render target
    unsigned char* pixmap;          // current render target RGBA data (wd * ht * 4 bytes)
    int wd, ht;                     // current render target pixmap width and height
    Clip* renderclip;               // clip if render target
    std::string targetname;         // render target name

    // overlay
    unsigned char* ovpixmap;        // overlay RGBA data
    int ovwd, ovht;                 // width and height of overlay pixmap
    unsigned char r, g, b, a;       // current RGBA values for drawing pixels
    bool alphablend;                // do alpha blending when drawing translucent pixels?
    bool only_draw_overlay;         // set by DoUpdate, reset by OnlyDrawOverlay
    overlay_position pos;           // where to display overlay
    const wxCursor* ovcursor;       // cursor to use when mouse is in overlay
    std::string cursname;           // remember cursor name
    int axx, axy, ayx, ayy;         // affine transformation values
    bool identity;                  // true if transformation values are 1,0,0,1
    int linewidth;                  // for lines and ellipses
    
    std::map<std::string,Clip*> clips;
    // named Clip data created by DoCopy or DoText and used by DoPaste

#ifdef ENABLE_SOUND
    // sound
    std::map<std::string,ISound*> sounds;
    ISoundEngine* engine;
#endif

    // text
    wxFont currfont;                // current font used by text command
    std::string fontname;           // name of current font
    int fontsize;                   // size of current font
    text_alignment align;           // text alignment
    unsigned int textbgRGBA;        // text background color

    // cell view
    unsigned int cellRGBA[256];     // cell RGBA values
    unsigned char* cellview;        // cell state data (cellwd * cellht bytes)
    unsigned char* cellview1;       // cell state data (cellwd * cellht bytes) double buffer
    unsigned char* zoomview;        // cell state data (cellwd * cellht bytes) for zoom out
    int cellwd, cellht;             // width and height of cell view
    int cellx, celly;               // x and y position of bottom left cell
    bool ishex;                     // whether to display in hex mode

    // camera
    double camx;                    // camera x position
    double camy;                    // camera y position
    double camzoom;                 // camera zoom
    double camangle;                // camera angle
    int camlayers;                  // camera layers
    double camlayerdepth;           // camera layer depth

    // theme
    bool theme;                     // whether a theme is active
    unsigned int aliveStartRGBA;    // cell just born RGBA
    unsigned int aliveEndRGBA;      // cell alive longest RGBA
    unsigned int deadStartRGBA;     // cell just died RGBA
    unsigned int deadEndRGBA;       // cell dead longest RGBA
    unsigned int unoccupiedRGBA;    // cell never occupied RGBA
    unsigned int borderRGBA;        // border RGBA
    unsigned char bordera;          // border alpha (RGB comes from View Settings)

    // grid
    bool grid;                      // whether to display grid lines
    int gridmajor;                  // major grid line interval
    unsigned int gridRGBA;          // grid line color
    unsigned int gridmajorRGBA;     // major grid line color
    bool customgridcolor;           // whether grid line color is custom
    bool customgridmajorcolor;      // whether major grid line color is custom

    // stars
    double* starx;                  // star x coordinates
    double* stary;                  // star y coordinates
    double* starz;                  // star z coordinates
    unsigned int starRGBA;          // star color
    bool stars;                     // whether to display stars
};

extern Overlay* curroverlay;    // pointer to current overlay (set by client)

#endif
