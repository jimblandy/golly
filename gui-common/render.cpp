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

/* -------------------- Some notes on Golly's display code ---------------------

 The rectangular area used to display patterns is called the viewport.
 All drawing in the viewport is done in this module using OpenGL ES.

 The main rendering routine is DrawPattern() -- see the end of this module.
 DrawPattern() does the following tasks:

 - Calls currlayer->algo->draw() to draw the current pattern.  It passes
   in renderer, an instance of golly_render (derived from liferender) which
   has these methods:
   - pixblit() draws a pixmap containing at least one live cell.
   - getcolors() provides access to the current layer's color arrays.

   Note that currlayer->algo->draw() does all the hard work of figuring
   out which parts of the viewport are dead and building all the pixmaps
   for the live parts.  The pixmaps contain suitably shrunken images
   when the scale is < 1:1 (ie. mag < 0).

 - Calls DrawGridLines() to overlay grid lines if they are visible.

 - Calls DrawGridBorder() to draw border around a bounded universe.

 - Calls DrawSelection() to overlay a translucent selection rectangle
   if a selection exists and any part of it is visible.

 - If the user is doing a paste, DrawPasteImage() draws the paste pattern
   stored in pastealgo.

 ----------------------------------------------------------------------------- */

#include "bigint.h"
#include "lifealgo.h"
#include "viewport.h"

#include "utils.h"       // for Warning, Fatal, etc
#include "prefs.h"       // for showgridlines, mingridmag, swapcolors, etc
#include "algos.h"       // for gBitmapPtr
#include "layer.h"       // currlayer, GetLayer, etc
#include "view.h"        // nopattupdate, waitingforpaste, pasterect, pastex, pastey, etc
#include "render.h"

#ifdef ANDROID_GUI
    // the Android version uses OpenGL ES 1
    #include <GLES/gl.h>
#endif

#ifdef IOS_GUI
    // the iOS version uses OpenGL ES 1
    #import <OpenGLES/ES1/gl.h>
    #import <OpenGLES/ES1/glext.h>
#endif

#ifdef WEB_GUI
    // the web version uses OpenGL ES 2
    #include <GLES2/gl2.h>
#endif

// -----------------------------------------------------------------------------

// local data used in golly_render routines:

static int currwd, currht;              // current width and height of viewport, in pixels
static unsigned char** icontextures;    // pointers to texture data for each icon
static GLuint texture8 = 0;             // texture for drawing 7x7 icons
static GLuint texture16 = 0;            // texture for drawing 15x15 icons
static GLuint texture32 = 0;            // texture for drawing 31x31 icons
static GLuint patternTexture = 0;       // texture for drawing pattern bitmaps at 1:1 scale

// fixed texture coordinates used by glTexCoordPointer
static const GLshort texture_coordinates[] = { 0,0, 1,0, 0,1, 1,1 };

// for drawing paste pattern
static lifealgo* pastealgo;             // universe containing paste pattern
static gRect pastebbox;                 // bounding box in cell coords (not necessarily minimal)
static bool drawing_paste = false;      // in DrawPasteImage?

/*!!!
// for drawing multiple layers
static int layerwd = -1;                // width of layer bitmap
static int layerht = -1;                // height of layer bitmap
static wxBitmap* layerbitmap = NULL;    // layer bitmap
*/

// -----------------------------------------------------------------------------

#ifdef WEB_GUI
    // the following 2 macros convert x,y positions in Golly's preferred coordinate
    // system (where 0,0 is top left corner of viewport and bottom right corner is
    // currwd,currht) into OpenGL ES 2's normalized coordinates (where 0.0,0.0 is in
    // middle of viewport, top right corner is 1.0,1.0 and bottom left corner is -1.0,-1.0)
    #define XCOORD(x)  (2.0 * (x) / float(currwd) - 1.0)
    #define YCOORD(y) -(2.0 * (y) / float(currht) - 1.0)
#else
    // the following 2 macros don't need to do anything because we've already
    // changed the viewport coordinate system to what Golly wants
    #define XCOORD(x) x
    #define YCOORD(y) y
#endif

// -----------------------------------------------------------------------------

#ifdef WEB_GUI

