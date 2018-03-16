--[[
This script lets people explore three-dimensional cellular automata
using Golly.  Inspired by the work of Carter Bays and his colleagues
(see http://www.cse.sc.edu/~bays/d4d4d4/).

The 3D drawing is based on code in "Macintosh Graphics in Modula-2"
by Russell L. Schnapp.  Note that orthographic projection is used to
speed up rendering (because all live cells look the same at a given
scale and rotation).

Author: Andrew Trevorrow (andrew@trevorrow.com), Feb 2018.

TODO: !!!

- implement paste, undo, redo, flip/rotate selection...
- shift-click and drag pencil/cross-hairs to move active plane
- allow saving as .vti file for use by Ready?
- add option for 6-face-neighbor rules (start or end rule with V?)
- also option for 12-neighbor sphere packing rules?
- support Busy Boxes (via rules starting with BB?)

NOTE: Do following changes for the Golly 3.2b1 release:

- fix textoption background problem in m.button code in oplus/init.lua

- implement g.settitle(string) so we can put pattname and 3D rule in
  window title and avoid using g.setname (which adds an undo item)

- implement g.setoption("showtimeline",0)
- implement g.setoption("showscrollbars",0)

- implement optional parameter for g.note and g.warn so scripts can
  disable the Cancel button:
  g.note(msg, cancel=true)
  g.warn(msg, cancel=true)

- add a menu bar (from gplus)

- get round() from gplus

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
local MAXN = 64                     -- maximum grid size
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
local minx,maxx,miny,maxy,minz,maxz -- boundary for live cells
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

-- boundary grid coords for the active plane
local minactivex, minactivey, minactivez
local maxactivex, maxactivey, maxactivez

-- boundary grid coords for the paste pattern
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

local buttonht = 20
local gap = 10                      -- space around buttons
local toolbarht = buttonht*2+gap*3  -- two rows of buttons

local drawcursor = "pencil"         -- cursor for drawing cells
local selectcursor = "cross"        -- cursor for selecting cells
local movecursor = "hand"           -- cursor for rotating grid
local currcursor = movecursor       -- current cursor
local arrow_cursor = false          -- true if cursor is in tool bar

local DEFAULT_RULE = "4,5/5"        -- initial rule
local rulestring = DEFAULT_RULE
local survivals = {}
local births = {}
survivals[4] = true                 -- WARNING: must match DEFAULT_RULE
survivals[5] = true                 -- ditto
births[5] = true                    -- ditto

local pattdir = g.getdir("data")    -- initial directory for OpenPattern/SavePattern
local scriptdir = g.getdir("app")   -- initial directory for RunScript
local scriptlevel = 0               -- greater than 0 if a user script is running

-- the default path for the script to run when 3D.lua starts up
local pathsep = g.getdir("app"):sub(-1)
local startup = g.getdir("app").."My-scripts"..pathsep.."3D-start.lua"

-- user settings are stored in this file
local settingsfile = g.getdir("data").."3D.ini"

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

----------------------------------------------------------------------

local function CheckFaces(f1v1, f1v2, f1v3, f1v4,
                          f2v1, f2v2, f2v3, f2v4)
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

local function DrawRearAxes()
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

local function DrawFrontAxes()
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

local function CreateTranslucentCell(clipname, color)
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

local function CreateLiveCube()
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

local function CreateLiveSphere()
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

local function DisplayCells(editing)
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
    
    -- draw cells from back to front (assumes vertex order set in CreateCube)
    local M = N-1
    if maxZ == z1 then
        -- draw cell at 0,0,M first
        for z = 0, M do
            for y = 0, M do
                for x = 0, M do
                    local zyxN = ((M-z) * N + y) * N + x
                    if testcell then
                        TestCell(editing, zyxN, x, y, M-z)
                    elseif grid1[zyxN] then
                        DrawLiveCell(x, y, M-z)
                    end
                end
            end
        end
    elseif maxZ == z2 then
        -- draw cell at 0,0,0 first
        for z = 0, M do
            for y = 0, M do
                for x = 0, M do
                    local zyxN = (z * N + y) * N + x
                    if testcell then
                        TestCell(editing, zyxN, x, y, z)
                    elseif grid1[zyxN] then
                        DrawLiveCell(x, y, z)
                    end
                end
            end
        end
    elseif maxZ == z3 then
        -- draw cell at 0,M,M first
        for z = 0, M do
            for y = 0, M do
                for x = 0, M do
                    local zyxN = ((M-z) * N + (M-y)) * N + x
                    if testcell then
                        TestCell(editing, zyxN, x, M-y, M-z)
                    elseif grid1[zyxN] then
                        DrawLiveCell(x, M-y, M-z)
                    end
                end
            end
        end
    elseif maxZ == z4 then
        -- draw cell at 0,M,0 first
        for z = 0, M do
            for y = 0, M do
                for x = 0, M do
                    local zyxN = (z * N + (M-y)) * N + x
                    if testcell then
                        TestCell(editing, zyxN, x, M-y, z)
                    elseif grid1[zyxN] then
                        DrawLiveCell(x, M-y, z)
                    end
                end
            end
        end
    elseif maxZ == z5 then
        -- draw cell at M,M,M first
        for z = 0, M do
            for y = 0, M do
                for x = 0, M do
                    local zyxN = ((M-z) * N + (M-y)) * N + (M-x)
                    if testcell then
                        TestCell(editing, zyxN, M-x, M-y, M-z)
                    elseif grid1[zyxN] then
                        DrawLiveCell(M-x, M-y, M-z)
                    end
                end
            end
        end
    elseif maxZ == z6 then
        -- draw cell at M,M,0 first
        for z = 0, M do
            for y = 0, M do
                for x = 0, M do
                    local zyxN = (z * N + (M-y)) * N + (M-x)
                    if testcell then
                        TestCell(editing, zyxN, M-x, M-y, z)
                    elseif grid1[zyxN] then
                        DrawLiveCell(M-x, M-y, z)
                    end
                end
            end
        end
    elseif maxZ == z7 then
        -- draw cell at M,0,M first
        for z = 0, M do
            for y = 0, M do
                for x = 0, M do
                    local zyxN = ((M-z) * N + y) * N + (M-x)
                    if testcell then
                        TestCell(editing, zyxN, M-x, y, M-z)
                    elseif grid1[zyxN] then
                        DrawLiveCell(M-x, y, M-z)
                    end
                end
            end
        end
    elseif maxZ == z8 then
        -- draw cell at M,0,0 first
        for z = 0, M do
            for y = 0, M do
                for x = 0, M do
                    local zyxN = (z * N + y) * N + (M-x)
                    if testcell then
                        TestCell(editing, zyxN, M-x, y, z)
                    elseif grid1[zyxN] then
                        DrawLiveCell(M-x, y, z)
                    end
                end
            end
        end
    end
    
    ov("blend 0")
end

--------------------------------------------------------------------------------

local function DrawToolBar()
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
    
    local editing = currcursor == drawcursor or currcursor == selectcursor
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

function InitBoundaries()
    minx = math.maxinteger
    miny = math.maxinteger
    minz = math.maxinteger
    maxx = math.mininteger
    maxy = math.mininteger
    maxz = math.mininteger
    -- next SetLiveCell/SetGrid2 call will update all values
end

----------------------------------------------------------------------

function ClearCells()
    grid1 = {}
    grid2 = {}
    popcount = 0
    InitBoundaries()
    -- remove selection
    selcount = 0
    selected = {}
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

function MoveActivePlane(newpos)
    if currcursor == drawcursor or currcursor == selectcursor then
        local mid = N//2
        if newpos + mid < 0 then return end
        if newpos + mid >= N then return end
        -- use the same orientation
        SetActivePlane(activeplane, newpos)
        Refresh()
    end
end

----------------------------------------------------------------------

function CycleActivePlane()
    if currcursor == drawcursor or currcursor == selectcursor then
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
        end
    elseif selected[pos] then
        selected[pos] = nil
        selcount = selcount - 1
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
            -- don't test for boundary shrinkage here (too expensive)
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

local function SetGrid2(pos, x, y, z)
    -- create a live cell in grid2
    grid2[pos] = 1
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
        for z = minz, maxz do
            local zN = z * N
            for y = miny, maxy do
                local zyN = (zN + y) * N
                for x = minx, maxx do
                    if grid1[zyN + x] then
                        livecells[#livecells+1] = {x-mid, y-mid, z-mid}
                    end
                end
            end
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
    op.textshadowx = 2
    op.textshadowy = 2
    if generating then
        ssbutton.setlabel("Stop", false)
    else
        ssbutton.setlabel("Start", false)
    end
    
    ov("textoption background "..oldbg) -- see above!!!
end

----------------------------------------------------------------------

local function Alive(x, y, z)
    -- wrap edges if necessary
    if x < 0 then x = N-1 elseif x >= N then x = 0 end
    if y < 0 then y = N-1 elseif y >= N then y = 0 end
    if z < 0 then z = N-1 elseif z >= N then z = 0 end
    -- return state if the given cell is alive, otherwise nil
    return grid1[ x + N * (y + N * z) ]
end

----------------------------------------------------------------------

local function WrapEmptyCell(emptycells, x, y, z)
    -- at least one of x,y,z needs to be wrapped
    if x < 0 then x = N-1 elseif x >= N then x = 0 end
    if y < 0 then y = N-1 elseif y >= N then y = 0 end
    if z < 0 then z = N-1 elseif z >= N then z = 0 end
    emptycells[ x + N * (y + N * z) ] = true
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
        for k,v in pairs(selected) do
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
    
    popcount = 0            -- SetGrid2 will increment popcount
    InitBoundaries()        -- SetGrid2 will set new boundaries
    local NxN = N*N

    local count1 = {}
    for k,_ in pairs(grid1) do
       count1[k] = 1
    end
    for k,_ in pairs(grid1) do
        local x = k % N
        local k2 = k + (x + 1) % N - x
        count1[k2] = (count1[k2] or 0) + 1
        local k2 = k + (x + N - 1) % N - x
        count1[k2] = (count1[k2] or 0) + 1
    end
    local count2 = {}
    for k,v in pairs(count1) do
       count2[k] = v
    end
    for k,v in pairs(count1) do
        local y = k // N % N
        local k2 = k + N * ((y + 1) % N - y)
        count2[k2] = (count2[k2] or 0) + v
        k2 = k + N * ((y + N - 1) % N - y)
        count2[k2] = (count2[k2] or 0) + v
    end
    count1 = {}
    for k,v in pairs(count2) do
       count1[k] = v
    end
    for k,v in pairs(count2) do
        local z = k // (N * N)
        local k2 = k + N * N * ((z + 1) % N - z)
        count1[k2] = (count1[k2] or 0) + v
        k2 = k + N * N * ((z + N - 1) % N - z)
        count1[k2] = (count1[k2] or 0) + v
    end
    grid2 = {}
    for k,v in pairs(count1) do
       if (grid1[k] and survivals[v-1]) or (births[v] and not grid1[k]) then
          SetGrid2(k, k % N, k // N % N, k // (N * N))
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
                tsize = tonumber(value)
            elseif keyword == "rule" then
                local saverule = rulestring
                if not ParseRule(value) then
                    f:close()
                    return "Unknown rule:\n"..value
                end
                trule = rulestring
                -- restore rulestring in case there is a later error
                rulestring = saverule
            elseif keyword == "generation" then
                tgens = tonumber(value)
            end
        else
            -- line should contain "x y z" coords (but y and z might be missing)
            local x, y, z = split(line)
            local mid = tsize // 2
            x = tonumber(x) + mid
            if y then prevy = tonumber(y) end
            if z then prevz = tonumber(z) end
            y = prevy + mid
            z = prevz + mid
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
        local prevy = math.maxinteger
        local prevz = math.maxinteger
        local mid = N//2
        for z = minz, maxz do
            local zN = z * N
            for y = miny, maxy do
                local zyN = (zN + y) * N
                for x = minx, maxx do
                    if grid1[zyN + x] then
                        -- reduce file size by only writing y and z coords if they have changed
                        if z == prevz and y == prevy then
                            f:write((x-mid).."\n")
                        elseif z == prevz then
                            f:write((x-mid).." "..(y-mid).."\n")
                            prevy = y
                        else
                            f:write((x-mid).." "..(y-mid).." "..(z-mid).."\n")
                            prevy = y
                            prevz = z
                        end
                    end
                end
            end
        end
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

function RunScript(filepath)
    if filepath then
        local f, msg = loadfile(filepath)
        if f then
            scriptlevel = scriptlevel+1
            -- error if scriptlevel reaches 100???!!!
            local status, err = pcall(f)
            scriptlevel = scriptlevel-1
            if err then
                g.continue("")
                if err == "GOLLY: ABORT SCRIPT" then
                    -- user hit escape
                    message = "Script aborted."
                else
                    g.warn("Runtime error in script:\n\n"..err)
                end
            end
            Refresh()
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
        scriptlevel = scriptlevel+1
        -- error if scriptlevel reaches 100???!!!
        local status, err = pcall(f)
        scriptlevel = scriptlevel-1
        if err then
            g.continue("")
            if err == "GOLLY: ABORT SCRIPT" then
                -- user hit escape
                message = "Script aborted."
            else
                g.warn("Runtime error in clipboard script:\n\n"..err)
            end
        end
        Refresh()
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

function RandomFill()
    local function getperc()
        ::try_again::
        local s = g.getstring("Enter a percentage (from 0 to 100):",
                              tostring(perc), "Random fill")
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
    SetActivePlane()
    InitialView()       -- calls Refresh()
end

----------------------------------------------------------------------

function GetSelectedCells()
    -- return an array of selected cell positions relative to mid cell
    local selcells = {}
    if selcount > 0 then
        local mid = N//2
        local M = N-1
        for z = 0, M do
            local zN = z * N
            for y = 0, M do
                local zyN = (zN + y) * N
                for x = 0, M do
                    if selected[zyN + x] then
                        selcells[#selcells+1] = {x-mid, y-mid, z-mid}
                    end
                end
            end
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
        local M = N-1
        for z = minz, maxz do
            local zN = z * N
            for y = miny, maxy do
                local zyN = (zN + y) * N
                for x = minx, maxx do
                    if selected[zyN + x] and grid1[zyN + x] then
                        livecells[#livecells+1] = {x-mid, y-mid, z-mid}
                    end
                end
            end
        end
    end
    return livecells
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
    if oldselcount > 0 then
        for _, xyz in ipairs(selcells) do
            local x, y, z = xyz[1]+mid, xyz[2]+mid, xyz[3]+mid
            if x >= 0 and x < N and
               y >= 0 and y < N and
               z >= 0 and z < N then
                selected[ (z * N + y) * N + x ] = true
                selcount = selcount + 1
            else
                selclipped = selclipped + 1
            end
        end
    end
    
    -- restore paste pattern, clipping any cells outside the new grid
    --!!!
    
    if clipcount > 0 or selclipped > 0 then
        message = ""
        if clipcount > 0 then message = "Clipped live cells = "..clipcount.."\n" end
        if selclipped > 0 then message = message.."Clipped selection cells = "..selclipped end
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
        Refresh()
    end
end

----------------------------------------------------------------------

function CopySelection()
    if selcount > 0 then
        -- save the selected live cells as a 3D pattern in clipboard
        local livecells = GetSelectedLiveCells()
        local prevy = math.maxinteger
        local prevz = math.maxinteger
        local lines = {}
        lines[1] = PatternHeader()
        for _, xyz in ipairs(livecells) do
            local x, y, z = xyz[1], xyz[2], xyz[3]
            -- x,y,z are relative to middle cell in grid
            if z == prevz and y == prevy then
                lines[#lines+1] = tostring(x)
            elseif z == prevz then
                lines[#lines+1] = x.." "..y
                prevy = y
            else
                lines[#lines+1] = x.." "..y.." "..z
                prevy = y
                prevz = z
            end
        end
        lines[#lines+1] = "" -- so we get \n at end of last line
        g.setclipstr(table.concat(lines,"\n"))
    end
end

----------------------------------------------------------------------

function ClearSelection()
    if selcount > 0 then
        -- kill all selected live cells
        for i = 0, N*N*N-1 do
            if grid1[i] and selected[i] then
                grid1[i] = nil
                popcount = popcount - 1
            end
        end
        Refresh()
    end
end

----------------------------------------------------------------------

function ClearOutside()
    if selcount > 0 then
        -- kill all unselected live cells
        for i = 0, N*N*N-1 do
            if grid1[i] and not selected[i] then
                grid1[i] = nil
                popcount = popcount - 1
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
        CopySelection()
        ClearSelection()    -- calls Refresh
    end
end

----------------------------------------------------------------------

function RemoveSelection()
    if selcount > 0 then
        selcount = 0
        selected = {}
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
        elseif newpattern.newpop == 0 then
            message = "Clipboard pattern is empty."
            Refresh()
        else
            -- newpattern contains valid info so set set pastecount and pastecell array
            minpastex = newpattern.newminx
            minpastey = newpattern.newminy
            minpastez = newpattern.newminz
            maxpastex = newpattern.newmaxx
            maxpastey = newpattern.newmaxy
            maxpastez = newpattern.newmaxz
            if newpattern.newsize == N then
                -- put paste pattern in same location as the cut/copy
                pastecount = newpattern.newpop
                pastecell = newpattern.newgrid
            else
                -- put paste pattern in middle of grid, clipping any cells outside
                --!!!
            end
            Refresh()
        end
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
        if selcount > 0 then
            for k,v in pairs(startselected) do
                selected[k] = true
            end
        end
        
        Refresh()
    end
end

----------------------------------------------------------------------

local function GetMidPoint(x, y, z)
    -- return mid point of cube at given grid position
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

local function Visible(x, y)
    -- return true if pixel at x,y is within area under tool bar
    if x < 0 or x >= ovwd then return false end
    if y <= toolbarht or y >= ovht then return false end
    return true
end

----------------------------------------------------------------------

function FitGrid()
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
        -- note that minx,maxx,miny,maxy,minz,maxz won't necessarily be
        -- the minimal bounding box if a live cell was deleted, so maybe
        -- set a flag if that happens and test it here (also reset the
        -- flag elsewhere such as in NextGeneration)???!!!
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
    if grid1[ (x+mid) + N * ((y+mid) + N * (z+mid))] then
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
behind it, as long as their mid points are within a half-cell radius of the
mouse click.
Or you can shift-click on a live cell to select it <em>and</em> any live cells
behind it.
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
<tr><td align=right> 5 &nbsp;</td><td>&nbsp; fill grid with random pattern at given % </td></tr>
<tr><td align=right> G &nbsp;</td><td>&nbsp; change the grid size </td></tr>
<tr><td align=right> R &nbsp;</td><td>&nbsp; change the rule </td></tr>
<tr><td align=right> , &nbsp;</td><td>&nbsp; move active plane away from axes </td></tr>
<tr><td align=right> . &nbsp;</td><td>&nbsp; move active plane closer to axes </td></tr>
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
Return {} if the pattern is empty, otherwise return the bounding box
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
Return the current grid size (3 to 64).
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
Then comes the coordinates for each live cell in the pattern,
one cell per line.  Each line has the form "x y z".
The y and/or z coordinates are optional; if missing they take on
the values from a previous line where the y/z values were supplied.
All coordinates are relative to the middle cell in the grid.
<li>
Empty lines or lines starting with "#" are ignored.
</ul>

<p>
The following is a small example of the 3D file format.
You can either save it in a .3d file, or copy it to the clipboard
and type shift-O (after returning to the 3D.lua window):

<dd><table border=0><pre>
3D
# A 3-dimensional analog of Life's glider.
# Discovered by Carter Bays in 1987.
version=1
gridsize=30
rule=5,6,7/6
generation=0
-1 -1 -1
0
-1 0
0
-1 -2 0
0
-1 0
0
-1 0 1
0</pre></table></dd>

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

    -- create activebox's rotated and projected vertices
    for i = 1, 8 do
        rotx[i], roty[i], rotz[i] = TransformPoint(activebox[i])
        projectedx[i] = round( rotx[i] ) + midx
        projectedy[i] = round( roty[i] ) + midy
    end

    -- test if mousex,mousey is inside activebox;
    -- up to 3 faces (all parallelograms) are visible
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
            selstate = false
        else
            selected[pos] = true
            selcount = selcount + 1
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

function EraseLiveCells(mousex, mousey)
    -- erase all live cells whose projected mid points are close to mousex,mousey
    if popcount > 0 then
        local changes = 0
        for z = minz, maxz do
            local zN = z * N
            for y = miny, maxy do
                local zyN = (zN + y) * N
                for x = minx, maxx do
                    if grid1[zyN + x] then
                        local px, py = GetMidPoint(x, y, z)
                        if abs(px - mousex) < HALFCELL and
                           abs(py - mousey) < HALFCELL then
                            grid1[zyN + x] = nil
                            popcount = popcount - 1
                            changes = changes + 1
                        end
                    end
                end
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
        for z = minz, maxz do
            local zN = z * N
            for y = miny, maxy do
                local zyN = (zN + y) * N
                for x = minx, maxx do
                    if grid1[zyN + x] then
                        local px, py = GetMidPoint(x, y, z)
                        if abs(px - mousex) < HALFCELL and
                           abs(py - mousey) < HALFCELL then
                            if not selected[zyN + x] then
                                selected[zyN + x] = true
                                selcount = selcount + 1
                                changes = changes + 1
                            end
                        end
                    end
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
    randbutton = op.button("Random...", RandomFill)
    rulebutton = op.button("Rule...", ChangeRule)
    ssbutton = op.button("Start", StartStop)
    s1button = op.button("+1", Step1)
    resetbutton = op.button("Reset", Reset)
    fitbutton = op.button("Fit", FitGrid)
    undobutton = op.button("Undo", Undo)
    redobutton = op.button("Redo", Redo)
    helpbutton = op.button("?", ShowHelp)
    exitbutton = op.button("X", ExitScript)
    
    -- create check boxes
    op.textshadowx = 0
    op.textshadowy = 0
    drawbox = op.checkbox("Draw", op.black, DrawMode)
    selectbox = op.checkbox("Select", op.black, SelectMode)
    movebox = op.checkbox("Move", op.black, MoveMode)
    
    ov("textoption background "..oldbg) -- see above!!!
end

----------------------------------------------------------------------

local showtoolbar = false   -- restore tool bar?

local function CheckLayerSize()
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

local function CheckCursor(xy)
    -- update cursor if mouse moves in/out of tool bar
    local editing = currcursor == drawcursor or currcursor == selectcursor
    if #xy > 0 then
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
                if activecell ~= oldcell then
                    Refresh()
                end
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
        -- initial pattern is glider #2 in rule 4,5/5
        -- from http://www.cse.sc.edu/~bays/d4d4d4/
        local mid = N//2
        SetLiveCell(mid,   mid,   mid)
        SetLiveCell(mid+1, mid,   mid)
        SetLiveCell(mid-1, mid+1, mid)
        SetLiveCell(mid+2, mid+1, mid)
        SetLiveCell(mid,   mid+2, mid)
        SetLiveCell(mid+1, mid+2, mid)
        SetLiveCell(mid,   mid,   mid-1)
        SetLiveCell(mid+1, mid,   mid-1)
        SetLiveCell(mid-1, mid+1, mid-1)
        SetLiveCell(mid+2, mid+1, mid-1)
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

local CMDCTRL = "cmd"
if g.os() ~= "Mac" then CMDCTRL = "ctrl" end

function HandleKey(event)
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
    elseif key == "5" then RandomFill()
    elseif key == "q" then ExitScript()
    else
        -- could be a keyboard shortcut (eg. for full screen)
        g.doevent(event)
    end
end

----------------------------------------------------------------------

function MainLoop()
    local mousedown = false
    local drawing = false
    local selecting = false
    local hand_erasing = false      -- erasing live cells with hand cursor?
    local hand_selecting = false    -- selecting live cells with hand cursor?
    local prevx, prevy
    while true do
        CheckLayerSize()
        
        local event = g.getevent()
        if event:find("^key") or event:find("^oclick") then
            message = nil
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
                if y > toolbarht and button == "left" then
                    mousedown = true
                    prevx = x
                    prevy = y
                    if currcursor == drawcursor then
                        if mods == "none" then
                            drawing = StartDrawing(x, y)
                        elseif mods == "shift" then
                            -- move active plane???!!!
                        end
                    elseif currcursor == selectcursor then
                        if mods == "none" then
                            selecting = StartSelecting(x, y)
                        elseif mods == "shift" then
                            -- move active plane???!!!
                        end
                    elseif currcursor == movecursor and mods == "alt" then
                        hand_erasing = true
                        EraseLiveCells(x, y)
                    elseif currcursor == movecursor and mods == "shift" then
                        hand_selecting = true
                        SelectLiveCells(x, y)
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
                elseif hand_erasing then
                    hand_erasing = false
                elseif hand_selecting then
                    hand_selecting = false
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
                    elseif hand_erasing then
                        EraseLiveCells(x, y)
                    elseif hand_selecting then
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
            elseif #activecell > 0 and (currcursor == drawcursor or currcursor == selectcursor) then
                activecell = ""
                Refresh()
            end
        else
            CheckCursor(mousepos)
        end
        
        if generating then NextGeneration() end
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
