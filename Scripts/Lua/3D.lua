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
Thanks to Chris Rowett for optimizing the DisplayCells code.

TODO: !!!

- allow saving pattern as .vti file for use by Ready?
- add support for 6-face-neighbor rules (via rules ending with V?)
- also support for 12-neighbor sphere packing rules?
- support Busy Boxes (via rules starting with BB?)

NOTE: Do following changes for the Golly 3.2b1 release:

- implement g.settitle(string) so we can put pattname and 3D rule in
  window title and avoid using g.setname (which adds an undo item)
- implement g.setoption("showscrollbars",0)
- implement "open filepath" event for g.getevent and get Golly to
  automatically start up 3D.lua if user opens a .rle3 file
- throttle ozoom* events (in here or in Golly)?
- fix unicode problems reported here:
  http://www.conwaylife.com/forums/viewtopic.php?f=7&t=3132#p52918
--]]

local g = golly()
--!!! require "gplus.strict"
local gp = require "gplus"
local int = gp.int
local round = gp.round
local validint = gp.validint
local split = gp.split

local ov = g.overlay
local op = require "oplus"
local DrawLine = op.draw_line

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
local LEN = CELLSIZE-BORDER*2       -- edge length of live cubes
local DEGTORAD = math.pi/180.0      -- converts degrees to radians

-- MIDGRID is used to ensure that rotation occurs about the
-- mid point of the middle cube in the grid
local MIDGRID = (N+1-(N%2))*HALFCELL
local MIDCELL = HALFCELL-MIDGRID

local BACK_COLOR = "0 0 65 255"     -- for drawing background
local LINE_COLOR = "60 60 90 255"   -- for drawing lattice lines
local xylattice = {}                -- lattice lines between XY axes
local xzlattice = {}                -- lattice lines between XZ axes
local yzlattice = {}                -- lattice lines between YZ axes
local xaxes = {}                    -- four X axes
local yaxes = {}                    -- four Y axes
local zaxes = {}                    -- four Z axes

local grid1 = {}                    -- sparse 3D matrix with up to N*N*N live cells
local grid2 = {}                    -- sparse 3D matrix for the next generation
local popcount = 0                  -- number of live cells
local pattname = "untitled"         -- pattern name
local showaxes = true               -- draw axes and lattice lines?
local showlines = true              -- draw lattice lines?
local generating = false            -- generate pattern?
local gencount = 0                  -- current generation count
local perc = 20                     -- initial percentage for random fill
local message = nil                 -- text message displayed by Refresh if not nil
local selcount = 0                  -- number of selected cells (live or dead)
local selected = {}                 -- grid positions of selected cells
local pastecount = 0                -- number of cells in paste pattern
local pastepatt = {}                -- grid positions of cells in paste pattern
local drawstate = 1                 -- for drawing/erasing cells
local selstate = true               -- for selecting/deselecting cells
local celltype = "cube"             -- draw live cell as cube/sphere/point
local DrawLiveCell                  -- set to Draw{Cube/Sphere/Point} or Add{Cube/Sphere/Point}ToBatch
local xybatch = {}                  -- coordinates for each cell when batch drawing
local layercoords = {}              -- coordinates for each cell in each layer
local depthshading = false          -- using depth shading?
local depthlayers = 32              -- number of shading layers
local zdepth, zdepth2               -- coefficients for z to depth layer mapping

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

-- for undo/redo
local undostack = {}                -- stack of states that can be undone
local redostack = {}                -- stack of states that can be redone
local startcount = 0                -- starting gencount (can be > 0)
local startindex = 0                -- index of starting state in undostack
local dirty = false                 -- pattern has been modified by user/script?
local undo_cleared = false          -- was ClearUndoRedo called?

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

local mbar                          -- the menu bar
local mbarht = 28                   -- height of menu bar

-- tool bar controls
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

-- tool bar height includes menu bar
local toolbarht = mbarht+buttonht+gap*2

local drawcursor = "pencil"         -- cursor for drawing cells
local selectcursor = "cross"        -- cursor for selecting cells
local movecursor = "hand"           -- cursor for rotating grid
local currcursor = movecursor       -- current cursor
local arrow_cursor = false          -- true if cursor is in tool bar

local DEFAULT_RULE = "3D5,6,7/6"    -- initial rule
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
    if not newrule:find("^3[dD]") then
        g.warn("Rule must start with 3D.")
        return false
    else
        -- strip off the 3D
        newrule = newrule:sub(3)
    end
    if not newrule:find("/") then
        g.warn("Rule must have a / separator.")
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
    rulestring = "3D"
    local initlen = #rulestring
    for i = 0, 26 do
        if survivals[i] then
            if #rulestring > initlen then rulestring = rulestring.."," end
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
        while true do
            local line = f:read("*l")
            if not line then break end
            -- if line ended with CRLF then remove the CR
            line = line:gsub("\r", "")
            local keyword, value = split(line,"=")
            -- look for a keyword used in WriteSettings
            if not value then
                -- ignore keyword
            elseif keyword == "startup" then startup = tostring(value)
            elseif keyword == "pattdir" then pattdir = tostring(value)
            elseif keyword == "scriptdir" then scriptdir = tostring(value)
            elseif keyword == "celltype" then celltype = tostring(value)
            elseif keyword == "perc" then perc = tonumber(value) or 20
            elseif keyword == "axes" then showaxes = tostring(value) == "true"
            elseif keyword == "lines" then showlines = tostring(value) == "true"
            elseif keyword == "shading" then depthshading = tostring(value) == "true"
            elseif keyword == "gridsize" then
                N = tonumber(value) or 30
                if N < MINN then N = MINN end
                if N > MAXN then N = MAXN end
                -- update parameters that depend on N
                MIDGRID = (N+1-(N%2))*HALFCELL
                MIDCELL = HALFCELL-MIDGRID
            elseif keyword == "rule" then
                rulestring = tostring(value)
                if not ParseRule(rulestring) then
                    g.warn("Resetting bad rule ("..rulestring..") to default.", false)
                    rulestring = DEFAULT_RULE
                end
            end
        end
        f:close()
    end
end

--------------------------------------------------------------------------------