static GLuint LoadShader(GLenum type, const char* shader_source)
{
    // create a shader object, load the shader source, and compile the shader
    
    GLuint shader = glCreateShader(type);
    if (shader == 0) return 0;
    
    glShaderSource(shader, 1, &shader_source, NULL);
    
    glCompileShader(shader);
    
    // check the compile status
    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        Warning("Error compiling shader!");
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

// -----------------------------------------------------------------------------

static GLuint pointProgram;         // program object for drawing points, lines, rects
static GLint positionLoc;           // location of v_Position attribute
static GLint pointSizeLoc;          // location of PointSize uniform
static GLint lineColorLoc;          // location of LineColor uniform

static GLuint textureProgram;       // program object for drawing 2D textures
static GLint texPosLoc;             // location of a_Position attribute
static GLint texCoordLoc;           // location of a_texCoord attribute
static GLint samplerLoc;            // location of s_texture uniform

bool InitOGLES2()
{
    // initialize the shaders and program objects required by OpenGL ES 2
    // (based on OpenGL's Hello_Triangle.c and Simple_Texture2D.c)
    
    // vertex shader used in pointProgram
    GLbyte v1ShaderStr[] =
        "attribute vec4 v_Position;   \n"
        "uniform float PointSize;     \n"
        "void main() {                \n"
        "    gl_Position = v_Position;\n"
        "    gl_PointSize = PointSize;\n"
        "}                            \n";
    
    // fragment shader used in pointProgram
    GLbyte f1ShaderStr[] =
        "uniform lowp vec4 LineColor; \n"
        "void main() {                \n"
        "    gl_FragColor = LineColor;\n"
        "}                            \n";
    
    // vertex shader used in textureProgram
    GLbyte v2ShaderStr[] =
        "attribute vec4 a_Position;   \n"
        "attribute vec2 a_texCoord;   \n"
        "varying vec2 v_texCoord;     \n"
        "void main() {                \n"
        "    gl_Position = a_Position;\n"
        "    v_texCoord = a_texCoord; \n"
        "}                            \n";
   
    // fragment shader used in textureProgram
    GLbyte f2ShaderStr[] =
        "precision mediump float;                            \n"
        "varying vec2 v_texCoord;                            \n"
        "uniform sampler2D s_texture;                        \n"
        "void main()                                         \n"
        "{                                                   \n"
        "    gl_FragColor = texture2D(s_texture, v_texCoord);\n"
        "}                                                   \n";
    
    GLuint vertex1Shader, fragment1Shader;
    GLuint vertex2Shader, fragment2Shader;
    
    // load the vertex/fragment shaders
    vertex1Shader = LoadShader(GL_VERTEX_SHADER, (const char*) v1ShaderStr);
    vertex2Shader = LoadShader(GL_VERTEX_SHADER, (const char*) v2ShaderStr);
    fragment1Shader = LoadShader(GL_FRAGMENT_SHADER, (const char*) f1ShaderStr);
    fragment2Shader = LoadShader(GL_FRAGMENT_SHADER, (const char*) f2ShaderStr);
    
    // create the program objects
    pointProgram = glCreateProgram();
    if (pointProgram == 0) return false;

    textureProgram = glCreateProgram();
    if (textureProgram == 0) return false;

    glAttachShader(pointProgram, vertex1Shader);
    glAttachShader(pointProgram, fragment1Shader);

    glAttachShader(textureProgram, vertex2Shader);
    glAttachShader(textureProgram, fragment2Shader);
    
    // link the program objects
    glLinkProgram(pointProgram);
    glLinkProgram(textureProgram);
    
    // check the link status
    GLint plinked;
    glGetProgramiv(pointProgram, GL_LINK_STATUS, &plinked);
    if (!plinked) {
        Warning("Error linking pointProgram!");
        glDeleteProgram(pointProgram);
        return false;
    }
    
    GLint tlinked;
    glGetProgramiv(textureProgram, GL_LINK_STATUS, &tlinked);
    if (!tlinked) {
        Warning("Error linking textureProgram!");
        glDeleteProgram(textureProgram);
        return false;
    }
    
    // get the attribute and uniform locations
    // (note that for IE 11 we must set the appropriate program???!!!)
    glUseProgram(pointProgram);
    positionLoc = glGetAttribLocation(pointProgram, "v_Position");
    pointSizeLoc = glGetUniformLocation(pointProgram, "PointSize");
    lineColorLoc = glGetUniformLocation(pointProgram, "LineColor");
    if (positionLoc == -1 || pointSizeLoc == -1 || lineColorLoc == -1) {
        Warning("Failed to get a location in pointProgram!");
        glDeleteProgram(pointProgram);
        return false;
    }
    
    glUseProgram(textureProgram);
    texPosLoc = glGetAttribLocation(textureProgram, "a_Position");
    texCoordLoc = glGetAttribLocation(textureProgram, "a_texCoord");
    samplerLoc = glGetUniformLocation (textureProgram, "s_texture");
    if (texPosLoc == -1 || texCoordLoc == -1 || samplerLoc == -1) {
        Warning("Failed to get a location in textureProgram!");
        glDeleteProgram(textureProgram);
        return false;
    }
    
    // create buffer for vertex data
    GLuint vertexPosObject;
    glGenBuffers(1, &vertexPosObject);
    glBindBuffer(GL_ARRAY_BUFFER, vertexPosObject);

    // create buffer for index data
    GLuint indexObject;
    GLushort indices[] = { 0, 1, 2, 0, 2, 3 };
    glGenBuffers(1, &indexObject);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexObject);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    
    // use the pointProgram initially
    glUseProgram(pointProgram);
    
    return true;
}

#endif // WEB_GUI

// -----------------------------------------------------------------------------

static void SetColor(int r, int g, int b, int a)
{
    #ifdef WEB_GUI
        GLfloat color[4];
        color[0] = r/255.0;
        color[1] = g/255.0;
        color[2] = b/255.0;
        color[3] = a/255.0;
        glUniform4fv(lineColorLoc, 1, color);
    #else
        glColor4ub(r, g, b, a);
    #endif
}

// -----------------------------------------------------------------------------

static void SetPointSize(int ptsize)
{
    #ifdef WEB_GUI
        glUniform1f(pointSizeLoc, float(ptsize));
    #else
        glPointSize(ptsize);
    #endif
}

// -----------------------------------------------------------------------------

