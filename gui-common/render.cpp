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
 All drawing in the viewport is done in this module using OpenGL ES 2.

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

#ifdef IOS_GUI
    // the iOS version uses OpenGL ES 2
    #import <OpenGLES/ES2/gl.h>
    #import <OpenGLES/ES2/glext.h>
#else
    // the Android and web versions also use OpenGL ES 2
    #include <GLES2/gl2.h>
#endif

// -----------------------------------------------------------------------------

// local data used in golly_render routines

int currwd, currht;                     // current width and height of viewport, in pixels
unsigned char dead_alpha = 255;         // alpha value for dead pixels
unsigned char live_alpha = 255;         // alpha value for live pixels
GLuint rgbatexture = 0;                 // texture name for drawing RGBA bitmaps
GLuint icontexture = 0;                 // texture name for drawing icons
GLuint celltexture = 0;                 // texture name for drawing magnified cells
unsigned char* iconatlas = NULL;        // pointer to texture atlas for current set of icons
unsigned char* cellatlas = NULL;        // pointer to texture atlas for current set of magnified cells

// cellatlas needs to be rebuilt if any of these parameters change
int prevnum = 0;                        // previous number of live states
int prevsize = 0;                       // previous cell size
unsigned char prevalpha;                // previous alpha component
unsigned char prevr[256];               // previous red component of each state
unsigned char prevg[256];               // previous green component of each state
unsigned char prevb[256];               // previous blue component of each state

// for drawing paste pattern
lifealgo* pastealgo;                    // universe containing paste pattern
gRect pastebbox;                        // bounding box in cell coords (not necessarily minimal)

GLuint pointProgram;                    // program object for drawing lines and rects
GLint positionLoc;                      // location of v_Position attribute
GLint lineColorLoc;                     // location of LineColor uniform

GLuint textureProgram;                  // program object for drawing 2D textures
GLint texPosLoc;                        // location of a_Position attribute
GLint texCoordLoc;                      // location of a_texCoord attribute
GLint samplerLoc;                       // location of s_texture uniform

// the following 2 macros convert x,y positions in Golly's preferred coordinate
// system (where 0,0 is top left corner of viewport and bottom right corner is
// currwd,currht) into OpenGL ES 2's normalized coordinates (where 0.0,0.0 is in
// middle of viewport, top right corner is 1.0,1.0 and bottom left corner is -1.0,-1.0)
#define XCOORD(x)  (2.0 * (x) / float(currwd) - 1.0)
#define YCOORD(y) -(2.0 * (y) / float(currht) - 1.0)

// -----------------------------------------------------------------------------

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

