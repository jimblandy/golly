--[[
This script lets people explore three-dimensional cellular automata
using Golly.  Inspired by the work of Carter Bays and his colleagues
(see http://www.cse.sc.edu/~bays/d4d4d4/).

The 3D drawing is based on code in "Macintosh Graphics in Modula-2"
by Russell L. Schnapp.  Note that orthographic projection is used to
speed up rendering (because all live cells look the same at a given
scale and rotation).

Author: Andrew Trevorrow (andrew@trevorrow.com), Feb 2018.

Thanks to Tom Rokicki for optimizing the NextGeneration code.

TODO: !!!

- implement undo/redo
- try probabilistic culling to speed up high-density rendering
- allow saving as .vti file for use by Ready?
- add option for 6-face-neighbor rules (start or end rule with V?)
- also option for 12-neighbor sphere packing rules?
- support Busy Boxes (via rules starting with BB?)

NOTE: Do following changes for the Golly 3.2b1 release:

- add menu bar and pop-up menus from oplus
- get round() from gplus
- fix textoption background problem in m.button code in oplus/init.lua
- implement g.settitle(string) so we can put pattname and 3D rule in
  window title and avoid using g.setname (which adds an undo item)
- implement g.setoption("showtimeline",0)
- implement g.setoption("showscrollbars",0)
- implement optional parameter for g.note and g.warn so scripts can
  disable the Cancel button:
  g.note(msg, cancel=true)
  g.warn(msg, cancel=true)
- implement g.sleep(millisecs) for use in Lua scripts
- implement "open filepath" event for g.getevent and get Golly to
  automatically start up 3D.lua if user opens a .3d file
- avoid "getclipstr error: no text in clipboard" (return empty string)
- throttle ozoom* events (in here or in Golly)?
- fix unicode problems reported here:
  http://www.conwaylife.com/forums/viewtopic.php?f=7&t=3132#p52918
--]]

local g = golly()
--!!! require "gplus.strict"
local gp = require "gplus"
local int = gp.int
local validint = gp.validint
local split = gp.split

local ov = g.overlay
local op = require "oplus"

local sin = math.sin
local cos = math.cos
local acos = math.acos
local sqrt = math.sqrt
local rand = math.random
local abs = math.abs
local min = math.min
local max = math.max
local floor = math.floor
local ceil = math.ceil

math.randomseed(os.time())          -- init seed for math.random

local N = 30                        -- initial grid size (N*N*N cells)
local MINN = 3                      -- minimum grid size
local MAXN = 100                    -- maximum grid size
local BORDER = 2                    -- space around live cubes
local MINSIZE = 1+BORDER*2          -- minimum size of empty cells
local MAXSIZE = 100                 -- maximum size of empty cells
local CELLSIZE = 15                 -- initial size of empty cells
local HALFCELL = CELLSIZE/2.0       -- for drawing mid points of cells
local LEN = CELLSIZE-BORDER*2       -- size of live cubes
local DEGTORAD = math.pi/180.0      -- converts degrees to radians

-- MIDGRID is used to ensure that rotation occurs about the
-- mid point of the middle cube in the grid
local MIDGRID = (N+1-(N%2))*HALFCELL

local BACK_COLOR = "0 0 65 255"     -- for drawing background
local LINE_COLOR = "60 60 90 255"   -- for drawing lattice lines
local X_COLOR = op.red              -- for drawing X axis
local Y_COLOR = op.green            -- for drawing Y axis
local Z_COLOR = op.blue             -- for drawing Z axis

local xylattice = {}                -- lattice lines between X,Y axes
local xzlattice = {}                -- lattice lines between X,Z axes
local yzlattice = {}                -- lattice lines between Y,Z axes
local xaxis = {}                    -- X axis
local yaxis = {}                    -- Y axis
local zaxis = {}                    -- Z axis

local grid1 = {}                    -- sparse 3D matrix with up to N*N*N live cells
local grid2 = {}                    -- sparse 3D matrix for the next generation
local popcount = 0                  -- number of live cells
local pattname = "untitled"         -- most recently saved/opened pattern
local showaxes = true               -- draw axes and lattice lines?
local generating = false            -- generate pattern?
local gencount = 0                  -- current generation count
local perc = 20                     -- initial percentage for random fill
local message = nil                 -- text message displayed by Refresh if not nil
local selcount = 0                  -- number of selected cells (live or dead)
local selected = {}                 -- grid positions of selected cells
local pastecount = 0                -- number of cells in paste pattern
local pastecell = {}                -- grid positions of cells in paste pattern
local drawstate = 1                 -- for drawing/erasing cells
local selstate = true               -- for selecting/deselecting cells
local celltype = "cube"             -- draw live cell as cube/sphere/point
local DrawLiveCell                  -- set to DrawCube or DrawSphere or DrawPoint

local active = {}                   -- grid positions of cells in active plane
local activeplane = "XY"            -- orientation of active plane (XY/XZ/YZ)
local activepos = 0                 -- active plane's cell position along 3rd axis
local activecell = ""               -- "x,y,z" if mouse is inside an active cell
local prevactive = ""               -- previous activecell

-- boundary grid coords for live cells (not always minimal)
local minx, miny, minz
local maxx, maxy, maxz
local minimal_live_bounds = true    -- becomes false if a live cell is deleted

-- boundary grid coords for selected cells (not always minimal)
local minselx, minsely, minselz
local maxselx, maxsely, maxselz
local minimal_sel_bounds = true     -- becomes false if a selected cell is deselected

-- boundary grid coords for the active plane (always minimal)
local minactivex, minactivey, minactivez
local maxactivex, maxactivey, maxactivez

-- boundary grid coords for the paste pattern (always minimal)
local minpastex, minpastey, minpastez
local maxpastex, maxpastey, maxpastez

local startN = N                    -- starting grid size
local startplane                    -- starting orientation of active plane
local startpos                      -- starting position of active plane
local startrule                     -- starting rule
local startcount = 0                -- starting gencount (can be > 0)
local startname                     -- starting pattern name
local startcells                    -- starting pattern
local startselcount                 -- starting selcount
local startselected                 -- starting selection

local refcube = {}                  -- invisible reference cube
local rotrefz = {}                  -- Z coords of refcube's rotated vertices
local rotx = {}                     -- rotated X coords for 8 vertices
local roty = {}                     -- rotated Y coords for 8 vertices
local rotz = {}                     -- rotated Z coords for 8 vertices
local projectedx = {}               -- projected X coords for 8 vertices
local projectedy = {}               -- projected Y coords for 8 vertices

-- current transformation matrix
local xixo, yixo, zixo
local xiyo, yiyo, ziyo
local xizo, yizo, zizo

local viewwd, viewht                -- current size of viewport
local ovwd, ovht                    -- current size of overlay
local minwd, minht = 600, 100       -- minimum size of overlay
local midx, midy                    -- overlay's middle pixel

-- tool bar controls
local newbutton                     -- New
local openbutton                    -- Open...
local savebutton                    -- Save...
local runbutton                     -- Run...
local gridbutton                    -- Grid...
local randbutton                    -- Random...
local rulebutton                    -- Rule...
local ssbutton                      -- Start/Stop
local s1button                      -- +1
local resetbutton                   -- Reset
local fitbutton                     -- Fit
local undobutton                    -- Undo
local redobutton                    -- Redo
local helpbutton                    -- ?
local exitbutton                    -- X
local drawbox                       -- check box for draw mode
local selectbox                     -- check box for select mode
local movebox                       -- check box for move mode

local pastemenu                     -- pop-up menu for choosing a paste action
local selmenu                       -- pop-up menu for choosing a selection action

local buttonht = 20
local gap = 10                      -- space around buttons
local toolbarht = buttonht*2+gap*3  -- two rows of buttons

local drawcursor = "pencil"         -- cursor for drawing cells
local selectcursor = "cross"        -- cursor for selecting cells
local movecursor = "hand"           -- cursor for rotating grid
local currcursor = movecursor       -- current cursor
local arrow_cursor = false          -- true if cursor is in tool bar

local DEFAULT_RULE = "5,6,7/6"      -- initial rule
local rulestring = DEFAULT_RULE
local survivals = {}
local births = {}
survivals[5] = true                 -- WARNING: must match DEFAULT_RULE
survivals[6] = true                 -- ditto
survivals[7] = true                 -- ditto
births[6] = true                    -- ditto

local pattdir = g.getdir("data")    -- initial directory for OpenPattern/SavePattern
local scriptdir = g.getdir("app")   -- initial directory for RunScript
local scriptlevel = 0               -- greater than 0 if a user script is running

-- the default path for the script to run when 3D.lua starts up
local pathsep = g.getdir("app"):sub(-1)
local startup = g.getdir("app").."My-scripts"..pathsep.."3D-start.lua"

-- user settings are stored in this file
local settingsfile = g.getdir("data").."3D.ini"

-- batch draw settings !BATCHDRAW!
local xybatch = {}                  -- coordinates for each cell
local usebatch = false              -- whether to use batch drawing (enable in Golly 3.2b1!!!)

----------------------------------------------------------------------

function AppendCounts(array, counts, bcount)
    local mincount = 0
    if bcount then mincount = 1 end
    for _, item in ipairs(array) do
        if validint(item) then
            local i = tonumber(item)
            if (i >= mincount) and (i <= 26) then
                counts[#counts+1] = i
            else
                if bcount then
                    g.warn("Birth count must be from 1 to 26.")
                else
                    g.warn("Survival count must be from 0 to 26.")
                end
                return false
            end
        else
            g.warn("Illegal number in rule: "..item)
            return false
        end
    end
    return true
end

--------------------------------------------------------------------------------

function ParseRule(newrule)
    -- parse newrule and set rulestring, survivals and births if ok
    if #newrule == 0 then newrule = DEFAULT_RULE end
    if not newrule:find("/") then
        g.warn("Rule has no / separator.")
        return false
    end
    local s,b = split(newrule,"/")
    if #s == 0 then s = {} else s = {split(s,",")} end
    if b == nil or #b == 0 then b = {} else b = {split(b,",")} end
    local news = {}
    local newb = {}
    if not AppendCounts(s, news, false) then return false end
    if not AppendCounts(b, newb, true) then return false end
       
    -- newrule is okay so set survivals and births
    for i = 0, 26 do
        survivals[i] = false
        births[i] = false
    end
    for _, i in ipairs(news) do survivals[i] = true end
    for _, i in ipairs(newb) do births[i] = true end
    
    -- set rulestring to canonical form
    rulestring = ""
    for i = 0, 26 do
        if survivals[i] then
            if #rulestring > 0 then rulestring = rulestring.."," end
            rulestring = rulestring..i
        end
    end
    rulestring = rulestring.."/"
    for i = 1, 26 do
        if births[i] then
            if rulestring:sub(-1) ~= "/" then rulestring = rulestring.."," end
            rulestring = rulestring..i
        end
    end    
    
    return true
end

--------------------------------------------------------------------------------

function ReadSettings()
    local f = io.open(settingsfile, "r")
    if f then
        -- must match order in WriteSettings
        startup = f:read("*l") or "3D-start.lua"
        N = tonumber(f:read("*l")) or 30
        rulestring = f:read("*l") or DEFAULT_RULE
        showaxes = (f:read("*l") or "true") == "true"
        celltype = f:read("*l") or "cube"
        perc = tonumber(f:read("*l")) or 20
        pattdir = f:read("*l") or g.getdir("data")
        scriptdir = f:read("*l") or g.getdir("app")
        f:close()
        
        -- update all parameters that depend on N
        MIDGRID = (N+1-(N%2))*HALFCELL
        
        if not ParseRule(rulestring) then
            g.warn("Resetting bad rule ("..rulestring..") to default.")
            rulestring = DEFAULT_RULE
        end
        
        -- celltype used to be true/false
        if celltype == "true" then
            celltype = "point"
        elseif celltype == "false" then
            celltype = "cube"
        end
    end
end

--------------------------------------------------------------------------------

function WriteSettings()
    local f = io.open(settingsfile, "w")
    if f then
        -- must match order in ReadSettings
        f:write(startup.."\n")
        f:write(tostring(N).."\n")
        f:write(rulestring.."\n")
        f:write(tostring(showaxes).."\n")
        f:write(celltype.."\n")
        f:write(tostring(perc).."\n")
        f:write(pattdir.."\n")
        f:write(scriptdir.."\n")
        f:close()
    end
end

----------------------------------------------------------------------

function SaveGollyState()
    local oldstate = {}
    oldstate.name = g.getname()
    g.setname("3D")
    g.update()  -- see new title
    oldstate.files = g.setoption("showfiles", 0)
    oldstate.tool = g.setoption("showtoolbar", 0)
    oldstate.status = g.setoption("showstatusbar", 0)
    oldstate.layer = g.setoption("showlayerbar", 0)
    oldstate.edit = g.setoption("showeditbar", 0)
    oldstate.tile = g.setoption("tilelayers", 0)
    oldstate.stack = g.setoption("stacklayers", 0)
    return oldstate
end

----------------------------------------------------------------------

function RestoreGollyState(oldstate)
    ov("delete")
    g.setname(oldstate.name)
    g.setoption("showfiles", oldstate.files)
    g.setoption("showtoolbar", oldstate.tool)
    g.setoption("showstatusbar", oldstate.status)
    g.setoption("showlayerbar", oldstate.layer)
    g.setoption("showeditbar", oldstate.edit)
    g.setoption("tilelayers", oldstate.tile)
    g.setoption("stacklayers", oldstate.stack)
end

--------------------------------------------------------------------------------

local function round(r)
    -- return same result as Python's round function
    if r >= 0 then
        return floor(r+0.5)
    else
        return ceil(r-0.5)
    end
end

----------------------------------------------------------------------

local function TransformPoint(point)
    -- rotate given 3D point
    local x, y, z = point[1], point[2], point[3]
    local newx = (x*xixo + y*xiyo + z*xizo)
    local newy = (x*yixo + y*yiyo + z*yizo)
    local newz = (x*zixo + y*ziyo + z*zizo)
    --[[ or do this to rotate about grid axes
    local newx = (x*xixo + y*yixo + z*zixo)
    local newy = (x*xiyo + y*yiyo + z*ziyo)
    local newz = (x*xizo + y*yizo + z*zizo)
    --]]
    return newx, newy, newz
end

----------------------------------------------------------------------

local function DrawLine(x1, y1, x2, y2)
    ov("line "..x1.." "..y1.." "..x2.." "..y2)
end

----------------------------------------------------------------------

local function HorizontalLine(x1, x2, y)
    -- draw a horizontal line of cells from x1,y to x2,y
    ov("line "..x1.." "..y.." "..x2.." "..y)
end

----------------------------------------------------------------------

local function DrawEdge(start, finish)
    DrawLine(projectedx[start],  projectedy[start],
             projectedx[finish], projectedy[finish])
end

----------------------------------------------------------------------

local function FillFace(ax, ay, bx, by, cx, cy, dx, dy, shade)
    -- fill given convex quadrilateral using modified code from here:
    -- http://www-users.mat.uni.torun.pl/~wrona/3d_tutor/tri_fillers.html
      
    -- relabel points so that ay <= by <= cy <= dy;
    -- note that given vertices are in cyclic order, so a is opposite c
    -- and b is opposite d
    if ay > cy then
        -- swap a and c (diagonally opposite)
        ax, cx = cx, ax
        ay, cy = cy, ay
    end
    if by > dy then
        -- swap b and d (diagonally opposite)
        bx, dx = dx, bx
        by, dy = dy, by
    end
    -- we now have ay <= cy and by <= dy
    if ay > by then
        -- swap a and b
        ax, bx = bx, ax
        ay, by = by, ay
    end
    if cy > dy then
        -- swap c and d
        cx, dx = dx, cx
        cy, dy = dy, cy
    end
    
    -- if top and/or bottom line is horizontal then we need to ensure
    -- that lines a->c and b->d don't intersect
    if ay == by then
        if (ax - bx) * (cx - dx) < 0 then
            -- swap ax and bx
            ax, bx = bx, ax
        end
    elseif cy == dy then
        if (ax - bx) * (cx - dx) < 0 then
            -- swap cx and dx
            cx, dx = dx, cx
        end
    end
    
    -- calculate deltas for interpolation
    local delta1 = 0.0
    local delta2 = 0.0
    local delta3 = 0.0
    local delta4 = 0.0
    if by-ay > 0 then delta1 = (bx-ax) / (by-ay) end
    if cy-ay > 0 then delta2 = (cx-ax) / (cy-ay) end
    if dy-by > 0 then delta3 = (dx-bx) / (dy-by) end
    if dy-cy > 0 then delta4 = (dx-cx) / (dy-cy) end
    
    -- draw horizontal segments from sx to ex (start and end X coords)
    ov(shade)
    local sx = ax
    local sy = ay
    local ex = sx
    while sy < by do
        HorizontalLine(round(sx), round(ex), sy)
        sy = sy + 1
        sx = sx + delta1
        ex = ex + delta2
    end
    sx = bx
    while sy < cy do
        HorizontalLine(round(sx), round(ex), sy)
        sy = sy + 1
        sx = sx + delta3
        ex = ex + delta2
    end
    ex = cx
    while sy < dy do
        HorizontalLine(round(sx), round(ex), sy)
        sy = sy + 1
        sx = sx + delta3
        ex = ex + delta4
    end
end

----------------------------------------------------------------------

local function CheckFaces(f1v1, f1v2, f1v3, f1v4,
                          f2v1, f2v2, f2v3, f2v4)

    local function ComputeShade(v1, v2)
        local x1, y1, z1 = rotx[v1], roty[v1], rotz[v1]
        local x2, y2, z2 = rotx[v2], roty[v2], rotz[v2]
        local costheta = (x1-x2) / sqrt( (x1-x2)*(x1-x2) +
                                         (y1-y2)*(y1-y2) +
                                         (z1-z2)*(z1-z2) )
        -- costheta ranges from -1.0 to 1.0
        local shade = 255 - int((costheta + 1.0) / 2.0 * 128)
        return "rgba "..shade.." "..shade.." "..shade.." 255"
    end

    -- test rotated z coords to see which face is in front
    if rotz[f1v1] < rotz[f2v1] then
        FillFace(projectedx[f1v1], projectedy[f1v1],
                 projectedx[f1v2], projectedy[f1v2],
                 projectedx[f1v3], projectedy[f1v3],
                 projectedx[f1v4], projectedy[f1v4],
                 ComputeShade(f1v1, f2v1))
    else
        FillFace(projectedx[f2v1], projectedy[f2v1],
                 projectedx[f2v2], projectedy[f2v2],
                 projectedx[f2v3], projectedy[f2v3],
                 projectedx[f2v4], projectedy[f2v4],
                 ComputeShade(f2v1, f1v1))
    end
end

----------------------------------------------------------------------

local function CreateCube(x, y, z)
    -- create cube at given grid position
    x = x * CELLSIZE + BORDER - MIDGRID
    y = y * CELLSIZE + BORDER - MIDGRID
    z = z * CELLSIZE + BORDER - MIDGRID
    
    -- WARNING: vertex order used here is assumed by other code:
    -- vertices 1,3,5,7 and 2,4,6,8 are front and back faces,
    -- vertices 1,2,8,7 and 3,4,6,5 are bottom and top faces,
    -- vertices 1,2,4,3 and 7,8,6,5 are left and right faces
    --
    --           +y
    --            |
    --            v4_________v6
    --           /|         /|
    --          / |        / |
    --        v3__|______v5  |
    --        |   |      |   |
    --        |   v2_____|___v8___+x
    --        |  /       |  /
    --        | /        | /
    --        v1_________v7
    --       /
    --     +z
    --
    return { {x    , y    , z+LEN},     -- v1
             {x    , y    , z    },     -- v2
             {x    , y+LEN, z+LEN},     -- v3
             {x    , y+LEN, z    },     -- v4
             {x+LEN, y+LEN, z+LEN},     -- v5
             {x+LEN, y+LEN, z    },     -- v6
             {x+LEN, y    , z+LEN},     -- v7
             {x+LEN, y    , z    }      -- v8
           }
end

----------------------------------------------------------------------

local function DisplayLine(line)
    -- use orthographic projection to transform line's start and end points
    local newx, newy = TransformPoint(line[1])
    local x1 = round(newx) + midx
    local y1 = round(newy) + midy
    
    newx, newy = TransformPoint(line[2])
    local x2 = round(newx) + midx
    local y2 = round(newy) + midy
    
    DrawLine(x1, y1, x2, y2)
end

----------------------------------------------------------------------

function DrawRearAxes()
    -- draw lattice lines and axes that are behind rotated reference cube
    -- assuming vertex order set in CreateCube
    local z1 = rotrefz[1]
    local z2 = rotrefz[2]
    local z3 = rotrefz[3]
    local z7 = rotrefz[7]
    
    ov("rgba"..LINE_COLOR)
    if z1 < z2 then
        -- front face of rotated refcube is visible
        for _, line in ipairs(xylattice) do DisplayLine(line) end
    end
    if z1 >= z7 then
        -- right face of rotated refcube is visible
        for _, line in ipairs(yzlattice) do DisplayLine(line) end
    end
    if z1 >= z3 then
        -- top face of rotated refcube is visible
        for _, line in ipairs(xzlattice) do DisplayLine(line) end
    end
    
    -- draw thicker anti-aliased axes
    ov("blend 1")
    ov("lineoption width 3")
    if z1 < z2  or z1 >= z3 then ov(X_COLOR); DisplayLine(xaxis) end
    if z1 < z2  or z1 >= z7 then ov(Y_COLOR); DisplayLine(yaxis) end
    if z1 >= z7 or z1 >= z3 then ov(Z_COLOR); DisplayLine(zaxis) end
    ov("lineoption width 1")
    ov("blend 0")
end

----------------------------------------------------------------------

function DrawFrontAxes()
    -- draw lattice lines and axes that are in front of rotated reference cube
    -- assuming vertex order set in CreateCube
    local z1 = rotrefz[1]
    local z2 = rotrefz[2]
    local z3 = rotrefz[3]
    local z7 = rotrefz[7]
    
    ov("rgba"..LINE_COLOR)
    if z1 >= z2 then
        -- back face of rotated refcube is visible
        for _, line in ipairs(xylattice) do DisplayLine(line) end
    end
    if z1 < z7 then
        -- left face of rotated refcube is visible
        for _, line in ipairs(yzlattice) do DisplayLine(line) end
    end
    if z1 < z3 then
        -- bottom face of rotated refcube is visible
        for _, line in ipairs(xzlattice) do DisplayLine(line) end
    end
    
    -- draw thicker anti-aliased axes
    ov("blend 1")
    ov("lineoption width 3")
    if z1 >= z2 or z1 < z3 then ov(X_COLOR); DisplayLine(xaxis) end
    if z1 >= z2 or z1 < z7 then ov(Y_COLOR); DisplayLine(yaxis) end
    if z1 <  z7 or z1 < z3 then ov(Z_COLOR); DisplayLine(zaxis) end
    ov("lineoption width 1")
    ov("blend 0")
end

----------------------------------------------------------------------

function CreateAxes()
    -- create axes and lattice lines
        
    -- put axes origin at most -ve corner of grid
    local o = -MIDGRID
    local endpt = o + N*CELLSIZE
    
    xaxis = {{o,o,o}, {endpt,o,o}}
    yaxis = {{o,o,o}, {o,endpt,o}}
    zaxis = {{o,o,o}, {o,o,endpt}}
    
    -- x,y lattice
    xylattice = {}
    for i = 1, N do
        local offset = i*CELLSIZE
        xylattice[#xylattice+1] = {{o+offset,o,o}, {o+offset,endpt,o}}
        xylattice[#xylattice+1] = {{o,o+offset,o}, {endpt,o+offset,o}}
    end
    
    -- x,z lattice
    xzlattice = {}
    for i = 1, N do
        local offset = i*CELLSIZE
        xzlattice[#xzlattice+1] = {{o,o,o+offset}, {endpt,o,o+offset}}
        xzlattice[#xzlattice+1] = {{o+offset,o,o}, {o+offset,o,endpt}}
    end
    
    -- y,z lattice
    yzlattice = {}
    for i = 1, N do
        local offset = i*CELLSIZE
        yzlattice[#yzlattice+1] = {{o,o+offset,o}, {o,o+offset,endpt}}
        yzlattice[#yzlattice+1] = {{o,o,o+offset}, {o,endpt,o+offset}}
    end
end

----------------------------------------------------------------------

function CreateTranslucentCell(clipname, color)
    -- create a clip containing a translucent cube with given color
    
    ov("create "..(CELLSIZE*2).." "..(CELLSIZE*2).." "..clipname)
    ov("target "..clipname)
    ov("rgba "..color)
    
    -- temporarily change BORDER and LEN so CreateCube fills cell
    local oldBORDER = BORDER
    local oldLEN = LEN
    BORDER = 1
    LEN = CELLSIZE-2 - (CELLSIZE%2)
    
    local midpos = N//2
    local cube = CreateCube(midpos, midpos, midpos)

    -- create cube's projected vertices (within clip)
    for i = 1, 8 do
        rotx[i], roty[i], rotz[i] = TransformPoint(cube[i])
        projectedx[i] = round( rotx[i] ) + CELLSIZE
        projectedy[i] = round( roty[i] ) + CELLSIZE
    end
    
    -- draw outer edges of cube then flood from center
    if rotz[1] < rotz[2] then
        -- front face is visible
        if rotz[5] < rotz[3] then
            -- right face is visible
            DrawEdge(1,3)
            if rotz[5] < rotz[7] then
                -- top face is visible
                DrawEdge(3,4)
                DrawEdge(4,6)
                DrawEdge(6,8)
                DrawEdge(8,7)
                DrawEdge(7,1)
            else
                -- bottom face is visible
                DrawEdge(3,5)
                DrawEdge(5,6)
                DrawEdge(6,8)
                DrawEdge(8,2)
                DrawEdge(2,1)
            end
        else
            -- left face is visible
            DrawEdge(5,7)
            if rotz[5] < rotz[7] then
                -- top face is visible
                DrawEdge(7,1)
                DrawEdge(1,2)
                DrawEdge(2,4)
                DrawEdge(4,6)
                DrawEdge(6,5)
            else
                -- bottom face is visible
                DrawEdge(7,8)
                DrawEdge(8,2)
                DrawEdge(2,4)
                DrawEdge(4,3)
                DrawEdge(3,5)
            end
        end
    else
        -- back face is visible
        if rotz[5] < rotz[3] then
            -- right face is visible
            DrawEdge(5,7)
            if rotz[5] < rotz[7] then
                -- top face is visible
                DrawEdge(7,8)
                DrawEdge(8,2)
                DrawEdge(2,4)
                DrawEdge(4,3)
                DrawEdge(3,5)
            else
                -- bottom face is visible
                DrawEdge(7,1)
                DrawEdge(1,2)
                DrawEdge(2,4)
                DrawEdge(4,6)
                DrawEdge(6,5)
            end
        else
            -- left face is visible
            DrawEdge(1,3)
            if rotz[5] < rotz[7] then
                -- top face is visible
                DrawEdge(3,5)
                DrawEdge(5,6)
                DrawEdge(6,8)
                DrawEdge(8,2)
                DrawEdge(2,1)
            else
                -- bottom face is visible
                DrawEdge(3,4)
                DrawEdge(4,6)
                DrawEdge(6,8)
                DrawEdge(8,7)
                DrawEdge(7,1)
            end
        end
    end

    ov("flood "..CELLSIZE.." "..CELLSIZE)
    
    -- restore BORDER and LEN
    BORDER = oldBORDER
    LEN = oldLEN
    
    ov("optimize "..clipname)
    ov("target")
end

----------------------------------------------------------------------

function CreateLiveCube()
    -- create a clip containing one rotated cube that will be used later
    -- to draw all live cells (this only works because all cubes are identical
    -- in appearance when using orthographic projection)
    ov("create "..(LEN*2).." "..(LEN*2).." c")
    ov("target c")
    
    local midpos = N//2
    local cube = CreateCube(midpos, midpos, midpos)

    -- create cube's projected vertices (within clip)
    for i = 1, 8 do
        rotx[i], roty[i], rotz[i] = TransformPoint(cube[i])
        projectedx[i] = round( rotx[i] ) + LEN
        projectedy[i] = round( roty[i] ) + LEN
    end
    
    -- draw up to 3 visible faces of cube, using cyclic vertex order set in CreateCube
    CheckFaces(1,3,5,7, 2,4,6,8)    -- front or back
    CheckFaces(1,2,8,7, 3,4,6,5)    -- bottom or top
    CheckFaces(1,2,4,3, 7,8,6,5)    -- left or right
    
    if LEN > 4 then
        -- draw anti-aliased edges around visible face(s)
        if LEN == 5 then
            ov("rgba 150 150 150 255")
        elseif LEN == 6 then
            ov("rgba 110 110 110 255")
        elseif LEN == 7 then
            ov("rgba 80 80 80 255")
        else
            ov("rgba 60 60 60 255")
        end
        ov("blend 1")
        ov("lineoption width "..(1 + int(LEN / 40.0)))
        
        if rotz[1] < rotz[2] then
            -- draw all edges around front face
            DrawEdge(1,3)
            DrawEdge(3,5)
            DrawEdge(5,7)
            DrawEdge(7,1)
            if rotz[1] < rotz[3] then
                -- draw remaining 3 edges around bottom face
                DrawEdge(1,2)
                DrawEdge(2,8)
                DrawEdge(8,7)
                if rotz[1] < rotz[7] then
                    -- draw remaining 2 edges around left face
                    DrawEdge(3,4)
                    DrawEdge(4,2)
                else
                    -- draw remaining 2 edges around right face
                    DrawEdge(5,6)
                    DrawEdge(6,8)
                end
            else
                -- draw 3 remaining edges around top face
                DrawEdge(3,4)
                DrawEdge(4,6)
                DrawEdge(6,5)
                if rotz[1] < rotz[7] then
                    -- draw remaining 2 edges around left face
                    DrawEdge(4,2)
                    DrawEdge(2,1)
                else
                    -- draw remaining 2 edges around right face
                    DrawEdge(6,8)
                    DrawEdge(8,7)
                end
            end
        else
            -- draw all edges around back face
            DrawEdge(2,4)
            DrawEdge(4,6)
            DrawEdge(6,8)
            DrawEdge(8,2)
            if rotz[1] < rotz[3] then
                -- draw remaining 3 edges around bottom face
                DrawEdge(2,1)
                DrawEdge(1,7)
                DrawEdge(7,8)
                if rotz[1] < rotz[7] then
                    -- draw remaining 2 edges around left face
                    DrawEdge(1,3)
                    DrawEdge(3,4)
                else
                    -- draw remaining 2 edges around right face
                    DrawEdge(7,5)
                    DrawEdge(5,6)
                end
            else
                -- draw 3 remaining edges around top face
                DrawEdge(6,5)
                DrawEdge(5,3)
                DrawEdge(3,4)
                if rotz[1] < rotz[7] then
                    -- draw remaining 2 edges around left face
                    DrawEdge(2,1)
                    DrawEdge(1,3)
                else
                    -- draw remaining 2 edges around right face
                    DrawEdge(8,7)
                    DrawEdge(7,5)
                end
            end
        end
        
        ov("lineoption width 1")
        ov("blend 0")
    end
    
    ov("optimize c")
    ov("target")
end

----------------------------------------------------------------------

function CreateLiveSphere()
    -- create a clip containing one sphere that will be used later
    -- to draw all live cells
    local diameter = CELLSIZE-2
    ov("create "..diameter.." "..diameter.." S")    -- s is used for selected cells
    ov("target S")

    local x = 0
    local y = 0
    local gray = 180 - diameter//10
    local grayinc = 3
    if diameter < 50 then grayinc = 8 - diameter//10 end
    local r = (diameter+1)//2
    ov("blend 1")
    while true do
        ov("rgba "..gray.." "..gray.." "..gray.." 255")
        -- draw a solid circle by setting the line width to the radius
        ov("lineoption width "..r)
        ov("ellipse "..x.." "..y.." "..diameter.." "..diameter)
        diameter = diameter - 2
        r = r - 1
        if r < 2 then break end
        x = x + r%2
        y = y + r%2
        gray = gray + grayinc
        if gray > 255 then gray = 255 end
    end
    ov("blend 0")
    ov("lineoption width 1")

    ov("optimize S")
    ov("target")
end

----------------------------------------------------------------------

local function ResetBatch() -- !BATCHDRAW!
    xybatch = {}
end

----------------------------------------------------------------------

local function DrawBatch() -- !BATCHDRAW!
    if celltype == "cube" then
        ov("paste "..table.concat(xybatch, " ").." c")
    elseif celltype == "sphere" then
        ov("paste "..table.concat(xybatch, " ").." S")
    else -- celltype == "point"
        ov(op.white)
        ov("set "..table.concat(xybatch, " "))
    end
end

----------------------------------------------------------------------

local function AddCubeToBatch(x, y, z) -- !BATCHDRAW!
    -- add live cell as a cube at given grid position
    x = x * CELLSIZE + HALFCELL - MIDGRID
    y = y * CELLSIZE + HALFCELL - MIDGRID
    z = z * CELLSIZE + HALFCELL - MIDGRID
    local newx, newy = TransformPoint({x, y, z})
    -- use orthographic projection
    x = round(newx) + midx - LEN
    y = round(newy) + midy - LEN
    -- add to the list to draw
    xybatch[#xybatch + 1] = x
    xybatch[#xybatch + 1] = y
end

----------------------------------------------------------------------

local function DrawCube(x, y, z)
    -- draw live cell as a cube at given grid position
    x = x * CELLSIZE + HALFCELL - MIDGRID
    y = y * CELLSIZE + HALFCELL - MIDGRID
    z = z * CELLSIZE + HALFCELL - MIDGRID
    local newx, newy = TransformPoint({x, y, z})
    -- use orthographic projection
    x = round(newx) + midx - LEN
    y = round(newy) + midy - LEN
    -- draw the clip created by CreateLiveCube
    ov("paste "..x.." "..y.." c")
end

----------------------------------------------------------------------

local function AddSphereToBatch(x, y, z)
    -- add live cell as a sphere at given grid position
    x = x * CELLSIZE + HALFCELL - MIDGRID
    y = y * CELLSIZE + HALFCELL - MIDGRID
    z = z * CELLSIZE + HALFCELL - MIDGRID
    local newx, newy = TransformPoint({x, y, z})
    -- use orthographic projection
    x = round(newx + midx - HALFCELL+1)     -- clip wd = CELLSIZE-2
    y = round(newy + midy - HALFCELL+1)     -- clip ht = CELLSIZE-2
    -- add to the list to draw
    xybatch[#xybatch + 1] = x
    xybatch[#xybatch + 1] = y
end

----------------------------------------------------------------------

local function DrawSphere(x, y, z)
    -- draw live cell as a sphere at given grid position
    x = x * CELLSIZE + HALFCELL - MIDGRID
    y = y * CELLSIZE + HALFCELL - MIDGRID
    z = z * CELLSIZE + HALFCELL - MIDGRID
    local newx, newy = TransformPoint({x, y, z})
    -- use orthographic projection
    x = round(newx + midx - HALFCELL+1)     -- clip wd = CELLSIZE-2
    y = round(newy + midy - HALFCELL+1)     -- clip ht = CELLSIZE-2
    -- draw the clip created by CreateLiveSphere
    ov("paste "..x.." "..y.." S")
end


----------------------------------------------------------------------

local function AddPointToBatch(x, y, z)
    -- add mid point of cell at given grid position
    x = x * CELLSIZE + HALFCELL - MIDGRID
    y = y * CELLSIZE + HALFCELL - MIDGRID
    z = z * CELLSIZE + HALFCELL - MIDGRID
    local newx, newy = TransformPoint({x, y, z})
    -- use orthographic projection
    x = round(newx) + midx
    y = round(newy) + midy
    -- add to the list to draw
    xybatch[#xybatch + 1] = x
    xybatch[#xybatch + 1] = y
end

----------------------------------------------------------------------

local function DrawPoint(x, y, z)
    -- draw mid point of cell at given grid position
    x = x * CELLSIZE + HALFCELL - MIDGRID
    y = y * CELLSIZE + HALFCELL - MIDGRID
    z = z * CELLSIZE + HALFCELL - MIDGRID
    local newx, newy = TransformPoint({x, y, z})
    -- use orthographic projection
    x = round(newx) + midx
    y = round(newy) + midy
    ov("set "..x.." "..y)
end

----------------------------------------------------------------------

local function DrawActiveCell(x, y, z)
    x = x * CELLSIZE + HALFCELL - MIDGRID
    y = y * CELLSIZE + HALFCELL - MIDGRID
    z = z * CELLSIZE + HALFCELL - MIDGRID
    local newx, newy = TransformPoint({x, y, z})
    -- use orthographic projection
    x = round(newx) + midx - CELLSIZE
    y = round(newy) + midy - CELLSIZE
    -- draw the clip created by CreateTranslucentCell
    ov("paste "..x.." "..y.." a")
end

----------------------------------------------------------------------

local function DrawSelectedCell(x, y, z)
    x = x * CELLSIZE + HALFCELL - MIDGRID
    y = y * CELLSIZE + HALFCELL - MIDGRID
    z = z * CELLSIZE + HALFCELL - MIDGRID
    local newx, newy = TransformPoint({x, y, z})
    -- use orthographic projection
    x = round(newx) + midx - CELLSIZE
    y = round(newy) + midy - CELLSIZE
    -- draw the clip created by CreateTranslucentCell
    ov("paste "..x.." "..y.." s")
end

----------------------------------------------------------------------

local function DrawPasteCell(x, y, z)
    x = x * CELLSIZE + HALFCELL - MIDGRID
    y = y * CELLSIZE + HALFCELL - MIDGRID
    z = z * CELLSIZE + HALFCELL - MIDGRID
    local newx, newy = TransformPoint({x, y, z})
    -- use orthographic projection
    x = round(newx) + midx - CELLSIZE
    y = round(newy) + midy - CELLSIZE
    -- draw the clip created by CreateTranslucentCell
    ov("paste "..x.." "..y.." p")
end

----------------------------------------------------------------------

local function TestCell(editing, gridpos, x, y, z)
    -- called from DisplayCells to test if given cell is in active plane,
    -- or if it's selected, or if it's a paste cell, and to draw it accordingly
    if editing then
        if active[gridpos] then
            if grid1[gridpos] then
                -- draw live cell within active plane
                DrawLiveCell(x, y, z)
            end
            DrawActiveCell(x, y, z)
            if selected[gridpos] then
                DrawSelectedCell(x, y, z)
            end
        else
            -- cell is outside active plane
            if grid1[gridpos] then
                -- live cell so draw white point
                ov(op.white)
                DrawPoint(x, y, z)
            end
            if selected[gridpos] then
                -- draw translucent green point
                ov("rgba 0 255 0 128")
                DrawPoint(x, y, z)
            end
        end
    else
        -- active plane is not displayed
        if grid1[gridpos] then
            DrawLiveCell(x, y, z)
        end
        if selected[gridpos] then
            DrawSelectedCell(x, y, z)
        end
    end
    if pastecell[gridpos] then
        DrawLiveCell(x, y, z)
        DrawPasteCell(x, y, z)
    end
end

----------------------------------------------------------------------

function DisplayCells(editing)
    -- find the rotated reference cube vertex with maximum Z coordinate
    local z1 = rotrefz[1]
    local z2 = rotrefz[2]
    local z3 = rotrefz[3]
    local z4 = rotrefz[4]
    local z5 = rotrefz[5]
    local z6 = rotrefz[6]
    local z7 = rotrefz[7]
    local z8 = rotrefz[8]
    local maxZ = max(z1,z2,z3,z4,z5,z6,z7,z8)

    ov("blend 1")
    local testcell = editing or selcount > 0 or pastecount > 0
    
    -- find the extended boundary of all live/active/selected/paste cells
    local MINX, MINY, MINZ, MAXX, MAXY, MAXZ = minx, miny, minz, maxx, maxy, maxz
    if testcell then
        if editing then
            if minactivex < MINX then MINX = minactivex end
            if minactivey < MINY then MINY = minactivey end
            if minactivez < MINZ then MINZ = minactivez end
            if maxactivex > MAXX then MAXX = maxactivex end
            if maxactivey > MAXY then MAXY = maxactivey end
            if maxactivez > MAXZ then MAXZ = maxactivez end
        end
        if selcount > 0 then
            if minselx < MINX then MINX = minselx end
            if minsely < MINY then MINY = minsely end
            if minselz < MINZ then MINZ = minselz end
            if maxselx > MAXX then MAXX = maxselx end
            if maxsely > MAXY then MAXY = maxsely end
            if maxselz > MAXZ then MAXZ = maxselz end
        end
        if pastecount > 0 then
            if minpastex < MINX then MINX = minpastex end
            if minpastey < MINY then MINY = minpastey end
            if minpastez < MINZ then MINZ = minpastez end
            if maxpastex > MAXX then MAXX = maxpastex end
            if maxpastey > MAXY then MAXY = maxpastey end
            if maxpastez > MAXZ then MAXZ = maxpastez end
        end
    end

    -- test batch draw  !BATCHDRAW!
    -- local t1 = g.millisecs()
    if usebatch and not testcell then
        ResetBatch()
        if celltype == "cube" then
            DrawLiveCell = AddCubeToBatch
        elseif celltype == "sphere" then
            DrawLiveCell = AddSphereToBatch
        else -- celltype == "point"
            DrawLiveCell = AddPointToBatch
        end
    end
    
    -- draw cells from back to front (assumes vertex order set in CreateCube)
    if maxZ == z1 then
        -- draw cell at MINX,MINY,MAXZ first
        for z = MAXZ, MINZ, -1 do
            for y = MINY, MAXY do
                for x = MINX, MAXX do
                    if testcell then
                        TestCell(editing, x+N*(y+N*z), x, y, z)
                    elseif grid1[x+N*(y+N*z)] then
                        DrawLiveCell(x, y, z)
                    end
                end
            end
        end
    elseif maxZ == z2 then
        -- draw cell at MINX,MINY,MINZ first
        for z = MINZ, MAXZ do
            for y = MINY, MAXY do
                for x = MINX, MAXX do
                    if testcell then
                        TestCell(editing, x+N*(y+N*z), x, y, z)
                    elseif grid1[x+N*(y+N*z)] then
                        DrawLiveCell(x, y, z)
                    end
                end
            end
        end
    elseif maxZ == z3 then
        -- draw cell at MINX,MAXY,MAXZ first
        for z = MAXZ, MINZ, -1 do
            for y = MAXY, MINY, -1 do
                for x = MINX, MAXX do
                    if testcell then
                        TestCell(editing, x+N*(y+N*z), x, y, z)
                    elseif grid1[x+N*(y+N*z)] then
                        DrawLiveCell(x, y, z)
                    end
                end
            end
        end
    elseif maxZ == z4 then
        -- draw cell at MINX,MAXY,MINZ first
        for z = MINZ, MAXZ do
            for y = MAXY, MINY, -1 do
                for x = MINX, MAXX do
                    if testcell then
                        TestCell(editing, x+N*(y+N*z), x, y, z)
                    elseif grid1[x+N*(y+N*z)] then
                        DrawLiveCell(x, y, z)
                    end
                end
            end
        end
    elseif maxZ == z5 then
        -- draw cell at MAXX,MAXY,MAXZ first
        for z = MAXZ, MINZ, -1 do
            for y = MAXY, MINY, -1 do
                for x = MAXX, MINX, -1 do
                    if testcell then
                        TestCell(editing, x+N*(y+N*z), x, y, z)
                    elseif grid1[x+N*(y+N*z)] then
                        DrawLiveCell(x, y, z)
                    end
                end
            end
        end
    elseif maxZ == z6 then
        -- draw cell at MAXX,MAXY,MINZ first
        for z = MINZ, MAXZ do
            for y = MAXY, MINY, -1 do
                for x = MAXX, MINX, -1 do
                    if testcell then
                        TestCell(editing, x+N*(y+N*z), x, y, z)
                    elseif grid1[x+N*(y+N*z)] then
                        DrawLiveCell(x, y, z)
                    end
                end
            end
        end
    elseif maxZ == z7 then
        -- draw cell at MAXX,MINY,MAXZ first
        for z = MAXZ, MINZ, -1 do
            for y = MINY, MAXY do
                for x = MAXX, MINX, -1 do
                    if testcell then
                        TestCell(editing, x+N*(y+N*z), x, y, z)
                    elseif grid1[x+N*(y+N*z)] then
                        DrawLiveCell(x, y, z)
                    end
                end
            end
        end
    elseif maxZ == z8 then
        -- draw cell at MAXX,MINY,MINZ first
        for z = MINZ, MAXZ do
            for y = MINY, MAXY do
                for x = MAXX, MINX, -1 do
                    if testcell then
                        TestCell(editing, x+N*(y+N*z), x, y, z)
                    elseif grid1[x+N*(y+N*z)] then
                        DrawLiveCell(x, y, z)
                    end
                end
            end
        end
    end

    -- test batch draw !BATCHDRAW!
    if usebatch and not testcell then
        DrawBatch()
    end
    -- t1 = g.millisecs() - t1
    -- message = string.format("%.2fms", t1)
    
    ov("blend 0")
end

--------------------------------------------------------------------------------

function DrawToolBar()
    ov("rgba 230 230 230 255")
    ov("fill 0 0 "..ovwd.." "..toolbarht)
    
    -- draw line at bottom edge of tool bar
    ov(op.gray)
    DrawLine(0, toolbarht-1, ovwd-1, toolbarht-1)

    local x = gap
    local y = gap

    newbutton.show(x, y)
    x = x + newbutton.wd + gap
    openbutton.show(x, y)
    x = x + openbutton.wd + gap
    savebutton.show(x, y)
    x = x + savebutton.wd + gap
    runbutton.show(x, y)
    x = x + runbutton.wd + gap
    gridbutton.show(x, y)
    x = x + gridbutton.wd + gap
    randbutton.show(x, y)
    x = x + randbutton.wd + gap
    rulebutton.show(x, y)
    -- next 2 buttons are at right end of tool bar
    x = ovwd - gap - exitbutton.wd
    exitbutton.show(x, y)
    x = x - gap - helpbutton.wd
    helpbutton.show(x, y)

    -- move to 2nd row of buttons
    x = gap
    y = buttonht + gap * 2
    
    ssbutton.show(x, y)
    x = x + ssbutton.wd + gap
    s1button.show(x, y)
    x = x + s1button.wd + gap
    resetbutton.show(x, y)
    x = x + resetbutton.wd + gap
    fitbutton.show(x, y)
    x = x + fitbutton.wd + gap * 3
    undobutton.show(x, y)
    x = x + undobutton.wd + gap
    redobutton.show(x, y)
    x = x + redobutton.wd + gap * 3
    drawbox.show(x, y, currcursor == drawcursor)
    x = x + drawbox.wd + gap
    selectbox.show(x, y, currcursor == selectcursor)
    x = x + selectbox.wd + gap
    movebox.show(x, y, currcursor == movecursor)
end

----------------------------------------------------------------------

function Refresh()
    if scriptlevel > 0 then
        -- scripts call Update() when they want to refresh
        return
    end

    -- fill overlay with background color
    ov("rgba "..BACK_COLOR)
    ov("fill")
    
    -- get Z coordinates of the vertices of a rotated reference cube
    -- (for doing various depth tests)
    for i = 1, 8 do
        local x, y, z = TransformPoint(refcube[i])
        rotrefz[i] = z
    end
    
    if showaxes then DrawRearAxes() end
    
    local editing = currcursor ~= movecursor
    if popcount > 0 or pastecount > 0 or selcount > 0 or editing then
        if pastecount > 0 then
            -- paste cells will be translucent red
            CreateTranslucentCell("p", "255 0 0 64")
        end
        if selcount > 0 then
            -- selected cells will be translucent green
            CreateTranslucentCell("s", "0 255 0 64")
        end
        if editing then
            -- cells in active plane will be translucent blue
            CreateTranslucentCell("a", "0 0 255 48")
        end
        if celltype == "cube" then
            CreateLiveCube()
            DrawLiveCell = DrawCube
        elseif celltype == "sphere" then
            CreateLiveSphere()
            DrawLiveCell = DrawSphere
        else -- celltype == "point"
            ov(op.white)
            DrawLiveCell = DrawPoint
        end
        DisplayCells(editing)
    end
    
    if showaxes then DrawFrontAxes() end
    
    -- show info in top left corner
    local info =
        "Pattern = "..pattname.."\n"..
        "Rule = "..rulestring.."\n"..
        "Generation = "..gencount.."\n"..
        "Population = "..popcount
    if selcount > 0 then
        info = info.."\nSelected cells = "..selcount
    end
    if editing then
        -- show cell coords of mouse if it's inside the active plane
        info = info.."\nx,y,z = "..activecell
    end
    ov(op.white)
    local wd, ht = op.maketext(info)
    op.pastetext(10, toolbarht + 10)
    if message then
        ov(op.yellow)
        op.maketext(message)
        op.pastetext(10, toolbarht + 10 + ht + 10)
    end
    
    if toolbarht > 0 then DrawToolBar() end
    
    -- the overlay is 100% opaque and covers entire viewport
    -- so we can call ov("update") rather than slower g.update()
    ov("update")
end

----------------------------------------------------------------------

function InitLiveBoundary()
    minx = math.maxinteger
    miny = math.maxinteger
    minz = math.maxinteger
    maxx = math.mininteger
    maxy = math.mininteger
    maxz = math.mininteger
    -- SetLiveCell/SetCellState/NextGeneration will update all values
    
    -- set the flag for MinimizeLiveBoundary
    minimal_live_bounds = true
end

----------------------------------------------------------------------

function MinimizeLiveBoundary()
    if popcount > 0 and not minimal_live_bounds then
        InitLiveBoundary()
        -- minimal_live_bounds is now true
        local NN = N*N
        for k,_ in pairs(grid1) do
            -- grid1[k] is a live cell
            local x = k % N
            local y = k % NN
            if x < minx then minx = x end
            if y < miny then miny = y end
            if k < minz then minz = k end
            if x > maxx then maxx = x end
            if y > maxy then maxy = y end
            if k > maxz then maxz = k end
        end
        miny = miny // N
        maxy = maxy // N
        minz = minz // NN
        maxz = maxz // NN
    end
end

----------------------------------------------------------------------

function InitSelectionBoundary()
    minselx = math.maxinteger
    minsely = math.maxinteger
    minselz = math.maxinteger
    maxselx = math.mininteger
    maxsely = math.mininteger
    maxselz = math.mininteger
    
    -- set the flag for MinimizeSelectionBoundary
    minimal_sel_bounds = true
end

----------------------------------------------------------------------

local function UpdateSelectionBoundary(x, y, z)
    if x < minselx then minselx = x end
    if y < minsely then minsely = y end
    if z < minselz then minselz = z end
    if x > maxselx then maxselx = x end
    if y > maxsely then maxsely = y end
    if z > maxselz then maxselz = z end
end

----------------------------------------------------------------------

function MinimizeSelectionBoundary()
    if selcount > 0 and not minimal_sel_bounds then
        -- find minimal bounding box of all selected cells
        InitSelectionBoundary()
        -- minimal_sel_bounds is now true
        local NN = N*N
        for k,_ in pairs(selected) do
            UpdateSelectionBoundary(k % N, (k // N) % N, k // NN)
        end
    end
end

----------------------------------------------------------------------

function ClearCells()
    grid1 = {}
    grid2 = {}
    popcount = 0
    InitLiveBoundary()
    -- remove selection
    selcount = 0
    selected = {}
    InitSelectionBoundary()
    -- remove paste pattern
    pastecount = 0
    pastecell = {}
    collectgarbage()    -- helps avoid long delay when script exits???!!! only on Mac OS 10.13???
end

----------------------------------------------------------------------

function SetActivePlane(orientation, pos)
    -- set the active plane; default is an XY plane half way along the Z axis
    activeplane = orientation or "XY"
    activepos = pos or 0
    active = {}
    local M = N-1
    if activeplane == "XY" then
        local z = N//2 + activepos
        local zNN = z * N * N
        for y = 0, M do
            local yN = y * N
            for x = 0, M do
                active[ x + yN + zNN ] = true
            end
        end
        minactivex = 0
        maxactivex = M
        minactivey = 0
        maxactivey = M
        minactivez = z
        maxactivez = z
    elseif activeplane == "YZ" then
        local x = N//2 + activepos
        for z = 0, M do
            for y = 0, M do
                active[ x + N * (y + N * z) ] = true
            end
        end
        minactivex = x
        maxactivex = x
        minactivey = 0
        maxactivey = M
        minactivez = 0
        maxactivez = M
    else -- activeplane == "XZ"
        local y = N//2 + activepos
        for z = 0, M do
            for x = 0, M do
                active[ x + N * (y + N * z) ] = true
            end
        end
        minactivex = 0
        maxactivex = M
        minactivey = y
        maxactivey = y
        minactivez = 0
        maxactivez = M
    end
end

----------------------------------------------------------------------

function MoveActivePlane(newpos, refresh)
    if currcursor ~= movecursor then
        local mid = N//2
        if newpos + mid < 0 then
            newpos = -mid
        elseif newpos + mid >= N then
            newpos = N-1-mid
        end
        -- use the same orientation
        SetActivePlane(activeplane, newpos)
        if refresh == nil or refresh then Refresh() end
    end
end

----------------------------------------------------------------------

function CycleActivePlane()
    if currcursor ~= movecursor then
        -- cycle to next orientation of active plane
        if activeplane == "XY" then
            SetActivePlane("YZ", activepos)
        elseif activeplane == "YZ" then
            SetActivePlane("XZ", activepos)
        else -- activeplane == "XZ"
            SetActivePlane("XY", activepos)
        end
        Refresh()
    end
end

----------------------------------------------------------------------

local function SetSelection(x, y, z, sel)
    local pos = x + N * (y + N * z)
    if sel then
        if not selected[pos] then
            selected[pos] = true
            selcount = selcount + 1
            UpdateSelectionBoundary(x, y, z)
        end
    elseif selected[pos] then
        selected[pos] = nil
        selcount = selcount - 1
        -- tell MinimizeSelectionBoundary that it needs to update the selection boundary
        minimal_sel_bounds = false
    end
end

----------------------------------------------------------------------

local function SetCellState(x, y, z, state)
    local pos = x + N * (y + N * z)
    if state > 0 then
        if not grid1[pos] then
            grid1[pos] = state
            popcount = popcount + 1
            -- boundary might expand
            if x < minx then minx = x end
            if y < miny then miny = y end
            if z < minz then minz = z end
            if x > maxx then maxx = x end
            if y > maxy then maxy = y end
            if z > maxz then maxz = z end
        else
            grid1[pos] = state
        end
    else
        -- state is 0
        if grid1[pos] then
            -- kill a live cell
            grid1[pos] = nil
            popcount = popcount - 1
            -- tell MinimizeLiveBoundary that it needs to update the live boundary
            minimal_live_bounds = false
        end
    end
end

----------------------------------------------------------------------

local function SetLiveCell(x, y, z)
    -- this must only be called to create a live cell
    grid1[ x + N * (y + N * z) ] = 1
    popcount = popcount + 1
    -- boundary might expand
    if x < minx then minx = x end
    if y < miny then miny = y end
    if z < minz then minz = z end
    if x > maxx then maxx = x end
    if y > maxy then maxy = y end
    if z > maxz then maxz = z end
end

----------------------------------------------------------------------

function GetCells()
    -- return an array of live cell positions relative to mid cell
    local livecells = {}
    if popcount > 0 then
        local mid = N//2
        local NN = N*N
        for k,_ in pairs(grid1) do
            -- grid1[k] is a live cell
            local x = k % N
            local y = (k // N) % N
            local z = k // NN
            livecells[#livecells+1] = {x-mid, y-mid, z-mid}
        end
    end
    return livecells
end

----------------------------------------------------------------------

function PutCells(livecells)
    -- restore pattern saved earlier by GetCells
    -- (note that grid must currently be empty)
    if popcount > 0 then
        g.note("Bug in PutCells: grid is not empty!")
    end
    local clipped = 0
    local mid = N//2
    for _, xyz in ipairs(livecells) do
        local x, y, z = xyz[1]+mid, xyz[2]+mid, xyz[3]+mid
        if x >= 0 and x < N and
           y >= 0 and y < N and
           z >= 0 and z < N then
            SetLiveCell(x, y, z)
        else
            clipped = clipped + 1
        end
    end
    return clipped
end

--------------------------------------------------------------------------------

function UpdateStartButton()
    -- fix m.button code in oplus/init.lua to avoid need to do this!!!
    local oldbg = ov("textoption background 0 0 0 0")
    
    -- change label in ssbutton without changing the button's width
    if generating then
        ssbutton.setlabel("Stop", false)
    else
        ssbutton.setlabel("Start", false)
    end
    
    ov("textoption background "..oldbg) -- see above!!!
end

----------------------------------------------------------------------

function SaveStart()
    -- save starting info for later Reset
    startN = N
    startplane = activeplane
    startpos = activepos
    startrule = rulestring
    startcount = gencount
    startname = pattname
    startcells = GetCells()
    startselcount = selcount
    startselected = {}
    if selcount > 0 then
        for k,_ in pairs(selected) do
            startselected[k] = true
        end
    end
end

----------------------------------------------------------------------

function NextGeneration()
    -- calculate and display the next generation using an algorithm
    -- described by Carter Bays (see Method B Option 2 on page 398 in
    -- http://www.complex-systems.com/pdf/01-3-1.pdf)
    
    if popcount == 0 then
        generating = false
        UpdateStartButton()
        message = "All cells are dead."
        Refresh()
        return
    end
    
    if gencount == startcount then SaveStart() end
    
    popcount = 0        -- incremented below
    InitLiveBoundary()  -- updated below

    local count1 = {}
    local NN = N * N
    local NNN = NN * N
    for k,_ in pairs(grid1) do
        count1[k] = (count1[k] or 0) + 1
        local y = k % NN
        local k2 = k + (y + N) % NN - y
        count1[k2] = (count1[k2] or 0) + 1
        k2 = k + (y + NN - N) % NN - y
        count1[k2] = (count1[k2] or 0) + 1
    end
    local count2 = {}
    for k,v in pairs(count1) do
        count2[k] = (count2[k] or 0) + v
        local x = k % N
        local k2 = k + (x + 1) % N - x
        count2[k2] = (count2[k2] or 0) + v
        k2 = k + (x + N - 1) % N - x
        count2[k2] = (count2[k2] or 0) + v
    end
    count1 = {}
    for k,v in pairs(count2) do
        count1[k] = (count1[k] or 0) + v
        local k2 = (k + NN) % NNN
        count1[k2] = (count1[k2] or 0) + v
        k2 = (k + NNN - NN) % NNN
        count1[k2] = (count1[k2] or 0) + v
    end
    for k,v in pairs(count1) do
        if (grid1[k] and survivals[v-1]) or (births[v] and not grid1[k]) then
            -- create a live cell in grid2
            grid2[k] = 1
            popcount = popcount + 1
            -- boundary might expand
            local x = k % N
            local y = k // N % N
            local z = k // NN
            if x < minx then minx = x end
            if y < miny then miny = y end
            if z < minz then minz = z end
            if x > maxx then maxx = x end
            if y > maxy then maxy = y end
            if z > maxz then maxz = z end
        end
    end
    
    -- clear all live cells in grid1 and swap grids
    grid1 = {}
    grid1, grid2 = grid2, grid1
    
    if popcount == 0 then
        generating = false
        UpdateStartButton()
    end
    gencount = gencount + 1
    Refresh()
end

----------------------------------------------------------------------

function NewPattern()
    pattname = "untitled"
    SetCursor(drawcursor)
    gencount = 0
    startcount = 0
    generating = false
    UpdateStartButton()
    ClearCells()
    SetActivePlane()
    InitialView()       -- calls Refresh()
end

----------------------------------------------------------------------

function ReadPattern(filepath)
    local f = io.open(filepath,"r")
    if not f then return "Failed to open file:\n"..filepath end
    
    local line = f:read("*l")
    if line == nil or line ~= "3D" then
        f:close()
        return "Invalid 3D pattern (first line must be 3D)."
    end

    -- read pattern into a temporary grid in case an error occurs
    local tsize = MAXN
    local trule = DEFAULT_RULE
    local tgens = 0
    local tpop = 0
    local tminx = math.maxinteger
    local tminy = math.maxinteger
    local tminz = math.maxinteger
    local tmaxx = math.mininteger
    local tmaxy = math.mininteger
    local tmaxz = math.mininteger
    local tgrid = {}
    
    -- safer and faster to read header lines first, then data lines!!!
    
    local prevy = 0
    local prevz = 0
    while true do
        line = f:read("*l")
        if not line then break end
        local ch1 = line:sub(1,1)
        if ch1 == "#" or #ch1 == 0 then
            -- ignore comment line or blank line
        elseif ch1 >= "a" and ch1 <= "z" then
            -- parse keyword=value (ignore unknown keyword)
            local keyword, value = split(line,"=")
            if not value then
                f:close()
                return "Line should be keyword=value:\n"..line
            end
            if keyword == "version" then
                if value ~= "1" then
                    f:close()
                    return "Unexpected version:\n"..value
                end
            elseif keyword == "gridsize" then
                tsize = tonumber(value) or MAXN
                if tsize < MINN then tsize = MINN end
                if tsize > MAXN then tsize = MAXN end
            elseif keyword == "rule" then
                local saverule = rulestring
                if not ParseRule(value) then
                    f:close()
                    return "Unknown rule:\n"..value
                end
                trule = rulestring
                -- restore rulestring etc in case there is a later error
                -- or we're doing a paste and want to ignore the specified rule
                ParseRule(saverule)
            elseif keyword == "generation" then
                tgens = tonumber(value) or 0
                if tgens < 0 then tgens = 0 end
            end
        else
            -- line can contain one or more "x y z" coords separated by commas
            -- (but y and z might be missing in deprecated format)
            for cell in line:gmatch("[^,]+") do
                local x, y, z = split(cell)
                local mid = tsize // 2
                x = tonumber(x) + mid
                if y then prevy = tonumber(y) end
                if z then prevz = tonumber(z) end
                y = prevy + mid
                z = prevz + mid
                -- ensure x,y,z values are within 0..tsize-1???!!!
                -- set live cell
                tgrid[ x + tsize * (y + tsize * z) ] = 1
                tpop = tpop + 1
                -- boundary might expand
                if x < tminx then tminx = x end
                if y < tminy then tminy = y end
                if z < tminz then tminz = z end
                if x > tmaxx then tmaxx = x end
                if y > tmaxy then tmaxy = y end
                if z > tmaxz then tmaxz = z end
            end
        end
    end
    f:close()
    
    -- success
    local newpattern = {
        newsize = tsize,
        newrule = trule,
        newgens = tgens,
        newpop = tpop,
        newminx = tminx,
        newminy = tminy,
        newminz = tminz,
        newmaxx = tmaxx,
        newmaxy = tmaxy,
        newmaxz = tmaxz,
        newgrid = tgrid
    }
    return nil, newpattern
end

----------------------------------------------------------------------

function UpdateCurrentGrid(newpattern)
    N = newpattern.newsize
    MIDGRID = (N+1-(N%2))*HALFCELL
    CreateAxes()
    ClearCells()        -- resets grid1, grid2, popcount and selection info
    
    grid1 = newpattern.newgrid
    popcount = newpattern.newpop
    minx = newpattern.newminx
    miny = newpattern.newminy
    minz = newpattern.newminz
    maxx = newpattern.newmaxx
    maxy = newpattern.newmaxy
    maxz = newpattern.newmaxz
    
    ParseRule(newpattern.newrule)   -- sets rulestring, survivals and births
    gencount = newpattern.newgens
    startcount = gencount           -- for Reset
    generating = false
    UpdateStartButton()
    SetCursor(movecursor)
    SetActivePlane()
    InitialView()       -- calls Refresh
end

----------------------------------------------------------------------

function OpenPattern(filepath)
    if filepath then
        local err, newpattern = ReadPattern(filepath)
        if err then
            g.warn(err)
        else
            -- pattern ok so use info in newpattern to update current grid;
            -- set pattname to file name at end of filepath
            pattname = filepath:match("^.+"..pathsep.."(.+)$")
            UpdateCurrentGrid(newpattern)
        end
    else
        -- prompt user for a .3d file to open
        local filetype = "3D file (*.3d)|*.3d"
        local path = g.opendialog("Open a 3D pattern", filetype, pattdir, "")
        if #path > 0 then
            -- update pattdir by stripping off the file name
            pattdir = path:gsub("[^"..pathsep.."]+$","")
            -- open the chosen pattern
            OpenPattern(path)
        end
    end
end

----------------------------------------------------------------------

function CopyClipboardToFile()
    -- create a temporary file containing clipboard text
    local filepath = g.getdir("temp").."clipboard.3d"
    local f = io.open(filepath,"w")
    if not f then
        g.warn("Failed to create temporary clipboard file!")
        return nil
    end
    -- NOTE: we can't do f:write(string.gsub(g.getclipstr(),"\r","\n"))
    -- because gsub returns 2 results and we'd get count appended to file!
    local clip = string.gsub(g.getclipstr(),"\r","\n")
    f:write(clip)
    f:close()
    return filepath
end

----------------------------------------------------------------------

function OpenClipboard()
    local filepath = CopyClipboardToFile()
    if filepath then
        local err, newpattern = ReadPattern(filepath)
        if err then
            g.warn(err)
        else
            -- pattern ok so use info in newpattern to update current grid
            pattname = "clipboard"
            UpdateCurrentGrid(newpattern)
        end
    end
end

----------------------------------------------------------------------

function PatternHeader(comments)
    -- return 3D pattern header lines
    comments = comments or ""
    return
        "3D\n"..
        comments..
        "version=1\n"..
        "gridsize="..N.."\n"..
        "rule="..rulestring.."\n"..
        "generation="..gencount         -- let caller append \n
end

----------------------------------------------------------------------

function WritePattern(filepath, comments)
    local f = io.open(filepath,"w")
    if not f then return "Failed to create file:\n"..filepath end
    f:write(PatternHeader(comments).."\n")
    if popcount > 0 then
        local mid = N//2
        local NN = N*N
        local line = ""
        local cellsonline = 0
        for k,_ in pairs(grid1) do
            -- grid1[k] is a live cell
            local x = k % N
            local y = (k // N) % N
            local z = k // NN
            local cell = (x-mid).." "..(y-mid).." "..(z-mid)
            if #line < 60 then
                if cellsonline > 0 then line = line.."," end
                line = line..cell
                cellsonline = cellsonline + 1
            else
                f:write(line..","..cell.."\n")
                line = ""
                cellsonline = 0
            end
        end
        if #line > 0 then f:write(line.."\n") end
    end
    f:close()
    return nil  -- success
end

----------------------------------------------------------------------

function GetComments(f)
    local comments = ""
    local line = f:read("*l")
    if line == nil or line ~= "3D" then return end
    while true do
        line = f:read("*l")
        if not line then break end
        local ch1 = line:sub(1,1)
        if #ch1 == 0 then
            -- ignore empty line
        elseif ch1 == "#" then
            comments = comments..line.."\n"
        elseif ch1 < "a" or ch1 > "z" then
            -- end of header info
            break
        end
    end
    return comments
end

----------------------------------------------------------------------

function SavePattern(filepath)
    if filepath then
        -- if filepath exists then extract any comment lines from the header
        -- info and copy them into the new file
        local comments = ""
        local f = io.open(filepath,"r")
        if f then
            comments = GetComments(f)
            f:close()
        end
        local err = WritePattern(filepath, comments)
        if err then
            g.warn(err)
        else
            -- set pattname to file name at end of filepath
            pattname = filepath:match("^.+"..pathsep.."(.+)$")
            Refresh()
        end
    else
        -- prompt user for file name and location
        local filetype = "3D file (*.3d)|*.3d"
        local path = g.savedialog("Save pattern", filetype, pattdir, pattname)
        if #path > 0 then
            -- update pattdir by stripping off the file name
            pattdir = path:gsub("[^"..pathsep.."]+$","")
            -- ensure file name ends with ".3d"
            if not path:find("%.3d$") then
                path = path..".3d"
            end
            -- save the current pattern
            SavePattern(path)
        end
    end
end

----------------------------------------------------------------------

function CallScript(func, fromclip)
    scriptlevel = scriptlevel+1
    -- error if scriptlevel reaches 100???!!!
    local status, err = pcall(func)
    scriptlevel = scriptlevel-1
    if err then
        g.continue("")
        if err == "GOLLY: ABORT SCRIPT" then
            -- user hit escape
            message = "Script aborted."
        else
            if fromclip then
                g.warn("Runtime error in clipboard script:\n\n"..err)
            else
                g.warn("Runtime error in script:\n\n"..err)
            end
        end
    end
    Refresh()
end

----------------------------------------------------------------------

function RunScript(filepath)
    if filepath then
        local f, msg = loadfile(filepath)
        if f then
            CallScript(f, false)
        else
            g.warn("Syntax error in script:\n\n"..msg)
        end
    else
        -- prompt user for a .lua file to run
        local filetype = "Lua file (*.lua)|*.lua"
        local path = g.opendialog("Choose a Lua script", filetype, scriptdir, "")
        if #path > 0 then
            -- update scriptdir by stripping off the file name
            scriptdir = path:gsub("[^"..pathsep.."]+$","")
            -- run the chosen script
            RunScript(path)
        end
    end
end

----------------------------------------------------------------------

function RunClipboard()
    local f, msg = load(g.getclipstr())
    if f then
        CallScript(f, true)
    else
        g.warn("Syntax error in clipboard script:\n\n"..msg)
    end
end

----------------------------------------------------------------------

function SetStartupScript()
    -- prompt user for a .lua file to run automatically when 3D.lua starts up
    local filetype = "Lua file (*.lua)|*.lua"
    local path = g.opendialog("Select your start-up script", filetype, scriptdir, "")
    if #path > 0 then
        -- update scriptdir by stripping off the file name
        scriptdir = path:gsub("[^"..pathsep.."]+$","")
        startup = path
        -- the above path will be saved by WriteSettings
    end
end

----------------------------------------------------------------------

function RandomPattern()
    -- make these menu options eventually!!!
    local randomfill = false
    local randomsphere = false

    local function getperc()
        ::try_again::
        local s = g.getstring("Enter density as a percentage (from 0 to 100):",
                              tostring(perc), "Random pattern")
        if validint(s) and (tonumber(s) >= 0) and (tonumber(s) <= 100) then
            perc = tonumber(s)
        else
            g.warn("Percentage must be an integer from 0 to 100.")
            goto try_again
        end
    end
    
    -- if user hits Cancel button we want to avoid aborting script
    local status, err = pcall(getperc)
    if err then
        g.continue("")  -- don't show error when script finishes
        return
    end
    
    pattname = "untitled"
    SetCursor(movecursor)
    gencount = 0
    startcount = 0
    generating = false
    UpdateStartButton()
    ClearCells()
    
    local minval, maxval
    if randomfill or N < 8 then
        minval = 0
        maxval = N-1
    else
        minval = N//8
        maxval = (N-1) - minval
    end
    if randomsphere then
        local mid = N//2
        local rsq = (maxval-minval)//2
        rsq = rsq*rsq
        for z = minval, maxval do
            local dz = z-mid
            local dz2 = dz*dz
            for y = minval, maxval do
                local dy = y-mid
                local dy2 = dy*dy
                for x = minval, maxval do
                    local dx = x-mid
                    local d = dx*dx + dy2 + dz2
                    if d <= rsq and rand(0,99) < perc then
                        SetLiveCell(x, y, z)
                    end
                end
            end
        end
    else
        for z = minval, maxval do
            for y = minval, maxval do
                for x = minval, maxval do
                    if rand(0,99) < perc then
                        SetLiveCell(x, y, z)
                    end
                end
            end
        end
    end
    
    SetActivePlane()
    InitialView()       -- calls Refresh()
end

----------------------------------------------------------------------

function GetSelectedCells()
    -- return an array of selected cell positions relative to mid cell
    local selcells = {}
    if selcount > 0 then
        local mid = N//2
        local NN = N*N
        for k,_ in pairs(selected) do
            -- selected[k] is a selected cell
            local x = k % N
            local y = (k // N) % N
            local z = k // NN
            selcells[#selcells+1] = {x-mid, y-mid, z-mid}
        end
    end
    return selcells
end

----------------------------------------------------------------------

function GetSelectedLiveCells()
    -- return an array of selected *live* cell positions relative to mid cell
    local livecells = {}
    if selcount > 0 then
        local mid = N//2
        local NN = N*N
        for k,_ in pairs(grid1) do
            -- grid1[k] is a live cell
            if selected[k] then
                local x = k % N
                local y = (k // N) % N
                local z = k // NN
                livecells[#livecells+1] = {x-mid, y-mid, z-mid}
            end
        end
    end
    return livecells
end

----------------------------------------------------------------------

function GetPasteCells()
    -- return an array of paste cell positions relative to mid cell
    local pcells = {}
    if pastecount > 0 then
        local mid = N//2
        local NN = N*N
        for k,_ in pairs(pastecell) do
            -- pastecell[k] is in paste pattern
            local x = k % N
            local y = (k // N) % N
            local z = k // NN
            pcells[#pcells+1] = {x-mid, y-mid, z-mid}
        end
    end
    return pcells
end

----------------------------------------------------------------------

function ChangeGridSize()
    local newN = N
    
    local function getsize()
        ::try_again::
        local s = g.getstring("Enter the new size (from "..MINN.." to "..MAXN.."):",
                              tostring(N), "Grid size")
        if validint(s) and (tonumber(s) >= MINN) and (tonumber(s) <= MAXN) then
            newN = tonumber(s)
        else
            g.warn("Grid size must be an integer from "..MINN.." to "..MAXN..".")
            goto try_again
        end
    end
    
    -- if user hits Cancel button we want to avoid aborting script
    local status, err = pcall(getsize)
    if err then
        g.continue("")  -- don't show error when script finishes
        return
    end
    
    if newN == N then return end
    
    -- save current pattern as an array of positions relative to mid cell
    local livecells = GetCells()
    
    -- save any selected cells as an array of positions relative to mid cell
    local oldselcount = selcount
    local selcells = GetSelectedCells()
    
    -- save any paste cells as an array of positions relative to mid cell
    local oldpastecount = pastecount
    local pcells = GetPasteCells()
    
    N = newN
    MIDGRID = (N+1-(N%2))*HALFCELL
    CreateAxes()
    
    -- active plane may need adjusting
    local mid = N//2
    if (mid + activepos >= N) or (mid + activepos < 0) then
        activepos = 0
    end
    SetActivePlane(activeplane, activepos)
    
    -- restore pattern, clipping any cells outside the new grid
    ClearCells()    
    local clipcount = PutCells(livecells)
    
    -- restore selection, clipping any cells outside the new grid
    local selclipped = 0
    selcount = 0
    selected = {}
    InitSelectionBoundary()
    if oldselcount > 0 then
        for _, xyz in ipairs(selcells) do
            local x, y, z = xyz[1]+mid, xyz[2]+mid, xyz[3]+mid
            if x >= 0 and x < N and
               y >= 0 and y < N and
               z >= 0 and z < N then
                selected[ x + N * (y + N * z) ] = true
                selcount = selcount + 1
                UpdateSelectionBoundary(x, y, z)
            else
                selclipped = selclipped + 1
            end
        end
    end
    
    -- restore paste pattern, clipping any cells outside the new grid
    local pclipped = 0
    pastecount = 0
    pastecell = {}
    minpastex = math.maxinteger
    minpastey = math.maxinteger
    minpastez = math.maxinteger
    maxpastex = math.mininteger
    maxpastey = math.mininteger
    maxpastez = math.mininteger
    if oldpastecount > 0 then
        for _, xyz in ipairs(pcells) do
            local x, y, z = xyz[1]+mid, xyz[2]+mid, xyz[3]+mid
            if x >= 0 and x < N and
               y >= 0 and y < N and
               z >= 0 and z < N then
                pastecell[ x + N * (y + N * z) ] = true
                pastecount = pastecount + 1
                if x < minpastex then minpastex = x end
                if y < minpastey then minpastey = y end
                if z < minpastez then minpastez = z end
                if x > maxpastex then maxpastex = x end
                if y > maxpastey then maxpastey = y end
                if z > maxpastez then maxpastez = z end
            else
                pclipped = pclipped + 1
            end
        end
    end
    
    if clipcount > 0 or selclipped > 0 or pclipped > 0 then
        message = ""
        if clipcount > 0 then message = "Clipped live cells = "..clipcount.."\n" end
        if selclipped > 0 then message = message.."Clipped selection cells = "..selclipped.."\n" end
        if pclipped > 0 then message = message.."Clipped paste cells = "..pclipped end
    end
    
    FitGrid()   -- calls Refresh()
end

----------------------------------------------------------------------

function ChangeRule()
    -- let user enter new rule as a string of the form s,s,.../b,b,...
    -- (the notation used at http://www.cse.sc.edu/~bays/d4d4d4/)
    
    local function getrule()
        local newrule = rulestring
        ::try_again::
        newrule = g.getstring("Enter the new rule in the form s,s,s,.../b,b,b,...\n" ..
                              "where s values are the neighbor counts for survival\n" ..
                              "and b values are the neighbor counts for birth:",
                              newrule, "Change rule")
        if not ParseRule(newrule) then goto try_again end
    end
    
    -- if user hits Cancel button we want to avoid aborting script
    local status, err = pcall(getrule)
    if err then
        g.continue("")  -- don't show error when script finishes
        return
    end
    
    Refresh()
end

----------------------------------------------------------------------

function SelectAll()
    if popcount > 0 then
        selcount = 0
        selected = {}
        for k,_ in pairs(grid1) do
            selected[k] = true
            selcount = selcount + 1
        end
        -- selection boundary matches live cell boundary
        minselx = minx
        minsely = miny
        minselz = minz
        maxselx = maxx
        maxsely = maxy
        maxselz = maxz
        minimal_sel_bounds = minimal_live_bounds
        Refresh()
    end
end

----------------------------------------------------------------------

function CopySelection()
    if selcount > 0 then
        -- save the selected live cells as a 3D pattern in clipboard,
        -- but only if there is at least one live cell selected
        local livecells = GetSelectedLiveCells()
        if #livecells == 0 then
            message = "There are no live cells selected."
            Refresh()
            return false
        end
        local lines = {}
        lines[1] = PatternHeader()
        local line = ""
        local cellsonline = 0
        for _, xyz in ipairs(livecells) do
            local x, y, z = xyz[1], xyz[2], xyz[3]
            -- x,y,z are relative to middle cell in grid
            local cell = x.." "..y.." "..z
            if #line < 60 then
                if cellsonline > 0 then line = line.."," end
                line = line..cell
                cellsonline = cellsonline + 1
            else
                lines[#lines+1] = line..","..cell
                line = ""
                cellsonline = 0
            end
        end
        if #line > 0 then lines[#lines+1] = line end
        -- append empty string so we get \n at end of last line
        lines[#lines+1] = ""
        g.setclipstr(table.concat(lines,"\n"))
        return true
    else
        return false
    end
end

----------------------------------------------------------------------

function ClearSelection()
    if selcount > 0 then
        -- kill all selected live cells
        for k,_ in pairs(selected) do
            -- selected[k] is a selected cell (live or dead)
            if grid1[k] then
                grid1[k] = nil
                popcount = popcount - 1
                minimal_live_bounds = false
            end
        end
        Refresh()
    end
end

----------------------------------------------------------------------

function ClearOutside()
    if selcount > 0 then
        -- kill all unselected live cells
        for k,_ in pairs(grid1) do
            -- grid1[k] is a live cell
            if not selected[k] then
                grid1[k] = nil
                popcount = popcount - 1
                minimal_live_bounds = false
            end
        end
        Refresh()
    end
end

----------------------------------------------------------------------

function CutSelection()
    if selcount > 0 then
        -- save the selected live cells as a 3D pattern in clipboard
        -- then kill them
        if CopySelection() then
            ClearSelection()    -- calls Refresh
        end
    end
end

----------------------------------------------------------------------

function RemoveSelection()
    if selcount > 0 then
        selcount = 0
        selected = {}
        InitSelectionBoundary()
        Refresh()
    end
end

----------------------------------------------------------------------

function FlipSelectionX()
    if selcount > 0 then
        -- reflect selected cells' X coords across YZ plane thru middle of selection
        MinimizeSelectionBoundary()
        local midx = minselx + (maxselx - minselx) / 2
        local cells = {}
        local NN = N*N
        for k,_ in pairs(selected) do
            local x = k % N
            local y = (k // N) % N
            local z = k // NN
            cells[#cells+1] = {round(midx*2) - x, y, z, grid1[k]}
            if grid1[k] then
                grid1[k] = nil
                popcount = popcount - 1
                minimal_live_bounds = false
            end
        end
        selected = {}
        -- flip doesn't change selection boundary so no need to call
        -- InitSelectionBoundary and UpdateSelectionBoundary
        for _,xyzs in ipairs(cells) do
            local x, y, z, live = xyzs[1], xyzs[2], xyzs[3], xyzs[4]
            local k = x + N * (y + N * z)
            selected[k] = true
            if live and not grid1[k] then
                -- best to use OR mode for selection actions
                SetLiveCell(x, y, z)
            end
        end
        Refresh()
    end
end

----------------------------------------------------------------------

function FlipSelectionY()
    if selcount > 0 then
        -- reflect selected cells' Y coords across XZ plane thru middle of selection
        MinimizeSelectionBoundary()
        local midy = minsely + (maxsely - minsely) / 2
        local cells = {}
        local NN = N*N
        for k,_ in pairs(selected) do
            local x = k % N
            local y = (k // N) % N
            local z = k // NN
            cells[#cells+1] = {x, round(midy*2) - y, z, grid1[k]}
            if grid1[k] then
                grid1[k] = nil
                popcount = popcount - 1
                minimal_live_bounds = false
            end
        end
        selected = {}
        -- flip doesn't change selection boundary so no need to call
        -- InitSelectionBoundary and UpdateSelectionBoundary
        for _,xyzs in ipairs(cells) do
            local x, y, z, live = xyzs[1], xyzs[2], xyzs[3], xyzs[4]
            local k = x + N * (y + N * z)
            selected[k] = true
            if live and not grid1[k] then
                -- best to use OR mode for selection actions
                SetLiveCell(x, y, z)
            end
        end
        Refresh()
    end
end

----------------------------------------------------------------------

function FlipSelectionZ()
    if selcount > 0 then
        -- reflect selected cells' Z coords across XY plane thru middle of selection
        MinimizeSelectionBoundary()
        local midz = minselz + (maxselz - minselz) / 2
        local cells = {}
        local NN = N*N
        for k,_ in pairs(selected) do
            local x = k % N
            local y = (k // N) % N
            local z = k // NN
            cells[#cells+1] = {x, y, round(midz*2) - z, grid1[k]}
            if grid1[k] then
                grid1[k] = nil
                popcount = popcount - 1
                minimal_live_bounds = false
            end
        end
        selected = {}
        -- flip doesn't change selection boundary so no need to call
        -- InitSelectionBoundary and UpdateSelectionBoundary
        for _,xyzs in ipairs(cells) do
            local x, y, z, live = xyzs[1], xyzs[2], xyzs[3], xyzs[4]
            local k = x + N * (y + N * z)
            selected[k] = true
            if live and not grid1[k] then
                -- best to use OR mode for selection actions
                SetLiveCell(x, y, z)
            end
        end
        Refresh()
    end
end

----------------------------------------------------------------------

function RotateSelectionX()
    if selcount > 0 then
        -- rotate selection clockwise about its X axis by 90 degrees
        MinimizeSelectionBoundary()
        local midy = minsely + (maxsely - minsely) // 2
        local midz = minselz + (maxselz - minselz) // 2
        local y0 = midy - midz
        local z0 = midz + midy + (maxselz - minselz) % 2    -- avoids drift
        local cells = {}
        local NN = N*N
        for k,_ in pairs(selected) do
            local x = k % N
            local y = (k // N) % N
            local z = k // NN
            cells[#cells+1] = {x, (y0+z) % N, (z0-y) % N, grid1[k]}
            if grid1[k] then
                grid1[k] = nil
                popcount = popcount - 1
                minimal_live_bounds = false
            end
        end
        selected = {}
        InitSelectionBoundary()
        for _,xyzs in ipairs(cells) do
            local x, y, z, live = xyzs[1], xyzs[2], xyzs[3], xyzs[4]
            local k = x + N * (y + N * z)
            selected[k] = true
            UpdateSelectionBoundary(x, y, z)
            if live and not grid1[k] then
                -- best to use OR mode for selection actions
                SetLiveCell(x, y, z)
            end
        end
        Refresh()
    end
end

----------------------------------------------------------------------

function RotateSelectionY()
    if selcount > 0 then
        -- rotate selection clockwise about its Y axis by 90 degrees
        MinimizeSelectionBoundary()
        local midx = minselx + (maxselx - minselx) // 2
        local midz = minselz + (maxselz - minselz) // 2
        local x0 = midx + midz + (maxselx - minselx) % 2    -- avoids drift
        local z0 = midz - midx
        local cells = {}
        local NN = N*N
        for k,_ in pairs(selected) do
            local x = k % N
            local y = (k // N) % N
            local z = k // NN
            cells[#cells+1] = {(x0-z) % N, y, (z0+x) % N, grid1[k]}
            if grid1[k] then
                grid1[k] = nil
                popcount = popcount - 1
                minimal_live_bounds = false
            end
        end
        selected = {}
        InitSelectionBoundary()
        for _,xyzs in ipairs(cells) do
            local x, y, z, live = xyzs[1], xyzs[2], xyzs[3], xyzs[4]
            local k = x + N * (y + N * z)
            selected[k] = true
            UpdateSelectionBoundary(x, y, z)
            if live and not grid1[k] then
                -- best to use OR mode for selection actions
                SetLiveCell(x, y, z)
            end
        end
        Refresh()
    end
end

----------------------------------------------------------------------

function RotateSelectionZ()
    if selcount > 0 then
        -- rotate selection clockwise about its Z axis by 90 degrees
        MinimizeSelectionBoundary()
        local midx = minselx + (maxselx - minselx) // 2
        local midy = minsely + (maxsely - minsely) // 2
        local x0 = midx - midy
        local y0 = midy + midx + (maxsely - minsely) % 2    -- avoids drift
        local cells = {}
        local NN = N*N
        for k,_ in pairs(selected) do
            local x = k % N
            local y = (k // N) % N
            local z = k // NN
            cells[#cells+1] = {(x0+y) % N, (y0-x) % N, z, grid1[k]}
            if grid1[k] then
                grid1[k] = nil
                popcount = popcount - 1
                minimal_live_bounds = false
            end
        end
        selected = {}
        InitSelectionBoundary()
        for _,xyzs in ipairs(cells) do
            local x, y, z, live = xyzs[1], xyzs[2], xyzs[3], xyzs[4]
            local k = x + N * (y + N * z)
            selected[k] = true
            UpdateSelectionBoundary(x, y, z)
            if live and not grid1[k] then
                -- best to use OR mode for selection actions
                SetLiveCell(x, y, z)
            end
        end
        Refresh()
    end
end

----------------------------------------------------------------------

function Paste()
    -- check if a paste is already pending
    if pastecount > 0 then return end

    -- if the clipboard contains a valid 3D pattern then create a sparse array
    -- of paste cells (to be displayed as translucent red)
    local filepath = CopyClipboardToFile()
    if filepath then
        local err, newpattern = ReadPattern(filepath)
        if err then
            message = "Clipboard does not contain a valid 3D pattern."
            Refresh()
            return
        end
        if newpattern.newpop == 0 then
            message = "Clipboard pattern is empty."
            Refresh()
            return
        end
        
        -- newpattern contains valid pattern, but might be too big
        minpastex = newpattern.newminx
        minpastey = newpattern.newminy
        minpastez = newpattern.newminz
        maxpastex = newpattern.newmaxx
        maxpastey = newpattern.newmaxy
        maxpastez = newpattern.newmaxz
        local pwd = maxpastex - minpastex + 1
        local pht = maxpastey - minpastey + 1
        local pdp = maxpastez - minpastez + 1
        if pwd > N or pht > N or pdp > N then
            message = "Clipboard pattern is too big ("..pwd.." x "..pht.." x "..pdp..")"
            Refresh()
            return
        end
        
        -- set pastecount and pastecell
        pastecount = newpattern.newpop
        pastecell = {}
        if newpattern.newsize == N then
            -- just put paste pattern in same location as the cut/copy
            pastecell = newpattern.newgrid
        else
            -- put paste pattern in middle of grid and update paste boundary
            local oldpx, oldpy, oldpz = minpastex, minpastey, minpastez
            minpastex = math.maxinteger
            minpastey = math.maxinteger
            minpastez = math.maxinteger
            maxpastex = math.mininteger
            maxpastey = math.mininteger
            maxpastez = math.mininteger
            local P = newpattern.newsize
            local PP = P*P
            for k,_ in pairs(newpattern.newgrid) do
                -- newpattern.newgrid[k] is a live cell
                local x = (k % P)      - oldpx + (N - pwd) // 2
                local y = (k // P % P) - oldpy + (N - pht) // 2
                local z = (k // PP)    - oldpz + (N - pdp) // 2
                pastecell[ x + N * (y + N * z) ] = 1
                if x < minpastex then minpastex = x end
                if y < minpastey then minpastey = y end
                if z < minpastez then minpastez = z end
                if x > maxpastex then maxpastex = x end
                if y > maxpastey then maxpastey = y end
                if z > maxpastez then maxpastez = z end
            end
        end
        Refresh()
    end
end

----------------------------------------------------------------------

function CancelPaste()
    if pastecount > 0 then
        pastecount = 0
        pastecell = {}
        Refresh()
    end
end

----------------------------------------------------------------------

function PasteOR()
    if pastecount > 0 then
        local NN = N*N
        for k,_ in pairs(pastecell) do
            if not grid1[k] then
                SetLiveCell(k % N, (k // N) % N, k // NN)
            end
        end
        pastecount = 0
        pastecell = {}
        Refresh()
    end
end

----------------------------------------------------------------------

function PasteXOR()
    if pastecount > 0 then
        local NN = N*N
        for k,_ in pairs(pastecell) do
            if grid1[k] then
                grid1[k] = nil
                popcount = popcount - 1
                minimal_live_bounds = false
            else
                SetLiveCell(k % N, (k // N) % N, k // NN)
            end
        end
        pastecount = 0
        pastecell = {}
        Refresh()
    end
end

----------------------------------------------------------------------

function FlipPasteX()
    if pastecount > 0 then
        -- reflect X coords across YZ plane thru middle of paste pattern
        local midx = minpastex + (maxpastex - minpastex) / 2
        local cells = {}
        local NN = N*N
        for k,_ in pairs(pastecell) do
            local x = k % N
            local y = (k // N) % N
            local z = k // NN
            cells[#cells+1] = {round(midx*2) - x, y, z}
        end
        -- flip doesn't change paste boundary
        pastecell = {}
        for _,xyz in ipairs(cells) do
            local x, y, z = xyz[1], xyz[2], xyz[3]
            pastecell[x + N * (y + N * z)] = 1
        end
        Refresh()
    end
end

----------------------------------------------------------------------

function FlipPasteY()
    if pastecount > 0 then
        -- reflect Y coords across XZ plane thru middle of paste pattern
        local midy = minpastey + (maxpastey - minpastey) / 2
        local cells = {}
        local NN = N*N
        for k,_ in pairs(pastecell) do
            local x = k % N
            local y = (k // N) % N
            local z = k // NN
            cells[#cells+1] = {x, round(midy*2) - y, z}
        end
        -- flip doesn't change paste boundary
        pastecell = {}
        for _,xyz in ipairs(cells) do
            local x, y, z = xyz[1], xyz[2], xyz[3]
            pastecell[x + N * (y + N * z)] = 1
        end
        Refresh()
    end
end

----------------------------------------------------------------------

function FlipPasteZ()
    if pastecount > 0 then
        -- reflect Z coords across XY plane thru middle of paste pattern
        local midz = minpastez + (maxpastez - minpastez) / 2
        local cells = {}
        local NN = N*N
        for k,_ in pairs(pastecell) do
            local x = k % N
            local y = (k // N) % N
            local z = k // NN
            cells[#cells+1] = {x, y, round(midz*2) - z}
        end
        -- flip doesn't change paste boundary
        pastecell = {}
        for _,xyz in ipairs(cells) do
            local x, y, z = xyz[1], xyz[2], xyz[3]
            pastecell[x + N * (y + N * z)] = 1
        end
        Refresh()
    end
end

----------------------------------------------------------------------

function RotatePasteX()
    if pastecount > 0 then
        -- rotate paste pattern clockwise about its X axis by 90 degrees
        local midy = minpastey + (maxpastey - minpastey) // 2
        local midz = minpastez + (maxpastez - minpastez) // 2
        local y0 = midy - midz
        local z0 = midz + midy + (maxpastez - minpastez) % 2    -- avoids drift
        local cells = {}
        local NN = N*N
        for k,_ in pairs(pastecell) do
            local x = k % N
            local y = (k // N) % N
            local z = k // NN
            cells[#cells+1] = {x, (y0+z) % N, (z0-y) % N}
        end
        pastecell = {}
        -- minpastex, maxpastex don't change
        minpastey = math.maxinteger
        minpastez = math.maxinteger
        maxpastey = math.mininteger
        maxpastez = math.mininteger
        for _,xyz in ipairs(cells) do
            local x, y, z = xyz[1], xyz[2], xyz[3]
            pastecell[x + N * (y + N * z)] = 1
            if y < minpastey then minpastey = y end
            if y > maxpastey then maxpastey = y end
            if z < minpastez then minpastez = z end
            if z > maxpastez then maxpastez = z end
        end
        Refresh()
    end
end

----------------------------------------------------------------------

function RotatePasteY()
    if pastecount > 0 then
        -- rotate paste pattern clockwise about its Y axis by 90 degrees
        local midx = minpastex + (maxpastex - minpastex) // 2
        local midz = minpastez + (maxpastez - minpastez) // 2
        local x0 = midx + midz + (maxpastex - minpastex) % 2    -- avoids drift
        local z0 = midz - midx
        local cells = {}
        local NN = N*N
        for k,_ in pairs(pastecell) do
            local x = k % N
            local y = (k // N) % N
            local z = k // NN
            cells[#cells+1] = {(x0-z) % N, y, (z0+x) % N}
        end
        pastecell = {}
        -- minpastey, maxpastey don't change
        minpastex = math.maxinteger
        minpastez = math.maxinteger
        maxpastex = math.mininteger
        maxpastez = math.mininteger
        for _,xyz in ipairs(cells) do
            local x, y, z = xyz[1], xyz[2], xyz[3]
            pastecell[x + N * (y + N * z)] = 1
            if x < minpastex then minpastex = x end
            if x > maxpastex then maxpastex = x end
            if z < minpastez then minpastez = z end
            if z > maxpastez then maxpastez = z end
        end
        Refresh()
    end
end

----------------------------------------------------------------------

function RotatePasteZ()
    if pastecount > 0 then
        -- rotate paste pattern clockwise about its Z axis by 90 degrees
        local midx = minpastex + (maxpastex - minpastex) // 2
        local midy = minpastey + (maxpastey - minpastey) // 2
        local x0 = midx - midy
        local y0 = midy + midx + (maxpastey - minpastey) % 2    -- avoids drift
        local cells = {}
        local NN = N*N
        for k,_ in pairs(pastecell) do
            local x = k % N
            local y = (k // N) % N
            local z = k // NN
            cells[#cells+1] = {(x0+y) % N, (y0-x) % N, z}
        end
        pastecell = {}
        -- minpastez, maxpastez don't change
        minpastex = math.maxinteger
        minpastey = math.maxinteger
        maxpastex = math.mininteger
        maxpastey = math.mininteger
        for _,xyz in ipairs(cells) do
            local x, y, z = xyz[1], xyz[2], xyz[3]
            pastecell[x + N * (y + N * z)] = 1
            if x < minpastex then minpastex = x end
            if x > maxpastex then maxpastex = x end
            if y < minpastey then minpastey = y end
            if y > maxpastey then maxpastey = y end
        end
        Refresh()
    end
end

----------------------------------------------------------------------

-- get pop-up menu stuff from oplus in 3.2b1!!!

function draw_line(x1, y1, x2, y2) ov("line "..x1.." "..y1.." "..x2.." "..y2) end
function fill_rect(x, y, wd, ht) ov("fill "..x.." "..y.." "..wd.." "..ht) end

function DrawPopUpMenu(p, chosenitem)
    -- draw pop-up window showing all items
    local numitems = #p.items
    if numitems == 0 then return end
    
    local oldfont = ov(p.menufont)
    local oldblend = ov("blend 1")
    local oldrgba = ov(p.bgcolor)

    local ht = p.menuht + 1
    local wd = p.menuwd
    local x = p.x
    local y = p.y
    fill_rect(x, y, wd, ht)
    
    -- draw translucent gray shadows
    ov("rgba 48 48 48 128")
    local shadowsize = 3
    fill_rect(x+shadowsize, y+ht, wd-shadowsize, shadowsize)
    fill_rect(x+wd, y+shadowsize, shadowsize, ht)
    
    x = x + p.menugap
    y = y + op.yoffset
    for i = 1, numitems do
        local item = p.items[i]
        if item.f == nil then
            -- item is a separator
            ov(p.discolor)
            draw_line(x-p.menugap, y+p.itemht//2, x-p.menugap+wd-1, y+p.itemht//2)
        else
            if i == chosenitem and item.enabled then
                ov(p.selcolor)
                fill_rect(x-p.menugap, y, wd, p.itemht)
            end
            local oldtextbg = ov("textoption background 0 0 0 0")
            if item.enabled then
                op.maketext(item.name, nil, p.menutext, op.textshadowx, op.textshadowy, op.textshadowrgba)
                op.pastetext(x, y + p.itemgap)
                ov(p.menutext)
            else
                op.maketext(item.name, nil, p.discolor)  -- no shadow if disabled
                op.pastetext(x, y + p.itemgap)
                ov(p.discolor)
            end
            ov("textoption background "..oldtextbg)
            if item.ticked then
                -- draw tick mark at right edge
                local x1 = x - p.menugap + wd - p.menugap
                local y1 = y + 6
                local x2 = x1 - 6
                local y2 = y + p.itemht - 8
                local oldwidth = ov("lineoption width 4")
                if item.enabled and (op.textshadowx > 0 or op.textshadowy > 0) then
                    local oldcolor = ov(op.textshadowrgba)
                    draw_line(x1+op.textshadowx, y1+op.textshadowy, x2+op.textshadowx, y2+op.textshadowy)
                    draw_line(x2+op.textshadowx, y2+op.textshadowy, x2+op.textshadowx-5, y2+op.textshadowy-3)
                    ov("rgba "..oldcolor)
                end
                draw_line(x1, y1, x2, y2)
                draw_line(x2, y2, x2-5, y2-3)
                ov("lineoption width "..oldwidth)
            end
        end
        y = y + p.itemht
    end

    ov("blend "..oldblend)
    ov("font "..oldfont)
    ov("rgba "..oldrgba)
end

----------------------------------------------------------------------

function GetPopUpItem(x, y, p)
    -- return index of item at given mouse location
    if x <= p.x or y <= p.y then return 0 end
    local numitems = #p.items
    local ht = numitems * p.itemht
    if y > p.y + ht then return 0 end
    if x > p.x + p.menuwd then return 0 end
    
    -- x,y is somewhere in a menu item
    local itemindex = math.floor((y - p.y) / p.itemht) + 1
    if itemindex > numitems then itemindex = numitems end

    return itemindex
end

----------------------------------------------------------------------

function choose_popup_item(p)
    -- return a chosen item from the given pop-up menu
    -- or nil if the item is disabled or no item is selected
    
    local t0 = g.millisecs()

    -- save entire overlay in bgclip
    local bgclip = string.gsub(tostring(p).."bg"," ","")
    ov("copy 0 0 0 0 "..bgclip)
    
    local chosenitem = 0
    DrawPopUpMenu(p, chosenitem)
    g.update()
    
    local x = p.x
    local y = p.y
    local prevx = x
    local prevy = y
    while true do
        local event = g.getevent()
        if event:find("^mup") then
            if g.millisecs() - t0 > 500 then
                break
            end
        elseif event:find("^oclick") then
            local _, sx, sy, butt, mods = split(event)
            if butt == "left" and mods == "none" then
                x = tonumber(sx)
                y = tonumber(sy)
                break
            end
        elseif event == "key enter none" or event == "key return none" then
            break
        end
        local xy = ov("xy")
        if #xy > 0 then
            x, y = split(xy)
            x = tonumber(x)
            y = tonumber(y)
            if x ~= prevx or y ~= prevy then
                -- check if mouse moved into or out of an item
                local olditem = chosenitem
                chosenitem = GetPopUpItem(x, y, p)
                if chosenitem ~= olditem then
                    ov("paste 0 0 "..bgclip)
                    DrawPopUpMenu(p, chosenitem)
                    g.update()
                end
                prevx = x
                prevy = y
            end
        end
    end

    chosenitem = GetPopUpItem(x, y, p)
    
    -- restore overlay
    ov("paste 0 0 "..bgclip)
    g.update()
    ov("delete "..bgclip)
    
    return chosenitem
end

--------------------------------------------------------------------------------

function popupmenu()
    -- return a table that makes it easy to create and use a pop-up menu
    local p = {}
    
    p.items = {}        -- array of items
    p.labelht = 0       -- height of label text
    p.itemht = 0        -- height of an item
    p.menuwd = 0        -- width of pop-up menu
    p.menuht = 0        -- height of pop-up menu
    p.x, p.y = 0, 0     -- top left location of pop-up menu

    p.menufont = "font 12 default-bold"
    p.menutext = op.white
    p.menugap = 10
    p.itemgap = 2
    p.bgcolor = "rgba 40 128 255 255"
    p.selcolor = "rgba 20 64 255 255"
    p.discolor = "rgba 88 176 255 255"
    
    local function check_width(itemname)
        local oldfont = ov(p.menufont)
        local oldblend = ov("blend 1")
        local wd, ht = op.maketext(itemname, nil, p.menutext, op.textshadowx, op.textshadowy, op.textshadowrgba)
        ov("blend "..oldblend)
        ov("font "..oldfont)
        p.labelht = ht
        p.itemht = ht + p.itemgap*2
        local itemwd = wd + p.menugap*2 + 20
        if itemwd > p.menuwd then p.menuwd = itemwd end
    end
    
    p.additem = function (itemname, onselect, args)
        args = args or {}
        check_width(itemname)
        p.items[#p.items+1] = { name=itemname, f=onselect, fargs=args, enabled=true, ticked=false }
        p.menuht = #p.items * p.itemht
    end
    
    p.enableitem = function (itemindex, bool)
        -- enable/disable the given item
        p.items[itemindex].enabled = bool
    end

    p.tickitem = function (itemindex, bool)
        -- tick/untick the given item
        p.items[itemindex].ticked = bool
    end
    
    p.setbgcolor = function (rgba)
        p.bgcolor = rgba
        local _,R,G,B,A = split(rgba)
        R = tonumber(R)
        G = tonumber(G)
        B = tonumber(B)
        A = tonumber(A)
        -- use a darker color when item is selected
        p.selcolor = "rgba "..max(0,R-48).." "..max(0,G-48).." "..max(0,B-48).." "..A
        -- use lighter color for disabled items and separator lines
        p.discolor = "rgba "..min(255,R+48).." "..min(255,G+48).." "..min(255,B+48).." "..A
    end
    
    p.show = function (x, y, ovwd, ovht)
        if x + p.menuwd > ovwd then x = x - p.menuwd - 2 end
        if y + p.menuht > ovht then y = ovht - p.menuht end
        p.x = x
        p.y = y
        local itemindex = choose_popup_item(p)
        if itemindex > 0 then
            local item = p.items[itemindex]
            if item and item.f and item.enabled then
                -- call this item's handler
                item.f( table.unpack(item.fargs) )
            end
        end
    end
    
    return p
end

----------------------------------------------------------------------

function ChoosePasteAction(mousex, mousey)
    -- show red pop-up menu at mousex,mousey and let user choose a paste action
    pastemenu.setbgcolor("rgba 176 48 48 255")
    pastemenu.show(mousex, mousey, ovwd, ovht)
end

----------------------------------------------------------------------

function ChooseSelectionAction(mousex, mousey)
    -- show green pop-up menu at mousex,mousey and let user choose a selection action
    selmenu.setbgcolor("rgba 0 128 0 255")
    selmenu.show(mousex, mousey, ovwd, ovht)
end

----------------------------------------------------------------------

function Undo()
    message = "UNDO IS NOT YET IMPLEMENTED!!!"
    Refresh()
end

----------------------------------------------------------------------

function Redo()
    message = "REDO IS NOT YET IMPLEMENTED!!!"
    Refresh()
end

----------------------------------------------------------------------

function Rotate(xangle, yangle, zangle, display)
    if display == nil then display = true end
    local x = xangle * DEGTORAD
    local y = yangle * DEGTORAD
    local z = zangle * DEGTORAD
    local cosrx = cos(x)
    local sinrx = sin(x)
    local cosry = cos(y)
    local sinry = sin(y)
    local cosrz = cos(z)
    local sinrz = sin(z)
    
    -- calculate transformation matrix for rotation
    -- (note that rotation is about fixed *screen* axes)
    local a = cosry*cosrz
    local b = cosry*sinrz
    local c = -sinry
    local d = sinrx*sinry*cosrz - cosrx*sinrz
    local e = sinrx*sinry*sinrz + cosrx*cosrz
    local f = sinrx*cosry
    local g = cosrx*sinry*cosrz + sinrx*sinrz
    local h = cosrx*sinry*sinrz - sinrx*cosrz
    local i = cosrx*cosry
    
    -- rotate global matrix by new matrix
    local anew = a*xixo + b*yixo + c*zixo
    local bnew = a*xiyo + b*yiyo + c*ziyo
    local cnew = a*xizo + b*yizo + c*zizo
    local dnew = d*xixo + e*yixo + f*zixo
    local enew = d*xiyo + e*yiyo + f*ziyo
    local fnew = d*xizo + e*yizo + f*zizo
    local gnew = g*xixo + h*yixo + i*zixo
    local hnew = g*xiyo + h*yiyo + i*ziyo
    local inew = g*xizo + h*yizo + i*zizo
    
    -- update global transformation matrix
    xixo = anew
    xiyo = bnew
    xizo = cnew
    yixo = dnew
    yiyo = enew
    yizo = fnew
    zixo = gnew
    ziyo = hnew
    zizo = inew

    if display then Refresh() end
end

----------------------------------------------------------------------

function Zoom(newsize)
    -- zoom in/out by changing size of cells
    if newsize < MINSIZE then return end
    if newsize > MAXSIZE then return end
    CELLSIZE = newsize
    HALFCELL = CELLSIZE/2.0
    MIDGRID = (N+1-(N%2))*HALFCELL
    LEN = CELLSIZE-BORDER*2
    CreateAxes()
    Refresh()
end

----------------------------------------------------------------------

function StartStop()
    generating = not generating
    UpdateStartButton()
    Refresh()
end

----------------------------------------------------------------------

function Step1()
    if generating then
        generating = false
        UpdateStartButton()
    end
    NextGeneration()
end

----------------------------------------------------------------------

function Reset()
    if gencount > startcount then
        -- restore starting grid size if necessary
        if N ~= startN then
            N = startN
            MIDGRID = (N+1-(N%2))*HALFCELL
            CreateAxes()
        end
        
        -- restore active plane
        SetActivePlane(startplane, startpos)
        
        -- restore starting rule
        ParseRule(startrule)
        
        -- restore starting pattern
        pattname = startname
        gencount = startcount
        generating = false
        UpdateStartButton()
        ClearCells()
        local clipcount = PutCells(startcells)
        if clipcount > 0 then
            -- should never happen
            g.warn("Bug in Reset! clipped cells = "..clipcount)
        end
        
        -- restore starting selection
        selcount = startselcount
        selected = {}
        InitSelectionBoundary()
        if selcount > 0 then
            local NN = N*N
            for k,_ in pairs(startselected) do
                selected[k] = true
                UpdateSelectionBoundary(k % N, (k // N) % N, k // NN)
            end
        end
        
        Refresh()
    end
end

----------------------------------------------------------------------

local function GetMidPoint(x, y, z)
    -- return mid point of cell at given grid position
    x = x * CELLSIZE + HALFCELL - MIDGRID
    y = y * CELLSIZE + HALFCELL - MIDGRID
    z = z * CELLSIZE + HALFCELL - MIDGRID
    local newx, newy = TransformPoint({x, y, z})
    -- use orthographic projection
    x = round(newx) + midx
    y = round(newy) + midy
    return x, y
end

----------------------------------------------------------------------

function FitGrid()
    local function Visible(x, y)
        -- return true if pixel at x,y is within area under tool bar
        if x < 0 or x >= ovwd then return false end
        if y <= toolbarht or y >= ovht then return false end
        return true
    end

    -- find largest CELLSIZE at which all corners of grid are visible
    CELLSIZE = MAXSIZE + 1
    repeat
        CELLSIZE = CELLSIZE-1
        HALFCELL = CELLSIZE/2.0
        MIDGRID = (N+1-(N%2))*HALFCELL
        LEN = CELLSIZE-BORDER*2
        CreateAxes()
        if CELLSIZE == MINSIZE then break end
        local x1, y1 = GetMidPoint(0,     0,   0)
        local x2, y2 = GetMidPoint(0,     0, N-1)
        local x3, y3 = GetMidPoint(0,   N-1,   0)
        local x4, y4 = GetMidPoint(N-1,   0,   0)
        local x5, y5 = GetMidPoint(0,   N-1, N-1)
        local x6, y6 = GetMidPoint(N-1, N-1,   0)
        local x7, y7 = GetMidPoint(N-1,   0, N-1)
        local x8, y8 = GetMidPoint(N-1, N-1, N-1)
        local xmin = min(x1,x2,x3,x4,x5,x6,x7,x8) - CELLSIZE
        local xmax = max(x1,x2,x3,x4,x5,x6,x7,x8) + CELLSIZE
        local ymin = min(y1,y2,y3,y4,y5,y6,y7,y8) - CELLSIZE
        local ymax = max(y1,y2,y3,y4,y5,y6,y7,y8) + CELLSIZE
    until Visible(xmin,ymin) and Visible(xmin,ymax) and
          Visible(xmax,ymin) and Visible(xmax,ymax)
    Refresh()
end

----------------------------------------------------------------------

-- getters for user scripts

function GetGeneration() return gencount end
function GetGridSize() return N end
function GetPercentage() return perc end
function GetPopulation() return popcount end
function GetRule() return rulestring end

----------------------------------------------------------------------

-- for user scripts
function GetBounds()
    if popcount > 0 then
        -- return the pattern's minimal bounding box
        MinimizeLiveBoundary()
        local mid = N//2
        return { minx-mid, maxx-mid, miny-mid, maxy-mid, minz-mid, maxz-mid }
    else
        return {}
    end
end

----------------------------------------------------------------------

-- for user scripts
function Step()
    if popcount > 0 then NextGeneration() end
end

----------------------------------------------------------------------

-- for user scripts
function SetRule(newrule)
    if not ParseRule(newrule) then
        error("Bad rule in SetRule: "..newrule, 2)
    end
end

----------------------------------------------------------------------

-- for user scripts (where 0,0,0 is middle cell in grid)
function GetCell(x, y, z)
    local mid = N//2
    if grid1[ (x+mid) + N * ((y+mid) + N * (z+mid)) ] then
        return 1
    else
        return 0
    end
end

----------------------------------------------------------------------

-- for user scripts (where 0,0,0 is middle cell in grid)
function SetCell(x, y, z, state)
    local mid = N//2
    SetCellState(x+mid, y+mid, z+mid, state)
end

----------------------------------------------------------------------

-- for user scripts (where 0,0,0 is middle cell in grid)
function SelectCell(x, y, z)
    local mid = N//2
    SetSelection(x+mid, y+mid, z+mid, true)
end

----------------------------------------------------------------------

-- for user scripts (where 0,0,0 is middle cell in grid)
function DeselectCell(x, y, z)
    local mid = N//2
    SetSelection(x+mid, y+mid, z+mid, false)
end

----------------------------------------------------------------------

-- for user scripts
function SetMessage(msg)
    message = msg
end

----------------------------------------------------------------------

-- for user scripts
function Update()
    local savelevel = scriptlevel
    scriptlevel = 0
    Refresh()
    scriptlevel = savelevel
end

----------------------------------------------------------------------

function ShowHelp()
    local htmldata = [[
<html><title>Golly Help: 3D.lua</title>
<body bgcolor="#FFFFCE">

<p>
<dd><a href="#intro"><b>Introduction</b></a></dd>
<dd><a href="#mouse"><b>Mouse Controls</b></a></dd>
<dd><a href="#keyboard"><b>Keyboard Shortcuts</b></a></dd>
<dd><a href="#scripts"><b>Running Scripts</b></a></dd>
<dd><a href="#rules"><b>Supported Rules</b></a></dd>
<dd><a href="#files"><b>The 3D File Format</b></a></dd>
<dd><a href="#refs"><b>Credits and References</b></a></dd>
</p>

<p><a name="intro"></a><br>
<font size=+1><b>Introduction</b></font>

<p>
3D.lua is a script that lets you explore three-dimensional cellular automata.
The script uses overlay commands to completely replace Golly's usual
interface (note that all your Golly settings will be restored when 3D.lua exits).

<p><a name="mouse"></a><br>
<font size=+1><b>Mouse Controls</b></font>

<p>
If the Move option is ticked then you can use the hand cursor
to rotate the pattern by clicking and dragging.
Rotation occurs around the middle cell in the grid.

<p>
It's also possible to do some editing with the hand cursor.
You can alt-click on a live cell to erase it <em>and</em> any live cells
behind it, as long as their mid points are within a half-cell radius of the mouse click.
Or you can shift-click on a live cell to select it <em>and</em> any live cells behind it.
This makes it easy to quickly erase or select isolated objects.

<p>
If the Draw option is ticked then you can use the pencil cursor
to draw or erase cells in the active plane (shown as translucent blue).
Note that any live cells outside the active plane are drawn as white points.
Click and drag outside the active plane to rotate the pattern.

<p>
If the Select option is ticked then you can use the cross-hairs cursor
to select or deselect cells in the active plane.
Any selected cells outside the active plane are drawn as translucent green points.
Click and drag outside the active plane to rotate the pattern.

<p>
To move the active plane to a different position, shift-click anywhere in the
active plane and drag the mouse.  Or you can hit the "," or "." keys.
Hit shift-A to change the active plane's orientation.

<p>
If a paste pattern is visible (its cells are red) then you can control-click or
right-click anywhere (using any cursor) to get a red pop-up menu that lets
you choose various paste actions.
If a selection exists (green cells) then you can control-click or right-click
to get a green pop-up menu with various selection actions.
If a paste pattern and a selection both exist then the paste menu takes precedence.

<p>
Use the mouse wheel at any time to zoom in/out.
The zoom is always centered on the middle cell in the grid.

<p><a name="keyboard"></a><br>
<font size=+1><b>Keyboard Shortcuts</b></font>

<p>
You can use the following keyboard shortcuts (but see <a href="#HandleKey">below</a>
how you can write a script to create new shortcuts or override any of the supplied
shortcuts):

<p>
<center>
<table cellspacing=1 border=2 cols=2 width="90%">
<tr><td align=center>Keys</td><td align=center>Actions</td></tr>
<tr><td align=right> enter &nbsp;</td><td>&nbsp; start/stop generating pattern </td></tr>
<tr><td align=right> space &nbsp;</td><td>&nbsp; advance pattern by one generation </td></tr>
<tr><td align=right> arrows &nbsp;</td><td>&nbsp; rotate about X/Y screen axes </td></tr>
<tr><td align=right> alt-arrows &nbsp;</td><td>&nbsp; rotate about Z screen axis </td></tr>
<tr><td align=right> ctrl-N &nbsp;</td><td>&nbsp; create a new, empty pattern </td></tr>
<tr><td align=right> ctrl-O &nbsp;</td><td>&nbsp; open a selected pattern file </td></tr>
<tr><td align=right> ctrl-S &nbsp;</td><td>&nbsp; save the current pattern in a file </td></tr>
<tr><td align=right> alt-S &nbsp;</td><td>&nbsp; select a start-up script </td></tr>
<tr><td align=right> shift-O &nbsp;</td><td>&nbsp; open pattern in clipboard </td></tr>
<tr><td align=right> shift-R &nbsp;</td><td>&nbsp; run script in clipboard </td></tr>
<tr><td align=right> ctrl-R &nbsp;</td><td>&nbsp; reset to the starting pattern </td></tr>
<tr><td align=right> Z &nbsp;</td><td>&nbsp; undo </td></tr>
<tr><td align=right> shift-Z &nbsp;</td><td>&nbsp; redo </td></tr>
<tr><td align=right> ctrl-X &nbsp;</td><td>&nbsp; cut </td></tr>
<tr><td align=right> ctrl-C &nbsp;</td><td>&nbsp; copy </td></tr>
<tr><td align=right> ctrl-V &nbsp;</td><td>&nbsp; paste </td></tr>
<tr><td align=right> alt-V &nbsp;</td><td>&nbsp; cancel paste </td></tr>
<tr><td align=right> delete &nbsp;</td><td>&nbsp; kill selected live cells </td></tr>
<tr><td align=right> shift-delete &nbsp;</td><td>&nbsp; kill unselected live cells </td></tr>
<tr><td align=right> A &nbsp;</td><td>&nbsp; select all </td></tr>
<tr><td align=right> K &nbsp;</td><td>&nbsp; remove selection </td></tr>
<tr><td align=right> B &nbsp;</td><td>&nbsp; back view (rotate 180 deg about Y axis) </td></tr>
<tr><td align=right> I &nbsp;</td><td>&nbsp; restore initial view </td></tr>
<tr><td align=right> F &nbsp;</td><td>&nbsp; fit entire grid within view </td></tr>
<tr><td align=right> [ &nbsp;</td><td>&nbsp; zoom out </td></tr>
<tr><td align=right> ] &nbsp;</td><td>&nbsp; zoom in </td></tr>
<tr><td align=right> P &nbsp;</td><td>&nbsp; cycle live cells (cubes/points/spheres) </td></tr>
<tr><td align=right> L &nbsp;</td><td>&nbsp; toggle lines </td></tr>
<tr><td align=right> T &nbsp;</td><td>&nbsp; toggle the tool bar </td></tr>
<tr><td align=right> 5 &nbsp;</td><td>&nbsp; create a random pattern with given density </td></tr>
<tr><td align=right> G &nbsp;</td><td>&nbsp; change the grid size </td></tr>
<tr><td align=right> R &nbsp;</td><td>&nbsp; change the rule </td></tr>
<tr><td align=right> , &nbsp;</td><td>&nbsp; move active plane closer (in initial view) </td></tr>
<tr><td align=right> . &nbsp;</td><td>&nbsp; move active plane further away (in initial view) </td></tr>
<tr><td align=right> shift-A &nbsp;</td><td>&nbsp; cycle active plane's orientation (XY/XZ/YZ) </td></tr>
<tr><td align=right> D &nbsp;</td><td>&nbsp; switch cursor to draw mode </td></tr>
<tr><td align=right> S &nbsp;</td><td>&nbsp; switch cursor to select mode </td></tr>
<tr><td align=right> M &nbsp;</td><td>&nbsp; switch cursor to move mode </td></tr>
<tr><td align=right> C &nbsp;</td><td>&nbsp; cycle cursor mode (draw/select/move) </td></tr>
<tr><td align=right> H &nbsp;</td><td>&nbsp; show this help </td></tr>
<tr><td align=right> Q &nbsp;</td><td>&nbsp; quit the script </td></tr>
</table>
</center>

<p><a name="scripts"></a><br>
<font size=+1><b>Running Scripts</b></font>

<p>
3D.lua can run other Lua scripts, either by clicking the "Run..." button
and selecting a .lua file, or by copying Lua code to the clipboard and
typing shift-R.  Try the latter method with this example that creates
a small random pattern in the middle of the grid:

<dd><table border=0><pre>
NewPattern()
local perc = GetPercentage()
local quarter = GetGridSize()//4
for z = -quarter, quarter do
    for y = -quarter, quarter do
        for x = -quarter, quarter do
            if math.random(0,99) &lt; perc then
                SetCell(x, y, z, 1)
            end
        end
    end
end
MoveMode() -- sets the hand cursor</pre></table></dd>

<p>
Any syntax or runtime errors in a script won't abort 3D.lua.
The script will terminate and you'll get a warning message, hopefully
with enough information that lets you fix the error.

<p><a name="HandleKey"></a>
It's possible to override any of the global functions in 3D.lua.
The following script shows how to override the HandleKey function in 3D.lua
to create a keyboard shortcut for running a particular script.
Even more useful, you can get 3D.lua to automatically run this script
when it starts up by typing alt-S and selecting a .lua file containing
this code:

<dd><table border=0><pre>
local g = golly()
local gp = require "gplus"
local savedHandler = HandleKey
function HandleKey(event)
    local _, key, mods = gp.split(event)
    if key == "r" and mods == "alt" then
        RunScript(g.getdir("app").."My-scripts/3D/random-rule.lua")
    else
        -- pass the event to the original HandleKey function
        savedHandler(event)
    end
end</pre></table></dd>

<p>
Here are descriptions of various functions in 3D.lua you might want to
use in your own scripts (the functions are in alphabetical order):

<p>
TO BE COMPLETED!!!

<a name="DeselectCell"></a><p><dt><b>DeselectCell(<i>x, y, z</i>)</b></dt>
<dd>
Deselect the given cell.
The x,y,z coordinates are relative to the middle cell in the grid.
</dd>

<a name="DrawMode"></a><p><dt><b>DrawMode()</b></dt>
<dd>
Switch to the pencil cursor and display the active plane.
</dd>

<a name="GetBounds"></a><p><dt><b>GetBounds()</b></dt>
<dd>
Return {} if the pattern is empty, otherwise return the minimal bounding box
of all live cells as an array with 6 values: {minx, maxx, miny, maxy, minz, maxz}.
The boundary values are relative to the middle cell in the grid.
</dd>

<a name="GetCell"></a><p><dt><b>GetCell(<i>x, y, z</i>)</b></dt>
<dd>
Return the state (0 or 1) of the given cell.
The x,y,z coordinates are relative to the middle cell in the grid.
</dd>

<a name="GetGeneration"></a><p><dt><b>GetGeneration()</b></dt>
<dd>
Return the generation count.
</dd>

<a name="GetGridSize"></a><p><dt><b>GetGridSize()</b></dt>
<dd>
Return the current grid size (3 to 100).
</dd>

<a name="GetPercentage"></a><p><dt><b>GetPercentage()</b></dt>
<dd>
Return the percentage (0 to 100) given in the most recent "Random..." dialog.
</dd>

<a name="GetPopulation"></a><p><dt><b>GetPopulation()</b></dt>
<dd>
Return the number of live cells in the current pattern.
</dd>

<a name="GetRule"></a><p><dt><b>GetRule()</b></dt>
<dd>
Return the current rule.
</dd>

<a name="MoveMode"></a><p><dt><b>MoveMode()</b></dt>
<dd>
Switch to the hand cursor.
</dd>

<a name="NewPattern"></a><p><dt><b>NewPattern()</b></dt>
<dd>
Create a new, empty pattern.  All undo/redo history is deleted.
</dd>

<a name="RunScript"></a><p><dt><b>RunScript(<i>filepath</i>)</b></dt>
<dd>
Run the specified .lua file.  If <i>filepath</i> is not supplied then
the user will be prompted to select a .lua file.
</dd>

<a name="SelectCell"></a><p><dt><b>SelectCell(<i>x, y, z</i>)</b></dt>
<dd>
Select the given cell.
The x,y,z coordinates are relative to the middle cell in the grid.
</dd>

<a name="SelectMode"></a><p><dt><b>SelectMode()</b></dt>
<dd>
Switch to the cross-hairs cursor and display the active plane.
</dd>

<a name="SetCell"></a><p><dt><b>SetCell(<i>x, y, z, state</i>)</b></dt>
<dd>
Set the given cell to the given state (0 or 1).
The x,y,z coordinates are relative to the middle cell in the grid.
</dd>

<a name="SetMessage"></a><p><dt><b>SetMessage(<i>msg</i>)</b></dt>
<dd>
The given string will be displayed by the next Update call.
Call SetMessage(nil) to clear the message.
</dd>

<a name="SetRule"></a><p><dt><b>SetRule(<i>rule</i>)</b></dt>
<dd>
Switch to the given rule.
</dd>

<a name="Step"></a><p><dt><b>Step()</b></dt>
<dd>
If the population is &gt; 0 then calculate the next generation.
</dd>

<a name="Update"></a><p><dt><b>Update()</b></dt>
<dd>
Update the display.  Note that 3D.lua automatically updates the display
when a script finishes, so there's no need to call Update() at the
end of a script.
</dd>

<p><a name="rules"></a><br>
<font size=+1><b>Supported Rules</b></font>

<p>
Use the "Rule..." button to change the current rule.
Rules are strings of the form "S,S,S,.../B,B,B,...".
The S values are the counts of neighboring live cells required
for a live cell to survive in the next generation.
The B values are the counts of neighboring live cells required for
birth; ie. a dead cell will become a live cell in the next generation.
Each cell has 26 neighbors so the S counts are from 0 to 26
and the B counts are from 1 to 26 (birth on 0 is not allowed).

<p><a name="files"></a><br>
<font size=+1><b>The 3D File Format</b></font>

<p>
3D.lua reads and writes 3D patterns in text files with a .3d extension.
The format of such files is very simple:

<p>
<ul>
<li>
The first line must contain "3D" and nothing else.
<li>
Then comes a number of header lines of the form "keyword=value".
The valid keywords are:

<p><table border=0 cellspacing=0 cellpadding=0>
<tr><td> version=<i>i</i> </td><td> &mdash; specifies the file format version (currently 1)</td></tr>
<tr><td> gridsize=<i>N</i> </td><td> &mdash; specifies the grid dimensions (<i>N</i>x<i>N</i>x<i>N</i>)</td></tr>
<tr><td> rule=<i>rulestring</i> </td><td> &mdash; specifies the 3D rule</td></tr>
<tr><td> generation=<i>g</i> </td><td> &mdash; specifies the generation number (usually 0)</td></tr>
<tr><td> &nbsp </td><tr>
</table>

<li>
Then comes lines with the "x y z" coordinates for each live cell in the pattern.
The coordinates are relative to the middle cell in the grid.
Multiple cells on a line are separated by commas.
The ordering of cells is essentially random.
<li>
Empty lines or lines starting with "#" are ignored.
</ul>

<p>
The following is a small example of the 3D file format.
You can either save it in a .3d file, or copy it to the clipboard
and type shift-O (after returning to the 3D.lua window):

<dd><table border=0><pre>
3D
# A 30c/30 orthogonal spaceship.
# Discovered by Andrew Trevorrow in March, 2018.
version=1
gridsize=64
rule=4,7/5,8
generation=0
0 1 -1,-1 -1 -3,-2 1 -2,-3 1 0,0 0 -1,-3 -2 -1,0 0 -2,0 -1 -1,-1 -2 -1
0 -2 -2,0 -2 -1,0 -1 0,-2 -1 1,0 0 0,-2 -3 -2,-2 -3 -1,0 -1 -2,0 0 -3
0 1 -2,-2 1 0,0 -1 -3,-3 0 -2,-2 2 -1,-1 1 -2</pre></table></dd>

<p><a name="refs"></a><br>
<font size=+1><b>Credits and References</b></font>

<p>
3D.lua was inspired by the work of Carter Bays and his colleagues:

<p>
Candidates for the Game of Life in Three Dimensions<br>
<a href="http://www.complex-systems.com/pdf/01-3-1.pdf">http://www.complex-systems.com/pdf/01-3-1.pdf</a>

<p>
Patterns for Simple Cellular Automata in a Universe of Dense-Packed Spheres<br>
<a href="http://www.complex-systems.com/pdf/01-5-1.pdf">http://www.complex-systems.com/pdf/01-5-1.pdf</a>

<p>
Classification of Semitotalistic Cellular Automata in Three Dimensions<br>
<a href="http://www.complex-systems.com/pdf/02-2-6.pdf">http://www.complex-systems.com/pdf/02-2-6.pdf</a>

<p>
A Note on the Discovery of a New Game of Three-dimensional Life<br>
<a href="http://www.complex-systems.com/pdf/02-3-1.pdf">http://www.complex-systems.com/pdf/02-3-1.pdf</a>

<p>
The Discovery of a New Glider for the Game of Three-Dimensional Life<br>
<a href="http://www.complex-systems.com/pdf/04-6-2.pdf">http://www.complex-systems.com/pdf/04-6-2.pdf</a>

<p>
Further Notes on the Game of Three-Dimensional Life<br>
<a href="http://www.complex-systems.com/pdf/08-1-4.pdf">http://www.complex-systems.com/pdf/08-1-4.pdf</a>

</body></html>
]]

    if g.os() == "Mac" then
        htmldata = htmldata:gsub(" enter ", " return ")
        htmldata = htmldata:gsub(" alt", " option")
        htmldata = htmldata:gsub("ctrl", "cmd")
    end
    
    local htmlfile = g.getdir("temp").."3D.html"
    local f = io.open(htmlfile,"w")
    if not f then
        g.warn("Failed to create 3D.html!")
        return
    end
    f:write(htmldata)
    f:close()
    
    g.open(htmlfile)
end

--------------------------------------------------------------------------------

function SetCursor(cursor)
    currcursor = cursor
    if not arrow_cursor then ov("cursor "..cursor) end
end

--------------------------------------------------------------------------------

function DrawMode()
    -- called when drawbox is clicked
    SetCursor(drawcursor)
    Refresh()
end

--------------------------------------------------------------------------------

function SelectMode()
    -- called when selectbox is clicked
    SetCursor(selectcursor)
    Refresh()
end

--------------------------------------------------------------------------------

function MoveMode()
    -- called when movebox is clicked
    SetCursor(movecursor)
    Refresh()
end

--------------------------------------------------------------------------------

function CycleCursor()
    -- cycle to next cursor mode
    if currcursor == drawcursor then
        SelectMode()
    elseif currcursor == selectcursor then
        MoveMode()
    else -- currcursor == movecursor
        DrawMode()
    end
end

----------------------------------------------------------------------

function CycleCellType()
    if celltype == "cube" then
        celltype = "sphere"
    elseif celltype == "sphere" then
        celltype = "point"
    else -- celltype == "point"
        celltype = "cube"
    end
    Refresh()
end

----------------------------------------------------------------------

function ToggleAxes()
    showaxes = not showaxes
    Refresh()
end

----------------------------------------------------------------------

function ToggleToolBar()
    if toolbarht > 0 then
        toolbarht = 0
        midy = int(ovht/2)
        -- hide all the controls
        newbutton.hide()
        openbutton.hide()
        savebutton.hide()
        runbutton.hide()
        gridbutton.hide()
        randbutton.hide()
        rulebutton.hide()
        exitbutton.hide()
        helpbutton.hide()
        ssbutton.hide()
        s1button.hide()
        resetbutton.hide()
        fitbutton.hide()
        undobutton.hide()
        redobutton.hide()
        drawbox.hide()
        selectbox.hide()
        movebox.hide()
    else
        toolbarht = buttonht*2+gap*3
        midy = int(ovht/2 + toolbarht/2)
    end
    Refresh()
end

----------------------------------------------------------------------

function ExitScript()
    g.exit()
end

----------------------------------------------------------------------

--[[ no longer used, but might come in handy

local function PointInPolygon(x, y, vertices)
    -- return true if the given mouse position is inside the given polygon
    -- (see https://stackoverflow.com/questions/31730923/check-if-point-lies-in-polygon-lua)
    local inside = false
    local n = #vertices
    local j = n
    for i = 1, n do
        local pix = projectedx[ vertices[i] ]
        local piy = projectedy[ vertices[i] ]
        local pjx = projectedx[ vertices[j] ]
        local pjy = projectedy[ vertices[j] ]
        if (piy < y and pjy >= y) or
           (pjy < y and piy >= y) then
            if pix + (y - piy) / (pjy - piy) * (pjx - pix) < x then
                inside = not inside
            end
        end
        j = i
    end
    return inside
end

--]]

----------------------------------------------------------------------

local function PointInTriangle(x, y, A, B, C)
    -- return true if x,y is inside the given triangle
    -- (see https://stackoverflow.com/questions/2049582/how-to-determine-if-a-point-is-in-a-2d-triangle)
    local ax, ay = projectedx[A], projectedy[A]
    local bx, by = projectedx[B], projectedy[B]
    local cx, cy = projectedx[C], projectedy[C]
    local as_x = x - ax
    local as_y = y - ay
    local s_ab = (bx-ax)*as_y - (by-ay)*as_x >= 0
    if (cx-ax)*as_y - (cy-ay)*as_x > 0 == s_ab then return false end
    if (cx-bx)*(y-by) - (cy-by)*(x-bx) > 0 ~= s_ab then return false end
    return true
end

----------------------------------------------------------------------

local function PointInFace(x, y, P, Q, R, S)
    -- return true if x,y is inside the given face (a parallelogram)
    return PointInTriangle(x, y, P, Q, R) or PointInTriangle(x, y, R, S, P)
end

----------------------------------------------------------------------

local function IntersectionPoint(x1,y1, x2,y2, x3,y3, x4,y4)
    -- return the intersection point of 2 line segments
    -- (see http://paulbourke.net/geometry/pointlineplane/pdb.c)
    local denom  = (y4-y3) * (x2-x1) - (x4-x3) * (y2-y1)
    local numera = (x4-x3) * (y1-y3) - (y4-y3) * (x1-x3)
    local numerb = (x2-x1) * (y1-y3) - (y2-y1) * (x1-x3)
    
    -- check if the lines are coincident
    if abs(numera) < 0.0001 and abs(numerb) < 0.0001 and abs(denom) < 0.0001 then
        return (x1 + x2) / 2, (y1 + y2) / 2
    end
    
    local mua = numera / denom
    return x1 + mua * (x2 - x1), y1 + mua * (y2 - y1)
end

----------------------------------------------------------------------

local function FindActiveCell(x, y, face)
    -- return cell coordinates of the active cell containing x,y
    -- which is somewhere inside the given face
    local mid = N//2
    local shift = N*CELLSIZE
    local hcells = N
    local vcells = N
    local xoffset = 0
    local yoffset = 0
    local zoffset = 0
    local Pv, Qv, Sv
    local Px, Py, Qx, Qy, Sx, Sy
    local Ix, Iy, Jx, Jy
    local x1, y1, x2, y2
    local A, B, a, b
    local cx, cy, cz

    local function CalculateAaBb()
        -- to find which cell contains x,y we need to calculate lengths A,a,B,b:
        -- (in this diagram hcells is 5 and vcells is 3, but for the given face
        -- the values are either 1 or N because the active plane is 1 cell thick)
        --
        --           S ___________________________________
        --            /      /      /      /      /      /
        --           /      /      /      /      /      /
        --          /______/______/______/______/______/
        --       J /------/------/------/-*x,y /      /
        --     /  /      /      /      / /    /      /
        --    /  /______/______/______/_/____/______/
        --   B //      /      /      / /    /      /
        --  / b/      /      /      / /    /      /
        -- / //______/______/______/_/____/______/
        --   P --a---                I           Q
        --     ---------A------------
        --
        Px, Py = projectedx[Pv], projectedy[Pv]
        Qx, Qy = projectedx[Qv], projectedy[Qv]
        Sx, Sy = projectedx[Sv], projectedy[Sv]
        
        -- find intersection point of PQ and line containing x,y parallel to PS
        -- (we must ensure x1,y1 and x2,y2 are outside the face's edges)
        x1 = x - (Sx - Px) * shift
        y1 = y - (Sy - Py) * shift
        x2 = x + (Sx - Px) * shift
        y2 = y + (Sy - Py) * shift
        Ix, Iy = IntersectionPoint(Px, Py, Qx, Qy, x1, y1, x2, y2)
        
        -- find intersection point of PS and line containing x,y parallel to PQ
        -- (we must ensure x1,y1 and x2,y2 are outside the face's edges)
        x1 = x - (Qx - Px) * shift
        y1 = y - (Qy - Py) * shift
        x2 = x + (Qx - Px) * shift
        y2 = y + (Qy - Py) * shift
        Jx, Jy = IntersectionPoint(Px, Py, Sx, Sy, x1, y1, x2, y2)
        
        A = sqrt( (Ix-Px)^2 + (Iy-Py)^2 )
        B = sqrt( (Jx-Px)^2 + (Jy-Py)^2 )
        a = sqrt( (Qx-Px)^2 + (Qy-Py)^2 ) / hcells
        b = sqrt( (Sx-Px)^2 + (Sy-Py)^2 ) / vcells
    end

    if activeplane == "XY" then
        -- F and B faces have N*N cells, the other faces (TULR) have N cells;
        -- we only need to use 3 vertices in each face, but they have to be
        -- chosen carefully so that the vectors PQ and PS span the parallelogram
        if face == "F" then
            Pv, Qv, Sv = 1, 7, 3    -- front vertices of active plane
        elseif face == "B" then
            Pv, Qv, Sv = 2, 8, 4    -- back vertices
        elseif face == "T" then
            Pv, Qv, Sv = 3, 5, 4    -- top vertices
            vcells = 1
            yoffset = N-1
        elseif face == "U" then
            Pv, Qv, Sv = 1, 7, 2    -- underneath (ie. bottom) vertices
            vcells = 1
        elseif face == "L" then
            Pv, Qv, Sv = 1, 2, 3    -- left vertices
            hcells = 1
        elseif face == "R" then     
            Pv, Qv, Sv = 7, 8, 5    -- right vertices
            hcells = 1
            xoffset = N-1
        end
        CalculateAaBb()
        cx = floor(A / a) + xoffset; if cx >= N then cx = N-1 end
        cy = floor(B / b) + yoffset; if cy >= N then cy = N-1 end
        cx = cx - mid
        cy = cy - mid
        cz = activepos
        
    elseif activeplane == "YZ" then
        -- L and R faces have N*N cells, the other faces (TUFB) have N cells
        if face == "L" then
            Pv, Qv, Sv = 2, 1, 4    -- left vertices
        elseif face == "R" then     
            Pv, Qv, Sv = 8, 7, 6    -- right vertices
        elseif face == "T" then
            Pv, Qv, Sv = 6, 5, 4    -- top vertices
            vcells = 1
            yoffset = N-1
        elseif face == "U" then
            Pv, Qv, Sv = 8, 7, 2    -- underneath (ie. bottom) vertices
            vcells = 1
        elseif face == "F" then
            Pv, Qv, Sv = 1, 7, 3    -- front vertices
            hcells = 1
            zoffset = N-1
        elseif face == "B" then
            Pv, Qv, Sv = 2, 8, 4    -- back vertices
            hcells = 1
        end
        CalculateAaBb()
        cy = floor(B / b) + yoffset; if cy >= N then cy = N-1 end
        cz = floor(A / a) + zoffset; if cz >= N then cz = N-1 end
        cy = cy - mid
        cz = cz - mid
        cx = activepos
        
    else -- activeplane == "XZ"
        -- T and U faces have N*N cells, the other faces (FBLR) have N cells
        if face == "T" then
            Pv, Qv, Sv = 4, 6, 3    -- top vertices
        elseif face == "U" then
            Pv, Qv, Sv = 2, 8, 1    -- underneath (ie. bottom) vertices
        elseif face == "F" then
            Pv, Qv, Sv = 1, 7, 3    -- front vertices
            vcells = 1
            zoffset = N-1
        elseif face == "B" then
            Pv, Qv, Sv = 4, 6, 2    -- back vertices
            vcells = 1
        elseif face == "L" then
            Pv, Qv, Sv = 4, 2, 3    -- left vertices
            hcells = 1
        elseif face == "R" then     
            Pv, Qv, Sv = 6, 8, 5    -- right vertices
            hcells = 1
            xoffset = N-1
        end
        CalculateAaBb()
        cx = floor(A / a) + xoffset; if cx >= N then cx = N-1 end
        cz = floor(B / b) + zoffset; if cz >= N then cz = N-1 end
        cx = cx - mid
        cz = cz - mid
        cy = activepos
    end
    
    -- return user coordinates (displayed later by Refresh)
    return cx, cy, cz
end

----------------------------------------------------------------------

function FindFace(mousex, mousey, box)
    -- create given box's rotated and projected vertices
    for i = 1, 8 do
        rotx[i], roty[i], rotz[i] = TransformPoint(box[i])
        projectedx[i] = round( rotx[i] ) + midx
        projectedy[i] = round( roty[i] ) + midy
    end
    
    -- find which face of given box contains mousex,mousey (if any);
    -- note that up to 3 faces (all parallelograms) are visible
    local face = ""
    if rotz[1] < rotz[2] then
        -- front face is visible
        if PointInFace(mousex, mousey, 1,3,5,7) then face = "F" end
    else
        -- back face is visible
        if PointInFace(mousex, mousey, 8,6,4,2) then face = "B" end
    end
    if #face == 0 then
        -- not in front/back face, so try right and left
        if rotz[5] < rotz[3] then
            -- right face is visible
            if PointInFace(mousex, mousey, 5,6,8,7) then face = "R" end
        else
            -- left face is visible
            if PointInFace(mousex, mousey, 1,2,4,3) then face = "L" end
        end
    end
    if #face == 0 then
        -- not in front/back/right/left so try top and bottom
        if rotz[5] < rotz[7] then
            -- top face is visible
            if PointInFace(mousex, mousey, 5,3,4,6) then face = "T" end
        else
            -- bottom face is visible (use U for underneath; B is for back)
            if PointInFace(mousex, mousey, 1,7,8,2) then face = "U" end
        end
    end
    return face     -- empty/F/B/R/L/T/U
end

----------------------------------------------------------------------

local function InsideActiveCell(mousex, mousey)
    -- if the given mouse position is inside the active plane then
    -- return a string containing the cell coords in the format "x,y,z"
    -- otherwise return an empty string

    -- create a box enclosing all active cells using same vertex order as CreateCube
    local x = minactivex * CELLSIZE - MIDGRID
    local y = minactivey * CELLSIZE - MIDGRID
    local z = minactivez * CELLSIZE - MIDGRID
    local xlen = (maxactivex - minactivex + 1) * CELLSIZE
    local ylen = (maxactivey - minactivey + 1) * CELLSIZE
    local zlen = (maxactivez - minactivez + 1) * CELLSIZE
    local activebox = {
        {x     , y     , z+zlen},     -- v1
        {x     , y     , z     },     -- v2
        {x     , y+ylen, z+zlen},     -- v3
        {x     , y+ylen, z     },     -- v4
        {x+xlen, y+ylen, z+zlen},     -- v5
        {x+xlen, y+ylen, z     },     -- v6
        {x+xlen, y     , z+zlen},     -- v7
        {x+xlen, y     , z     }      -- v8
    }

    -- test if mousex,mousey is inside a visible face of activebox
    local face = FindFace(mousex, mousey, activebox)

    if #face > 0 then
        -- determine which active cell contains mousex,mousey
        local cx, cy, cz = FindActiveCell(mousex, mousey, face)
        return cx..","..cy..","..cz
    else
        return ""
    end
end

----------------------------------------------------------------------

function StartDrawing(mousex, mousey)
    local oldcell = activecell
    activecell = InsideActiveCell(mousex, mousey)
    if #activecell > 0 then
        -- toggle the state of the clicked cell
        local x, y, z = split(activecell, ",")
        local mid = N//2
        x = tonumber(x) + mid
        y = tonumber(y) + mid
        z = tonumber(z) + mid
        local pos = x + N * (y + N * z)
        if grid1[pos] then
            -- death
            grid1[pos] = nil
            popcount = popcount - 1
            minimal_live_bounds = false
            drawstate = 0
        else
            -- birth
            SetLiveCell(x, y, z)
            drawstate = 1
        end
        Refresh()
        prevactive = activecell
        return true
    else
        if activecell ~= oldcell then Refresh() end
        return false
    end
end

----------------------------------------------------------------------

function StartSelecting(mousex, mousey)
    local oldcell = activecell
    activecell = InsideActiveCell(mousex, mousey)
    if #activecell > 0 then
        -- toggle the selection state of the clicked cell
        local x, y, z = split(activecell, ",")
        local mid = N//2
        x = tonumber(x) + mid
        y = tonumber(y) + mid
        z = tonumber(z) + mid
        local pos = x + N * (y + N * z)
        if selected[pos] then
            -- deselect
            selected[pos] = nil
            selcount = selcount - 1
            minimal_sel_bounds = false
            selstate = false
        else
            selected[pos] = true
            selcount = selcount + 1
            UpdateSelectionBoundary(x, y, z)
            selstate = true
        end
        Refresh()
        prevactive = activecell
        return true
    else
        if activecell ~= oldcell then Refresh() end
        return false
    end
end

----------------------------------------------------------------------

function SetLine(x1, y1, z1, x2, y2, z2, setfunction, state)
    -- draw/erase/select/deselect a line of cells from x1,y1,z1 to x2,y2,z2
    -- using a 3D version of Bresenham's algorithm
    -- (note that x1,y1,z1 has already been set and x2,y2,z2 is a different cell)
    local dx = x2 - x1
    local dy = y2 - y1
    local dz = z2 - z1
    local xinc = 1; if dx < 0 then xinc = -1 end
    local yinc = 1; if dy < 0 then yinc = -1 end
    local zinc = 1; if dz < 0 then zinc = -1 end
    local ax = abs(dx)
    local ay = abs(dy)
    local az = abs(dz)
    local dx2 = ax * 2
    local dy2 = ay * 2
    local dz2 = az * 2
    
    if ax >= ay and ax >= az then
        local e1 = dy2 - ax
        local e2 = dz2 - ax
        while x1 ~= x2 do
            setfunction(x1, y1, z1, state)
            if e1 > 0 then
                y1 = y1 + yinc
                e1 = e1 - dx2
            end
            if e2 > 0 then
                z1 = z1 + zinc
                e2 = e2 - dx2
            end
            e1 = e1 + dy2
            e2 = e2 + dz2
            x1 = x1 + xinc
        end
    elseif ay >= ax and ay >= az then
        local e1 = dx2 - ay
        local e2 = dz2 - ay
        while y1 ~= y2 do
            setfunction(x1, y1, z1, state)
            if e1 > 0 then
                x1 = x1 + xinc
                e1 = e1 - dy2
            end
            if e2 > 0 then
                z1 = z1 + zinc
                e2 = e2 - dy2
            end
            e1 = e1 + dx2
            e2 = e2 + dz2
            y1 = y1 + yinc
        end
    else
        local e1 = dy2 - az
        local e2 = dx2 - az
        while z1 ~= z2 do
            setfunction(x1, y1, z1, state)
            if e1 > 0 then
                y1 = y1 + yinc
                e1 = e1 - dz2
            end
            if e2 > 0 then
                x1 = x1 + xinc
                e2 = e2 - dz2
            end
            e1 = e1 + dy2
            e2 = e2 + dx2
            z1 = z1 + zinc
        end
    end
    setfunction(x1, y1, z1, state)
end

----------------------------------------------------------------------

function DrawCells(mousex, mousey)
    -- draw/erase cells in active plane
    local oldcell = activecell
    activecell = InsideActiveCell(mousex, mousey)
    if #activecell > 0 and activecell ~= prevactive then
        -- mouse has moved to a different cell
        local mid = N//2
        local x, y, z = split(activecell, ",")
        x = tonumber(x) + mid
        y = tonumber(y) + mid
        z = tonumber(z) + mid
        -- draw/erase a line of cells from prevactive to activecell
        local prevx, prevy, prevz = split(prevactive, ",")
        prevx = tonumber(prevx) + mid
        prevy = tonumber(prevy) + mid
        prevz = tonumber(prevz) + mid
        SetLine(prevx, prevy, prevz, x, y, z, SetCellState, drawstate)
        Refresh()
        prevactive = activecell
    else
        if activecell ~= oldcell then Refresh() end
    end
end

----------------------------------------------------------------------

function SelectCells(mousex, mousey)
    -- select/deselect cells in active plane
    local oldcell = activecell
    activecell = InsideActiveCell(mousex, mousey)
    if #activecell > 0 and activecell ~= prevactive then
        -- mouse has moved to a different cell
        local x, y, z = split(activecell, ",")
        local mid = N//2
        x = tonumber(x) + mid
        y = tonumber(y) + mid
        z = tonumber(z) + mid
        -- select/deselect a line of cells from prevactive to activecell
        local prevx, prevy, prevz = split(prevactive, ",")
        prevx = tonumber(prevx) + mid
        prevy = tonumber(prevy) + mid
        prevz = tonumber(prevz) + mid
        SetLine(prevx, prevy, prevz, x, y, z, SetSelection, selstate)
        Refresh()
        prevactive = activecell
    else
        if activecell ~= oldcell then Refresh() end
    end
end

----------------------------------------------------------------------

function StopDrawing()
    -- add cell changes to undo list
    --!!!
end

----------------------------------------------------------------------

function StopSelecting()
    -- add selection changes to undo list
    --!!!
end

----------------------------------------------------------------------

function StartDraggingPaste(mousex, mousey)
    -- test if mouse click is in a paste cell
    local NN = N*N
    for k,_ in pairs(pastecell) do
        local px, py = GetMidPoint(k%N, (k//N)%N, k//NN)
        if abs(px - mousex) < HALFCELL and
           abs(py - mousey) < HALFCELL then
            -- create a box enclosing all paste cells using same vertex order as CreateCube
            local x = minpastex * CELLSIZE - MIDGRID
            local y = minpastey * CELLSIZE - MIDGRID
            local z = minpastez * CELLSIZE - MIDGRID
            local xlen = (maxpastex - minpastex + 1) * CELLSIZE
            local ylen = (maxpastey - minpastey + 1) * CELLSIZE
            local zlen = (maxpastez - minpastez + 1) * CELLSIZE
            local pastebox = {
                {x     , y     , z+zlen},     -- v1
                {x     , y     , z     },     -- v2
                {x     , y+ylen, z+zlen},     -- v3
                {x     , y+ylen, z     },     -- v4
                {x+xlen, y+ylen, z+zlen},     -- v5
                {x+xlen, y+ylen, z     },     -- v6
                {x+xlen, y     , z+zlen},     -- v7
                {x+xlen, y     , z     }      -- v8
            }
            -- return the face containing mousex,mousey
            return FindFace(mousex, mousey, pastebox)
        end
    end
    return ""
end

----------------------------------------------------------------------

function MovePastePattern(deltax, deltay, deltaz)
    -- move the paste pattern by the given amounts, if possible
    if minpastex + deltax < 0 then deltax = -minpastex end
    if minpastey + deltay < 0 then deltay = -minpastey end
    if minpastez + deltaz < 0 then deltaz = -minpastez end
    if maxpastex + deltax >= N then deltax = N-1 - maxpastex end
    if maxpastey + deltay >= N then deltay = N-1 - maxpastey end
    if maxpastez + deltaz >= N then deltaz = N-1 - maxpastez end
    
    if deltax == 0 and deltay == 0 and deltaz == 0 then return end

    minpastex = minpastex + deltax
    minpastey = minpastey + deltay
    minpastez = minpastez + deltaz
    maxpastex = maxpastex + deltax
    maxpastey = maxpastey + deltay
    maxpastez = maxpastez + deltaz
    
    local pcells = GetPasteCells()
    local mid = N//2
    pastecell = {}
    for _, xyz in ipairs(pcells) do
        local x = xyz[1] + mid + deltax
        local y = xyz[2] + mid + deltay
        local z = xyz[3] + mid + deltaz
        pastecell[ x + N * (y + N * z) ] = true
    end
    
    Refresh()
end

----------------------------------------------------------------------

function DragPaste(mousex, mousey, prevx, prevy, face)
    -- create a large temporary active plane parallel to given face
    local oldN = N
    local oldplane = activeplane
    local oldpos = activepos
    N = N*3
    MIDGRID = (N+1-(N%2))*HALFCELL
    if face == "F" or face == "B" then
        -- mouse in front/back face
        SetActivePlane("XY", 0)
    elseif face == "T" or face == "U" then
        -- mouse in top/bottom face
        SetActivePlane("XZ", 0)
    else
        -- mouse in left/right face
        SetActivePlane("YZ", 0)
    end
    
    -- find the cell locations of mousex,mousey and prevx,prevy in the temporary plane
    local oldcell = InsideActiveCell(prevx, prevy)
    local newcell = InsideActiveCell(mousex, mousey)
    
    -- restore the original active plane
    N = oldN
    MIDGRID = (N+1-(N%2))*HALFCELL
    SetActivePlane(oldplane, oldpos)
    
    -- check if mouse stayed in same cell, or moved outside temporary plane
    if oldcell == newcell or #oldcell == 0 or #newcell == 0 then
        return
    end
    
    -- calculate how many cells the mouse has moved
    local oldx, oldy, oldz = split(oldcell,",")
    local newx, newy, newz = split(newcell,",")
    local deltax = tonumber(newx) - tonumber(oldx)
    local deltay = tonumber(newy) - tonumber(oldy)
    local deltaz = tonumber(newz) - tonumber(oldz)
    
    MovePastePattern(deltax, deltay, deltaz)
end

----------------------------------------------------------------------

local save_cells    -- for restoring live cells under a moving selection
local selxyz        -- store x,y,z positions of selected cells
local livexyz       -- store x,y,z positions of selected live cells

function StartDraggingSelection(mousex, mousey)
    -- test if mouse click is in a selected cell
    MinimizeSelectionBoundary()
    local NN = N*N
    for k,_ in pairs(selected) do
        local px, py = GetMidPoint(k%N, (k//N)%N, k//NN)
        if abs(px - mousex) < HALFCELL and
           abs(py - mousey) < HALFCELL then
            -- create a box enclosing all selected cells using same vertex order as CreateCube
            local x = minselx * CELLSIZE - MIDGRID
            local y = minsely * CELLSIZE - MIDGRID
            local z = minselz * CELLSIZE - MIDGRID
            local xlen = (maxselx - minselx + 1) * CELLSIZE
            local ylen = (maxsely - minsely + 1) * CELLSIZE
            local zlen = (maxselz - minselz + 1) * CELLSIZE
            local selbox = {
                {x     , y     , z+zlen},     -- v1
                {x     , y     , z     },     -- v2
                {x     , y+ylen, z+zlen},     -- v3
                {x     , y+ylen, z     },     -- v4
                {x+xlen, y+ylen, z+zlen},     -- v5
                {x+xlen, y+ylen, z     },     -- v6
                {x+xlen, y     , z+zlen},     -- v7
                {x+xlen, y     , z     }      -- v8
            }
            local face = FindFace(mousex, mousey, selbox)
            if #face > 0 then
                -- initialize save_cells, selxyz and livexyz for use in MoveSelection
                save_cells = {}
                selxyz = {}
                livexyz = {}
                for k,_ in pairs(selected) do
                    -- selected[k] is a selected cell
                    local x = k % N
                    local y = (k // N) % N
                    local z = k // NN
                    selxyz[#selxyz+1] = {x, y, z}
                    if grid1[k] then
                        -- selected cell is a live cell
                        livexyz[#livexyz+1] = {x, y, z}
                    end
                end
            end
            return face
        end
    end
    return ""
end

----------------------------------------------------------------------

function MoveSelection(deltax, deltay, deltaz)
    -- move all selected cells by the given amounts, if possible
    if minselx + deltax < 0 then deltax = -minselx end
    if minsely + deltay < 0 then deltay = -minsely end
    if minselz + deltaz < 0 then deltaz = -minselz end
    if maxselx + deltax >= N then deltax = N-1 - maxselx end
    if maxsely + deltay >= N then deltay = N-1 - maxsely end
    if maxselz + deltaz >= N then deltaz = N-1 - maxselz end
    
    if deltax == 0 and deltay == 0 and deltaz == 0 then return end

    minselx = minselx + deltax
    minsely = minsely + deltay
    minselz = minselz + deltaz
    maxselx = maxselx + deltax
    maxsely = maxsely + deltay
    maxselz = maxselz + deltaz
    
    -- kill all live cells in the current selection
    local savepop = popcount
    local NN = N*N
    for k,_ in pairs(selected) do
        if grid1[k] then
            grid1[k] = nil
            popcount = popcount - 1
            minimal_live_bounds = false
        end
    end
    
    if popcount == 0 and selcount == savepop then
        -- all live cells (and only those cells) were selected
        -- so we can handle this common case much faster by simply
        -- putting the selected cells in their new positions
        selected = {}
        for i, xyz in ipairs(livexyz) do
            local x = xyz[1] + deltax
            local y = xyz[2] + deltay
            local z = xyz[3] + deltaz
            local k = x + N * (y + N * z)
            selected[k] = true
            grid1[k] = 1
            livexyz[i] = {x, y, z}
        end
        popcount = savepop
        
        -- new live boundary is same as new selection boundary
        -- (and minimal because StartDraggingSelection called MinimizeSelectionBoundary)
        minx = minselx
        miny = minsely
        minz = minselz
        maxx = maxselx
        maxy = maxsely
        maxz = maxselz
        minimal_live_bounds = true
    else
        -- avoid modifying any live cells under the moving selection
        -- by restoring the live cells in save_cells (if any)
        for k,_ in pairs(save_cells) do
            if not grid1[k] then
                grid1[k] = 1
                popcount = popcount + 1
                -- boundary might expand
                local x = k % N
                local y = (k // N) % N
                local z = k // NN
                if x < minx then minx = x end
                if y < miny then miny = y end
                if z < minz then minz = z end
                if x > maxx then maxx = x end
                if y > maxy then maxy = y end
                if z > maxz then maxz = z end
            end
        end
        
        -- move the selected cells to their new positions
        -- and save any live cells in the new selection in save_cells
        save_cells = {}
        selected = {}
        for i, xyz in ipairs(selxyz) do
            local x = xyz[1] + deltax
            local y = xyz[2] + deltay
            local z = xyz[3] + deltaz
            local k = x + N * (y + N * z)
            selected[k] = true
            selxyz[i] = {x, y, z}
            if grid1[k] then save_cells[k] = true end
        end
        
        -- put live cells saved in livexyz into their new positions
        for i, xyz in ipairs(livexyz) do
            local x = xyz[1] + deltax
            local y = xyz[2] + deltay
            local z = xyz[3] + deltaz
            local k = x + N * (y + N * z)
            if not grid1[k] then
                grid1[k] = 1
                popcount = popcount + 1
                -- boundary might expand
                if x < minx then minx = x end
                if y < miny then miny = y end
                if z < minz then minz = z end
                if x > maxx then maxx = x end
                if y > maxy then maxy = y end
                if z > maxz then maxz = z end
            end
            livexyz[i] = {x, y, z}
        end
    end
    
    Refresh()
end

----------------------------------------------------------------------

function DragSelection(mousex, mousey, prevx, prevy, face)
    -- create a large temporary active plane parallel to given face
    local oldN = N
    local oldplane = activeplane
    local oldpos = activepos
    N = N*3
    MIDGRID = (N+1-(N%2))*HALFCELL
    if face == "F" or face == "B" then
        -- mouse in front/back face
        SetActivePlane("XY", 0)
    elseif face == "T" or face == "U" then
        -- mouse in top/bottom face
        SetActivePlane("XZ", 0)
    else
        -- mouse in left/right face
        SetActivePlane("YZ", 0)
    end
    
    -- find the cell locations of mousex,mousey and prevx,prevy in the temporary plane
    local oldcell = InsideActiveCell(prevx, prevy)
    local newcell = InsideActiveCell(mousex, mousey)
    
    -- restore the original active plane
    N = oldN
    MIDGRID = (N+1-(N%2))*HALFCELL
    SetActivePlane(oldplane, oldpos)
    
    -- check if mouse stayed in same cell, or moved outside temporary plane
    if oldcell == newcell or #oldcell == 0 or #newcell == 0 then
        return
    end
    
    -- calculate how many cells the mouse has moved
    local oldx, oldy, oldz = split(oldcell,",")
    local newx, newy, newz = split(newcell,",")
    local deltax = tonumber(newx) - tonumber(oldx)
    local deltay = tonumber(newy) - tonumber(oldy)
    local deltaz = tonumber(newz) - tonumber(oldz)
    
    MoveSelection(deltax, deltay, deltaz)
end

----------------------------------------------------------------------

function StopDraggingSelection()
    save_cells = {}
    selxyz = {}
    livexyz = {}
end

----------------------------------------------------------------------

function StartDraggingPlane(mousex, mousey)
    local oldcell = activecell
    activecell = InsideActiveCell(mousex, mousey)
    if activecell ~= oldcell then Refresh() end
    return #activecell > 0
end

----------------------------------------------------------------------

function DragActivePlane(mousex, mousey, prevx, prevy)
    -- create a box enclosing all active cells using same vertex order as CreateCube
    local x = minactivex * CELLSIZE - MIDGRID
    local y = minactivey * CELLSIZE - MIDGRID
    local z = minactivez * CELLSIZE - MIDGRID
    local xlen = (maxactivex - minactivex + 1) * CELLSIZE
    local ylen = (maxactivey - minactivey + 1) * CELLSIZE
    local zlen = (maxactivez - minactivez + 1) * CELLSIZE
    local activebox = {
        {x     , y     , z+zlen},     -- v1
        {x     , y     , z     },     -- v2
        {x     , y+ylen, z+zlen},     -- v3
        {x     , y+ylen, z     },     -- v4
        {x+xlen, y+ylen, z+zlen},     -- v5
        {x+xlen, y+ylen, z     },     -- v6
        {x+xlen, y     , z+zlen},     -- v7
        {x+xlen, y     , z     }      -- v8
    }

    -- create activebox's rotated vertices
    for i = 1, 8 do
        rotx[i], roty[i], rotz[i] = TransformPoint(activebox[i])
    end
    
    -- create a temporary active plane perpendicular to the nearest thin face
    -- and 3 times larger (to ensure all of the real active plane is enclosed)
    local oldN = N
    local oldplane = activeplane
    local oldpos = activepos
    N = N*3
    MIDGRID = (N+1-(N%2))*HALFCELL
    if activeplane == "XY" then
        if (rotz[5] <= rotz[1] and rotz[5] <= rotz[3] and rotz[7] <= rotz[3]) or
           (rotz[4] <= rotz[8] and rotz[4] <= rotz[6] and rotz[2] <= rotz[6]) then
            -- right/left face is nearest
            SetActivePlane("YZ", 0)
        else
            -- top/bottom face is nearest
            SetActivePlane("XZ", 0)
        end
    elseif activeplane == "YZ" then
        if (rotz[5] <= rotz[8] and rotz[5] <= rotz[6] and rotz[7] <= rotz[6]) or
           (rotz[4] <= rotz[1] and rotz[4] <= rotz[3] and rotz[2] <= rotz[3]) then
            -- front/back face is nearest
            SetActivePlane("XY", 0)
        else
            -- top/bottom face is nearest
            SetActivePlane("XZ", 0)
        end
    else -- activeplane == "XZ"
        if (rotz[5] <= rotz[4] and rotz[5] <= rotz[6] and rotz[3] <= rotz[6]) or
           (rotz[4] <= rotz[5] and rotz[4] <= rotz[3] and rotz[6] <= rotz[3]) then
            -- front/back face is nearest
            SetActivePlane("XY", 0)
        else
            -- left/right face is nearest
            SetActivePlane("YZ", 0)
        end
    end
    
    -- find the cell locations of mousex,mousey and prevx,prevy in the temporary plane
    local oldcell = InsideActiveCell(prevx, prevy)
    local newcell = InsideActiveCell(mousex, mousey)
    
    -- restore the original active plane
    N = oldN
    MIDGRID = (N+1-(N%2))*HALFCELL
    SetActivePlane(oldplane, oldpos)
    
    -- check if mouse stayed in same cell, or moved outside temporary plane
    if oldcell == newcell or #oldcell == 0 or #newcell == 0 then
        return
    end
    
    -- calculate how many cells the mouse has moved
    local oldx, oldy, oldz = split(oldcell,",")
    local newx, newy, newz = split(newcell,",")
    local deltax = tonumber(newx) - tonumber(oldx)
    local deltay = tonumber(newy) - tonumber(oldy)
    local deltaz = tonumber(newz) - tonumber(oldz)
    
    -- move the active plane by the appropriate delta but don't call Refresh yet
    if activeplane == "XY" then
        MoveActivePlane(activepos + deltaz, false)
    elseif activeplane == "YZ" then
        MoveActivePlane(activepos + deltax, false)
    else -- activeplane == "XZ"
        MoveActivePlane(activepos + deltay, false)
    end
    
    activecell = InsideActiveCell(mousex, mousey)
    Refresh()
end

----------------------------------------------------------------------

function EraseLiveCells(mousex, mousey)
    -- erase all live cells whose projected mid points are close to mousex,mousey
    if popcount > 0 then
        local changes = 0
        local NN = N*N
        for k,_ in pairs(grid1) do
            -- grid1[k] is a live cell
            local x = k % N
            local y = (k // N) % N
            local z = k // NN
            local px, py = GetMidPoint(x, y, z)
            if abs(px - mousex) < HALFCELL and
               abs(py - mousey) < HALFCELL then
                grid1[k] = nil
                popcount = popcount - 1
                minimal_live_bounds = false
                changes = changes + 1
            end
        end
        if changes > 0 then Refresh() end
    end
end

----------------------------------------------------------------------

function SelectLiveCells(mousex, mousey)
    -- select all live cells whose projected mid points are close to mousex,mousey
    if popcount > 0 then
        local changes = 0
        local NN = N*N
        for k,_ in pairs(grid1) do
            -- grid1[k] is a live cell
            local x = k % N
            local y = (k // N) % N
            local z = k // NN
            local px, py = GetMidPoint(x, y, z)
            if abs(px - mousex) < HALFCELL and
               abs(py - mousey) < HALFCELL then
                if not selected[k] then
                    selected[k] = true
                    selcount = selcount + 1
                    UpdateSelectionBoundary(x, y, z)
                    changes = changes + 1
                end
            end
        end
        if changes > 0 then Refresh() end
    end
end

----------------------------------------------------------------------

function CreateOverlay()
    -- overlay covers entire viewport (more if viewport is too small)
    viewwd, viewht = g.getview(g.getlayer())
    ovwd, ovht = viewwd, viewht
    if ovwd < minwd then ovwd = minwd end
    if ovht < minht then ovht = minht end
    midx = int(ovwd/2)
    midy = int(ovht/2 + toolbarht/2)
    ov("create "..ovwd.." "..ovht)
    ov("cursor "..currcursor)

    -- for info text
    ov("font 11 default-bold")
    ov("textoption background "..BACK_COLOR)

    -- parameters for tool bar buttons
    op.buttonht = buttonht
    op.textgap = 8                          -- gap between edge of button and its label
    op.textfont = "font 10 default-bold"    -- font for labels
    op.textshadowx = 2
    op.textshadowy = 2
    if g.os() == "Mac" then op.yoffset = -1 end
    if g.os() == "Linux" then op.textfont = "font 10 default" end

    -- fix m.button code in oplus/init.lua to avoid need to do this!!!
    local oldbg = ov("textoption background 0 0 0 0")
    
    -- create tool bar buttons
    newbutton = op.button("New", NewPattern)
    openbutton = op.button("Open...", OpenPattern)
    savebutton = op.button("Save...", SavePattern)
    runbutton = op.button("Run...", RunScript)
    gridbutton = op.button("Grid...", ChangeGridSize)
    randbutton = op.button("Random...", RandomPattern)
    rulebutton = op.button("Rule...", ChangeRule)
    ssbutton = op.button("Start", StartStop)
    s1button = op.button("+1", Step1)
    resetbutton = op.button("Reset", Reset)
    fitbutton = op.button("Fit", FitGrid)
    undobutton = op.button("Undo", Undo)
    redobutton = op.button("Redo", Redo)
    helpbutton = op.button("?", ShowHelp)
    exitbutton = op.button("X", ExitScript)
    
    -- create check boxes (don't shadow text)
    op.textshadowx = 0
    op.textshadowy = 0
    drawbox = op.checkbox("Draw", op.black, DrawMode)
    selectbox = op.checkbox("Select", op.black, SelectMode)
    movebox = op.checkbox("Move", op.black, MoveMode)
    op.textshadowx = 2
    op.textshadowy = 2
    
    ov("textoption background "..oldbg) -- see above!!!
    
    -- create pop-up menu for paste actions (eventually from op!!!)
    pastemenu = popupmenu()
    pastemenu.additem("Paste OR", PasteOR)
    pastemenu.additem("Paste XOR", PasteXOR)
    pastemenu.additem("---", nil)
    pastemenu.additem("Flip X Coords", FlipPasteX)
    pastemenu.additem("Flip Y Coords", FlipPasteY)
    pastemenu.additem("Flip Z Coords", FlipPasteZ)
    pastemenu.additem("---", nil)
    pastemenu.additem("Rotate X Axis", RotatePasteX)
    pastemenu.additem("Rotate Y Axis", RotatePasteY)
    pastemenu.additem("Rotate Z Axis", RotatePasteZ)
    pastemenu.additem("---", nil)
    pastemenu.additem("Cancel Paste", CancelPaste)
    
    -- create pop-up menu for selection actions (eventually from op!!!)
    selmenu = popupmenu()
    selmenu.additem("Cut", CutSelection)
    selmenu.additem("Copy", CopySelection)
    selmenu.additem("Clear", ClearSelection)
    selmenu.additem("Clear Outside", ClearOutside)
    selmenu.additem("---", nil)
    selmenu.additem("Flip X Coords", FlipSelectionX)
    selmenu.additem("Flip Y Coords", FlipSelectionY)
    selmenu.additem("Flip Z Coords", FlipSelectionZ)
    selmenu.additem("---", nil)
    selmenu.additem("Rotate X Axis", RotateSelectionX)
    selmenu.additem("Rotate Y Axis", RotateSelectionY)
    selmenu.additem("Rotate Z Axis", RotateSelectionZ)
    selmenu.additem("---", nil)
    selmenu.additem("Remove Selection", RemoveSelection)
end

----------------------------------------------------------------------

local showtoolbar = false   -- restore tool bar?

function CheckLayerSize()
    -- if viewport size has changed then resize the overlay
    local newwd, newht = g.getview(g.getlayer())
    if newwd ~= viewwd or newht ~= viewht then
        viewwd, viewht = newwd, newht
        ovwd, ovht = viewwd, viewht
        if ovwd < minwd then ovwd = minwd end
        if ovht < minht then ovht = minht end
        local fullscreen = g.getoption("fullscreen")
        if fullscreen == 1 and toolbarht > 0 then
            -- hide tool bar but restore it when we exit full screen mode
            toolbarht = 0
            showtoolbar = true
        elseif fullscreen == 0 and showtoolbar then
            if toolbarht == 0 then
                -- restore tool bar
                toolbarht = buttonht*2+gap*3
            end
            showtoolbar = false
        end
        midx = int(ovwd/2)
        midy = int(ovht/2 + toolbarht/2)
        ov("resize "..ovwd.." "..ovht)
        Refresh()
    end
end

----------------------------------------------------------------------

function CheckCursor(xy)
    local editing = currcursor ~= movecursor
    if #xy > 0 then
        -- update cursor if mouse moves in/out of tool bar
        local x, y = split(xy)
        x = tonumber(x)
        y = tonumber(y)
        if y < toolbarht then
            if not arrow_cursor then
                -- mouse moved inside tool bar
                ov("cursor arrow")
                arrow_cursor = true
                if #activecell > 0 and editing then
                    activecell = ""
                    Refresh()
                end
            end
        else
            if arrow_cursor then
                -- mouse moved outside tool bar
                ov("cursor "..currcursor)
                arrow_cursor = false
            end
            if editing then
                local oldcell = activecell
                activecell = InsideActiveCell(x, y)
                if activecell ~= oldcell then Refresh() end
            end
        end
    elseif #activecell > 0 and editing then
        activecell = ""
        Refresh()
    end
end

----------------------------------------------------------------------

function InitialView()
    -- initialize our transformation matrix
    xixo = 1.0; yixo = 0.0; zixo = 0.0
    xiyo = 0.0; yiyo = 1.0; ziyo = 0.0
    xizo = 0.0; yizo = 0.0; zizo = 1.0

    -- rotate so all axes are behind pattern but don't call Refresh
    Rotate(160, 45, 0, false)
    
    FitGrid()   -- calls Refresh
end

----------------------------------------------------------------------

function Initialize()
    CreateOverlay()
    CreateAxes()
    
    -- create reference cube (never displayed)
    refcube = CreateCube(0,0,0)
    
    ClearCells()
    if rulestring == DEFAULT_RULE then
        -- initial pattern is the Life-like glider in rule 5,6,7/6
        local mid = N//2
        SetLiveCell(mid,   mid,   mid)
        SetLiveCell(mid-1, mid,   mid)
        SetLiveCell(mid-2, mid,   mid)
        SetLiveCell(mid,   mid+1, mid)
        SetLiveCell(mid-1, mid+2, mid)
        SetLiveCell(mid,   mid,   mid-1)
        SetLiveCell(mid-1, mid,   mid-1)
        SetLiveCell(mid-2, mid,   mid-1)
        SetLiveCell(mid,   mid+1, mid-1)
        SetLiveCell(mid-1, mid+2, mid-1)
    else
        --[[ do a random fill??? or let user decide via startup script???
        local M = N-1
        for z = 0, M do
            for y = 0, M do
                for x = 0, M do
                    if rand(0,99) < perc then
                        SetLiveCell(x, y, z)
                    end
                end
            end
        end
        --]]
    end
    
    SetCursor(movecursor)
    SetActivePlane()
    InitialView()       -- calls Refresh
    
    -- run the user's startup script if it exists
    local f = io.open(startup, "r")
    if f then
        f:close()
        RunScript(startup)
    end
end

----------------------------------------------------------------------

function HandleKey(event)
    local CMDCTRL = "cmd"
    if g.os() ~= "Mac" then CMDCTRL = "ctrl" end
    local _, key, mods = split(event)
    if key == "enter" or key == "return" then StartStop()
    elseif key == "space" then Step1()
    elseif key == "down"  and mods == "none" then Rotate(-5,  0,  0)
    elseif key == "up"    and mods == "none" then Rotate( 5,  0,  0)
    elseif key == "left"  and mods == "none" then Rotate( 0, -5,  0)
    elseif key == "right" and mods == "none" then Rotate( 0,  5,  0)
    elseif key == "down"  and mods == "alt"  then Rotate( 0,  0, -5)
    elseif key == "right" and mods == "alt"  then Rotate( 0,  0, -5)
    elseif key == "up"    and mods == "alt"  then Rotate( 0,  0,  5)
    elseif key == "left"  and mods == "alt"  then Rotate( 0,  0,  5)
    elseif key == "delete" and mods == "none" then ClearSelection()
    elseif key == "delete" and mods == "shift" then ClearOutside()
    elseif key == "n" and mods == CMDCTRL then NewPattern()
    elseif key == "o" and mods == CMDCTRL then OpenPattern()
    elseif key == "s" and mods == CMDCTRL then SavePattern()
    elseif key == "s" and mods == "alt" then SetStartupScript()
    elseif key == "o" and mods == "shift" then OpenClipboard()
    elseif key == "r" and mods == "shift" then RunClipboard()
    elseif key == "r" and mods == CMDCTRL then Reset()
    elseif key == "r" and mods == "none" then ChangeRule()
    elseif key == "g" and (mods == "none" or mods == CMDCTRL) then ChangeGridSize()
    elseif key == "a" and (mods == "none" or mods == CMDCTRL) then SelectAll()
    elseif key == "k" and (mods == "none" or mods == CMDCTRL) then RemoveSelection()
    elseif key == "b" then Rotate(0, 180, 0)
    elseif key == "z" and (mods == "none" or mods == CMDCTRL) then Undo()
    elseif key == "z" and (mods == "shift" or mods == CMDCTRL.."shift") then Redo()
    elseif key == "x" and mods == CMDCTRL then CutSelection()
    elseif key == "c" and mods == CMDCTRL then CopySelection()
    elseif key == "v" and (mods == "none" or mods == CMDCTRL) then Paste()
    elseif key == "v" and mods == "alt" then CancelPaste()
    elseif key == "[" then Zoom(CELLSIZE-1)
    elseif key == "]" then Zoom(CELLSIZE+1)
    elseif key == "i" then InitialView()
    elseif key == "f" then FitGrid()
    elseif key == "p" then CycleCellType()
    elseif key == "l" then ToggleAxes()
    elseif key == "t" then ToggleToolBar()
    elseif key == "," then MoveActivePlane(activepos+1)
    elseif key == "." then MoveActivePlane(activepos-1)
    elseif key == "a" and mods == "shift" then CycleActivePlane()
    elseif key == "c" and mods == "none" then CycleCursor()
    elseif key == "m" and mods == "none" then MoveMode()
    elseif key == "d" and mods == "none" then DrawMode()
    elseif key == "s" and mods == "none" then SelectMode()
    elseif key == "h" then ShowHelp()
    elseif key == "5" then RandomPattern()
    elseif key == "q" then ExitScript()
    else
        -- could be a keyboard shortcut (eg. for full screen)
        g.doevent(event)
    end
end

----------------------------------------------------------------------

function MainLoop()
    local mousedown = false         -- mouse button is down?
    local drawing = false           -- draw/erase cells with pencil cursor?
    local selecting = false         -- (de)select cells with cross-hairs cursor?
    local drag_paste = false        -- drag paste pattern with hand cursor?
    local drag_selection = false    -- drag selected cells with hand cursor?
    local drag_active = false       -- drag active plane with pencil/cross-hairs?
    local hand_erase = false        -- erase live cells with hand cursor?
    local hand_select = false       -- select live cells with hand cursor?
    local dragface = ""             -- which paste/selection face is being dragged
    local prevx, prevy              -- previous mouse position
    
    while true do
        CheckLayerSize()            -- may need to resize the overlay
        
        local event = g.getevent()
        if event:find("^key") or event:find("^oclick") then
            if message then
                message = nil       -- remove the most recent message
                Refresh()
            end
        end
      
        event = op.process(event)
        -- event is empty if op.process handled the given event
        if #event > 0 then
            if event:find("^key") then
                HandleKey(event)
            elseif event:find("^oclick") then
                local _, x, y, button, mods = split(event)
                x = tonumber(x)
                y = tonumber(y)
                if y > toolbarht then
                    if (button == "right" and (mods == "none" or mods == "ctrl")) or
                       (button == "left" and mods == "ctrl") then
                        if pastecount > 0 then
                            ChoosePasteAction(x, y)
                        elseif selcount > 0 then
                            ChooseSelectionAction(x, y)
                        end
                    elseif button == "left" then
                        mousedown = true
                        prevx = x
                        prevy = y
                        if currcursor == drawcursor then
                            if mods == "none" then
                                drawing = StartDrawing(x, y)
                            elseif mods == "shift" then
                                drag_active = StartDraggingPlane(x, y)
                            end
                        elseif currcursor == selectcursor then
                            if mods == "none" then
                                selecting = StartSelecting(x, y)
                            elseif mods == "shift" then
                                drag_active = StartDraggingPlane(x, y)
                            end
                        else
                            -- currcursor == movecursor
                            if mods == "none" then
                                if pastecount > 0 then
                                    dragface = StartDraggingPaste(x, y)
                                    drag_paste = #dragface > 0
                                end
                                if not drag_paste and selcount > 0 then
                                    dragface= StartDraggingSelection(x, y)
                                    drag_selection = #dragface > 0
                                end
                            elseif mods == "alt" then
                                hand_erase = true
                                EraseLiveCells(x, y)
                            elseif mods == "shift" then
                                hand_select = true
                                SelectLiveCells(x, y)
                            end
                        end
                    end
                end
            elseif event:find("^mup") then
                mousedown = false
                if drawing then
                    StopDrawing()
                    drawing = false
                elseif selecting then
                    StopSelecting()
                    selecting = false
                elseif drag_paste then
                    drag_paste = false
                elseif drag_selection then
                    StopDraggingSelection()
                    drag_selection = false
                elseif drag_active then
                    drag_active = false
                elseif hand_erase then
                    hand_erase = false
                elseif hand_select then
                    hand_select = false
                end
            elseif event:find("^ozoomout") then
                if not arrow_cursor then Zoom(CELLSIZE-1) end
            elseif event:find("^ozoomin") then
                if not arrow_cursor then Zoom(CELLSIZE+1) end
            end
        end
        
        local mousepos = ov("xy")
        if mousedown then
            if #mousepos > 0 then
                local x, y = split(mousepos)
                x = tonumber(x)
                y = tonumber(y)
                if x ~= prevx or y ~= prevy then
                    -- mouse has moved
                    if drawing then
                        DrawCells(x, y)
                    elseif selecting then
                        SelectCells(x, y)
                    elseif drag_paste then
                        DragPaste(x, y, prevx, prevy, dragface)
                    elseif drag_selection then
                        DragSelection(x, y, prevx, prevy, dragface)
                    elseif drag_active then
                        DragActivePlane(x, y, prevx, prevy)
                    elseif hand_erase then
                        EraseLiveCells(x, y)
                    elseif hand_select then
                        SelectLiveCells(x, y)
                    else
                        -- rotate the view
                        local deltax = x - prevx
                        local deltay = y - prevy
                        Rotate(round(-deltay/2.0), round(deltax/2.0), 0)
                    end
                    prevx = x
                    prevy = y
                end
            elseif #activecell > 0 and currcursor ~= movecursor then
                activecell = ""
                Refresh()
            end
        else
            CheckCursor(mousepos)
            if not generating then
                -- don't hog the CPU
                -- implement g.sleep using wxMilliSleep!!!
                -- g.sleep(5)
            end
        end
        
        if generating and not mousedown then NextGeneration() end
    end
end

----------------------------------------------------------------------

ReadSettings()
local oldstate = SaveGollyState()
Initialize()

status, err = xpcall(MainLoop, gp.trace)
if err then g.continue(err) end
-- the following code is always executed

RestoreGollyState(oldstate)
WriteSettings()