static void FillRect(int x, int y, int wd, int ht)
{
    GLfloat rect[] = {
        XCOORD(x),    YCOORD(y+ht),  // left, bottom
        XCOORD(x+wd), YCOORD(y+ht),  // right, bottom
        XCOORD(x+wd), YCOORD(y),     // right, top
        XCOORD(x),    YCOORD(y),     // left, top
    };
    #ifdef WEB_GUI
        glBufferData(GL_ARRAY_BUFFER, 8 * sizeof(GLfloat), rect, GL_STATIC_DRAW);
        glVertexAttribPointer(positionLoc, 2, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(positionLoc);
    #else
        glVertexPointer(2, GL_FLOAT, 0, rect);
    #endif
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

// -----------------------------------------------------------------------------

static void DisableTextures()
{
    #ifdef WEB_GUI
        // don't do anything (avoid WebGL warnings)
    #else
        if (glIsEnabled(GL_TEXTURE_2D)) {
            glDisable(GL_TEXTURE_2D);
        };
    #endif
}

// -----------------------------------------------------------------------------

void DrawTexture(unsigned char* rgbdata, int x, int y, int w, int h)
{
    // called from ios_render::pixblit to draw a pattern bitmap at 1:1 scale

	if (patternTexture == 0) {
        // only need to create texture name once
        glGenTextures(1, &patternTexture);
    }

    #ifdef WEB_GUI
        glUseProgram(textureProgram);
        glActiveTexture(GL_TEXTURE0);
        // bind our texture
        glBindTexture(GL_TEXTURE_2D, patternTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    #else
        if (!glIsEnabled(GL_TEXTURE_2D)) {
            // restore texture color and enable textures
            SetColor(255, 255, 255, 255);
            glEnable(GL_TEXTURE_2D);
            // bind our texture
            glBindTexture(GL_TEXTURE_2D, patternTexture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexCoordPointer(2, GL_SHORT, 0, texture_coordinates);
        }
    #endif
    
    // update the texture with the new bitmap data (in RGB format)
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, rgbdata);

    #ifdef WEB_GUI
        GLfloat vertices[] = {
            XCOORD(x),     YCOORD(y), 0.0,      // Position 0 = left,top
            0.0,  0.0,                          // TexCoord 0
            XCOORD(x),     YCOORD(y + h), 0.0,  // Position 1 = left,bottom
            0.0,  1.0,                          // TexCoord 1
            XCOORD(x + h), YCOORD(y + h), 0.0,  // Position 2 = right,bottom
            1.0,  1.0,                          // TexCoord 2
            XCOORD(x + h), YCOORD(y), 0.0,      // Position 3 = right,top
            1.0,  0.0                           // TexCoord 3
        };
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        
        glVertexAttribPointer(texPosLoc, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GL_FLOAT), (const GLvoid*)(0));
        glVertexAttribPointer(texCoordLoc, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GL_FLOAT), (const GLvoid*)(3 * sizeof(GL_FLOAT)));
        glEnableVertexAttribArray(texPosLoc);
        glEnableVertexAttribArray(texCoordLoc);
        
        glUniform1i(samplerLoc, 0);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

        glUseProgram(pointProgram);
    #else
        // we assume w and h are powers of 2
        GLfloat vertices[] = {
            x,   y,
            x+w, y,
            x,   y+h,
            x+w, y+h,
        };
        glVertexPointer(2, GL_FLOAT, 0, vertices);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    #endif
}

// -----------------------------------------------------------------------------

void DrawPoints(unsigned char* rgbdata, int x, int y, int w, int h)
{
    // called from golly_render::pixblit to draw pattern at 1:1 scale
    // if numstates is 2 or we're drawing the paste image

    const int maxcoords = 1024;     // must be multiple of 2
    GLfloat points[maxcoords];
    int numcoords = 0;

    DisableTextures();
    SetPointSize(1);

    unsigned char deadr = currlayer->cellr[0];
    unsigned char deadg = currlayer->cellg[0];
    unsigned char deadb = currlayer->cellb[0];
    unsigned char prevr = deadr;
    unsigned char prevg = deadg;
    unsigned char prevb = deadb;
    SetColor(deadr, deadg, deadb, 255);

    unsigned char r, g, b;
    int i = 0;
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            r = rgbdata[i++];
            g = rgbdata[i++];
            b = rgbdata[i++];
            if (r != deadr || g != deadg || b != deadb) {
                // we've got a live pixel
                bool changecolor = (r != prevr || g != prevg || b != prevb);
                if (changecolor || numcoords == maxcoords) {
                    if (numcoords > 0) {
                        #ifdef WEB_GUI
                            glBufferData(GL_ARRAY_BUFFER, numcoords * sizeof(GLfloat), points, GL_STATIC_DRAW);
                            glVertexAttribPointer(positionLoc, 2, GL_FLOAT, GL_FALSE, 0, 0);
                            glEnableVertexAttribArray(positionLoc);
                        #else
                            glVertexPointer(2, GL_FLOAT, 0, points);
                        #endif
                        glDrawArrays(GL_POINTS, 0, numcoords/2);
                        numcoords = 0;
                    }
                    if (changecolor) {
                        prevr = r;
                        prevg = g;
                        prevb = b;
                        SetColor(r, g, b, 255);
                    }
                }
                points[numcoords++] = XCOORD(x + col + 0.5);
                points[numcoords++] = YCOORD(y + row + 0.5);
            }
        }
    }

    if (numcoords > 0) {
        #ifdef WEB_GUI
            glBufferData(GL_ARRAY_BUFFER, numcoords * sizeof(GLfloat), points, GL_STATIC_DRAW);
            glVertexAttribPointer(positionLoc, 2, GL_FLOAT, GL_FALSE, 0, 0);
            glEnableVertexAttribArray(positionLoc);
        #else
            glVertexPointer(2, GL_FLOAT, 0, points);
        #endif
        glDrawArrays(GL_POINTS, 0, numcoords/2);
    }
}

// -----------------------------------------------------------------------------

static int prevsize = 0;

void DrawIcons(unsigned char* statedata, int x, int y, int w, int h, int pmscale, int stride)
{
    // called from golly_render::pixblit to draw icons for each live cell;
    // assume pmscale > 2 (should be 8, 16 or 32 or 64)
    int iconsize = pmscale;

    // on high density screens the max scale is 1:64, but instead of
    // supporting 63x63 icons we simply scale up the 31x31 icons
    // (leaving a barely noticeable 1px gap at the right and bottom edges)
    if (pmscale == 64) {
        iconsize = 32;
    }

    // create icon textures once
	if (texture8 == 0) glGenTextures(1, &texture8);
	if (texture16 == 0) glGenTextures(1, &texture16);
	if (texture32 == 0) glGenTextures(1, &texture32);

    #ifdef WEB_GUI
        glUseProgram(textureProgram);
        glActiveTexture(GL_TEXTURE0);
        prevsize = 0;                   // always rebind
    #else
        if (!glIsEnabled(GL_TEXTURE_2D)) {
            // restore texture color and enable textures
            SetColor(255, 255, 255, 255);
            glEnable(GL_TEXTURE_2D);
            prevsize = 0;               // force rebinding
        }
    #endif
    
    if (iconsize != prevsize) {
        prevsize = iconsize;
        // bind appropriate icon texture
        if (iconsize == 8) glBindTexture(GL_TEXTURE_2D, texture8);
        if (iconsize == 16) glBindTexture(GL_TEXTURE_2D, texture16);
        if (iconsize == 32) glBindTexture(GL_TEXTURE_2D, texture32);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        #ifdef WEB_GUI
            // no need to do anything here
        #else
            glTexCoordPointer(2, GL_SHORT, 0, texture_coordinates);
        #endif
    }

    int prevstate = 0;
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            unsigned char state = statedata[row*stride + col];
            if (state > 0 && icontextures[state]) {
                
                if (state != prevstate) {
                    prevstate = state;
                    // update the texture with the new icon data (in RGBA format)
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, iconsize, iconsize, 0, GL_RGBA, GL_UNSIGNED_BYTE, icontextures[state]);
                }
                
                int xpos = x + col * pmscale;
                int ypos = y + row * pmscale;
                
                #ifdef WEB_GUI
                    GLfloat vertices[] = {
                        XCOORD(xpos),           YCOORD(ypos), 0.0,              // Position 0 = left,top
                        0.0,  0.0,                                              // TexCoord 0
                        XCOORD(xpos),           YCOORD(ypos + pmscale), 0.0,    // Position 1 = left,bottom
                        0.0,  1.0,                                              // TexCoord 1
                        XCOORD(xpos + pmscale), YCOORD(ypos + pmscale), 0.0,    // Position 2 = right,bottom
                        1.0,  1.0,                                              // TexCoord 2
                        XCOORD(xpos + pmscale), YCOORD(ypos), 0.0,              // Position 3 = right,top
                        1.0,  0.0                                               // TexCoord 3
                    };
                    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
                    
                    glVertexAttribPointer(texPosLoc, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GL_FLOAT), (const GLvoid*)(0));
                    glVertexAttribPointer(texCoordLoc, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GL_FLOAT), (const GLvoid*)(3 * sizeof(GL_FLOAT)));
                    glEnableVertexAttribArray(texPosLoc);
                    glEnableVertexAttribArray(texCoordLoc);
                    
                    glUniform1i(samplerLoc, 0);
                    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
                #else
                    GLfloat vertices[] = {
                        xpos,           ypos,
                        xpos + pmscale, ypos,
                        xpos,           ypos + pmscale,
                        xpos + pmscale, ypos + pmscale,
                    };
                    glVertexPointer(2, GL_FLOAT, 0, vertices);
                    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                #endif
            }
        }
    }
    
    #ifdef WEB_GUI
        glUseProgram(pointProgram);
    #endif
}