bool InitOGLES2()
{
    // initialize the shaders and program objects required by OpenGL ES 2
    // (based on OpenGL's Hello_Triangle.c and Simple_Texture2D.c)
    
    // vertex shader used in pointProgram
    GLbyte v1ShaderStr[] =
        "attribute vec4 v_Position;   \n"
        "void main() {                \n"
        "    gl_Position = v_Position;\n"
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
    glUseProgram(pointProgram);
    positionLoc = glGetAttribLocation(pointProgram, "v_Position");
    lineColorLoc = glGetUniformLocation(pointProgram, "LineColor");
    if (positionLoc == -1 || lineColorLoc == -1) {
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
    
    // create buffer for vertex data (used for drawing lines and rects)
    GLuint vertexPosObject;
    glGenBuffers(1, &vertexPosObject);
    glBindBuffer(GL_ARRAY_BUFFER, vertexPosObject);

    // create buffer for index data (used for drawing textures)
    // where each cell = 2 triangles with 2 shared vertices (0 and 2)
    //
    //    0 *---* 3
    //      | \ |
    //    1 *---* 2
    //
    GLushort indices[] = { 0, 1, 2, 0, 2, 3 };
    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    
    // use the pointProgram initially
    glUseProgram(pointProgram);
    
    return true;
}

// -----------------------------------------------------------------------------

static void SetColor(int r, int g, int b, int a)
{
    GLfloat color[4];
    color[0] = r/255.0;
    color[1] = g/255.0;
    color[2] = b/255.0;
    color[3] = a/255.0;
    glUniform4fv(lineColorLoc, 1, color);
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
    glBufferData(GL_ARRAY_BUFFER, 8 * sizeof(GLfloat), rect, GL_STATIC_DRAW);
    glVertexAttribPointer(positionLoc, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(positionLoc);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

// -----------------------------------------------------------------------------

void DrawRGBAData(unsigned char* rgbadata, int x, int y, int w, int h)
{
    // called from golly_render::pixblit to draw a pattern bitmap at 1:1 scale

    // only need to create texture name once
	if (rgbatexture == 0) glGenTextures(1, &rgbatexture);

    glUseProgram(textureProgram);
    glActiveTexture(GL_TEXTURE0);
    
    glBindTexture(GL_TEXTURE_2D, rgbatexture);
    glUniform1i(samplerLoc, 0);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    glVertexAttribPointer(texPosLoc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GL_FLOAT), (const GLvoid*)(0));
    glVertexAttribPointer(texCoordLoc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GL_FLOAT), (const GLvoid*)(2 * sizeof(GL_FLOAT)));
    glEnableVertexAttribArray(texPosLoc);
    glEnableVertexAttribArray(texCoordLoc);
    
    // update the texture with the new RGBA data
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgbadata);

    GLfloat vertices[] = {
        XCOORD(x),     YCOORD(y),      // Position 0 = left,top
        0.0,  0.0,                     // TexCoord 0
        XCOORD(x),     YCOORD(y + h),  // Position 1 = left,bottom
        0.0,  1.0,                     // TexCoord 1
        XCOORD(x + w), YCOORD(y + h),  // Position 2 = right,bottom
        1.0,  1.0,                     // TexCoord 2
        XCOORD(x + w), YCOORD(y),      // Position 3 = right,top
        1.0,  0.0                      // TexCoord 3
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

    glUseProgram(pointProgram);
}

// -----------------------------------------------------------------------------

static void LoadIconAtlas(int iconsize, int numicons)
{
    // load the texture atlas containing all icons for later use in DrawIcons
    
    // create icon texture name once
	if (icontexture == 0) glGenTextures(1, &icontexture);

    glUseProgram(textureProgram);
    glActiveTexture(GL_TEXTURE0);   // need???!!!
    
    glBindTexture(GL_TEXTURE_2D, icontexture);
    glUniform1i(samplerLoc, 0);

    // need these calls otherwise texture atlas is rendered black
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    int atlaswd = iconsize * numicons;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlaswd, iconsize, 0, GL_RGBA, GL_UNSIGNED_BYTE, iconatlas);
    
#if 0
    // test above code by displaying the entire atlas
    GLfloat wd = atlaswd;
    GLfloat ht = iconsize;
    int x = 0;
    int y = 0;
    GLfloat vertices[] = {
        XCOORD(x),      YCOORD(y),         // Position 0 = left,top
        0.0,  0.0,                         // TexCoord 0
        XCOORD(x),      YCOORD(y + ht),    // Position 1 = left,bottom
        0.0,  1.0,                         // TexCoord 1
        XCOORD(x + wd), YCOORD(y + ht),    // Position 2 = right,bottom
        1.0,  1.0,                         // TexCoord 2
        XCOORD(x + wd), YCOORD(y),         // Position 3 = right,top
        1.0,  0.0                          // TexCoord 3
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glVertexAttribPointer(texPosLoc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GL_FLOAT), (const GLvoid*)(0));
    glVertexAttribPointer(texCoordLoc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GL_FLOAT), (const GLvoid*)(2 * sizeof(GL_FLOAT)));    
    glEnableVertexAttribArray(texPosLoc);
    glEnableVertexAttribArray(texCoordLoc);
    
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
#endif

    glUseProgram(pointProgram);     // rethink!!!
}

// -----------------------------------------------------------------------------