function WriteSettings()
    local f = io.open(settingsfile, "w")
    if f then
        -- keywords must match those in ReadSettings (but order doesn't matter)
        f:write("startup=", startup, "\n")
        f:write("pattdir=", pattdir, "\n")
        f:write("scriptdir=", scriptdir, "\n")
        f:write("gridsize=", tostring(N), "\n")
        f:write("celltype=", celltype, "\n")
        f:write("rule=", rulestring, "\n")
        f:write("perc=", tostring(perc), "\n")
        f:write("axes="..tostring(showaxes), "\n")
        f:write("lines="..tostring(showlines), "\n")
        f:write("shading="..tostring(depthshading), "\n")
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
    oldstate.time = g.setoption("showtimeline", 0)
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
    g.setoption("showtimeline", oldstate.time)
    g.setoption("showtoolbar", oldstate.tool)
    g.setoption("showstatusbar", oldstate.status)
    g.setoption("showlayerbar", oldstate.layer)
    g.setoption("showeditbar", oldstate.edit)
    g.setoption("tilelayers", oldstate.tile)
    g.setoption("stacklayers", oldstate.stack)
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

local function HorizontalLine(x1, x2, y)
    -- draw a horizontal line of pixels from x1,y to x2,y
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
        -- return a shade of gray assuming a light source in front of grid
        local x1, y1, z1 = rotx[v1], roty[v1], rotz[v1]
        local x2, y2, z2 = rotx[v2], roty[v2], rotz[v2]
        local costheta = (z1-z2) / sqrt( (x1-x2)*(x1-x2) +
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

local function DisplayLine(startpt, endpt)
    -- use orthographic projection to transform line's start and end points
    local newx, newy = TransformPoint(startpt)
    local x1 = round(newx) + midx
    local y1 = round(newy) + midy
    
    newx, newy = TransformPoint(endpt)
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
    
    if showlines then
        ov("rgba"..LINE_COLOR)
        if z1 < z2 then
            -- front face of rotated refcube is visible
            for _, pt in ipairs(xylattice) do DisplayLine(pt[1], pt[2]) end
        end
        if z1 >= z7 then
            -- right face of rotated refcube is visible
            for _, pt in ipairs(yzlattice) do DisplayLine(pt[1], pt[2]) end
        end
        if z1 >= z3 then
            -- top face of rotated refcube is visible
            for _, pt in ipairs(xzlattice) do DisplayLine(pt[1], pt[2]) end
        end
    end
    
    -- draw darker anti-aliased lines for rear axes
    local dark_red = "rgba 128 0 0 255"
    local dark_green = "rgba 0 128 0 255"
    local dark_blue = "rgba 0 0 128 255"

    ov("blend 1")
    ov("lineoption width 3")
    
    if z1 <  z2 or z1 >= z3 then ov(dark_red);   DisplayLine(xaxes[1], xaxes[2]) end
    if z1 <  z2 or z1 >= z7 then ov(dark_green); DisplayLine(yaxes[1], yaxes[2]) end
    if z1 >= z7 or z1 >= z3 then ov(dark_blue);  DisplayLine(zaxes[1], zaxes[2]) end
    
    if z1 <  z2 or z1 <  z3 then ov(dark_red);   DisplayLine(xaxes[3], xaxes[4]) end
    if z1 >= z2 or z1 >= z7 then ov(dark_green); DisplayLine(yaxes[3], yaxes[4]) end
    if z1 <  z7 or z1 >= z3 then ov(dark_blue);  DisplayLine(zaxes[3], zaxes[4]) end
    
    if z1 >= z2 or z1 <  z3 then ov(dark_red);   DisplayLine(xaxes[5], xaxes[6]) end
    if z1 >= z2 or z1 <  z7 then ov(dark_green); DisplayLine(yaxes[5], yaxes[6]) end
    if z1 <  z3 or z1 <  z7 then ov(dark_blue);  DisplayLine(zaxes[5], zaxes[6]) end
    
    if z1 >= z2 or z1 >= z3 then ov(dark_red);   DisplayLine(xaxes[7], xaxes[8]) end
    if z1 <  z2 or z1 <  z7 then ov(dark_green); DisplayLine(yaxes[7], yaxes[8]) end
    if z1 >= z7 or z1 <  z3 then ov(dark_blue);  DisplayLine(zaxes[7], zaxes[8]) end
    
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
    
    if showlines then
        ov("rgba"..LINE_COLOR)
        if z1 >= z2 then
            -- back face of rotated refcube is visible
            for _, pt in ipairs(xylattice) do DisplayLine(pt[1], pt[2]) end
        end
        if z1 < z7 then
            -- left face of rotated refcube is visible
            for _, pt in ipairs(yzlattice) do DisplayLine(pt[1], pt[2]) end
        end
        if z1 < z3 then
            -- bottom face of rotated refcube is visible
            for _, pt in ipairs(xzlattice) do DisplayLine(pt[1], pt[2]) end
        end
    end
    
    -- draw brighter anti-aliased lines for front axes
    ov("blend 1")
    ov("lineoption width 3")
    
    if z1 >= z2 or z1 < z3 then ov(op.red);   DisplayLine(xaxes[1], xaxes[2]) end
    if z1 >= z2 or z1 < z7 then ov(op.green); DisplayLine(yaxes[1], yaxes[2]) end
    if z1 <  z7 or z1 < z3 then ov(op.blue);  DisplayLine(zaxes[1], zaxes[2]) end
    
    if z1 >= z2 or z1 >= z3 then ov(op.red);   DisplayLine(xaxes[3], xaxes[4]) end
    if z1 <  z2 or z1 <  z7 then ov(op.green); DisplayLine(yaxes[3], yaxes[4]) end
    if z1 >= z7 or z1 <  z3 then ov(op.blue);  DisplayLine(zaxes[3], zaxes[4]) end
    
    if z1 <  z2 or z1 >= z3 then ov(op.red);   DisplayLine(xaxes[5], xaxes[6]) end
    if z1 <  z2 or z1 >= z7 then ov(op.green); DisplayLine(yaxes[5], yaxes[6]) end
    if z1 >= z3 or z1 >= z7 then ov(op.blue);  DisplayLine(zaxes[5], zaxes[6]) end
    
    if z1 <  z2 or z1 <  z3 then ov(op.red);   DisplayLine(xaxes[7], xaxes[8]) end
    if z1 >= z2 or z1 >= z7 then ov(op.green); DisplayLine(yaxes[7], yaxes[8]) end
    if z1 <  z7 or z1 >= z3 then ov(op.blue);  DisplayLine(zaxes[7], zaxes[8]) end
    
    ov("lineoption width 1")
    ov("blend 0")
end

----------------------------------------------------------------------

function CreateAxes()
    -- create axes and lattice lines
        
    -- put axes origin at most -ve corner of grid
    local o = -MIDGRID
    local endpt = o + N*CELLSIZE
    
    xaxes = { {o,o,o},         {endpt,o,o},
              {o,endpt,o},     {endpt,endpt,o},
              {o,endpt,endpt}, {endpt,endpt,endpt},
              {o,o,endpt},     {endpt,o,endpt}
            }
    yaxes = { {o,o,o},         {o,endpt,o},
              {o,o,endpt},     {o,endpt,endpt},
              {endpt,o,endpt}, {endpt,endpt,endpt},
              {endpt,o,o},     {endpt,endpt,o}
            }
    zaxes = { {o,o,o},         {o,o,endpt},
              {endpt,o,o},     {endpt,o,endpt},
              {endpt,endpt,o}, {endpt,endpt,endpt},
              {o,endpt,o},     {o,endpt,endpt}
            }
    
    xylattice = {}
    for i = 1, N do
        local offset = i*CELLSIZE
        xylattice[#xylattice+1] = {{o+offset,o,o}, {o+offset,endpt,o}}
        xylattice[#xylattice+1] = {{o,o+offset,o}, {endpt,o+offset,o}}
    end
    
    xzlattice = {}
    for i = 1, N do
        local offset = i*CELLSIZE
        xzlattice[#xzlattice+1] = {{o,o,o+offset}, {endpt,o,o+offset}}
        xzlattice[#xzlattice+1] = {{o+offset,o,o}, {o+offset,o,endpt}}
    end
    
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

function CreateLayers(clip)
    local adjust = floor(96 / depthlayers)
    ov("target "..clip)
    ov("copy 0 0 0 0 "..clip.."1")
    ov("target "..clip.."1")
    ov("optimize "..clip.."1")
    for i = 2, depthlayers do
        ov("copy 0 0 0 0 "..clip..i)
        ov("target "..clip..i)
        ov("replace *#-"..adjust.." *#-"..adjust.." *#-"..adjust.." *#")
        ov("optimize "..clip..i)
    end
end

----------------------------------------------------------------------

local HALFCUBECLIP  -- half the wd/ht of the clip containing a live cube

function CreateLiveCube()
    -- create a clip containing one rotated cube that will be used later
    -- to draw all live cells (this only works because all cubes are identical
    -- in appearance when using orthographic projection)
    
    -- largest size of a rotated cube with edge length L is sqrt(3)*L
    HALFCUBECLIP = round(sqrt(3) * LEN / 2.0)
    ov("create "..(HALFCUBECLIP*2).." "..(HALFCUBECLIP*2).." c")
    ov("target c")
    
    local midpos = N//2
    local cube = CreateCube(midpos, midpos, midpos)

    -- create cube's projected vertices (within clip)
    for i = 1, 8 do
        rotx[i], roty[i], rotz[i] = TransformPoint(cube[i])
        projectedx[i] = round( rotx[i] ) + HALFCUBECLIP
        projectedy[i] = round( roty[i] ) + HALFCUBECLIP
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

    -- create faded versions of the clip if depth shading
    if depthshading then
        CreateLayers("c")
    end

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
        x = x + 1
        y = y + 1
        gray = gray + grayinc
        if gray > 255 then gray = 255 end
    end
    ov("blend 0")
    ov("lineoption width 1")

    ov("optimize S")

    -- create faded versions of the clip if depth shading
    if depthshading then
        CreateLayers("S")
    end

    ov("target")
end


----------------------------------------------------------------------

local function AddCubeToBatchDepth(x, y, z)
    -- add live cell as a cube at given grid position
    x = x * CELLSIZE + MIDCELL
    y = y * CELLSIZE + MIDCELL
    z = z * CELLSIZE + MIDCELL
    -- transform point
    local newx = (x*xixo + y*xiyo + z*xizo)
    local newy = (x*yixo + y*yiyo + z*yizo)
    local newz = (x*zixo + y*ziyo + z*zizo)
    -- use orthographic projection
    x = round(newx) + midx - HALFCUBECLIP
    y = round(newy) + midy - HALFCUBECLIP
    -- add to the list to draw
    local layer = floor(depthlayers * (newz + zdepth) / zdepth2) + 1
    if layer < 1 then layer = 1 end
    if layer > depthlayers then layer = depthlayers end
    local xylist = layercoords[layer]
    xylist[#xylist + 1] = x
    xylist[#xylist + 1] = y
end

----------------------------------------------------------------------

local function AddCubeToBatch(x, y, z)
    -- add live cell as a cube at given grid position
    x = x * CELLSIZE + MIDCELL
    y = y * CELLSIZE + MIDCELL
    z = z * CELLSIZE + MIDCELL
    -- transform point
    local newx = (x*xixo + y*xiyo + z*xizo)
    local newy = (x*yixo + y*yiyo + z*yizo)
    -- use orthographic projection
    x = round(newx) + midx - HALFCUBECLIP
    y = round(newy) + midy - HALFCUBECLIP
    -- add to the list to draw
    xybatch[#xybatch + 1] = x
    xybatch[#xybatch + 1] = y
end

----------------------------------------------------------------------

local function AddSphereToBatchDepth(x, y, z)
    -- add live cell as a sphere at given grid position
    x = x * CELLSIZE + MIDCELL
    y = y * CELLSIZE + MIDCELL
    z = z * CELLSIZE + MIDCELL
    -- transform point
    local newx = (x*xixo + y*xiyo + z*xizo)
    local newy = (x*yixo + y*yiyo + z*yizo)
    local newz = (x*zixo + y*ziyo + z*zizo)
    -- use orthographic projection
    x = round(newx + midx - HALFCELL+1)     -- clip wd = CELLSIZE-2
    y = round(newy + midy - HALFCELL+1)     -- clip ht = CELLSIZE-2
    -- add to the list to draw
    local layer = floor(depthlayers * (newz + zdepth) / zdepth2) + 1
    if layer < 1 then layer = 1 end
    if layer > depthlayers then layer = depthlayers end
    local xylist = layercoords[layer]
    xylist[#xylist + 1] = x
    xylist[#xylist + 1] = y
end

----------------------------------------------------------------------

local function AddSphereToBatch(x, y, z)
    -- add live cell as a sphere at given grid position
    x = x * CELLSIZE + MIDCELL
    y = y * CELLSIZE + MIDCELL
    z = z * CELLSIZE + MIDCELL
    -- transform point
    local newx = (x*xixo + y*xiyo + z*xizo)
    local newy = (x*yixo + y*yiyo + z*yizo)
    -- use orthographic projection
    x = round(newx + midx - HALFCELL+1)     -- clip wd = CELLSIZE-2
    y = round(newy + midy - HALFCELL+1)     -- clip ht = CELLSIZE-2
    -- add to the list to draw
    xybatch[#xybatch + 1] = x
    xybatch[#xybatch + 1] = y
end

----------------------------------------------------------------------

local function AddPointToBatch(x, y, z)
    -- add mid point of cell at given grid position
    x = x * CELLSIZE + MIDCELL
    y = y * CELLSIZE + MIDCELL
    z = z * CELLSIZE + MIDCELL
    -- transform point
    local newx = (x*xixo + y*xiyo + z*xizo)
    local newy = (x*yixo + y*yiyo + z*yizo)
    -- use orthographic projection
    x = round(newx) + midx
    y = round(newy) + midy
    -- add to the list to draw
    xybatch[#xybatch + 1] = x
    xybatch[#xybatch + 1] = y
end

----------------------------------------------------------------------

function DrawBatchLayer(depth, coordlist)
    if celltype == "cube" then
        ov("paste "..table.concat(coordlist, " ").." c"..depth)
    elseif celltype == "sphere" then
        ov("paste "..table.concat(coordlist, " ").." S"..depth)
    else -- celltype == "point"
        ov(op.white)
        ov("set "..table.concat(coordlist, " "))
    end
end

----------------------------------------------------------------------

function DrawBatch()
    if depthshading and celltype ~= "point" then
        for i = depthlayers, 1, -1 do
            if #layercoords[i] > 0 then
                DrawBatchLayer(i, layercoords[i])
                layercoords[i] = {}
            end
        end
    else
        DrawBatchLayer("", xybatch)
        xybatch = {}
    end
end

----------------------------------------------------------------------

local function DrawCube(x, y, z)
    -- draw live cell as a cube at given grid position
    x = x * CELLSIZE + MIDCELL
    y = y * CELLSIZE + MIDCELL
    z = z * CELLSIZE + MIDCELL
    -- transform point
    local newx = (x*xixo + y*xiyo + z*xizo)
    local newy = (x*yixo + y*yiyo + z*yizo)
    -- use orthographic projection
    x = round(newx) + midx - HALFCUBECLIP
    y = round(newy) + midy - HALFCUBECLIP
    -- draw the clip created by CreateLiveCube
    ov("paste "..x.." "..y.." c")
end

----------------------------------------------------------------------

local function DrawSphere(x, y, z)
    -- draw live cell as a sphere at given grid position
    x = x * CELLSIZE + MIDCELL
    y = y * CELLSIZE + MIDCELL
    z = z * CELLSIZE + MIDCELL
    -- transform point
    local newx = (x*xixo + y*xiyo + z*xizo)
    local newy = (x*yixo + y*yiyo + z*yizo)
    -- use orthographic projection
    x = round(newx + midx - HALFCELL+1)     -- clip wd = CELLSIZE-2
    y = round(newy + midy - HALFCELL+1)     -- clip ht = CELLSIZE-2
    -- draw the clip created by CreateLiveSphere
    ov("paste "..x.." "..y.." S")
end

----------------------------------------------------------------------

local function DrawPoint(x, y, z)
    -- draw mid point of cell at given grid position
    x = x * CELLSIZE + MIDCELL
    y = y * CELLSIZE + MIDCELL
    z = z * CELLSIZE + MIDCELL
    -- transform point
    local newx = (x*xixo + y*xiyo + z*xizo)
    local newy = (x*yixo + y*yiyo + z*yizo)
    -- use orthographic projection
    x = round(newx) + midx
    y = round(newy) + midy
    ov("set "..x.." "..y)
end

----------------------------------------------------------------------

local function DrawActiveCell(x, y, z)
    x = x * CELLSIZE + MIDCELL
    y = y * CELLSIZE + MIDCELL
    z = z * CELLSIZE + MIDCELL
    -- transform point
    local newx = (x*xixo + y*xiyo + z*xizo)
    local newy = (x*yixo + y*yiyo + z*yizo)
    -- use orthographic projection
    x = round(newx) + midx - CELLSIZE
    y = round(newy) + midy - CELLSIZE
    -- draw the clip created by CreateTranslucentCell
    ov("paste "..x.." "..y.." a")
end

----------------------------------------------------------------------

local function DrawSelectedCell(x, y, z)
    x = x * CELLSIZE + MIDCELL
    y = y * CELLSIZE + MIDCELL
    z = z * CELLSIZE + MIDCELL
    -- transform point
    local newx = (x*xixo + y*xiyo + z*xizo)
    local newy = (x*yixo + y*yiyo + z*yizo)
    -- use orthographic projection
    x = round(newx) + midx - CELLSIZE
    y = round(newy) + midy - CELLSIZE
    -- draw the clip created by CreateTranslucentCell
    ov("paste "..x.." "..y.." s")
end

----------------------------------------------------------------------

local function DrawPasteCell(x, y, z)
    x = x * CELLSIZE + MIDCELL
    y = y * CELLSIZE + MIDCELL
    z = z * CELLSIZE + MIDCELL
    -- transform point
    local newx = (x*xixo + y*xiyo + z*xizo)
    local newy = (x*yixo + y*yiyo + z*yizo)
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
    if pastepatt[gridpos] then
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

    -- determine order to traverse x, y and z in the grid
    local fromz, toz, stepz, fromy, toy, stepy, fromx, tox, stepx
    if maxZ == z1 then
        fromx, fromy, fromz = MINX, MINY, MAXZ
    elseif maxZ == z2 then
        fromx, fromy, fromz = MINX, MINY, MINZ
    elseif maxZ == z3 then
        fromx, fromy, fromz = MINX, MAXY, MAXZ
    elseif maxZ == z4 then
        fromx, fromy, fromz = MINX, MAXY, MINZ
    elseif maxZ == z5 then
        fromx, fromy, fromz = MAXX, MAXY, MAXZ
    elseif maxZ == z6 then
        fromx, fromy, fromz = MAXX, MAXY, MINZ
    elseif maxZ == z7 then
        fromx, fromy, fromz = MAXX, MINY, MAXZ
    elseif maxZ == z8 then
        fromx, fromy, fromz = MAXX, MINY, MINZ
    end

    if (fromx == MINX) then tox, stepx = MAXX, 1 else tox, stepx = MINX, -1 end
    if (fromy == MINY) then toy, stepy = MAXY, 1 else toy, stepy = MINY, -1 end
    if (fromz == MINZ) then toz, stepz = MAXZ, 1 else toz, stepz = MINZ, -1 end

    -- draw cells from back to front (assumes vertex order set in CreateCube)
    local i, j
    local stepi, stepj = N*stepy, N*stepz

    if testcell then
        j = N*fromz
        for z = fromz, toz, stepz do
            i = N*(fromy+j)
            for y = fromy, toy, stepy do
                for x = fromx, tox, stepx do
                    TestCell(editing, i+x, x, y, z)
                end
                i = i+stepi
            end
            j = j+stepj
        end
    else
        -- only live cells need to be drawn so use batch mode
        if depthshading then
            zdepth = N*CELLSIZE*0.5
            zdepth2 = zdepth+zdepth
            if celltype == "cube" then
                DrawLiveCell = AddCubeToBatchDepth
            elseif celltype == "sphere" then
                DrawLiveCell = AddSphereToBatchDepth
            else -- celltype == "point"
                DrawLiveCell = AddPointToBatch
            end
        else
            if celltype == "cube" then
                DrawLiveCell = AddCubeToBatch
            elseif celltype == "sphere" then
                DrawLiveCell = AddSphereToBatch
            else -- celltype == "point"
                DrawLiveCell = AddPointToBatch
            end
        end
        j = N*fromz
        for z = fromz, toz, stepz do
            i = N*(fromy+j)
            for y = fromy, toy, stepy do
                for x = fromx, tox, stepx do
                    if grid1[i+x] then
                        DrawLiveCell(x, y, z)
                    end
                end
                i = i+stepi
            end
            j = j+stepj
        end
        DrawBatch()
    end

    ov("blend 0")
end

--------------------------------------------------------------------------------

function EnableControls(bool)
    -- disable/enable unsafe menu items so that user scripts can call op.process
    -- File menu:
    mbar.enableitem(1, 1, bool)     -- New Pattern
    mbar.enableitem(1, 2, bool)     -- Random Pattern
    mbar.enableitem(1, 3, bool)     -- Open Pattern
    mbar.enableitem(1, 4, bool)     -- Open Clipboard
    mbar.enableitem(1, 5, bool)     -- Save Pattern
    mbar.enableitem(1, 7, bool)     -- Run Script
    mbar.enableitem(1, 8, bool)     -- Run Clipboard
    mbar.enableitem(1, 9, bool)     -- Set Startup Script
    if bool then
        -- ExitScript will abort 3D.lua
        mbar.setitem(1, 11, "Exit 3D.lua")
    else
        -- ExitScript will abort the user script
        mbar.setitem(1, 11, "Exit Script")
    end
    -- Edit menu:
    mbar.enableitem(2, 1, bool)     -- Undo
    mbar.enableitem(2, 2, bool)     -- Redo
    mbar.enableitem(2, 4, bool)     -- Cut
    mbar.enableitem(2, 5, bool)     -- Copy
    mbar.enableitem(2, 6, bool)     -- Paste
    mbar.enableitem(2, 7, bool)     -- Cancel Paste
    mbar.enableitem(2, 8, bool)     -- Clear
    mbar.enableitem(2, 9, bool)     -- Clear Outside
    mbar.enableitem(2, 11, bool)    -- Select All
    mbar.enableitem(2, 12, bool)    -- Cancel Selection
    mbar.enableitem(2, 14, bool)    -- Middle Pattern
    mbar.enableitem(2, 15, bool)    -- Middle Selection
    mbar.enableitem(2, 16, bool)    -- Middle Paste
    -- Control menu:
    mbar.enableitem(3, 1, bool)     -- Start/Stop Generating
    mbar.enableitem(3, 2, bool)     -- Next Generation
    mbar.enableitem(3, 3, bool)     -- Reset
    mbar.enableitem(3, 5, bool)     -- Set Rule
    -- View menu:
    mbar.enableitem(4, 3, bool)     -- Set Grid Size
    
    -- disable/enable unsafe buttons
    ssbutton.enable(bool)
    s1button.enable(bool)
    resetbutton.enable(bool)
    undobutton.enable(bool)
    redobutton.enable(bool)

    -- disable/enable unsafe check boxes
    drawbox.enable(bool)
    selectbox.enable(bool)
    movebox.enable(bool)
end

--------------------------------------------------------------------------------

function DrawMenuBar()
    if scriptlevel == 0 then
        mbar.enableitem(2, 1, #undostack > 0)   -- Undo
        mbar.enableitem(2, 2, #redostack > 0)   -- Redo
        mbar.enableitem(2, 4, selcount > 0)     -- Cut
        mbar.enableitem(2, 5, selcount > 0)     -- Copy
        mbar.enableitem(2, 7, pastecount > 0)   -- Cancel Paste
        mbar.enableitem(2, 8, selcount > 0)     -- Clear
        mbar.enableitem(2, 9, selcount > 0)     -- Clear Outside
        mbar.enableitem(2, 11, popcount > 0)    -- Select All
        mbar.enableitem(2, 12, selcount > 0)    -- Cancel Selection
        mbar.enableitem(2, 14, popcount > 0)    -- Middle Pattern
        mbar.enableitem(2, 15, selcount > 0)    -- Middle Selection
        mbar.enableitem(2, 16, pastecount > 0)  -- Middle Paste
        mbar.enableitem(3, 1, popcount > 0)     -- Start/Stop Generating
        mbar.enableitem(3, 2, popcount > 0)     -- Next Generation
        mbar.enableitem(3, 3, gencount > startcount)    -- Reset
    end
    
    mbar.tickitem(4, 5, celltype == "cube")
    mbar.tickitem(4, 6, celltype == "sphere")
    mbar.tickitem(4, 7, celltype == "point")
    mbar.tickitem(4, 9, showaxes)
    mbar.tickitem(4, 10, showlines)
    mbar.tickitem(4, 11, depthshading)
    
    mbar.show(0, 0, ovwd, mbarht)
end

--------------------------------------------------------------------------------

function DrawToolBar()
    ov("rgba 230 230 230 255")
    ov("fill 0 0 "..ovwd.." "..toolbarht)
    
    DrawMenuBar()
    
    -- draw line at bottom edge of tool bar
    ov(op.gray)
    DrawLine(0, toolbarht-1, ovwd-1, toolbarht-1)
    
    if scriptlevel == 0 then
        -- enable/disable buttons
        ssbutton.enable(popcount > 0)
        s1button.enable(popcount > 0)
        resetbutton.enable(gencount > startcount)
        undobutton.enable(#undostack > 0)
        redobutton.enable(#redostack > 0)
    end
    
    local x = gap
    local y = mbarht + gap
    
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

    -- last 2 buttons are at right end of tool bar
    x = ovwd - gap - exitbutton.wd
    exitbutton.show(x, y)
    x = x - gap - helpbutton.wd
    helpbutton.show(x, y)
end

----------------------------------------------------------------------

function Refresh(update)
    if scriptlevel > 0 and not update then
        -- user scripts need to call Update() when they want to refresh
        return
    end
    
    -- turn off event checking to avoid partial updates of overlay
    -- (eg. due to user resizing window while a pattern is generating)
    g.check(false)

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
    local name = pattname
    if dirty then name = "*"..name end
    local info =
        "Pattern = "..name.."\n"..
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
    
    g.check(true)   -- restore event checking
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

--------------------------------------------------------------------------------

function UpdateStartButton()
    -- change label in ssbutton without changing the button's width,
    -- and also update 1st item in Control menu
    if generating then
        ssbutton.setlabel("Stop", false)
        mbar.setitem(3, 1, "Stop Generating")
    else
        ssbutton.setlabel("Start", false)
        mbar.setitem(3, 1, "Start Generating")
    end
end

----------------------------------------------------------------------

function StopGenerating()
    if generating then
        generating = false
        UpdateStartButton()
    end
end

----------------------------------------------------------------------

function SaveState()
    -- return a table containing current state
    local state = {}
    
    -- save current grid size
    state.saveN = N
    
    -- save current active plane
    state.saveplane = activeplane
    state.savepos = activepos
    
    -- save current cursor
    state.savecursor = currcursor
    
    -- save current rule
    state.saverule = rulestring
    
    -- save current pattern
    state.savedirty = dirty
    state.savename = pattname
    state.savegencount = gencount
    state.savepopcount = popcount
    state.savecells = {}
    if popcount > 0 then
        for k,_ in pairs(grid1) do state.savecells[k] = true end
    end
    state.saveminx = minx
    state.saveminy = miny
    state.saveminz = minz
    state.savemaxx = maxx
    state.savemaxy = maxy
    state.savemaxz = maxz
    state.saveminimallive = minimal_live_bounds
    
    -- save current selection
    state.saveselcount = selcount
    state.saveselected = {}
    if selcount > 0 then
        for k,_ in pairs(selected) do state.saveselected[k] = true end
    end
    state.saveminselx = minselx
    state.saveminsely = minsely
    state.saveminselz = minselz
    state.savemaxselx = maxselx
    state.savemaxsely = maxsely
    state.savemaxselz = maxselz
    state.saveminimalsel = minimal_sel_bounds
    
    -- save current paste pattern
    state.savepcount = pastecount
    state.savepaste = {}
    if pastecount > 0 then
        for k,_ in pairs(pastepatt) do state.savepaste[k] = true end
    end
    state.saveminpastex = minpastex
    state.saveminpastey = minpastey
    state.saveminpastez = minpastez
    state.savemaxpastex = maxpastex
    state.savemaxpastey = maxpastey
    state.savemaxpastez = maxpastez
    
    return state
end

----------------------------------------------------------------------

function RestoreState(state)
    -- restore state from given info (created earlier by SaveState)
    
    -- restore grid size
    N = state.saveN
    MIDGRID = (N+1-(N%2))*HALFCELL
    MIDCELL = HALFCELL-MIDGRID
    CreateAxes()
    
    -- restore active plane (depends on N)
    SetActivePlane(state.saveplane, state.savepos)
    
    -- restore cursor (determines whether active plane is displayed)
    currcursor = state.savecursor
    if not arrow_cursor then ov("cursor "..currcursor) end
    
    -- restore rule
    ParseRule(state.saverule)
    
    -- restore pattern
    dirty = state.savedirty
    pattname = state.savename
    gencount = state.savegencount
    popcount = state.savepopcount
    grid1 = {}
    if popcount > 0 then
        for k,_ in pairs(state.savecells) do grid1[k] = true end
    end
    minx = state.saveminx
    miny = state.saveminy
    minz = state.saveminz
    maxx = state.savemaxx
    maxy = state.savemaxy
    maxz = state.savemaxz
    minimal_live_bounds = state.saveminimallive
    
    -- restore selection
    selcount = state.saveselcount
    selected = {}
    if selcount > 0 then
        for k,_ in pairs(state.saveselected) do selected[k] = true end
    end
    minselx = state.saveminselx
    minsely = state.saveminsely
    minselz = state.saveminselz
    maxselx = state.savemaxselx
    maxsely = state.savemaxsely
    maxselz = state.savemaxselz
    minimal_sel_bounds = state.saveminimalsel
    
    -- restore paste pattern
    pastecount = state.savepcount
    pastepatt = {}
    if pastecount > 0 then
        for k,_ in pairs(state.savepaste) do pastepatt[k] = true end
    end
    minpastex = state.saveminpastex
    minpastey = state.saveminpastey
    minpastez = state.saveminpastez
    maxpastex = state.savemaxpastex
    maxpastey = state.savemaxpastey
    maxpastez = state.savemaxpastez
end

----------------------------------------------------------------------

function ClearUndoRedo()
    -- this might be called if a user script is running (eg. if it calls NewPattern)
    undostack = {}
    redostack = {}
    dirty = false
    undo_cleared = true
end

----------------------------------------------------------------------

function Undo()
    -- ignore if user script is running
    -- (scripts can call SaveState and RestoreState if they need to undo stuff)
    if scriptlevel > 0 then return end
    
    if #undostack > 0 then
        StopGenerating()
        -- push current state onto redostack
        redostack[#redostack+1] = SaveState()
        -- pop state off undostack and restore it
        RestoreState( table.remove(undostack) )
        Refresh()
    end
end

----------------------------------------------------------------------

function Redo()
    -- ignore if user script is running
    if scriptlevel > 0 then return end
    
    if #redostack > 0 then
        StopGenerating()
        -- push current state onto undostack
        undostack[#undostack+1] = SaveState()
        -- pop state off redostack and restore it
        RestoreState( table.remove(redostack) )
        Refresh()
    end
end

----------------------------------------------------------------------

function RememberCurrentState()
    -- ignore if user script is running
    if scriptlevel > 0 then return end
    
    redostack = {}
    undostack[#undostack+1] = SaveState()
end

----------------------------------------------------------------------

function RestoreStartingGen()
    -- called from Reset to restore starting state

    -- push current state onto redostack
    redostack[#redostack+1] = SaveState()
    
    -- unwind undostack until we get to startindex
    while startindex < #undostack do
        local state = table.remove(undostack)
        redostack[#redostack+1] = state
    end
    
    -- pop starting state off undostack and restore it
    RestoreState( table.remove(undostack) )
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
    pastepatt = {}
    collectgarbage()    -- helps avoid long delay when script exits???!!! only on Mac OS 10.13???
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
        if newpos == activepos then return end

        -- note that refresh is false if called from DragActivePlane
        -- (in which case RememberCurrentState has already been called
        -- by StartDraggingPlane)
        if refresh then RememberCurrentState() end

        -- use the same orientation
        SetActivePlane(activeplane, newpos)
        if refresh then Refresh() end
    end
end

----------------------------------------------------------------------

function CycleActivePlane()
    if currcursor ~= movecursor then
        RememberCurrentState()
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
            dirty = true
            -- boundary might expand
            if x < minx then minx = x end
            if y < miny then miny = y end
            if z < minz then minz = z end
            if x > maxx then maxx = x end
            if y > maxy then maxy = y end
            if z > maxz then maxz = z end
        end
    else
        -- state is 0
        if grid1[pos] then
            -- kill a live cell
            grid1[pos] = nil
            popcount = popcount - 1
            dirty = true
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
    dirty = true
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
        g.warn("Bug in PutCells: grid is not empty!")
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

----------------------------------------------------------------------

function NextGeneration()
    -- calculate and display the next generation using an algorithm
    -- described by Carter Bays (see Method B Option 2 on page 398 in
    -- http://www.complex-systems.com/pdf/01-3-1.pdf)
    
    if popcount == 0 then
        StopGenerating()
        message = "All cells are dead."
        Refresh()
        return
    end
    
    if gencount == startcount then
        -- remember position in undo stack that stores the starting state
        -- (for later use by Reset)
        startindex = #undostack
    end
    
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
            -- don't set dirty flag here!
            local x = k % N
            local y = k // N % N
            local z = k // NN
            -- update boundary
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
        StopGenerating()
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
    StopGenerating()
    ClearCells()
    SetActivePlane()
    ClearUndoRedo()
    InitialView()       -- calls Refresh
end

----------------------------------------------------------------------

function ReadPattern(filepath)
    local f = io.open(filepath,"r")
    if not f then return "Failed to open file:\n"..filepath end

    local line = f:read("*l")
    if line == nil or not line:find("^3D") then
        f:close()
        return "Invalid RLE3 file (first line must start with 3D)."
    end
    -- if line ended with CRLF then remove the CR
    line = line:gsub("\r", "")

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
    local x0, y0, z0 = 0, 0, 0

    -- parse 1st line (format is "3D key=val key=val key=val ...")
    local keys_and_vals = { split(line) }
    -- keys_and_vals[1] == "3D"
    for i = 2, #keys_and_vals do
        local keyword, value = split(keys_and_vals[i],"=")
        if value == nil then
            -- ignore keyword
        elseif keyword == "version" then
            if value ~= "1" then
                f:close()
                return "Unexpected version: "..value
            end
        elseif keyword == "size" then
            tsize = tonumber(value) or MAXN
            if tsize < MINN then tsize = MINN end
            if tsize > MAXN then tsize = MAXN end
        elseif keyword == "pos" then
            x0, y0, z0 = split(value,",")
            x0 = tonumber(x0) or 0
            y0 = tonumber(y0) or 0
            z0 = tonumber(z0) or 0
        elseif keyword == "gen" then
            tgens = tonumber(value) or 0
            if tgens < 0 then tgens = 0 end
        end
    end

    local wd, ht, dp
    local x, y, z = 0, 0, 0
    local runcount = 0

    while true do
        local line = f:read("*l")
        if not line then break end
        -- if line ended with CRLF then remove the CR
        line = line:gsub("\r", "")
        local ch = line:sub(1,1)
        if ch == "#" or #ch == 0 then
            -- ignore comment line or blank line
        elseif ch == "x" then
            -- parse header
            wd, ht, dp, trule = line:match("x=(.+) y=(.+) z=(.+) rule=(.+)$")
            wd = tonumber(wd)
            ht = tonumber(ht)
            dp = tonumber(dp)
            if wd and ht and dp then
                if max(wd, ht, dp) > tsize then
                    f:close()
                    return "The pattern size is bigger than the grid size!"
                end
                x = x0
                y = y0
                z = z0
                -- check that pattern is positioned within given grid size
                if x < 0 or x + wd > tsize or
                   y < 0 or y + ht > tsize or
                   z < 0 or z + dp > tsize then
                    f:close()
                    return "The pattern is positioned outside the grid!"
                end
            else
                f:close()
                return "Bad number in header line:\n"..line
            end
            local saverule = rulestring
            if not ParseRule(trule) then
                f:close()
                return "Unknown rule: "..trule
            end
            trule = rulestring
            -- restore rulestring etc in case there is a later error
            -- or we're doing a paste and want to ignore the specified rule
            ParseRule(saverule)
        else
            -- parse RLE3 data
            for i = 1, #line do
                ch = line:sub(i,i)
                if ch >= "0" and ch <= "9" then
                    runcount = runcount * 10 + tonumber(ch)
                else
                    if runcount == 0 then runcount = 1 end
                    if ch == "b" then
                        x = x + runcount
                    elseif ch == "o" then
                        repeat
                            tgrid[ x + tsize * (y + tsize * z) ] = 1
                            tpop = tpop + 1
                            -- update boundary
                            if x < tminx then tminx = x end
                            if y < tminy then tminy = y end
                            if z < tminz then tminz = z end
                            if x > tmaxx then tmaxx = x end
                            if y > tmaxy then tmaxy = y end
                            if z > tmaxz then tmaxz = z end
                            x = x + 1
                            runcount = runcount - 1
                        until runcount == 0
                    elseif ch == "$" then
                        x = x0
                        y = y + runcount
                    elseif ch == "/" then
                        x = x0
                        y = y0
                        z = z + runcount
                    elseif ch == "!" then
                        break
                    else
                        f:close()
                        return "Unexpected character: "..ch
                    end
                    runcount = 0
                end
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
    MIDCELL = HALFCELL-MIDGRID
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
    -- note that ClearCells has set minimal_live_bounds = true
    
    ParseRule(newpattern.newrule)   -- sets rulestring, survivals and births
    gencount = newpattern.newgens
    startcount = gencount           -- for Reset
    StopGenerating()
    SetCursor(movecursor)
    SetActivePlane()
    ClearUndoRedo()     -- dirty = false
    InitialView()       -- calls Refresh
end

----------------------------------------------------------------------

function OpenPattern(filepath)
    if filepath then
        local err, newpattern = ReadPattern(filepath)
        if err then
            g.warn(err, false)
        else
            -- pattern ok so use info in newpattern to update current grid;
            -- set pattname to file name at end of filepath
            pattname = filepath:match("^.+"..pathsep.."(.+)$")
            UpdateCurrentGrid(newpattern)
        end
    else
        -- prompt user for a .rle3 file to open
        local filetype = "RLE3 file (*.rle3)|*.rle3"
        local path = g.opendialog("Open a pattern", filetype, pattdir, "")
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
    local filepath = g.getdir("temp").."clipboard.rle3"
    local f = io.open(filepath,"w")
    if not f then
        g.warn("Failed to create temporary clipboard file!", false)
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
            g.warn(err, false)
        else
            -- pattern ok so use info in newpattern to update current grid
            pattname = "clipboard"
            UpdateCurrentGrid(newpattern)
        end
    end
end

----------------------------------------------------------------------

function PatternHeader(xpos, ypos, zpos)
    -- return RLE3 header line
    local header = "3D version=1 size="..N
    if popcount > 0 and (xpos ~= 0 or ypos ~= 0 or zpos ~= 0) then
        header = header..string.format(" pos=%d,%d,%d", xpos, ypos, zpos)
    end
    if gencount > 0 then
        header = header.." gen="..gencount
    end
    -- note that we let caller append \n if necessary
    return header
end

----------------------------------------------------------------------

function WritePattern(filepath, comments)
    local f = io.open(filepath,"w")
    if not f then return "Failed to create RLE3 file:\n"..filepath end
    
    MinimizeLiveBoundary()
    f:write(PatternHeader(minx, miny, minz), "\n")
    
    if #comments > 0 then
        -- each comment line should start with # and end with \n
        f:write(comments)
    end
    if popcount == 0 then
        f:write(string.format("x=0 y=0 z=0 rule=%s\n", rulestring))
        f:write("!\n")
    else
        local wd = maxx - minx + 1
        local ht = maxy - miny + 1
        local dp = maxz - minz + 1
        f:write(string.format("x=%d y=%d z=%d rule=%s\n", wd, ht, dp, rulestring))
    
        local line = ""
        local orun = 0
        local brun = 0
        local dollrun = 0
        local slashrun = 0
        
        local function AddRun(count, ch)
            if #line >= 67 then f:write(line,"\n"); line = "" end
            if count > 2 then
                line = line..count..ch
            elseif count == 2 then
                line = line..ch..ch
            else
                line = line..ch
            end
        end
        
        -- traverse all cells within live boundary sorted by Z,Y,X coords
        for z = minz, maxz do
            for y = miny, maxy do
                for x = minx, maxx do
                    if grid1[ x + N * (y + N * z) ] then
                        -- live cell
                        orun = orun + 1
                        if slashrun > 0 then
                            AddRun(slashrun, "/")
                            slashrun = 0
                        end
                        if dollrun > 0 then
                            AddRun(dollrun, "$")
                            dollrun = 0
                        end
                        if brun > 0 then
                            AddRun(brun, "b")
                            brun = 0
                        end
                    else
                        -- dead cell
                        brun = brun + 1
                        if orun > 0 then
                            if slashrun > 0 then
                                AddRun(slashrun, "/")
                                slashrun = 0
                            end
                            if dollrun > 0 then
                                AddRun(dollrun, "$")
                                dollrun = 0
                            end
                            AddRun(orun, "o")
                            orun = 0
                        end
                    end
                end
                if orun > 0 then
                    if slashrun > 0 then
                        AddRun(slashrun, "/")
                        slashrun = 0
                    end
                    if dollrun > 0 then
                        AddRun(dollrun, "$")
                        dollrun = 0
                    end
                    AddRun(orun, "o")
                    orun = 0
                end
                brun = 0
                dollrun = dollrun + 1
            end
            dollrun = 0
            if z < maxz then
                slashrun = slashrun + 1
            else
                if #line >= 70 then f:write(line,"\n"); line = "" end
                line = line.."!"
            end
        end
        if #line > 0 then f:write(line,"\n") end
    end
    f:close()
    return nil  -- success
end

----------------------------------------------------------------------

function GetComments(f)
    local comments = ""
    local line = f:read("*l")
    if line == nil then return comments end
    while true do
        line = f:read("*l")
        if not line then break end
        -- if line ended with CRLF then remove the CR
        line = line:gsub("\r", "")
        local ch = line:sub(1,1)
        if ch == "#" then
            comments = comments..line.."\n"
        elseif ch == "x" then
            -- end of RLE3 header info
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
            g.warn(err, false)
        else
            -- set pattname to file name at end of filepath
            pattname = filepath:match("^.+"..pathsep.."(.+)$")
            dirty = false
            Refresh()
        end
    else
        -- prompt user for file name and location
        local filetype = "RLE3 file (*.rle3)|*.rle3"
        local path = g.savedialog("Save pattern", filetype, pattdir, pattname)
        if #path > 0 then
            -- update pattdir by stripping off the file name
            pattdir = path:gsub("[^"..pathsep.."]+$","")
            -- ensure file name ends with ".rle3"
            if not path:find("%.rle3$") then path = path..".rle3" end
            -- save the current pattern
            SavePattern(path)
        end
    end
end

----------------------------------------------------------------------

function CallScript(func, fromclip)
    -- avoid infinite recursion
    if scriptlevel == 100 then
        g.warn("Script is too recursive!", false)
        return
    end
    
    if scriptlevel == 0 then
        RememberCurrentState()
        undo_cleared = false    -- becomes true if ClearUndoRedo is called
        EnableControls(false)   -- disable most menu items and buttons
    end
    
    scriptlevel = scriptlevel + 1
    local status, err = pcall(func)
    scriptlevel = scriptlevel - 1
    
    -- note that if the script called NewPattern/RandomPattern/OpenPattern
    -- or any other function that called ClearUndoRedo then all the
    -- undo/redo history is deleted and dirty is set to false,
    -- but a later SetCell call might then set dirty to true,
    -- so the following test fixes that
    if scriptlevel == 0 and undo_cleared then
        -- ensure the dirty flag is false
        dirty = false
        -- if the script called NextGeneration (via Step) after calling ClearUndoRedo
        -- (or vice versa) then startindex won't be valid, so set startcount to gencount
        -- to ensure Reset does nothing
        startcount = gencount
    end
    
    if err then
        g.continue("")
        if err == "GOLLY: ABORT SCRIPT" then
            -- user hit escape
            message = "Script aborted."
        else
            if fromclip then
                g.warn("Runtime error in clipboard script:\n\n"..err, false)
            else
                g.warn("Runtime error in script:\n\n"..err, false)
            end
        end
    end
    
    if scriptlevel == 0 then
        EnableControls(true)    -- enable menu items and buttons that were disabled above
        Refresh()               -- calls DrawToolBar
    end
end

----------------------------------------------------------------------

function RunScript(filepath)
    if filepath then
        local f, msg = loadfile(filepath)
        if f then
            CallScript(f, false)
        else
            g.warn("Syntax error in script:\n\n"..msg, false)
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
        g.warn("Syntax error in clipboard script:\n\n"..msg, false)
    end
end

----------------------------------------------------------------------

function SetStartupScript()
    -- prompt user for a .lua file to run automatically when 3D.lua starts up
    local filetype = "Lua file (*.lua)|*.lua"
    local path = g.opendialog("Select your startup script", filetype, scriptdir, "")
    if #path > 0 then
        -- update scriptdir by stripping off the file name
        scriptdir = path:gsub("[^"..pathsep.."]+$","")
        startup = path
        -- the above path will be saved by WriteSettings
    end
end

----------------------------------------------------------------------

function RandomPattern(percentage)
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
    
    if percentage then
        perc = percentage
        if perc < 0 then perc = 0 end
        if perc > 100 then perc = 100 end
    else
        -- prompt user for the percentage;
        -- if user hits Cancel button we want to avoid aborting script
        local status, err = pcall(getperc)
        if err then
            g.continue("")  -- don't show error when script finishes
            return
        end
    end
    
    pattname = "untitled"
    SetCursor(movecursor)
    gencount = 0
    startcount = 0
    StopGenerating()
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
    ClearUndoRedo()
    InitialView()       -- calls Refresh
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
        for k,_ in pairs(pastepatt) do
            -- pastepatt[k] is in paste pattern
            local x = k % N
            local y = (k // N) % N
            local z = k // NN
            pcells[#pcells+1] = {x-mid, y-mid, z-mid}
        end
    end
    return pcells
end

----------------------------------------------------------------------

function SetGridSize(newsize)
    -- change grid size to newsize or prompt user if newsize is nil
    
    local function getsize()
        ::try_again::
        local s = g.getstring("Enter the new size (from "..MINN.." to "..MAXN.."):",
                              tostring(N), "Grid size")
        if validint(s) and (tonumber(s) >= MINN) and (tonumber(s) <= MAXN) then
            newsize = tonumber(s)
        else
            g.warn("Grid size must be an integer from "..MINN.." to "..MAXN..".")
            -- note that if user hit the Cancel button then the next g.* call
            -- (in this case g.getstring) will cause pcall to abort with an error
            goto try_again
        end
    end
    
    if newsize then
        if newsize < MINN then newsize = MINN end
        if newsize > MAXN then newsize = MAXN end
    else
        -- if user hits Cancel button we want to avoid aborting script
        local status, err = pcall(getsize)
        if err then
            g.continue("")  -- don't show error when script finishes
            return
        end
    end
    
    if newsize == N then return end
    
    RememberCurrentState()
    
    -- save current pattern as an array of positions relative to mid cell
    local livecells = GetCells()
    
    -- save any selected cells as an array of positions relative to mid cell
    local oldselcount = selcount
    local selcells = GetSelectedCells()
    
    -- save any paste cells as an array of positions relative to mid cell
    local oldpastecount = pastecount
    local pcells = GetPasteCells()
    
    N = newsize
    MIDGRID = (N+1-(N%2))*HALFCELL
    MIDCELL = HALFCELL-MIDGRID
    CreateAxes()
    
    -- active plane may need adjusting
    local mid = N//2
    if (mid + activepos >= N) or (mid + activepos < 0) then
        activepos = 0
    end
    SetActivePlane(activeplane, activepos)
    
    -- restore pattern, clipping any cells outside the new grid
    ClearCells()
    local olddirty = dirty
    local clipcount = PutCells(livecells)
    if clipcount > 0 then
        dirty = true
    else
        -- PutCells sets dirty true if there are live cells,
        -- but pattern hasn't really changed if no cells were clipped
        dirty = olddirty
    end
    
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
    pastepatt = {}
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
                pastepatt[ x + N * (y + N * z) ] = true
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
    
    FitGrid()   -- calls Refresh
end

----------------------------------------------------------------------

function ChangeRule()
    -- let user enter new rule as a string of the form 3Ds,s,.../b,b,...
    -- (the notation used at http://www.cse.sc.edu/~bays/d4d4d4/)
    
    local function getrule()
        local newrule = rulestring
        ::try_again::
        newrule = g.getstring("Enter the new rule in the form 3Ds,s,s,.../b,b,b,...\n" ..
                              "where s values are the neighbor counts for survival\n" ..
                              "and b values are the neighbor counts for birth:",
                              newrule, "Change rule")
        if not ParseRule(newrule) then goto try_again end
    end
    
    local oldrule = rulestring
    
    -- if user hits Cancel button we want to avoid aborting script
    local status, err = pcall(getrule)
    if err then
        g.continue("")  -- don't show error when script finishes
        return
    end
    
    if oldrule ~= rulestring then
        -- ParseRule has set rulestring so we need to temporarily switch
        -- it back to oldrule and call RememberCurrentState
        local newrule = rulestring
        rulestring = oldrule
        RememberCurrentState()
        rulestring = newrule
    end
    
    Refresh()
end

----------------------------------------------------------------------

function CreateRLE3Selection()
    -- convert selection to lines of RLE3 data
    -- (note that selcount > 0 and at least one live cell is selected)
    MinimizeSelectionBoundary()
    local wd = maxselx - minselx + 1
    local ht = maxsely - minsely + 1
    local dp = maxselz - minselz + 1
    
    local lines = {}
    lines[1] = PatternHeader(minselx, minsely, minselz)
    lines[2] = string.format("x=%d y=%d z=%d rule=%s", wd, ht, dp, rulestring)
    
    local line = ""
    local orun = 0
    local brun = 0
    local dollrun = 0
    local slashrun = 0
    
    local function AddRun(count, ch)
        if #line >= 67 then lines[#lines+1] = line; line = "" end
        if count > 2 then
            line = line..count..ch
        elseif count == 2 then
            line = line..ch..ch
        else
            line = line..ch
        end
    end
    
    -- traverse selected cells sorted by Z,Y,X coords
    for z = minselz, maxselz do
        for y = minsely, maxsely do
            for x = minselx, maxselx do
                local pos = x + N * (y + N * z)
                if selected[pos] and grid1[pos] then
                    -- this is a selected live cell
                    orun = orun + 1
                    if slashrun > 0 then
                        AddRun(slashrun, "/")
                        slashrun = 0
                    end
                    if dollrun > 0 then
                        AddRun(dollrun, "$")
                        dollrun = 0
                    end
                    if brun > 0 then
                        AddRun(brun, "b")
                        brun = 0
                    end
                else
                    -- dead cell or unselected live cell
                    brun = brun + 1
                    if orun > 0 then
                        if slashrun > 0 then
                            AddRun(slashrun, "/")
                            slashrun = 0
                        end
                        if dollrun > 0 then
                            AddRun(dollrun, "$")
                            dollrun = 0
                        end
                        AddRun(orun, "o")
                        orun = 0
                    end
                end
            end
            if orun > 0 then
                if slashrun > 0 then
                    AddRun(slashrun, "/")
                    slashrun = 0
                end
                if dollrun > 0 then
                    AddRun(dollrun, "$")
                    dollrun = 0
                end
                AddRun(orun, "o")
                orun = 0
            end
            brun = 0
            dollrun = dollrun + 1
        end
        dollrun = 0
        if z < maxselz then
            slashrun = slashrun + 1
        else
            if #line >= 70 then lines[#lines+1] = line; line = "" end
            line = line.."!"
        end
    end
    if #line > 0 then lines[#lines+1] = line end
    
    return lines
end

----------------------------------------------------------------------

function CopySelection()
    if selcount > 0 then
        -- save the selected live cells as an RLE3 pattern in clipboard,
        -- but only if there is at least one live cell selected
        local livecells = GetSelectedLiveCells()
        if #livecells == 0 then
            message = "There are no live cells selected."
            Refresh()
            return false
        end
        local lines = CreateRLE3Selection()
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
        RememberCurrentState()
        -- kill all selected live cells
        for k,_ in pairs(selected) do
            -- selected[k] is a selected cell (live or dead)
            if grid1[k] then
                grid1[k] = nil
                popcount = popcount - 1
                dirty = true
                minimal_live_bounds = false
            end
        end
        Refresh()
    end
end

----------------------------------------------------------------------

function ClearOutside()
    if selcount > 0 then
        RememberCurrentState()
        -- kill all unselected live cells
        for k,_ in pairs(grid1) do
            -- grid1[k] is a live cell
            if not selected[k] then
                grid1[k] = nil
                popcount = popcount - 1
                dirty = true
                minimal_live_bounds = false
            end
        end
        Refresh()
    end
end

----------------------------------------------------------------------

function CutSelection()
    if selcount > 0 then
        -- save the selected live cells as an RLE3 pattern in clipboard
        -- then kill them
        if CopySelection() then
            ClearSelection()    -- calls RememberCurrentState and Refresh
            return true
        end
    end
    return false
end

----------------------------------------------------------------------

function CancelSelection()
    if selcount > 0 then
        RememberCurrentState()
        selcount = 0
        selected = {}
        InitSelectionBoundary()
        Refresh()
    end
end

----------------------------------------------------------------------

function SelectAll()
    if popcount > 0 then
        RememberCurrentState()
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
    else
        -- there are no live cells so remove any existing selection
        CancelSelection()
    end
end

----------------------------------------------------------------------

function FlipSelectionX()
    if selcount > 0 then
        RememberCurrentState()
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
                -- SetLiveCell below will set dirty = true
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
        RememberCurrentState()
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
                -- SetLiveCell below will set dirty = true
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
        RememberCurrentState()
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
                -- SetLiveCell below will set dirty = true
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
        RememberCurrentState()
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
                -- SetLiveCell below will set dirty = true
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
        RememberCurrentState()
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
                -- SetLiveCell below will set dirty = true
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
        RememberCurrentState()
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
                -- SetLiveCell below will set dirty = true
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

function CancelPaste()
    if pastecount > 0 then
        RememberCurrentState()
        pastecount = 0
        pastepatt = {}
        Refresh()
    end
end

----------------------------------------------------------------------

function Paste()
    -- if a paste pattern already exists then cancel it
    local savedstate = false
    if pastecount > 0 then
        CancelPaste()
        savedstate = true
    end

    -- if the clipboard contains a valid RLE3 pattern then create a paste pattern
    -- in middle of grid
    local filepath = CopyClipboardToFile()
    if filepath then
        local err, newpattern = ReadPattern(filepath)
        if err then
            message = "Clipboard does not contain a valid RLE3 pattern."
            Refresh()
            return false
        end
        if newpattern.newpop == 0 then
            message = "Clipboard pattern is empty."
            Refresh()
            return false
        end
        
        -- newpattern contains valid pattern, but might be too big
        local minpx = newpattern.newminx
        local minpy = newpattern.newminy
        local minpz = newpattern.newminz
        local pwd = newpattern.newmaxx - minpx + 1
        local pht = newpattern.newmaxy - minpy + 1
        local pdp = newpattern.newmaxz - minpz + 1
        if pwd > N or pht > N or pdp > N then
            message = "Clipboard pattern is too big ("..pwd.." x "..pht.." x "..pdp..")"
            Refresh()
            return false
        end
        
        if not savedstate then RememberCurrentState() end
        
        -- set pastecount and pastepatt
        pastecount = newpattern.newpop
        pastepatt = {}
        -- put paste pattern in middle of grid and update paste boundary
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
            -- (note that we add 1 to match the result of MiddlePaste)
            local x = (k % P)      - minpx + (N - pwd + 1) // 2
            local y = (k // P % P) - minpy + (N - pht + 1) // 2
            local z = (k // PP)    - minpz + (N - pdp + 1) // 2
            pastepatt[ x + N * (y + N * z) ] = 1
            if x < minpastex then minpastex = x end
            if y < minpastey then minpastey = y end
            if z < minpastez then minpastez = z end
            if x > maxpastex then maxpastex = x end
            if y > maxpastey then maxpastey = y end
            if z > maxpastez then maxpastez = z end
        end
        Refresh()
        return true
    else
        -- filepath is nil
        return false
    end
end

----------------------------------------------------------------------

function PasteOR()
    if pastecount > 0 then
        RememberCurrentState()
        -- SetLiveCell will set dirty = true
        local NN = N*N
        for k,_ in pairs(pastepatt) do
            if not grid1[k] then
                SetLiveCell(k % N, (k // N) % N, k // NN)
            end
        end
        pastecount = 0
        pastepatt = {}
        Refresh()
    end
end

----------------------------------------------------------------------

function PasteXOR()
    if pastecount > 0 then
        RememberCurrentState()
        local NN = N*N
        for k,_ in pairs(pastepatt) do
            if grid1[k] then
                grid1[k] = nil
                popcount = popcount - 1
                dirty = true
                minimal_live_bounds = false
            else
                SetLiveCell(k % N, (k // N) % N, k // NN)
            end
        end
        pastecount = 0
        pastepatt = {}
        Refresh()
    end
end

----------------------------------------------------------------------

function FlipPasteX()
    if pastecount > 0 then
        RememberCurrentState()
        -- reflect X coords across YZ plane thru middle of paste pattern
        local midx = minpastex + (maxpastex - minpastex) / 2
        local cells = {}
        local NN = N*N
        for k,_ in pairs(pastepatt) do
            local x = k % N
            local y = (k // N) % N
            local z = k // NN
            cells[#cells+1] = {round(midx*2) - x, y, z}
        end
        -- flip doesn't change paste boundary
        pastepatt = {}
        for _,xyz in ipairs(cells) do
            local x, y, z = xyz[1], xyz[2], xyz[3]
            pastepatt[x + N * (y + N * z)] = 1
        end
        Refresh()
    end
end

----------------------------------------------------------------------

function FlipPasteY()
    if pastecount > 0 then
        RememberCurrentState()
        -- reflect Y coords across XZ plane thru middle of paste pattern
        local midy = minpastey + (maxpastey - minpastey) / 2
        local cells = {}
        local NN = N*N
        for k,_ in pairs(pastepatt) do
            local x = k % N
            local y = (k // N) % N
            local z = k // NN
            cells[#cells+1] = {x, round(midy*2) - y, z}
        end
        -- flip doesn't change paste boundary
        pastepatt = {}
        for _,xyz in ipairs(cells) do
            local x, y, z = xyz[1], xyz[2], xyz[3]
            pastepatt[x + N * (y + N * z)] = 1
        end
        Refresh()
    end
end

----------------------------------------------------------------------

function FlipPasteZ()
    if pastecount > 0 then
        RememberCurrentState()
        -- reflect Z coords across XY plane thru middle of paste pattern
        local midz = minpastez + (maxpastez - minpastez) / 2
        local cells = {}
        local NN = N*N
        for k,_ in pairs(pastepatt) do
            local x = k % N
            local y = (k // N) % N
            local z = k // NN
            cells[#cells+1] = {x, y, round(midz*2) - z}
        end
        -- flip doesn't change paste boundary
        pastepatt = {}
        for _,xyz in ipairs(cells) do
            local x, y, z = xyz[1], xyz[2], xyz[3]
            pastepatt[x + N * (y + N * z)] = 1
        end
        Refresh()
    end
end

----------------------------------------------------------------------

function RotatePasteX()
    if pastecount > 0 then
        RememberCurrentState()
        -- rotate paste pattern clockwise about its X axis by 90 degrees
        local midy = minpastey + (maxpastey - minpastey) // 2
        local midz = minpastez + (maxpastez - minpastez) // 2
        local y0 = midy - midz
        local z0 = midz + midy + (maxpastez - minpastez) % 2    -- avoids drift
        local cells = {}
        local NN = N*N
        for k,_ in pairs(pastepatt) do
            local x = k % N
            local y = (k // N) % N
            local z = k // NN
            cells[#cells+1] = {x, (y0+z) % N, (z0-y) % N}
        end
        pastepatt = {}
        -- minpastex, maxpastex don't change
        minpastey = math.maxinteger
        minpastez = math.maxinteger
        maxpastey = math.mininteger
        maxpastez = math.mininteger
        for _,xyz in ipairs(cells) do
            local x, y, z = xyz[1], xyz[2], xyz[3]
            pastepatt[x + N * (y + N * z)] = 1
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
        RememberCurrentState()
        -- rotate paste pattern clockwise about its Y axis by 90 degrees
        local midx = minpastex + (maxpastex - minpastex) // 2
        local midz = minpastez + (maxpastez - minpastez) // 2
        local x0 = midx + midz + (maxpastex - minpastex) % 2    -- avoids drift
        local z0 = midz - midx
        local cells = {}
        local NN = N*N
        for k,_ in pairs(pastepatt) do
            local x = k % N
            local y = (k // N) % N
            local z = k // NN
            cells[#cells+1] = {(x0-z) % N, y, (z0+x) % N}
        end
        pastepatt = {}
        -- minpastey, maxpastey don't change
        minpastex = math.maxinteger
        minpastez = math.maxinteger
        maxpastex = math.mininteger
        maxpastez = math.mininteger
        for _,xyz in ipairs(cells) do
            local x, y, z = xyz[1], xyz[2], xyz[3]
            pastepatt[x + N * (y + N * z)] = 1
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
        RememberCurrentState()
        -- rotate paste pattern clockwise about its Z axis by 90 degrees
        local midx = minpastex + (maxpastex - minpastex) // 2
        local midy = minpastey + (maxpastey - minpastey) // 2
        local x0 = midx - midy
        local y0 = midy + midx + (maxpastey - minpastey) % 2    -- avoids drift
        local cells = {}
        local NN = N*N
        for k,_ in pairs(pastepatt) do
            local x = k % N
            local y = (k // N) % N
            local z = k // NN
            cells[#cells+1] = {(x0+y) % N, (y0-x) % N, z}
        end
        pastepatt = {}
        -- minpastez, maxpastez don't change
        minpastex = math.maxinteger
        minpastey = math.maxinteger
        maxpastex = math.mininteger
        maxpastey = math.mininteger
        for _,xyz in ipairs(cells) do
            local x, y, z = xyz[1], xyz[2], xyz[3]
            pastepatt[x + N * (y + N * z)] = 1
            if x < minpastex then minpastex = x end
            if x > maxpastex then maxpastex = x end
            if y < minpastey then minpastey = y end
            if y > maxpastey then maxpastey = y end
        end
        Refresh()
    end
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

function Zoom(newsize)
    -- zoom in/out by changing size of cells
    if newsize < MINSIZE then return end
    if newsize > MAXSIZE then return end
    CELLSIZE = newsize
    HALFCELL = CELLSIZE/2.0
    MIDGRID = (N+1-(N%2))*HALFCELL
    MIDCELL = HALFCELL-MIDGRID
    LEN = CELLSIZE-BORDER*2
    CreateAxes()
    Refresh()
end

----------------------------------------------------------------------

function StartStop()
    generating = not generating
    UpdateStartButton()
    Refresh()
    if generating and popcount > 0 then
        RememberCurrentState()
        -- EventLoop will call NextGeneration
    end
end

----------------------------------------------------------------------

function Step1()
    StopGenerating()
    -- note that NextGeneration does nothing if popcount is 0
    if popcount > 0 then
        RememberCurrentState()
    end
    NextGeneration()
end

----------------------------------------------------------------------

function Reset()
    -- user scripts cannot call Reset (they can use SaveState and RestoreState)
    if scriptlevel > 0 then return end
    
    if gencount > startcount then
        RestoreStartingGen()
        StopGenerating()
        Refresh()
    end
end

----------------------------------------------------------------------

local function GetMidPoint(x, y, z)
    -- return mid point of cell at given grid position
    x = x * CELLSIZE + MIDCELL
    y = y * CELLSIZE + MIDCELL
    z = z * CELLSIZE + MIDCELL
    -- transform point
    local newx = (x*xixo + y*xiyo + z*xizo)
    local newy = (x*yixo + y*yiyo + z*yizo)
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
        MIDCELL = HALFCELL-MIDGRID
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
        return { minx-mid, maxx-mid,
                 miny-mid, maxy-mid,
                 minz-mid, maxz-mid }
    else
        return {}
    end
end

----------------------------------------------------------------------

-- for user scripts
function GetSelectionBounds()
    if selcount > 0 then
        -- return the selection's minimal bounding box
        MinimizeSelectionBoundary()
        local mid = N//2
        return { minselx-mid, maxselx-mid,
                 minsely-mid, maxsely-mid,
                 minselz-mid, maxselz-mid }
    else
        return {}
    end
end

----------------------------------------------------------------------

-- for user scripts
function GetPasteBounds()
    if pastecount > 0 then
        -- return the paste pattern's minimal bounding box
        local mid = N//2
        return { minpastex-mid, maxpastex-mid,
                 minpastey-mid, maxpastey-mid,
                 minpastez-mid, maxpastez-mid }
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

-- for user scripts
function GetCell(x, y, z)
    local mid = N//2
    if grid1[ (x+mid) + N * ((y+mid) + N * (z+mid)) ] then
        return 1
    else
        return 0
    end
end

----------------------------------------------------------------------

-- for user scripts
function SetCell(x, y, z, state)
    local mid = N//2
    SetCellState(x+mid, y+mid, z+mid, state)
end

----------------------------------------------------------------------

-- for user scripts
function SelectCell(x, y, z)
    local mid = N//2
    SetSelection(x+mid, y+mid, z+mid, true)
end

----------------------------------------------------------------------

-- for user scripts
function SelectedCell(x, y, z)
    if selcount == 0 then
        return false
    else
        local mid = N//2
        return selected[x+mid + N * (y+mid + N * (z+mid))]
    end
end

----------------------------------------------------------------------

-- for user scripts
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
function PasteExists()
    return pastecount > 0
end

----------------------------------------------------------------------

-- for user scripts
function SelectionExists()
    return selcount > 0
end

----------------------------------------------------------------------

-- for user scripts
function DoPaste(x, y, z, mode)
    if PasteExists() then
        -- move paste pattern to x,y,z
        local mid = N//2
        local deltax = x+mid - minpastex
        local deltay = y+mid - minpastey
        local deltaz = z+mid - minpastez
        MovePastePattern(deltax, deltay, deltaz)
        -- now do the paste using the given mode
        if mode == "or" then
            PasteOR()
        elseif mode == "xor" then
            PasteXOR()
        else
            error("Bad mode in DoPaste!", 2)
        end
    end
end

----------------------------------------------------------------------

-- for user scripts
function FlipPaste(coordinates)
    if     coordinates == "x" then FlipPasteX()
    elseif coordinates == "y" then FlipPasteY()
    elseif coordinates == "z" then FlipPasteZ()
    else
        error("Bad coordinates in FlipPaste!", 2)
    end
end

----------------------------------------------------------------------

-- for user scripts
function RotatePaste(axis)
    if     axis == "x" then RotatePasteX()
    elseif axis == "y" then RotatePasteY()
    elseif axis == "z" then RotatePasteZ()
    else
        error("Bad axis in RotatePaste!", 2)
    end
end

----------------------------------------------------------------------

-- for user scripts
function FlipSelection(coordinates)
    if     coordinates == "x" then FlipSelectionX()
    elseif coordinates == "y" then FlipSelectionY()
    elseif coordinates == "z" then FlipSelectionZ()
    else
        error("Bad coordinates in FlipSelection!", 2)
    end
end

----------------------------------------------------------------------

-- for user scripts
function RotateSelection(axis)
    if     axis == "x" then RotateSelectionX()
    elseif axis == "y" then RotateSelectionY()
    elseif axis == "z" then RotateSelectionZ()
    else
        error("Bad axis in RotateSelection!", 2)
    end
end

----------------------------------------------------------------------

-- for user scripts
function Update()
    Refresh(true)
end

----------------------------------------------------------------------

function ShowHelp()
    local htmldata = [[
<html><title>Golly Help: 3D.lua</title>
<body bgcolor="#FFFFCE">

<p>
<dd><a href="#intro"><b>Introduction</b></a></dd>
<dd><a href="#mouse"><b>Mouse controls</b></a></dd>
<dd><a href="#keyboard"><b>Keyboard shortcuts</b></a></dd>
<dd><a href="#menus"><b>Menus</b></a></dd>
<dd>&nbsp;&nbsp;&nbsp;&nbsp; <a href="#file"><b>File menu</b></a></dd>
<dd>&nbsp;&nbsp;&nbsp;&nbsp; <a href="#edit"><b>Edit menu</b></a></dd>
<dd>&nbsp;&nbsp;&nbsp;&nbsp; <a href="#control"><b>Control menu</b></a></dd>
<dd>&nbsp;&nbsp;&nbsp;&nbsp; <a href="#view"><b>View menu</b></a></dd>
<dd><a href="#scripts"><b>Running scripts</b></a></dd>
<dd>&nbsp;&nbsp;&nbsp;&nbsp; <a href="#shortcuts"><b>Creating your own keyboard shortcuts</b></a></dd>
<dd>&nbsp;&nbsp;&nbsp;&nbsp; <a href="#functions"><b>Script functions</b></a></dd>
<dd>&nbsp;&nbsp;&nbsp;&nbsp; <a href="#coords"><b>Cell coordinates</b></a></dd>
<dd><a href="#rules"><b>Supported rules</b></a></dd>
<dd><a href="#rle3"><b>RLE3 file format</b></a></dd>
<dd><a href="#refs"><b>Credits and references</b></a></dd>
</p>

<p><a name="intro"></a><br>
<font size=+1><b>Introduction</b></font>

<p>
3D.lua is a script that lets you explore three-dimensional cellular automata.
The script uses overlay commands to completely replace Golly's usual
interface (note that all your Golly settings will be restored when 3D.lua exits).

<p><a name="mouse"></a><br>
<font size=+1><b>Mouse controls</b></font>

<p>
If the Move option is ticked then you can use the hand cursor
to rotate the view by clicking and dragging.
Rotation occurs around the middle cell in the grid.

<p>
The hand cursor can also be used to move a paste pattern (red cells)
or a selection (green cells) to a new location within the grid, but only
within a certain plane, depending on where the initial click occurred.
Imagine a box enclosing the paste pattern or all the selected cells.
One of the faces of this box will contain the initial click.
Movement is only allowed within the plane parallel to the clicked face.

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
Click and drag outside the active plane to rotate the view.

<p>
If the Select option is ticked then you can use the cross-hairs cursor
to select or deselect cells in the active plane.
Any selected cells outside the active plane are drawn as translucent green points.
Click and drag outside the active plane to rotate the view.

<p>
To move the active plane to a different position, shift-click anywhere in the
active plane and drag the mouse.  Or you can hit the "," or "." keys.
Hit shift-A to change the active plane's orientation.

<p>
If a paste pattern is visible (red cells) then you can control-click or
right-click anywhere, using any cursor, to get a red pop-up menu that lets
you choose various paste actions.
The paste pattern can also be dragged to a different location using any cursor.

<p>
If a selection exists (green cells) then you can control-click or right-click
to get a green pop-up menu with various selection actions.
If a paste pattern and a selection both exist then the paste menu takes precedence.

<p>
Use the mouse wheel at any time to zoom in/out.
The zoom is always centered on the middle cell in the grid.

<p><a name="keyboard"></a><br>
<font size=+1><b>Keyboard shortcuts</b></font>

<p>
You can use the following keyboard shortcuts (but see <a href="#shortcuts">below</a>
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
<tr><td align=right> P &nbsp;</td><td>&nbsp; cycle live cells (cubes/spheres/points) </td></tr>
<tr><td align=right> L &nbsp;</td><td>&nbsp; toggle lattice lines </td></tr>
<tr><td align=right> shift-L &nbsp;</td><td>&nbsp; toggle axes and lattice lines </td></tr>
<tr><td align=right> alt-D &nbsp;</td><td>&nbsp; toggle depth shading </td></tr>
<tr><td align=right> T &nbsp;</td><td>&nbsp; toggle the menu bar and tool bar </td></tr>
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
<tr><td align=right> Q &nbsp;</td><td>&nbsp; quit 3D.lua </td></tr>
</table>
</center>

<p><a name="menus"></a><br>
<font size=+1><b>Menus</b></font>

<p>
3D.lua has its own menu bar.  It contains menus with items that are
somewhat similar to those in Golly's menu bar.

<p><a name="file"></a><br>
<font size=+1><b>File menu</b></font>

<a name="new"></a><p><dt><b>New Pattern</b></dt>
<dd>
Create a new, empty pattern.
All undo/redo history is deleted.
The active plane is displayed, ready to be edited using the pencil cursor.
</dd>

<a name="rand"></a><p><dt><b>Random Pattern...</b></dt>
<dd>
Create a new pattern randomly filled with live cells at a given density.
All undo/redo history is deleted.
</dd>

<a name="open"></a><p><dt><b>Open Pattern...</b></dt>
<dd>
Open a selected <a href="#rle3">RLE3</a> pattern file.
All undo/redo history is deleted.
</dd>

<a name="openclip"></a><p><dt><b>Open Clipboard</b></dt>
<dd>
Open the <a href="#rle3">RLE3</a> pattern stored in the clipboard.
All undo/redo history is deleted.
</dd>

<a name="save"></a><p><dt><b>Save Pattern...</b></dt>
<dd>
Save the current pattern in a <a href="#rle3">.rle3</a> file.
</dd>

<a name="run"></a><p><dt><b>Run Script...</b></dt>
<dd>
Run a selected Lua script.
</dd>

<a name="runclip"></a><p><dt><b>Run Clipboard</b></dt>
<dd>
Run the Lua code stored in the clipboard.
</dd>

<a name="startup"></a><p><dt><b>Set Startup Script...</b></dt>
<dd>
Select a Lua script that will be run automatically every time 3D.lua starts up.
</dd>

<a name="exit"></a><p><dt><b>Exit 3D.lua</b></dt>
<dd>
Terminate 3D.lua.  If there are any unsaved changes (indicated by an asterisk at
the start of the pattern name) then you'll be asked if you really want to exit.
Note that hitting the escape key will immediately abort 3D.lua without
doing any check for unsaved changes.
</dd>

<p><a name="edit"></a><br>
<font size=+1><b>Edit menu</b></font>

<a name="undo"></a><p><dt><b>Undo</b></dt>
<dd>
Undo the most recent change.  This could be an editing change or a generating change.
</dd>

<a name="redo"></a><p><dt><b>Redo</b></dt>
<dd>
Redo the most recently undone change.
</dd>

<a name="cut"></a><p><dt><b>Cut</b></dt>
<dd>
Copy all selected live cells to the clipboard in <a href="#rle3">RLE3</a> format,
then delete those cells (but they remain selected).
</dd>

<a name="copy"></a><p><dt><b>Copy</b></dt>
<dd>
Copy all selected live cells to the clipboard in <a href="#rle3">RLE3</a> format.
</dd>

<a name="paste"></a><p><dt><b>Paste</b></dt>
<dd>
If the clipboard contains a valid, non-empty <a href="#rle3">RLE3</a> pattern that fits
within the current grid then a paste pattern (comprised of red cells) is created in the
middle of the grid.  You can use any cursor to drag the paste pattern to any position
within the grid, then control-click or right-click anywhere to get a pop-up menu that lets
you flip/rotate the paste pattern or paste it into the grid using either OR mode or XOR mode.
</dd>

<a name="cancelpaste"></a><p><dt><b>Cancel Paste</b></dt>
<dd>
Remove the paste pattern.
</dd>

<a name="clear"></a><p><dt><b>Clear</b></dt>
<dd>
Delete all the selected live cells.
</dd>

<a name="outside"></a><p><dt><b>Clear Outside</b></dt>
<dd>
Delete all the live cells that are not selected.
</dd>

<a name="selall"></a><p><dt><b>Select All</b></dt>
<dd>
Select all the live cells.  Selected cells appear green.
Assuming there is no paste pattern, you can control-click or right-click anywhere to get
a pop-up menu that lets you perform various actions on the selection.
</dd>

<a name="cancelsel"></a><p><dt><b>Cancel Selection</b></dt>
<dd>
Remove the selection.
</dd>

<a name="midpatt"></a><p><dt><b>Middle Pattern</b></dt>
<dd>
Move the pattern to the middle of the grid.
</dd>

<a name="midsel"></a><p><dt><b>Middle Selection</b></dt>
<dd>
Move the selection to the middle of the grid.
</dd>

<a name="midpaste"></a><p><dt><b>Middle Paste</b></dt>
<dd>
Move the paste pattern to the middle of the grid.
</dd>

<p><a name="control"></a><br>
<font size=+1><b>Control menu</b></font>

<a name="startstop"></a><p><dt><b>Start/Stop Generating</b></dt>
<dd>
Start or stop generating the current pattern.
You can only start generating if there is at least one live cell.
Generating stops automatically if the pattern dies out.
</dd>

<a name="next"></a><p><dt><b>Next Generation</b></dt>
<dd>
Calculate and display the next generation (but only if there
is at least one live cell).
</dd>

<a name="reset"></a><p><dt><b>Reset</b></dt>
<dd>
Restore the starting generation.
The undo history is automatically rewound to the correct place.
</dd>

<a name="setrule"></a><p><dt><b>Set Rule</b></dt>
<dd>
Show a dialog box that lets you change the current <a href="#rules">rule</a>.
</dd>

<p><a name="view"></a><br>
<font size=+1><b>View menu</b></font>

<a name="initial"></a><p><dt><b>Initial View</b></dt>
<dd>
Restore the scale and rotation used when 3D.lua started up.
Note that if you hit the up arrow 4 times and the right arrow 4 times
then you'll see a single XY face parallel with the screen.
</dd>

<a name="fit"></a><p><dt><b>Fit Grid</b></dt>
<dd>
Change the scale so the entire grid just fits within the window.
</dd>

<a name="gridsize"></a><p><dt><b>Set Grid Size</b></dt>
<dd>
Show a dialog box that lets you change the grid size.
If the current pattern doesn't fit inside the new size then
you'll see a message stating how many live cells were clipped.
</dd>

<a name="cubes"></a><p><dt><b>Cubes</b></dt>
<dd>
If ticked then live cells are displayed as cubes.
</dd>

<a name="spheres"></a><p><dt><b>Spheres</b></dt>
<dd>
If ticked then live cells are displayed as spheres.
</dd>

<a name="points"></a><p><dt><b>Points</b></dt>
<dd>
If ticked then live cells are displayed as points.
Note that if the active plane is shown then any live cells outside
that plane are always displayed as points.
</dd>

<a name="axes"></a><p><dt><b>Show Axes</b></dt>
<dd>
If ticked then the edges of the grid are displayed
(X axes are red, Y axes are green, Z axes are blue).
</dd>

<a name="lines"></a><p><dt><b>Show Lattice Lines</b></dt>
<dd>
If ticked (and Show Axes is ticked) then lattice lines are displayed
on the three faces of the grid that intersect at the corner with
minimum <a href="#coords">cell coordinates</a>
(the far, bottom left corner in the initial view).
</dd>

<a name="shading"></a><p><dt><b>Use Depth Shading</b></dt>
<dd>
If ticked then live cells are drawn slightly darker the further
away they are from the front of the screen.
</dd>

<a name="help"></a><p><dt><b>Help</b></dt>
<dd>
Show this help.
</dd>

<p><a name="scripts"></a><br>
<font size=+1><b>Running scripts</b></font>

<p>
3D.lua can run other Lua scripts, either by selecting File > Run Script
and choosing a .lua file, or by copying Lua code to the clipboard and
selecting File > Run Clipboard.  Try the latter method with this example
that creates a small random pattern in the middle of the grid:

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

<p><a name="shortcuts"></a><br>
<font size=+1><b>Creating your own keyboard shortcuts</b></font>

<p>
It's possible to override any of the global functions in 3D.lua.
The following script shows how to override the HandleKey function
to create a keyboard shortcut for running a particular script.
Even more useful, you can get 3D.lua to run this script automatically
when it starts up by going to File > Set Startup Script and selecting
a .lua file containing this code:

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

<p><a name="functions"></a><br>
<font size=+1><b>Script functions</b></font>

<p>
Here is an alphabetical list of the various functions in 3D.lua you might
want to call from your own scripts:
<p>
<dd>
<table cellspacing=0 cellpadding=0>
<tr>
<td valign=top>
<a href="#CancelPaste"><b>CancelPaste</b></a><br>
<a href="#CancelSelection"><b>CancelSelection</b></a><br>
<a href="#ClearOutside"><b>ClearOutside</b></a><br>
<a href="#ClearSelection"><b>ClearSelection</b></a><br>
<a href="#CopySelection"><b>CopySelection</b></a><br>
<a href="#CutSelection"><b>CutSelection</b></a><br>
<a href="#DeselectCell"><b>DeselectCell</b></a><br>
<a href="#DoPaste"><b>DoPaste</b></a><br>
<a href="#DrawMode"><b>DrawMode</b></a><br>
<a href="#FitGrid"><b>FitGrid</b></a><br>
<a href="#FlipPaste"><b>FlipPaste</b></a><br>
<a href="#FlipSelection"><b>FlipSelection</b></a>
</td>
<td valign=top width=30> </td>
<td valign=top>
<a href="#GetBounds"><b>GetBounds</b></a><br>
<a href="#GetCell"><b>GetCell</b></a><br>
<a href="#GetGeneration"><b>GetGeneration</b></a><br>
<a href="#GetGridSize"><b>GetGridSize</b></a><br>
<a href="#GetPasteBounds"><b>GetPasteBounds</b></a><br>
<a href="#GetPercentage"><b>GetPercentage</b></a><br>
<a href="#GetPopulation"><b>GetPopulation</b></a><br>
<a href="#GetRule"><b>GetRule</b></a><br>
<a href="#GetSelectionBounds"><b>GetSelectionBounds</b></a><br>
<a href="#InitialView"><b>InitialView</b></a><br>
<a href="#MoveMode"><b>MoveMode</b></a><br>
<a href="#NewPattern"><b>NewPattern</b></a>
</td>
<td valign=top width=30> </td>
<td valign=top>
<a href="#OpenPattern"><b>OpenPattern</b></a><br>
<a href="#Paste"><b>Paste</b></a><br>
<a href="#PasteExists"><b>PasteExists</b></a><br>
<a href="#RandomPattern"><b>RandomPattern</b></a><br>
<a href="#RestoreState"><b>RestoreState</b></a><br>
<a href="#Rotate"><b>Rotate</b></a><br>
<a href="#RotatePaste"><b>RotatePaste</b></a><br>
<a href="#RotateSelection"><b>RotateSelection</b></a><br>
<a href="#RunScript"><b>RunScript</b></a><br>
<a href="#SavePattern"><b>SavePattern</b></a><br>
<a href="#SaveState"><b>SaveState</b></a><br>
<a href="#SelectAll"><b>SelectAll</b></a>
</td>
<td valign=top width=30> </td>
<td valign=top>
<a href="#SelectCell"><b>SelectCell</b></a><br>
<a href="#SelectedCell"><b>SelectedCell</b></a><br>
<a href="#SelectionExists"><b>SelectionExists</b></a><br>
<a href="#SelectMode"><b>SelectMode</b></a><br>
<a href="#SetCell"><b>SetCell</b></a><br>
<a href="#SetGridSize"><b>SetGridSize</b></a><br>
<a href="#SetMessage"><b>SetMessage</b></a><br>
<a href="#SetRule"><b>SetRule</b></a><br>
<a href="#Step"><b>Step</b></a><br>
<a href="#Update"><b>Update</b></a>
</td>
</tr>
</table>
</dd>
</p>

<a name="CancelPaste"></a><p><dt><b>CancelPaste()</b></dt>
<dd>
Remove any existing paste pattern.
</dd>

<a name="CancelSelection"></a><p><dt><b>CancelSelection()</b></dt>
<dd>
Deselect all selected cells.
</dd>

<a name="ClearOutside"></a><p><dt><b>ClearOutside()</b></dt>
<dd>
Delete all live cells that are not selected.
</dd>

<a name="ClearSelection"></a><p><dt><b>ClearSelection()</b></dt>
<dd>
Delete all live cells that are selected.
Note that the cells remain selected.
</dd>

<a name="CopySelection"></a><p><dt><b>CopySelection()</b></dt>
<dd>
Return true if all the selected live cells can be saved in the clipboard
as an RLE3 pattern.
Return false if there are no selected live cells.
</dd>

<a name="CutSelection"></a><p><dt><b>CutSelection()</b></dt>
<dd>
Return true if all the selected live cells can be saved in the clipboard
as an RLE3 pattern.  If so then all the selected live cells are deleted.
Return false if there are no selected live cells.
</dd>

<a name="DeselectCell"></a><p><dt><b>DeselectCell(<i>x, y, z</i>)</b></dt>
<dd>
Deselect the given cell.
The x,y,z coordinates are relative to the middle cell in the grid.
</dd>

<a name="DoPaste"></a><p><dt><b>DoPaste(<i>x, y, z, mode</i>)</b></dt>
<dd>
If a paste pattern exists then move it to the given position
and paste it into the grid using the given mode ("or" or "xor").
The x,y,z coordinates are relative to the middle cell in the grid
and specify the desired position of the paste boundary's minimum corner.
</dd>

<a name="DrawMode"></a><p><dt><b>DrawMode()</b></dt>
<dd>
Switch to the pencil cursor.
The next <a href="#Update">Update</a> call will display the active plane.
</dd>

<a name="FitGrid"></a><p><dt><b>FitGrid()</b></dt>
<dd>
Zoom in or out so that the entire grid will be visible.
Call <a href="#Update">Update</a> to see the result.
</dd>

<a name="FlipPaste"></a><p><dt><b>FlipPaste(<i>coordinates</i>)</b></dt>
<dd>
If the paste pattern exists then flip the given coordinates ("x", "y" or "z").
For example, if given "x" then the X coordinates of all cells in the paste pattern will be
reflected across the YZ plane running through the middle of the paste pattern.
</dd>

<a name="FlipSelection"></a><p><dt><b>FlipSelection(<i>coordinates</i>)</b></dt>
<dd>
If a selection exists then flip the given coordinates ("x", "y" or "z").
For example, if given "x" then the X coordinates of all selected cells will be
reflected across the YZ plane running through the middle of the selection.
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

<a name="GetPasteBounds"></a><p><dt><b>GetPasteBounds()</b></dt>
<dd>
Return {} if there is no paste pattern, otherwise return its minimal bounding box
as an array with 6 values: {minx, maxx, miny, maxy, minz, maxz}.
The boundary values are relative to the middle cell in the grid.
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

<a name="GetSelectionBounds"></a><p><dt><b>GetSelectionBounds()</b></dt>
<dd>
Return {} if there are no selected cells, otherwise return the minimal bounding box
of all selected cells (live or dead) as an array with 6 values: {minx, maxx, miny, maxy, minz, maxz}.
The boundary values are relative to the middle cell in the grid.
</dd>

<a name="InitialView"></a><p><dt><b>InitialView()</b></dt>
<dd>
Restore the initial view displayed by 3D.lua when it first starts up.
Call <a href="#Update">Update</a> to see the result.
</dd>

<a name="MoveMode"></a><p><dt><b>MoveMode()</b></dt>
<dd>
Switch to the hand cursor.
</dd>

<a name="NewPattern"></a><p><dt><b>NewPattern()</b></dt>
<dd>
Create a new, empty pattern.
All undo/redo history is deleted.
</dd>

<a name="OpenPattern"></a><p><dt><b>OpenPattern(<i>filepath</i>)</b></dt>
<dd>
Open the specified RLE3 pattern file.  If the <i>filepath</i> is not supplied then
the user will be prompted to select a .rle3 file.
All undo/redo history is deleted.
</dd>

<a name="Paste"></a><p><dt><b>Paste()</b></dt>
<dd>
Return true if the clipboard contains a valid, non-empty RLE3 pattern that fits
within the current grid.  If so then a paste pattern is created in the middle
of the grid.  You can then call <a href="#FlipPaste">FlipPaste</a> or
<a href="#RotatePaste">RotatePaste</a> to modify the paste pattern.
Call <a href="#DoPaste">DoPaste</a> when you want to actually paste the pattern into the grid.
</dd>

<a name="PasteExists"></a><p><dt><b>PasteExists()</b></dt>
<dd>
Return true if a paste pattern exists.
</dd>

<a name="RandomPattern"></a><p><dt><b>RandomPattern(<i>percentage</i>)</b></dt>
<dd>
Create a new, random pattern with the given percentage density (0 to 100) of live cells.
If the <i>percentage</i> is not supplied then the user will be prompted for it.
All undo/redo history is deleted.
</dd>

<a name="RestoreState"></a><p><dt><b>RestoreState(<i>state</i>)</b></dt>
<dd>
Restore the state saved earlier by <a href="#SaveState">SaveState</a>.
</dd>

<a name="Rotate"></a><p><dt><b>Rotate(<i>xdegrees, ydegrees, zdegrees</i>)</b></dt>
<dd>
Rotate the grid axes by the given amounts (integers from -359 to +359).
Call <a href="#Update">Update</a> to see the result.
</dd>

<a name="RotatePaste"></a><p><dt><b>RotatePaste(<i>axis</i>)</b></dt>
<dd>
If the paste pattern exists then rotate it 90 degrees clockwise
about the given axis ("x", "y" or "z").
</dd>

<a name="RotateSelection"></a><p><dt><b>RotateSelection(<i>axis</i>)</b></dt>
<dd>
If a selection exists then rotate it 90 degrees clockwise
about the given axis ("x", "y" or "z").
</dd>

<a name="RunScript"></a><p><dt><b>RunScript(<i>filepath</i>)</b></dt>
<dd>
Run the specified .lua file.  If the <i>filepath</i> is not supplied then
the user will be prompted to select a .lua file.
</dd>

<a name="SavePattern"></a><p><dt><b>SavePattern(<i>filepath</i>)</b></dt>
<dd>
Save the current pattern in a specified RLE3 file.  If the <i>filepath</i>
is not supplied then the user will be prompted for its name and location.
</dd>

<a name="SaveState"></a><p><dt><b>SaveState()</b></dt>
<dd>
Return an object representing the current state.  The object can be given
later to <a href="#RestoreState">RestoreState</a> to restore the saved state.
The saved information includes the grid size, the active plane's orientation
and position, the cursor mode, the rule, the pattern and its generation count,
the selection, and the paste pattern.
</dd>

<a name="SelectAll"></a><p><dt><b>SelectAll()</b></dt>
<dd>
Select all live cells.  If there are no live cells then any existing selection
is removed.
</dd>

<a name="SelectCell"></a><p><dt><b>SelectCell(<i>x, y, z</i>)</b></dt>
<dd>
Select the given cell.
The x,y,z coordinates are relative to the middle cell in the grid.
</dd>

<a name="SelectedCell"></a><p><dt><b>SelectedCell(<i>x, y, z</i>)</b></dt>
<dd>
Return true if the given cell is selected (live or dead).
</dd>

<a name="SelectionExists"></a><p><dt><b>SelectionExists()</b></dt>
<dd>
Return true if at least one cell is selected (live or dead).
</dd>

<a name="SelectMode"></a><p><dt><b>SelectMode()</b></dt>
<dd>
Switch to the cross-hairs cursor.
The next <a href="#Update">Update</a> call will display the active plane.
</dd>

<a name="SetCell"></a><p><dt><b>SetCell(<i>x, y, z, state</i>)</b></dt>
<dd>
Set the given cell to the given state (0 or 1).
The x,y,z coordinates are relative to the middle cell in the grid.
</dd>

<a name="SetGridSize"></a><p><dt><b>SetGridSize(<i>newsize</i>)</b></dt>
<dd>
Change the grid size to the new value (3 to 100).
If the <i>newsize</i> is not supplied then the user will be prompted for a value.
</dd>

<a name="SetMessage"></a><p><dt><b>SetMessage(<i>msg</i>)</b></dt>
<dd>
The given string will be displayed by the next <a href="#Update">Update</a> call.
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

<p><a name="coords"></a><br>
<font size=+1><b>Cell coordinates</b></font>

<p>
Many of the above script functions accept or return cell coordinates.
All coordinates are relative to the middle cell in the grid, so a call
like SetCell(0,0,0,1) will turn on the middle cell.
If N is the grid size then the minimum coordinate value is -floor(N/2)
and the maximum coordinate value is floor((N-1)/2).

<p>
The following snippet creates a diagonal line of cells from the
grid corner with the minimum cell coordinates to the corner with
the maximum cell coordinates:

<dd><table border=0><pre>
local N = GetGridSize()
for c = -(N//2), (N-1)//2 do
    SetCell(c, c, c, 1)
end</pre></table></dd>

<p><a name="rules"></a><br>
<font size=+1><b>Supported rules</b></font>

<p>
Rules are strings of the form "3DS,S,S,.../B,B,B,...".
The S values are the counts of neighboring live cells required
for a live cell to survive in the next generation.
The B values are the counts of neighboring live cells required for
birth; ie. a dead cell will become a live cell in the next generation.
Each cell has 26 neighbors so the S counts are from 0 to 26
and the B counts are from 1 to 26 (birth on 0 is not allowed).
Use Control > Set Rule to change the current rule.

<p><a name="rle3"></a><br>
<font size=+1><b>RLE3 file format</b></font>

<p>
3D.lua can read and write patterns as text files with a .rle3 extension.
The format is known as RLE3 and is a simple extension of the well-known
RLE format used by Golly:

<p>
<ul>
<li>
The first line must start with "3D" and be followed by a number of "keyword=value"
pairs separated by spaces.  The valid keywords are:

<p><table border=0 cellspacing=0 cellpadding=0>
<tr><td> version=<i>i</i> </td> <td> &nbsp; &mdash; specifies the file format version (currently 1)</td></tr>
<tr><td> size=<i>N</i> </td>    <td> &nbsp; &mdash; specifies the grid dimensions (<i>N</i>x<i>N</i>x<i>N</i>)</td></tr>
<tr><td> pos=<i>x,y,z</i> </td> <td> &nbsp; &mdash; specifies the pattern's position within the grid</td></tr>
<tr><td> gen=<i>g</i> </td>     <td> &nbsp; &mdash; specifies the generation number</td></tr>
<tr><td> &nbsp; </td></tr>
</table>

<li>
If the pos and gen keywords are not present then their values are set to 0.
Any unknown keywords are simply ignored.
<li>
The first line can be followed by optional comment lines starting with "#".
<li>
Then comes a line specifying the pattern's size and the 3D rule string:

<p><table border=0 cellspacing=0 cellpadding=0>
<tr><td> x=<i>width</i> y=<i>height</i> z=<i>depth</i> rule=<i>string</i></td></tr>
<tr><td> &nbsp; </td></tr>
</table>

<li>
The remaining lines contain the pattern data in a run-length encoded format.
The only difference to the standard RLE format is the use of "/"
to move to the next plane (ie. increase the z coordinate).
<li>
Any empty lines (after the first line) are ignored.
</ul>

<p>
The following is a small example of the RLE3 file format.
You can either save it in a .rle3 file, or copy it to the clipboard
and type shift-O (after returning to the 3D.lua window):

<dd><table border=0><pre>
3D version=1 size=40 pos=19,18,18
# A 10c/10 orthogonal spaceship.
# Found by Andrew Trevorrow in April, 2018.
x=2 y=4 z=4 rule=3D4,7/5,8
$bo$bo/bo$bo$bo$oo/oo$bo$bo$bo/$bo$bo!</pre></table></dd>

<p><a name="refs"></a><br>
<font size=+1><b>Credits and references</b></font>

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
        g.warn("Failed to create 3D.html!", false)
        return
    end
    f:write(htmldata)
    f:close()
    
    g.open(htmlfile)
end

--------------------------------------------------------------------------------

function SetCursor(cursor)
    if currcursor ~= cursor then
        RememberCurrentState()
        currcursor = cursor
        if not arrow_cursor then ov("cursor "..currcursor) end
    end
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

function SetCellType(newtype)
    if newtype == "cube" then
        celltype = newtype
    elseif newtype == "sphere" then
        celltype = newtype
    elseif newtype == "point" then
        celltype = newtype
    end
    Refresh()
end

----------------------------------------------------------------------

function ToggleAxes()
    showaxes = not showaxes
    Refresh()
end

----------------------------------------------------------------------

function ToggleLines()
    showlines = not showlines
    Refresh()
end

----------------------------------------------------------------------

function ToggleDepthShading()
    depthshading = not depthshading
    Refresh()
end

----------------------------------------------------------------------

function ToggleToolBar()
    if toolbarht > 0 then
        toolbarht = 0
        midy = int(ovht/2)
        -- hide all the controls
        mbar.hide()
        ssbutton.hide()
        s1button.hide()
        resetbutton.hide()
        fitbutton.hide()
        undobutton.hide()
        redobutton.hide()
        drawbox.hide()
        selectbox.hide()
        movebox.hide()
        exitbutton.hide()
        helpbutton.hide()
    else
        toolbarht = mbarht+buttonht+gap*2
        midy = int(ovht/2 + toolbarht/2)
    end
    Refresh()
end

----------------------------------------------------------------------

function ExitScript()
    local function savechanges()
        -- probably need a g.savechanges command so user sees the proper dialog!!!
        g.warn("There are unsaved changes.\n" ..
               "Do you really want to exit 3D.lua?")
        -- need to call a g.* command so error is returned by pcall if user hit Cancel
        g.doevent("")
    end

    if dirty and scriptlevel == 0 then
        -- ask user if they really want to exit
        local status, err = pcall(savechanges)
        if err then
            g.continue("")
            return          -- user hit Cancel
        end
    end
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
        RememberCurrentState()
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
            dirty = true
            minimal_live_bounds = false
            drawstate = 0
        else
            -- birth
            SetLiveCell(x, y, z)    -- sets dirty = true
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
        RememberCurrentState()
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

function StartDraggingPaste(mousex, mousey)
    -- test if mouse click is in a paste cell
    local NN = N*N
    for k,_ in pairs(pastepatt) do
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
    pastepatt = {}
    for _, xyz in ipairs(pcells) do
        local x = xyz[1] + mid + deltax
        local y = xyz[2] + mid + deltay
        local z = xyz[3] + mid + deltaz
        pastepatt[ x + N * (y + N * z) ] = true
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
    MIDCELL = HALFCELL-MIDGRID
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
    MIDCELL = HALFCELL-MIDGRID
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
                RememberCurrentState()
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
    
    -- RememberCurrentState was called in StartDraggingSelection

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
            dirty = true
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
                dirty = true
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
                dirty = true
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
    MIDCELL = HALFCELL-MIDGRID
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
    MIDCELL = HALFCELL-MIDGRID
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
    if #activecell > 0 then
        RememberCurrentState()
        return true
    else
        return false
    end
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
    MIDCELL = HALFCELL-MIDGRID
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
    MIDCELL = HALFCELL-MIDGRID
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

function MiddlePaste()
    -- move paste pattern to middle of grid
    if pastecount > 0 then
        -- calculate the delta amounts needed to move paste pattern to middle of grid
        local deltax = (N - (maxpastex - minpastex)) // 2 - minpastex
        local deltay = (N - (maxpastey - minpastey)) // 2 - minpastey
        local deltaz = (N - (maxpastez - minpastez)) // 2 - minpastez
        if deltax == 0 and deltay == 0 and deltaz == 0 then return end
        
        RememberCurrentState()
        local pcells = {}
        local NN = N*N
        for k,_ in pairs(pastepatt) do
            pcells[#pcells+1] = {k % N, (k // N) % N, k // NN}
        end
        pastepatt = {}
        for _,xyz in ipairs(pcells) do
            local x = xyz[1] + deltax
            local y = xyz[2] + deltay
            local z = xyz[3] + deltaz
            pastepatt[x + N * (y + N * z)] = true
        end
        
        -- update the paste boundary
        minpastex = minpastex + deltax
        minpastey = minpastey + deltay
        minpastez = minpastez + deltaz
        maxpastex = maxpastex + deltax
        maxpastey = maxpastey + deltay
        maxpastez = maxpastez + deltaz
        
        Refresh()
    end
end

----------------------------------------------------------------------

function MiddleSelection()
    -- move selection to middle of grid
    if selcount > 0 then
        MinimizeSelectionBoundary()
        -- calculate the delta amounts needed to move selection to middle of grid
        local deltax = (N - (maxselx - minselx)) // 2 - minselx
        local deltay = (N - (maxsely - minsely)) // 2 - minsely
        local deltaz = (N - (maxselz - minselz)) // 2 - minselz
        if deltax == 0 and deltay == 0 and deltaz == 0 then return end
        
        RememberCurrentState()
        -- only set dirty = true if live cells are selected
        local selcells = {}
        local livecells = {}    -- for live cells in selection
        local NN = N*N
        for k,_ in pairs(selected) do
            local x = k % N
            local y = (k // N) % N
            local z = k // NN
            selcells[#selcells+1] = {x, y, z}
            if grid1[k] then
                grid1[k] = nil
                popcount = popcount - 1
                dirty = true
                minimal_live_bounds = false
                livecells[#livecells+1] = {x, y, z}
            end
        end
        selected = {}
        for _,xyz in ipairs(selcells) do
            local x = xyz[1] + deltax
            local y = xyz[2] + deltay
            local z = xyz[3] + deltaz
            selected[x + N * (y + N * z)] = true
        end
        -- move live cells that were selected
        for _,xyz in ipairs(livecells) do
            local x = xyz[1] + deltax
            local y = xyz[2] + deltay
            local z = xyz[3] + deltaz
            local k = x + N * (y + N * z)
            if not grid1[k] then
                grid1[k] = 1
                popcount = popcount + 1
                -- dirty set to true above
                -- boundary might expand
                if x < minx then minx = x end
                if y < miny then miny = y end
                if z < minz then minz = z end
                if x > maxx then maxx = x end
                if y > maxy then maxy = y end
                if z > maxz then maxz = z end
            end
        end
        
        -- update the selection boundary
        minselx = minselx + deltax
        minsely = minsely + deltay
        minselz = minselz + deltaz
        maxselx = maxselx + deltax
        maxsely = maxsely + deltay
        maxselz = maxselz + deltaz
        -- MinimizeSelectionBoundary set minimal_sel_bounds to true
        
        Refresh()
    end
end

----------------------------------------------------------------------

function MiddlePattern()
    -- move pattern to middle of grid
    if popcount > 0 then
        MinimizeLiveBoundary()
        -- calculate the delta amounts needed to move pattern to middle of grid
        local deltax = (N - (maxx - minx)) // 2 - minx
        local deltay = (N - (maxy - miny)) // 2 - miny
        local deltaz = (N - (maxz - minz)) // 2 - minz
        if deltax == 0 and deltay == 0 and deltaz == 0 then return end
        
        RememberCurrentState()
        dirty = true
        local livecells = {}
        local NN = N*N
        for k,_ in pairs(grid1) do
            livecells[#livecells+1] = {k % N, (k // N) % N, k // NN}
        end
        grid1 = {}
        for _,xyz in ipairs(livecells) do
            local x = xyz[1] + deltax
            local y = xyz[2] + deltay
            local z = xyz[3] + deltaz
            grid1[x + N * (y + N * z)] = 1
        end
        
        -- update the live cell boundary
        minx = minx + deltax
        miny = miny + deltay
        minz = minz + deltaz
        maxx = maxx + deltax
        maxy = maxy + deltay
        maxz = maxz + deltaz
        -- MinimizeLiveBoundary set minimal_live_bounds to true
        
        Refresh()
    end
end

----------------------------------------------------------------------

-- use this flag in EraseLiveCells and SelectLiveCells to call
-- RememberCurrentState just before the first change (if any)
local firstchange = false

function EraseLiveCells(mousex, mousey, firstcall)
    -- erase all live cells whose projected mid points are close to mousex,mousey
    if popcount > 0 then
        if firstcall then firstchange = false end
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
                if not firstchange then
                    RememberCurrentState()
                    dirty = true
                    firstchange = true
                end
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

function SelectLiveCells(mousex, mousey, firstcall)
    -- select all live cells whose projected mid points are close to mousex,mousey
    if popcount > 0 then
        if firstcall then firstchange = false end
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
                    if not firstchange then
                        RememberCurrentState()
                        firstchange = true
                    end
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

    -- parameters for menu bar and tool bar buttons
    op.buttonht = buttonht
    op.textgap = 8                          -- gap between edge of button and its label
    op.textfont = "font 10 default-bold"    -- font for button labels
    op.menufont = "font 11 default-bold"    -- font for menu and item labels
    op.textshadowx = 2
    op.textshadowy = 2
    if g.os() == "Mac" then op.yoffset = -1 end
    if g.os() == "Linux" then
        op.textfont = "font 10 default"
        op.menufont = "font 11 default"
    end

    -- create the menu bar and add some menus
    -- (note that changes to the order of menus or their items will require
    -- changes to DrawMenuBar and EnableControls)
    mbar = op.menubar()
    mbar.addmenu("File")
    mbar.addmenu("Edit")
    mbar.addmenu("Control")
    mbar.addmenu("View")
    
    -- add items to File menu
    mbar.additem(1, "New Pattern", NewPattern)
    mbar.additem(1, "Random Pattern...", RandomPattern)
    mbar.additem(1, "Open Pattern...", OpenPattern)
    mbar.additem(1, "Open Clipboard", OpenClipboard)
    mbar.additem(1, "Save Pattern...", SavePattern)
    mbar.additem(1, "---", nil)
    mbar.additem(1, "Run Script...", RunScript)
    mbar.additem(1, "Run Clipboard", RunClipboard)
    mbar.additem(1, "Set Startup Script...", SetStartupScript)
    mbar.additem(1, "---", nil)
    mbar.additem(1, "Exit 3D.lua", ExitScript)
    
    -- add items to Edit menu
    mbar.additem(2, "Undo", Undo)
    mbar.additem(2, "Redo", Redo)
    mbar.additem(2, "---", nil)
    mbar.additem(2, "Cut", CutSelection)
    mbar.additem(2, "Copy", CopySelection)
    mbar.additem(2, "Paste", Paste)
    mbar.additem(2, "Cancel Paste", CancelPaste)
    mbar.additem(2, "Clear", ClearSelection)
    mbar.additem(2, "Clear Outside", ClearOutside)
    mbar.additem(2, "---", nil)
    mbar.additem(2, "Select All", SelectAll)
    mbar.additem(2, "Cancel Selection", CancelSelection)
    mbar.additem(2, "---", nil)
    mbar.additem(2, "Middle Pattern", MiddlePattern)
    mbar.additem(2, "Middle Selection", MiddleSelection)
    mbar.additem(2, "Middle Paste", MiddlePaste)

    -- add items to Control menu
    mbar.additem(3, "Start Generating", StartStop)
    mbar.additem(3, "Next Generation", Step1)
    mbar.additem(3, "Reset", Reset)
    mbar.additem(3, "---", nil)
    mbar.additem(3, "Set Rule...", ChangeRule)
    
    -- add items to View menu
    mbar.additem(4, "Initial View", InitialView)
    mbar.additem(4, "Fit Grid", FitGrid)
    mbar.additem(4, "Set Grid Size...", SetGridSize)
    mbar.additem(4, "---", nil)
    mbar.additem(4, "Cubes", SetCellType, {"cube"})
    mbar.additem(4, "Spheres", SetCellType, {"sphere"})
    mbar.additem(4, "Points", SetCellType, {"point"})
    mbar.additem(4, "---", nil)
    mbar.additem(4, "Show Axes", ToggleAxes)
    mbar.additem(4, "Show Lattice Lines", ToggleLines)
    mbar.additem(4, "Use Depth Shading", ToggleDepthShading)
    mbar.additem(4, "---", nil)
    mbar.additem(4, "Help", ShowHelp)
    
    -- create tool bar buttons
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
    
    -- create a pop-up menu for paste actions
    pastemenu = op.popupmenu()
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
    
    -- create a pop-up menu for selection actions
    selmenu = op.popupmenu()
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
    selmenu.additem("Cancel Selection", CancelSelection)
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
                toolbarht = mbarht+buttonht+gap*2
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
    
    -- update the transformation matrix
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

function InitialView()
    -- initialize the transformation matrix
    xixo = 1.0; yixo = 0.0; zixo = 0.0
    xiyo = 0.0; yiyo = 1.0; ziyo = 0.0
    xizo = 0.0; yizo = 0.0; zizo = 1.0

    -- rotate to a nice view but don't call Refresh
    Rotate(160, 20, 0, false)
    -- user can hit the up arrow 4 times and the right arrow 4 times
    -- to see an untilted XY plane parallel with the screen
    
    FitGrid()   -- calls Refresh
end

----------------------------------------------------------------------

function Initialize()
    CreateOverlay()
    CreateAxes()
    
    -- clear depth shading lists
    for i = 1, depthlayers do
        layercoords[i] = {}
    end

    -- create reference cube (never displayed)
    refcube = CreateCube(0,0,0)
    
    ClearCells()
    if rulestring == DEFAULT_RULE then
        -- initial pattern is the Life-like glider in rule 3D5,6,7/6
        local mid = N//2
        SetLiveCell(mid,   mid+1, mid)
        SetLiveCell(mid+1, mid,   mid)
        SetLiveCell(mid-1, mid-1, mid)
        SetLiveCell(mid,   mid-1, mid)
        SetLiveCell(mid+1, mid-1, mid)
        SetLiveCell(mid,   mid+1, mid-1)
        SetLiveCell(mid+1, mid,   mid-1)
        SetLiveCell(mid-1, mid-1, mid-1)
        SetLiveCell(mid,   mid-1, mid-1)
        SetLiveCell(mid+1, mid-1, mid-1)
        dirty = false
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
        dirty = false
        --]]
    end
    
    SetActivePlane()
    InitialView()           -- calls Refresh
    
    -- run the user's startup script if it exists
    local f = io.open(startup, "r")
    if f then
        f:close()
        RunScript(startup)
        ClearUndoRedo()     -- don't want to undo startup script
        Refresh()
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
    elseif key == "5" and mods == "none" then RandomPattern()
    elseif key == "n" and mods == CMDCTRL then NewPattern()
    elseif key == "o" and mods == CMDCTRL then OpenPattern()
    elseif key == "s" and mods == CMDCTRL then SavePattern()
    elseif key == "o" and mods == "shift" then OpenClipboard()
    elseif key == "r" and mods == "shift" then RunClipboard()
    elseif key == "r" and mods == CMDCTRL then Reset()
    elseif key == "r" and mods == "none" then ChangeRule()
    elseif key == "g" and (mods == "none" or mods == CMDCTRL) then SetGridSize()
    elseif key == "a" and (mods == "none" or mods == CMDCTRL) then SelectAll()
    elseif key == "k" and (mods == "none" or mods == CMDCTRL) then CancelSelection()
    elseif key == "b" and mods == "none" then Rotate(0, 180, 0)
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
    elseif key == "l" and mods == "none" then ToggleLines()
    elseif key == "l" and mods == "shift" then ToggleAxes()
    elseif key == "d" and mods == "alt" then ToggleDepthShading()
    elseif key == "t" then ToggleToolBar()
    elseif key == "," then MoveActivePlane(activepos+1, true)
    elseif key == "." then MoveActivePlane(activepos-1, true)
    elseif key == "a" and mods == "shift" then CycleActivePlane()
    elseif key == "c" and mods == "none" then CycleCursor()
    elseif key == "d" and mods == "none" then DrawMode()
    elseif key == "s" and mods == "none" then SelectMode()
    elseif key == "m" and mods == "none" then MoveMode()
    elseif key == "h" and mods == "none" then ShowHelp()
    elseif key == "q" then ExitScript()
    else
        -- could be a keyboard shortcut (eg. for full screen)
        g.doevent(event)
    end
end

----------------------------------------------------------------------

function EventLoop()
    local mousedown = false         -- mouse button is down?
    local drawing = false           -- draw/erase cells with pencil cursor?
    local selecting = false         -- (de)select cells with cross-hairs cursor?
    local drag_paste = false        -- drag paste pattern with any cursor?
    local drag_selection = false    -- drag selected cells with hand cursor?
    local drag_active = false       -- drag active plane with pencil/cross-hairs?
    local hand_erase = false        -- erase live cells with hand cursor?
    local hand_select = false       -- select live cells with hand cursor?
    local dragface = ""             -- which paste/selection face is being dragged
    local prevx, prevy              -- previous mouse position
    
    while true do
        local event = g.getevent()
        if #event == 0 then
            if not mousedown then
                if not generating then
                    g.sleep(5)      -- don't hog the CPU when idle
                end
                CheckLayerSize()    -- may need to resize the overlay
            end
        else
            if message and (event:find("^key") or event:find("^oclick")) then
                message = nil
                Refresh()           -- remove the most recent message
            end
            event = op.process(event)
            if #event == 0 then
                -- op.process handled the given event
            elseif event:find("^key") then
                -- don't do key action if mouse button is down (can clobber undo history)
                if not mousedown then
                    HandleKey(event)
                end
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
                        if pastecount > 0 then
                            -- paste pattern can be dragged using any cursor
                            dragface = StartDraggingPaste(x, y)
                            drag_paste = #dragface > 0
                        end
                        if drag_paste then
                            -- ignore currcursor
                            RememberCurrentState()
                        elseif currcursor == drawcursor then
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
                                if selcount > 0 then
                                    dragface = StartDraggingSelection(x, y)
                                    drag_selection = #dragface > 0
                                end
                            elseif mods == "alt" then
                                hand_erase = true
                                EraseLiveCells(x, y, true)
                            elseif mods == "shift" then
                                hand_select = true
                                SelectLiveCells(x, y, true)
                            end
                        end
                    end
                end
            elseif event:find("^mup") then
                mousedown = false
                if drawing then
                    drawing = false
                elseif selecting then
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
                        EraseLiveCells(x, y, false)
                    elseif hand_select then
                        SelectLiveCells(x, y, false)
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
            -- mouse button is not down
            CheckCursor(mousepos)
            if generating then NextGeneration() end
        end
    end
end

----------------------------------------------------------------------

ReadSettings()
local oldstate = SaveGollyState()
Initialize()

status, err = xpcall(EventLoop, gp.trace)
if err then g.continue(err) end
-- the following code is always executed

RestoreGollyState(oldstate)
WriteSettings()