// -----------------------------------------------------------------------------

void DrawMagnifiedTwoStateCells(unsigned char* statedata, int x, int y, int w, int h, int pmscale, int stride)
{
    // called from golly_render::pixblit to draw cells magnified by pmscale (2, 4, ... 2^MAX_MAG)
    // when number of states is 2
    int cellsize = pmscale > 2 ? pmscale - 1 : pmscale;

    const int maxcoords = 1024;     // must be multiple of 2
    GLfloat points[maxcoords];
    int numcoords = 0;

    DisableTextures();
    SetPointSize(cellsize);

    // all live cells are in state 1 so only need to set color once
    SetColor(currlayer->cellr[1],
             currlayer->cellg[1],
             currlayer->cellb[1], 255);
    
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            unsigned char state = statedata[row*stride + col];
            if (state > 0) {
                if (numcoords == maxcoords) {
                    #ifdef WEB_GUI
                        glBufferData(GL_ARRAY_BUFFER, numcoords * sizeof(GLfloat), points, GL_STATIC_DRAW);
                        glVertexAttribPointer(positionLoc, 2, GL_FLOAT, GL_FALSE, 0, 0);
                        glEnableVertexAttribArray(positionLoc);
                    #else
                        glVertexPointer(2, GL_FLOAT, 0, points);
                    #endif
                    glDrawArrays(GL_POINTS, 0, numcoords/2);
                    numcoords = 0;
                }
                // get mid point of cell
                GLfloat midx = XCOORD(x + col*pmscale + cellsize/2.0);
                GLfloat midy = YCOORD(y + row*pmscale + cellsize/2.0);
                #ifdef WEB_GUI
                    if (midx > 1.0 || midy < -1.0)
                #else
                    if (midx > float(currwd) || midy > float(currht))
                #endif
                {   // midx,midy is outside viewport so we need to use FillRect to see partially
                    // visible cell at right/bottom edge
                    FillRect(x + col*pmscale, y + row*pmscale, cellsize, cellsize);
                } else {
                    points[numcoords++] = midx;
                    points[numcoords++] = midy;
                }
            }
        }
    }

    if (numcoords > 0) {
        #ifdef WEB_GUI
            glBufferData(GL_ARRAY_BUFFER, numcoords * sizeof(GLfloat), points, GL_STATIC_DRAW);
            glVertexAttribPointer(positionLoc, 2, GL_FLOAT, GL_FALSE, 0, 0);
            glEnableVertexAttribArray(positionLoc);
        #else
            glVertexPointer(2, GL_FLOAT, 0, points);
        #endif
        glDrawArrays(GL_POINTS, 0, numcoords/2);
    }
}

// -----------------------------------------------------------------------------

void DrawMagnifiedCells(unsigned char* statedata, int x, int y, int w, int h, int pmscale, int stride, int numstates)
{
    // called from golly_render::pixblit to draw cells magnified by pmscale (2, 4, ... 2^MAX_MAG)
    // when numstates is > 2
    int cellsize = pmscale > 2 ? pmscale - 1 : pmscale;

    const int maxcoords = 256;      // must be multiple of 2
    GLfloat points[256][maxcoords];
    int numcoords[256] = {0};

    DisableTextures();
    SetPointSize(cellsize);

    // following code minimizes color changes due to state changes
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            unsigned char state = statedata[row*stride + col];
            if (state > 0) {
                if (numcoords[state] == maxcoords) {
                    // this shouldn't happen too often
                    SetColor(currlayer->cellr[state],
                             currlayer->cellg[state],
                             currlayer->cellb[state], 255);
                    #ifdef WEB_GUI
                        glBufferData(GL_ARRAY_BUFFER, numcoords[state] * sizeof(GLfloat), points[state], GL_STATIC_DRAW);
                        glVertexAttribPointer(positionLoc, 2, GL_FLOAT, GL_FALSE, 0, 0);
                        glEnableVertexAttribArray(positionLoc);
                    #else
                        glVertexPointer(2, GL_FLOAT, 0, points[state]);
                    #endif
                    glDrawArrays(GL_POINTS, 0, numcoords[state]/2);
                    numcoords[state] = 0;
                }
                // get mid point of cell
                GLfloat midx = XCOORD(x + col*pmscale + cellsize/2.0);
                GLfloat midy = YCOORD(y + row*pmscale + cellsize/2.0);
                #ifdef WEB_GUI
                    if (midx > 1.0 || midy < -1.0)
                #else
                    if (midx > float(currwd) || midy > float(currht))
                #endif
                {   // midx,midy is outside viewport so we need to use FillRect to see partially
                    // visible cell at right/bottom edge
                    SetColor(currlayer->cellr[state],
                             currlayer->cellg[state],
                             currlayer->cellb[state], 255);
                    FillRect(x + col*pmscale, y + row*pmscale, cellsize, cellsize);
                } else {
                    points[state][numcoords[state]++] = midx;
                    points[state][numcoords[state]++] = midy;
                }
            }
        }
    }

    for (int state = 1; state < numstates; state++) {
        if (numcoords[state] > 0) {
            SetColor(currlayer->cellr[state],
                     currlayer->cellg[state],
                     currlayer->cellb[state], 255);
            #ifdef WEB_GUI
                glBufferData(GL_ARRAY_BUFFER, numcoords[state] * sizeof(GLfloat), points[state], GL_STATIC_DRAW);
                glVertexAttribPointer(positionLoc, 2, GL_FLOAT, GL_FALSE, 0, 0);
                glEnableVertexAttribArray(positionLoc);
            #else
                glVertexPointer(2, GL_FLOAT, 0, points[state]);
            #endif
            glDrawArrays(GL_POINTS, 0, numcoords[state]/2);
        }
    }
}

// -----------------------------------------------------------------------------

class golly_render : public liferender
{
public:
    golly_render() {}
    virtual ~golly_render() {}
    virtual void killrect(int x, int y, int w, int h);
    virtual void pixblit(int x, int y, int w, int h, char* pm, int pmscale);
    virtual void getcolors(unsigned char** r, unsigned char** g, unsigned char** b);
};

golly_render renderer;     // create instance

// -----------------------------------------------------------------------------