void DrawIcons(unsigned char* statedata, int x, int y, int w, int h, int pmscale, int stride, int numicons)
{
    // called from golly_render::pixblit to draw icons for each live cell;
    // assume pmscale > 2 (should be 8, 16 or 32 -- if higher then the 31x31 icons
    // will be scaled up)

    glUseProgram(textureProgram);
    glActiveTexture(GL_TEXTURE0);
    
    glBindTexture(GL_TEXTURE_2D, icontexture);
    glUniform1i(samplerLoc, 0);
    
    glVertexAttribPointer(texPosLoc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GL_FLOAT), (const GLvoid*)(0));
    glVertexAttribPointer(texCoordLoc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GL_FLOAT), (const GLvoid*)(2 * sizeof(GL_FLOAT)));
    glEnableVertexAttribArray(texPosLoc);
    glEnableVertexAttribArray(texCoordLoc);

    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            unsigned char state = statedata[row*stride + col];
            if (state > 0) {
                int xpos = x + col * pmscale;
                int ypos = y + row * pmscale;
                
                GLfloat xleft =   XCOORD(xpos);
                GLfloat xright =  XCOORD(xpos + pmscale);
                GLfloat ytop =    YCOORD(ypos);
                GLfloat ybottom = YCOORD(ypos + pmscale);
                
                GLfloat tleft = (state-1) * 1.0 / numicons;
                GLfloat tright = state * 1.0 / numicons;
                
                GLfloat vertices[] = {
                    xleft,  ytop,       // Position 0 = left,top
                    tleft,  0.0,        // TexCoord 0
                    xleft,  ybottom,    // Position 1 = left,bottom
                    tleft,  1.0,        // TexCoord 1
                    xright, ybottom,    // Position 2 = right,bottom
                    tright, 1.0,        // TexCoord 2
                    xright, ytop,       // Position 3 = right,top
                    tright, 0.0         // TexCoord 3
                };
                glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
                glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
            }
        }
    }
    
    glUseProgram(pointProgram);     // rethink!!!
}

// -----------------------------------------------------------------------------

static bool ChangeCellAtlas(int cellsize, int numcells, unsigned char alpha)
{
    if (numcells != prevnum) return true;
    if (cellsize != prevsize) return true;
    if (alpha != prevalpha) return true;
    
    for (int state = 1; state <= numcells; state++) {
        if (currlayer->cellr[state] != prevr[state]) return true;
        if (currlayer->cellg[state] != prevg[state]) return true;
        if (currlayer->cellb[state] != prevb[state]) return true;
    }
    
    return false;   // no need to change cellatlas
}

// -----------------------------------------------------------------------------

static void LoadCellAtlas(int cellsize, int numcells, unsigned char alpha)
{
    // cellatlas might need to be (re)created
    if (ChangeCellAtlas(cellsize, numcells, alpha)) {
        prevnum = numcells;
        prevsize = cellsize;
        prevalpha = alpha;
        for (int state = 1; state <= numcells; state++) {
            prevr[state] = currlayer->cellr[state];
            prevg[state] = currlayer->cellg[state];
            prevb[state] = currlayer->cellb[state];
        }
        
        if (cellatlas) free(cellatlas);
        
        // allocate enough memory for texture atlas to store RGBA pixels for a row of cells
        // (note that we use calloc so all alpha bytes are initially 0)
        int rowbytes = numcells * cellsize * 4;
        cellatlas = (unsigned char*) calloc(rowbytes * cellsize, 1);
        
        if (cellatlas) {
            // set pixels in top row
            int tpos = 0;
            for (int state = 1; state <= numcells; state++) {
                unsigned char r = currlayer->cellr[state];
                unsigned char g = currlayer->cellg[state];
                unsigned char b = currlayer->cellb[state];
                
                // if the cell size is > 2 then there is a 1 pixel gap at right and bottom edge of each cell
                int cellwd = cellsize > 2 ? cellsize-1 : 2;
                for (int i = 0; i < cellwd; i++) {
                    cellatlas[tpos++] = r;
                    cellatlas[tpos++] = g;
                    cellatlas[tpos++] = b;
                    cellatlas[tpos++] = alpha;
                }
                if (cellsize > 2) tpos += 4;    // skip transparent pixel at right edge of cell
            }
            // copy top row to remaining rows
            int remrows = cellsize > 2 ? cellsize - 2 : 1;
            for (int i = 1; i <= remrows; i++) {
                memcpy(&cellatlas[i * rowbytes], cellatlas, rowbytes);
            }
        }
    }
    
    // create cell texture name once
	if (celltexture == 0) glGenTextures(1, &celltexture);
    
    glUseProgram(textureProgram);
    glActiveTexture(GL_TEXTURE0);   // need???!!!
    
    glBindTexture(GL_TEXTURE_2D, celltexture);
    glUniform1i(samplerLoc, 0);

    // need these calls otherwise texture atlas is rendered black
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // load the texture atlas containing all magnified cells for later use in DrawMagnifiedCells
    int atlaswd = cellsize * numcells;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlaswd, cellsize, 0, GL_RGBA, GL_UNSIGNED_BYTE, cellatlas);

    glUseProgram(pointProgram);     // rethink!!!
}

// -----------------------------------------------------------------------------