void golly_render::killrect(int x, int y, int w, int h)
{
#if 0
    // is Tom's hashdraw code doing unnecessary work???
    if (x >= currwd || y >= currht) return;
    if (x + w <= 0 || y + h <= 0) return;

    if (w <= 0 || h <= 0) return;

    // clip given rect so it's within viewport
    int clipx = x < 0 ? 0 : x;
    int clipy = y < 0 ? 0 : y;
    int clipr = x + w;
    int clipb = y + h;
    if (clipr > currwd) clipr = currwd;
    if (clipb > currht) clipb = currht;
    int clipwd = clipr - clipx;
    int clipht = clipb - clipy;

    // use a different pale color each time to see any probs
    DisableTextures();
    SetColor((rand()&127)+128, (rand()&127)+128, (rand()&127)+128, 255);
    FillRect(clipx, clipy, clipwd, clipht);
#else
    // no need to do anything because background has already been filled by glClear in DrawPattern
#endif
}

// -----------------------------------------------------------------------------

void golly_render::pixblit(int x, int y, int w, int h, char* pmdata, int pmscale)
{
    // is Tom's hashdraw code doing unnecessary work???
    if (x >= currwd || y >= currht) return;
    if (x + w <= 0 || y + h <= 0) return;

    // stride is the horizontal pixel width of the image data
    int stride = w/pmscale;

    // clip data outside viewport
    if (pmscale > 1) {
        // pmdata contains 1 byte per `pmscale' pixels, so we must be careful
        // and adjust x, y, w and h by multiples of `pmscale' only
        if (x < 0) {
            int dx = -x/pmscale*pmscale;
            pmdata += dx/pmscale;
            w -= dx;
            x += dx;
        }
        if (y < 0) {
            int dy = -y/pmscale*pmscale;
            pmdata += dy/pmscale*stride;
            h -= dy;
            y += dy;
        }
        if (x + w >= currwd + pmscale) w = (currwd - x + pmscale - 1)/pmscale*pmscale;
        if (y + h >= currht + pmscale) h = (currht - y + pmscale - 1)/pmscale*pmscale;
    }

    int numstates = currlayer->algo->NumCellStates();
    if (pmscale == 1) {
        // draw rgb pixel data at scale 1:1
        if (drawing_paste || numstates == 2) {
            // we can't use DrawTexture to draw paste image because glTexImage2D clobbers
            // any background pattern, so we use DrawPoints which is usually faster than
            // DrawTexture in a sparsely populated universe with only 2 states (eg. Life)
            DrawPoints((unsigned char*) pmdata, x, y, w, h);
        } else {
            DrawTexture((unsigned char*) pmdata, x, y, w, h);
        }
    } else if (showicons && pmscale > 4 && icontextures) {
        // draw icons at scales 1:8 or above
        DrawIcons((unsigned char*) pmdata, x, y, w/pmscale, h/pmscale, pmscale, stride);
    } else {
        // draw magnified cells, assuming pmdata contains (w/pmscale)*(h/pmscale) bytes
        // where each byte contains a cell state
        if (numstates == 2) {
            DrawMagnifiedTwoStateCells((unsigned char*) pmdata, x, y, w/pmscale, h/pmscale, pmscale, stride);
        } else {
            DrawMagnifiedCells((unsigned char*) pmdata, x, y, w/pmscale, h/pmscale, pmscale, stride, numstates);
        }
    }
}

// -----------------------------------------------------------------------------

void golly_render::getcolors(unsigned char** r, unsigned char** g, unsigned char** b)
{
    *r = currlayer->cellr;
    *g = currlayer->cellg;
    *b = currlayer->cellb;
}

// -----------------------------------------------------------------------------

void DrawGridBorder(int wd, int ht)
{
    // universe is bounded so draw any visible border regions
    pair<int,int> ltpxl = currlayer->view->screenPosOf(currlayer->algo->gridleft,
                                                       currlayer->algo->gridtop,
                                                       currlayer->algo);
    pair<int,int> rbpxl = currlayer->view->screenPosOf(currlayer->algo->gridright,
                                                       currlayer->algo->gridbottom,
                                                       currlayer->algo);
    int left = ltpxl.first;
    int top = ltpxl.second;
    int right = rbpxl.first;
    int bottom = rbpxl.second;
    if (currlayer->algo->gridwd == 0) {
        left = 0;
        right = wd-1;
    }
    if (currlayer->algo->gridht == 0) {
        top = 0;
        bottom = ht-1;
    }

    // note that right and/or bottom might be INT_MAX so avoid adding to cause overflow
    if (currlayer->view->getmag() > 0) {
        // move to bottom right pixel of cell at gridright,gridbottom
        if (right < wd) right += (1 << currlayer->view->getmag()) - 1;
        if (bottom < ht) bottom += (1 << currlayer->view->getmag()) - 1;
        if (currlayer->view->getmag() == 1) {
            // there are no gaps at scale 1:2
            if (right < wd) right++;
            if (bottom < ht) bottom++;
        }
    } else {
        if (right < wd) right++;
        if (bottom < ht) bottom++;
    }

    if (left < 0 && right >= wd && top < 0 && bottom >= ht) {
        // border isn't visible (ie. grid fills viewport)
        return;
    }

    DisableTextures();
    SetColor(borderrgb.r, borderrgb.g, borderrgb.b, 255);

    if (left >= wd || right < 0 || top >= ht || bottom < 0) {
        // no part of grid is visible so fill viewport with border
        FillRect(0, 0, wd, ht);
        return;
    }

    // avoid drawing overlapping rects below
    int rtop = 0;
    int rheight = ht;

    if (currlayer->algo->gridht > 0) {
        if (top > 0) {
            // top border is visible
            FillRect(0, 0, wd, top);
            // reduce size of rect below
            rtop = top;
            rheight -= top;
        }
        if (bottom < ht) {
            // bottom border is visible
            FillRect(0, bottom, wd, ht - bottom);
            // reduce size of rect below
            rheight -= ht - bottom;
        }
    }

    if (currlayer->algo->gridwd > 0) {
        if (left > 0) {
            // left border is visible
            FillRect(0, rtop, left, rheight);
        }
        if (right < wd) {
            // right border is visible
            FillRect(right, rtop, wd - right, rheight);
        }
    }
}

// -----------------------------------------------------------------------------

void DrawSelection(gRect& rect, bool active)
{
    // draw semi-transparent rectangle
    DisableTextures();
    if (active) {
        SetColor(selectrgb.r, selectrgb.g, selectrgb.b, 128);
    } else {
        // use light gray to indicate an inactive selection
        SetColor(160, 160, 160, 128);
    }
    FillRect(rect.x, rect.y, rect.width, rect.height);
}

// -----------------------------------------------------------------------------

void CreatePasteImage(lifealgo* palgo, gRect& bbox)
{
    // set globals used in DrawPasteImage
    pastealgo = palgo;
    pastebbox = bbox;
}

// -----------------------------------------------------------------------------