void DrawMagnifiedCells(unsigned char* statedata, int x, int y, int w, int h, int pmscale, int stride, int numcells)
{
    // called from golly_render::pixblit to draw cells magnified by pmscale (2, 4, ... 2^MAX_MAG)

    glUseProgram(textureProgram);
    glActiveTexture(GL_TEXTURE0);

    glBindTexture(GL_TEXTURE_2D, celltexture);
    glUniform1i(samplerLoc, 0);
                
    glVertexAttribPointer(texPosLoc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GL_FLOAT), (const GLvoid*)(0));
    glVertexAttribPointer(texCoordLoc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GL_FLOAT), (const GLvoid*)(2 * sizeof(GL_FLOAT)));
    glEnableVertexAttribArray(texPosLoc);
    glEnableVertexAttribArray(texCoordLoc);

    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            unsigned char state = statedata[row*stride + col];
            if (state > 0) {
                int xpos = x + col * pmscale;
                int ypos = y + row * pmscale;
                
                GLfloat xleft =   XCOORD(xpos);
                GLfloat xright =  XCOORD(xpos + pmscale);
                GLfloat ytop =    YCOORD(ypos);
                GLfloat ybottom = YCOORD(ypos + pmscale);
                
                GLfloat tleft = (state-1) * 1.0 / numcells;
                GLfloat tright = state * 1.0 / numcells;
                
                GLfloat vertices[] = {
                    xleft,  ytop,       // Position 0 = left,top
                    tleft,  0.0,        // TexCoord 0
                    xleft,  ybottom,    // Position 1 = left,bottom
                    tleft,  1.0,        // TexCoord 1
                    xright, ybottom,    // Position 2 = right,bottom
                    tright, 1.0,        // TexCoord 2
                    xright, ytop,       // Position 3 = right,top
                    tright, 0.0         // TexCoord 3
                };
                glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
                glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
            }
        }
    }
    
    glUseProgram(pointProgram);     // rethink!!!
}

// -----------------------------------------------------------------------------

class golly_render : public liferender
{
public:
    golly_render() {}
    virtual ~golly_render() {}
    virtual void pixblit(int x, int y, int w, int h, unsigned char* pm, int pmscale);
    virtual void getcolors(unsigned char** r, unsigned char** g, unsigned char** b,
                           unsigned char* deada, unsigned char* livea);
};

golly_render renderer;     // create instance

// -----------------------------------------------------------------------------

void golly_render::pixblit(int x, int y, int w, int h, unsigned char* pmdata, int pmscale)
{
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
    
    if (pmscale == 1) {
        // draw RGBA pixel data at scale 1:1
        DrawRGBAData(pmdata, x, y, w, h);
        
    } else if (showicons && pmscale > 4 && iconatlas) {
        // draw icons at scales 1:8 and above
        DrawIcons(pmdata, x, y, w/pmscale, h/pmscale, pmscale, stride, currlayer->numicons);
        
    } else {
        // draw magnified cells, assuming pmdata contains (w/pmscale)*(h/pmscale) bytes
        // where each byte contains a cell state
        DrawMagnifiedCells(pmdata, x, y, w/pmscale, h/pmscale, pmscale, stride, currlayer->numicons);
    }
}

// -----------------------------------------------------------------------------

void golly_render::getcolors(unsigned char** r, unsigned char** g, unsigned char** b,
                             unsigned char* deada, unsigned char* livea)
{
    *r = currlayer->cellr;
    *g = currlayer->cellg;
    *b = currlayer->cellb;
    *deada = dead_alpha;
    *livea = live_alpha;
}

// -----------------------------------------------------------------------------