void DestroyPasteImage()
{
    // no need to do anything
}

// -----------------------------------------------------------------------------

int PixelsToCells(int pixels, int mag) {
    // convert given # of screen pixels to corresponding # of cells
    if (mag >= 0) {
        int cellsize = 1 << mag;
        return (pixels + cellsize - 1) / cellsize;
    } else {
        // mag < 0; no need to worry about overflow
        return pixels << -mag;
    }
}

// -----------------------------------------------------------------------------

void SetPasteRect(int wd, int ht)
{
    int x, y, pastewd, pasteht;
    int mag = currlayer->view->getmag();

    // find cell coord of current paste position
    pair<bigint, bigint> pcell = currlayer->view->at(pastex, pastey);

    // determine bottom right cell
    bigint right = pcell.first;     right += wd;    right -= 1;
    bigint bottom = pcell.second;   bottom += ht;   bottom -= 1;

    // best to use same method as in Selection::Visible
    pair<int,int> lt = currlayer->view->screenPosOf(pcell.first, pcell.second, currlayer->algo);
    pair<int,int> rb = currlayer->view->screenPosOf(right, bottom, currlayer->algo);

    if (mag > 0) {
        // move rb to pixel at bottom right corner of cell
        rb.first += (1 << mag) - 1;
        rb.second += (1 << mag) - 1;
        if (mag > 1) {
            // avoid covering gaps at scale 1:4 and above
            rb.first--;
            rb.second--;
        }
    }

    x = lt.first;
    y = lt.second;
    pastewd = rb.first - lt.first + 1;
    pasteht = rb.second - lt.second + 1;

    // this should never happen but play safe
    if (pastewd <= 0) pastewd = 1;
    if (pasteht <= 0) pasteht = 1;

    // don't let pasterect get too far beyond left/top edge of viewport
    if (x + pastewd < 64) {
        if (pastewd >= 64)
            x = 64 - pastewd;
        else if (x < 0)
            x = 0;
        pastex = x;
    }
    if (y + pasteht < 64) {
        if (pasteht >= 64)
            y = 64 - pasteht;
        else if (y < 0)
            y = 0;
        pastey = y;
    }

    SetRect(pasterect, x, y, pastewd, pasteht);
    // NSLog(@"pasterect: x=%d y=%d wd=%d ht=%d", pasterect.x, pasterect.y, pasterect.width, pasterect.height);
}

// -----------------------------------------------------------------------------

void DrawPasteImage()
{
    // calculate pasterect
    SetPasteRect(pastebbox.width, pastebbox.height);

    int pastemag = currlayer->view->getmag();
    gRect cellbox = pastebbox;

    // calculate intersection of pasterect and current viewport for use
    // as a temporary viewport
    int itop = pasterect.y;
    int ileft = pasterect.x;
    int ibottom = itop + pasterect.height - 1;
    int iright = ileft + pasterect.width - 1;
    if (itop < 0) {
        itop = 0;
        cellbox.y += PixelsToCells(-pasterect.y, pastemag);
    }
    if (ileft < 0) {
        ileft = 0;
        cellbox.x += PixelsToCells(-pasterect.x, pastemag);
    }
    if (ibottom > currht - 1) ibottom = currht - 1;
    if (iright > currwd - 1) iright = currwd - 1;
    int pastewd = iright - ileft + 1;
    int pasteht = ibottom - itop + 1;

    // set size of translucent rect before following adjustment
    int rectwd = pastewd;
    int rectht = pasteht;

    if (pastemag > 0) {
        // make sure pastewd/ht don't have partial cells
        int cellsize = 1 << pastemag;
        int gap = 1;                        // gap between cells
        if (pastemag == 1) gap = 0;         // no gap at scale 1:2
        if ((pastewd + gap) % cellsize > 0)
            pastewd += cellsize - ((pastewd + gap) % cellsize);
        if ((pasteht + gap) % cellsize != 0)
            pasteht += cellsize - ((pasteht + gap) % cellsize);
    }

    cellbox.width = PixelsToCells(pastewd, pastemag);
    cellbox.height = PixelsToCells(pasteht, pastemag);

    // create temporary viewport
    viewport tempview(pastewd, pasteht);
    int midx, midy;
    if (pastemag > 1) {
        // allow for gap between cells
        midx = cellbox.x + (cellbox.width - 1) / 2;
        midy = cellbox.y + (cellbox.height - 1) / 2;
    } else {
        midx = cellbox.x + cellbox.width / 2;
        midy = cellbox.y + cellbox.height / 2;
    }
    tempview.setpositionmag(midx, midy, pastemag);

    // temporarily turn off grid lines
    bool saveshow = showgridlines;
    showgridlines = false;

    // temporarily change currwd and currht
    int savewd = currwd;
    int saveht = currht;
    currwd = tempview.getwidth();
    currht = tempview.getheight();

    #ifdef WEB_GUI
        // temporarily change OpenGL viewport's origin and size to match tempview
        glViewport(ileft, saveht-currht-itop, currwd, currht);
    #else
        glTranslatef(ileft, itop, 0);
    #endif

    // draw paste pattern
    drawing_paste = true;
    pastealgo->draw(tempview, renderer);
    drawing_paste = false;

    #ifdef WEB_GUI
        // restore OpenGL viewport's origin and size
        glViewport(0, 0, savewd, saveht);
    #else
        glTranslatef(-ileft, -itop, 0);
    #endif

    showgridlines = saveshow;
    currwd = savewd;
    currht = saveht;

    // overlay translucent rect to show paste area
    DisableTextures();
    SetColor(pastergb.r, pastergb.g, pastergb.b, 64);
    FillRect(ileft, itop, rectwd, rectht);
}

// -----------------------------------------------------------------------------