void DrawSelection(gRect& rect, bool active)
{
    // draw semi-transparent rectangle
    if (active) {
        SetColor(selectrgb.r, selectrgb.g, selectrgb.b, 128);
    } else {
        // use light gray to indicate an inactive selection
        SetColor(160, 160, 160, 128);
    }
    FillRect(rect.x, rect.y, rect.width, rect.height);
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

void InitPaste(lifealgo* palgo, gRect& bbox)
{
    // set globals used in DrawPasteImage
    pastealgo = palgo;
    pastebbox = bbox;
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

    // temporarily change OpenGL viewport's origin and size to match tempview
    glViewport(ileft, saveht-currht-itop, currwd, currht);
    
    // make dead pixels 100% transparent and live pixels 100% opaque
    dead_alpha = 0;
    live_alpha = 255;

    // draw paste pattern
    pastealgo->draw(tempview, renderer);

    // restore OpenGL viewport's origin and size
    glViewport(0, 0, savewd, saveht);

    showgridlines = saveshow;
    currwd = savewd;
    currht = saveht;

    // overlay translucent rect to show paste area
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
            GLfloat points[] = { XCOORD(-0.5), YCOORD(v-0.5),
                                 XCOORD(wd),   YCOORD(v-0.5) };
            glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(GLfloat), points, GL_STATIC_DRAW);
            glVertexAttribPointer(positionLoc, 2, GL_FLOAT, GL_FALSE, 0, 0);
            glEnableVertexAttribArray(positionLoc);
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
            GLfloat points[] = { XCOORD(h-0.5), YCOORD(-0.5),
                                 XCOORD(h-0.5), YCOORD(ht) };
            glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(GLfloat), points, GL_STATIC_DRAW);
            glVertexAttribPointer(positionLoc, 2, GL_FLOAT, GL_FALSE, 0, 0);
            glEnableVertexAttribArray(positionLoc);
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
                GLfloat points[] = { XCOORD(-0.5), YCOORD(v-0.5),
                                     XCOORD(wd),   YCOORD(v-0.5) };
                glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(GLfloat), points, GL_STATIC_DRAW);
                glVertexAttribPointer(positionLoc, 2, GL_FLOAT, GL_FALSE, 0, 0);
                glEnableVertexAttribArray(positionLoc);
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
                GLfloat points[] = { XCOORD(h-0.5), YCOORD(-0.5),
                                     XCOORD(h-0.5), YCOORD(ht) };
                glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(GLfloat), points, GL_STATIC_DRAW);
                glVertexAttribPointer(positionLoc, 2, GL_FLOAT, GL_FALSE, 0, 0);
                glEnableVertexAttribArray(positionLoc);
                glDrawArrays(GL_LINES, 0, 2);
            }
        }
    }
}

// -----------------------------------------------------------------------------

void DrawPattern(int tileindex)
{
    gRect r;
    int currmag = currlayer->view->getmag();

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
                                            currmag);
        else if ( currlayer->view->x > currlayer->algo->gridright )
            currlayer->view->setpositionmag(currlayer->algo->gridright,
                                            currlayer->view->y,
                                            currmag);
    }
    if ( currlayer->algo->gridht > 0) {
        if ( currlayer->view->y < currlayer->algo->gridtop )
            currlayer->view->setpositionmag(currlayer->view->x,
                                            currlayer->algo->gridtop,
                                            currmag);
        else if ( currlayer->view->y > currlayer->algo->gridbottom )
            currlayer->view->setpositionmag(currlayer->view->x,
                                            currlayer->algo->gridbottom,
                                            currmag);
    }

    if (nopattupdate) {
        // don't draw incomplete pattern, just draw grid lines and border
        currwd = currlayer->view->getwidth();
        currht = currlayer->view->getheight();
        if ( showgridlines && currmag >= mingridmag ) {
            DrawGridLines(currwd, currht);
        }
        if ( currlayer->algo->gridwd > 0 || currlayer->algo->gridht > 0 ) {
            DrawGridBorder(currwd, currht);
        }
        return;
    }

    if (showicons && currmag > 2) {
        // only show icons at scales 1:8 and above
        if (currmag == 3) {
            iconatlas = currlayer->atlas7x7;
            LoadIconAtlas(8, currlayer->numicons);
        } else if (currmag == 4) {
            iconatlas = currlayer->atlas15x15;
            LoadIconAtlas(16, currlayer->numicons);
        } else {
            iconatlas = currlayer->atlas31x31;
            LoadIconAtlas(32, currlayer->numicons);
        }
    } else if (currmag > 0) {
        LoadCellAtlas(1 << currmag, currlayer->numicons, 255);
    }

    currwd = currlayer->view->getwidth();
    currht = currlayer->view->getheight();
    
    // all pixels are initially opaque
    dead_alpha = 255;
    live_alpha = 255;

    // draw pattern using a sequence of pixblit calls
    currlayer->algo->draw(*currlayer->view, renderer);

    if ( showgridlines && currmag >= mingridmag ) {
        DrawGridLines(currwd, currht);
    }

    // if universe is bounded then draw border regions (if visible)
    if ( currlayer->algo->gridwd > 0 || currlayer->algo->gridht > 0 ) {
        DrawGridBorder(currwd, currht);
    }

    if ( currlayer->currsel.Visible(&r) ) {
        DrawSelection(r, true);
    }

    if (waitingforpaste) {
        DrawPasteImage();
    }
}