void DrawGridLines(int wd, int ht)
{
    int cellsize = 1 << currlayer->view->getmag();
    int h, v, i, topbold, leftbold;

    if (showboldlines) {
        // ensure that origin cell stays next to bold lines;
        // ie. bold lines scroll when pattern is scrolled
        pair<bigint, bigint> lefttop = currlayer->view->at(0, 0);
        leftbold = lefttop.first.mod_smallint(boldspacing);
        topbold = lefttop.second.mod_smallint(boldspacing);
        if (currlayer->originx != bigint::zero) {
            leftbold -= currlayer->originx.mod_smallint(boldspacing);
        }
        if (currlayer->originy != bigint::zero) {
            topbold -= currlayer->originy.mod_smallint(boldspacing);
        }
        if (mathcoords) topbold--;   // show origin cell above bold line
    } else {
        // avoid gcc warning
        topbold = leftbold = 0;
    }

    DisableTextures();
    glLineWidth(1.0);

    // set the stroke color depending on current bg color
    int r = currlayer->cellr[0];
    int g = currlayer->cellg[0];
    int b = currlayer->cellb[0];
    int gray = (int) ((r + g + b) / 3.0);
    if (gray > 127) {
        // darker lines
        SetColor(r > 32 ? r - 32 : 0,
                 g > 32 ? g - 32 : 0,
                 b > 32 ? b - 32 : 0, 255);
    } else {
        // lighter lines
        SetColor(r + 32 < 256 ? r + 32 : 255,
                 g + 32 < 256 ? g + 32 : 255,
                 b + 32 < 256 ? b + 32 : 255, 255);
    }

    // draw all plain lines first;
    // note that we need to subtract 0.5 from each coordinate to avoid uneven spacing

    i = showboldlines ? topbold : 1;
    v = 0;
    while (true) {
        v += cellsize;
        if (v >= ht) break;
        if (showboldlines) i++;
        if (i % boldspacing != 0 && v >= 0 && v < ht) {
            #ifdef WEB_GUI
                GLfloat points[] = { XCOORD(-0.5), YCOORD(v-0.5),
                                     XCOORD(wd),   YCOORD(v-0.5) };
                glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(GLfloat), points, GL_STATIC_DRAW);
                glVertexAttribPointer(positionLoc, 2, GL_FLOAT, GL_FALSE, 0, 0);
                glEnableVertexAttribArray(positionLoc);
            #else
                GLfloat points[] = {   -0.5, v-0.5,
                                     wd-0.5, v-0.5 };
                glVertexPointer(2, GL_FLOAT, 0, points);
            #endif
            glDrawArrays(GL_LINES, 0, 2);
        }
    }
    i = showboldlines ? leftbold : 1;
    h = 0;
    while (true) {
        h += cellsize;
        if (h >= wd) break;
        if (showboldlines) i++;
        if (i % boldspacing != 0 && h >= 0 && h < wd) {
            #ifdef WEB_GUI
                GLfloat points[] = { XCOORD(h-0.5), YCOORD(-0.5),
                                     XCOORD(h-0.5), YCOORD(ht) };
                glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(GLfloat), points, GL_STATIC_DRAW);
                glVertexAttribPointer(positionLoc, 2, GL_FLOAT, GL_FALSE, 0, 0);
                glEnableVertexAttribArray(positionLoc);
            #else
                GLfloat points[] = { h-0.5,   -0.5,
                                     h-0.5, ht-0.5 };
                glVertexPointer(2, GL_FLOAT, 0, points);
            #endif
            glDrawArrays(GL_LINES, 0, 2);
        }
    }

    if (showboldlines) {
        // draw bold lines in slightly darker/lighter color
        if (gray > 127) {
            // darker lines
            SetColor(r > 64 ? r - 64 : 0,
                     g > 64 ? g - 64 : 0,
                     b > 64 ? b - 64 : 0, 255);
        } else {
            // lighter lines
            SetColor(r + 64 < 256 ? r + 64 : 255,
                     g + 64 < 256 ? g + 64 : 255,
                     b + 64 < 256 ? b + 64 : 255, 255);
        }
        
        i = topbold;
        v = 0;
        while (true) {
            v += cellsize;
            if (v >= ht) break;
            i++;
            if (i % boldspacing == 0 && v >= 0 && v < ht) {
                #ifdef WEB_GUI
                    GLfloat points[] = { XCOORD(-0.5), YCOORD(v-0.5),
                                         XCOORD(wd),   YCOORD(v-0.5) };
                    glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(GLfloat), points, GL_STATIC_DRAW);
                    glVertexAttribPointer(positionLoc, 2, GL_FLOAT, GL_FALSE, 0, 0);
                    glEnableVertexAttribArray(positionLoc);
                #else
                    GLfloat points[] = {   -0.5, v-0.5,
                                         wd-0.5, v-0.5 };
                    glVertexPointer(2, GL_FLOAT, 0, points);
                #endif
                glDrawArrays(GL_LINES, 0, 2);
            }
        }
        i = leftbold;
        h = 0;
        while (true) {
            h += cellsize;
            if (h >= wd) break;
            i++;
            if (i % boldspacing == 0 && h >= 0 && h < wd) {
                #ifdef WEB_GUI
                    GLfloat points[] = { XCOORD(h-0.5), YCOORD(-0.5),
                                         XCOORD(h-0.5), YCOORD(ht) };
                    glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(GLfloat), points, GL_STATIC_DRAW);
                    glVertexAttribPointer(positionLoc, 2, GL_FLOAT, GL_FALSE, 0, 0);
                    glEnableVertexAttribArray(positionLoc);
                #else
                    GLfloat points[] = { h-0.5,   -0.5,
                                         h-0.5, ht-0.5 };
                    glVertexPointer(2, GL_FLOAT, 0, points);
                #endif
                glDrawArrays(GL_LINES, 0, 2);
            }
        }
    }
}

// -----------------------------------------------------------------------------

/*!!!

void DrawOneLayer(EAGLContext* dc)
{
    wxMemoryDC layerdc;
    layerdc.SelectObject(*layerbitmap);

    if (showicons && currlayer->view->getmag() > 2) {
        // only show icons at scales 1:8 and above
        if (currlayer->view->getmag() == 3) {
            icontextures = currlayer->textures7x7;
        } else if (currlayer->view->getmag() == 4) {
            icontextures = currlayer->textures15x15;
        } else {
            icontextures = currlayer->textures31x31;
        }
    }

    currlayer->algo->draw(*currlayer->view, renderer);
    layerdc.SelectObject(wxNullBitmap);

    // make dead pixels 100% transparent; live pixels use opacity setting
    MaskDeadPixels(layerbitmap, layerwd, layerht, int(2.55 * opacity));

    // draw result
    dc.DrawBitmap(*layerbitmap, 0, 0, true);
}

// -----------------------------------------------------------------------------

void DrawStackedLayers(EAGLContext* dc)
{
    // check if layerbitmap needs to be created or resized
    if ( layerwd != currlayer->view->getwidth() ||
        layerht != currlayer->view->getheight() ) {
        layerwd = currlayer->view->getwidth();
        layerht = currlayer->view->getheight();
        delete layerbitmap;
        // create a bitmap with depth 32 so it has an alpha channel
        layerbitmap = new wxBitmap(layerwd, layerht, 32);
        if (!layerbitmap) {
            Fatal(_("Not enough memory for layer bitmap!"));
            return;
        }
    }

    // temporarily turn off grid lines
    bool saveshow = showgridlines;
    showgridlines = false;

    // draw patterns in layers 1..numlayers-1
    for ( int i = 1; i < numlayers; i++ ) {
        Layer* savelayer = currlayer;
        currlayer = GetLayer(i);

        // use real current layer's viewport
        viewport* saveview = currlayer->view;
        currlayer->view = savelayer->view;

        // avoid drawing a cloned layer more than once??? draw first or last clone???

        if ( !currlayer->algo->isEmpty() ) {
            DrawOneLayer(dc);
        }

        // draw this layer's selection if necessary
        wxRect r;
        if ( currlayer->currsel.Visible(&r) ) {
            DrawSelection(dc, r, i == currindex);
        }

        // restore viewport and currlayer
        currlayer->view = saveview;
        currlayer = savelayer;
    }

    showgridlines = saveshow;
}

// -----------------------------------------------------------------------------

void DrawTileFrame(EAGLContext* dc, wxRect& trect, wxBrush& brush, int wd)
{
    trect.Inflate(wd);
    wxRect r = trect;

    r.height = wd;
    FillRect(dc, r, brush);       // top edge

    r.y += trect.height - wd;
    FillRect(dc, r, brush);       // bottom edge

    r = trect;
    r.width = wd;
    FillRect(dc, r, brush);       // left edge

    r.x += trect.width - wd;
    FillRect(dc, r, brush);       // right edge
}

// -----------------------------------------------------------------------------

void DrawTileBorders(EAGLContext* dc)
{
    if (tileborder <= 0) return;    // no borders

    // draw tile borders in bigview window
    int wd, ht;
    bigview->GetClientSize(&wd, &ht);
    if (wd < 1 || ht < 1) return;

    wxBrush brush;
    // most people will choose either a very light or very dark color for dead cells,
    // so draw mid gray border around non-current tiles
    brush.SetColour(144, 144, 144);
    wxRect trect;
    for ( int i = 0; i < numlayers; i++ ) {
        if (i != currindex) {
            trect = GetLayer(i)->tilerect;
            DrawTileFrame(dc, trect, brush, tileborder);
        }
    }

    // draw green border around current tile
    trect = GetLayer(currindex)->tilerect;
    brush.SetColour(0, 255, 0);
    DrawTileFrame(dc, trect, brush, tileborder);
}

!!!*/

// -----------------------------------------------------------------------------

void DrawPattern(int tileindex)
{
    gRect r;
    int colorindex = currindex;
    /*!!!
    Layer* savelayer = NULL;
    viewport* saveview0 = NULL;
    */

    // fill the background with state 0 color
    glClearColor(currlayer->cellr[0]/255.0,
                 currlayer->cellg[0]/255.0,
                 currlayer->cellb[0]/255.0,
                 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    // if grid is bounded then ensure viewport's central cell is not outside grid edges
    if ( currlayer->algo->gridwd > 0) {
        if ( currlayer->view->x < currlayer->algo->gridleft )
            currlayer->view->setpositionmag(currlayer->algo->gridleft,
                                            currlayer->view->y,
                                            currlayer->view->getmag());
        else if ( currlayer->view->x > currlayer->algo->gridright )
            currlayer->view->setpositionmag(currlayer->algo->gridright,
                                            currlayer->view->y,
                                            currlayer->view->getmag());
    }
    if ( currlayer->algo->gridht > 0) {
        if ( currlayer->view->y < currlayer->algo->gridtop )
            currlayer->view->setpositionmag(currlayer->view->x,
                                            currlayer->algo->gridtop,
                                            currlayer->view->getmag());
        else if ( currlayer->view->y > currlayer->algo->gridbottom )
            currlayer->view->setpositionmag(currlayer->view->x,
                                            currlayer->algo->gridbottom,
                                            currlayer->view->getmag());
    }

    if (nopattupdate) {
        // don't draw incomplete pattern, just draw grid lines and border
        currwd = currlayer->view->getwidth();
        currht = currlayer->view->getheight();
        if ( showgridlines && currlayer->view->getmag() >= mingridmag ) {
            DrawGridLines(currwd, currht);
        }
        if ( currlayer->algo->gridwd > 0 || currlayer->algo->gridht > 0 ) {
            DrawGridBorder(currwd, currht);
        }
        return;
    }

    /*!!!
    if ( numlayers > 1 && tilelayers ) {
        if ( tileindex < 0 ) {
            DrawTileBorders(dc);
            return;
        }
        // tileindex >= 0 so temporarily change some globals to draw this tile
        if ( syncviews && tileindex != currindex ) {
            // make sure this layer uses same location and scale as current layer
            GetLayer(tileindex)->view->setpositionmag(currlayer->view->x,
                                                      currlayer->view->y,
                                                      currlayer->view->getmag());
        }
        savelayer = currlayer;
        currlayer = GetLayer(tileindex);
        viewptr = currlayer->tilewin;
        colorindex = tileindex;
    } else if ( numlayers > 1 && stacklayers ) {
        // draw all layers starting with layer 0 but using current layer's viewport
        savelayer = currlayer;
        if ( currindex != 0 ) {
            // change currlayer to layer 0
            currlayer = GetLayer(0);
            saveview0 = currlayer->view;
            currlayer->view = savelayer->view;
        }
        colorindex = 0;
    }
    */

    if (showicons && currlayer->view->getmag() > 2) {
        // only show icons at scales 1:8 and above
        if (currlayer->view->getmag() == 3) {
            icontextures = currlayer->textures7x7;
        } else if (currlayer->view->getmag() == 4) {
            icontextures = currlayer->textures15x15;
        } else {
            icontextures = currlayer->textures31x31;
        }
    }

    currwd = currlayer->view->getwidth();
    currht = currlayer->view->getheight();

    // draw pattern using a sequence of pixblit calls
    currlayer->algo->draw(*currlayer->view, renderer);

    if ( showgridlines && currlayer->view->getmag() >= mingridmag ) {
        DrawGridLines(currwd, currht);
    }

    // if universe is bounded then draw border regions (if visible)
    if ( currlayer->algo->gridwd > 0 || currlayer->algo->gridht > 0 ) {
        DrawGridBorder(currwd, currht);
    }

    if ( currlayer->currsel.Visible(&r) ) {
        DrawSelection(r, colorindex == currindex);
    }

    /*!!!
    if ( numlayers > 1 && stacklayers ) {
        // must restore currlayer before we call DrawStackedLayers
        currlayer = savelayer;
        if ( saveview0 ) {
            // restore layer 0's viewport
            GetLayer(0)->view = saveview0;
        }
        // draw layers 1, 2, ... numlayers-1
        DrawStackedLayers(dc);
    }
    */

    if (waitingforpaste) {
        DrawPasteImage();
    }

    /*!!!
    if ( numlayers > 1 && tilelayers ) {
        // restore globals changed above
        currlayer = savelayer;
        viewptr = currlayer->tilewin;
    }
    */
}
