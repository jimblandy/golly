--[[
This script lets people explore three-dimensional cellular automata
using Golly.  Inspired by the work of Carter Bays and his colleagues
(see http://www.cse.sc.edu/~bays/d4d4d4/).

The 3D drawing is based on code in "Macintosh Graphics in Modula-2"
by Russell L. Schnapp.  Note that orthographic projection is used to
speed up rendering (because all live cells look the same at a given
scale and rotation).

Author: Andrew Trevorrow (andrew@trevorrow.com), Feb 2018.

Thanks to Tom Rokicki for optimizing the generating code.
Thanks to Chris Rowett for optimizing the rendering code and many
other improvements.
--]]

local g = golly()
-- require "gplus.strict"
local gp = require "gplus"
local int = gp.int
local round = gp.round
local validint = gp.validint
local split = gp.split

local ov = g.overlay
local ovt = g.ovtable
local op = require "oplus"
local DrawLine = op.draw_line

local sin = math.sin
local cos = math.cos
local sqrt = math.sqrt
local rand = math.random
local abs = math.abs
local min = math.min
local max = math.max
local floor = math.floor

math.randomseed(os.time())          -- init seed for math.random

local N = 30                        -- initial grid size (N*N*N cells)
local MINN = 3                      -- minimum grid size
local MAXN = 100                    -- maximum grid size (must be even for BusyBoxes)
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

-- these global strings can be changed by a startup script to customize colors
BACK_COLOR = "rgba 0 0 80 255"      -- for background
LINE_COLOR = "rgba 70 70 100 255"   -- for lattice lines (should be close to BACK_COLOR)
INFO_COLOR = op.white               -- for info text
MSG_COLOR = op.yellow               -- for message text
POINT_COLOR = op.white              -- for drawing points
SELPT_COLOR = "rgba 0 255 0 128"    -- for selected points (should be translucent)
PASTE_COLOR = "rgba 255 0 0 64"     -- for paste pattern (must be translucent)
SELECT_COLOR = "rgba 0 255 0 64"    -- for selected cells (ditto)
ACTIVE_COLOR = "rgba 0 0 255 48"    -- for active plane (ditto)
PASTE_MENU = "rgba 176 48 48 255"   -- for paste menu background (should match PASTE_COLOR)
SELECT_MENU = "rgba 0 128 0 255"    -- for select menu background (should match SELECT_COLOR)
X_COLOR = op.red                    -- for front X axes
Y_COLOR = op.green                  -- for front Y axes
Z_COLOR = op.blue                   -- for front Z axes
REARX_COLOR = "rgba 128 0 0 255"    -- for rear X axes (should be darker than front axes)
REARY_COLOR = "rgba 0 128 0 255"    -- for rear Y axes (ditto)
REARZ_COLOR = "rgba 0 0 128 255"    -- for rear Z axes (ditto)
START_COLOR = "rgba 0 150 0 255"    -- for Start button's background
SELSTART_COLOR = "rgba 0 90 0 255"  -- for Start button's selected background
STOP_COLOR = "rgba 210 0 0 255"     -- for Stop button's background
SELSTOP_COLOR = "rgba 150 0 0 255"  -- for Stop button's selected background

-- following are for BusyBoxes
EVEN_COLOR = "rgba 255 180 255 255"     -- for even cell points and spheres (pale magenta)
ODD_COLOR = "rgba 140 255 255 255"      -- for odd cell points and spheres (pale cyan)
EVEN_CUBE = "replace *# *#-50 *# *#"    -- for even cell cubes (change white to pale magenta)
ODD_CUBE = "replace *#-110 *# *# *#"    -- for odd cell cubes (change white to pale cyan)

-- table versions of point and select point colors
pointcol = {split(POINT_COLOR)}
selpointcol = {split(SELPT_COLOR)}
usingpointcol = false

local xylattice = {}                -- lattice lines between XY axes
local xzlattice = {}                -- lattice lines between XZ axes
local yzlattice = {}                -- lattice lines between YZ axes
local xaxes = {}                    -- four X axes
local yaxes = {}                    -- four Y axes
local zaxes = {}                    -- four Z axes

local grid1 = {}                    -- sparse 3D matrix with up to N*N*N live cells
local popcount = 0                  -- number of live cells
local pattname = "untitled"         -- pattern name
local currtitle = ""                -- current window title
local showaxes = true               -- draw axes?
local showlines = true              -- draw lattice lines?
local generating = false            -- generate pattern?
local gencount = 0                  -- current generation count
local stepsize = 1                  -- display each generation
local stopgen = 0                   -- when to stop generating (if > 0)
local perc = 20                     -- initial percentage for RandomPattern
local randstring = "20"             -- initial string for RandomPattern
local message = nil                 -- text message displayed by Refresh if not nil
local selcount = 0                  -- number of selected cells (live or dead)
local selected = {}                 -- grid positions of selected cells
local pastecount = 0                -- number of cells in paste pattern
local pastepatt = {}                -- grid positions of cells in paste pattern
local drawstate = 1                 -- for drawing/erasing cells
local selstate = true               -- for selecting/deselecting cells
local celltype = "cube"             -- draw live cell as cube/sphere/point
local DrawLiveCell                  -- set to AddCellToBatch, etc
local DrawBusyBox                   -- set to DrawBusyCube, etc
local xybatch = {}                  -- coordinates for each cell when batch drawing
local xyindex = 2                   -- index of next position in xybatch
local layercoords = {}              -- coordinates for each cell in each layer
local layerindices = {}             -- index of last entry in each layer
local layercurrent = {}             -- current layer
local layerindex = 2                -- current layer last entry index
local depthshading = true           -- whether using depth shading
local depthlayers = 64              -- number of shading layers
local depthrange = 224              -- rgb levels for depth shading
local layerlast                     -- last layer used
local mindepth, maxdepth            -- minimum and maximum depth (with corner pointing at screen)
local zdepth, zdepth2               -- coefficients for z to depth layer mapping
local xyline = { "lines" }          -- coordinates for batch line draw
local xyln = 2                      -- index of next position in xyline

local active = {}                   -- grid positions of cells in active plane
local activeplane = "XY"            -- orientation of active plane (XY/XZ/YZ)
local activepos = 0                 -- active plane's cell position along 3rd axis
local activecell = ""               -- "x,y,z" if mouse is inside an active cell
local prevactive = ""               -- previous activecell

-- boundary grid coords for live cells (not always minimal)
local minx, miny, minz
local maxx, maxy, maxz
local minimal_live_bounds = true    -- becomes false if a live cell is deleted
local liveedge = false              -- becomes true if there is at least one cell on the grid edge

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
local startstate = {}               -- starting state (used by Reset)
local dirty = false                 -- pattern has been modified?

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
local minwd, minht = 900, 400       -- minimum size of overlay
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
local drawbox                       -- radio button for draw mode
local selectbox                     -- radio button for select mode
local movebox                       -- radio button for move mode
local stepslider                    -- slider for adjusting stepsize

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

DEFAULT_RULE = "3D5..7/6"           -- initial rule
local rulestring = ""               -- so very first Initialize calls ParseRule(DEFAULT_RULE)
local survivals, births             -- set by ParseRule
local NextGeneration                -- ditto (set to NextGenMoore, NextGen6Faces, etc)

pattdir = g.getdir("data")          -- initial directory for OpenPattern/SavePattern
scriptdir = g.getdir("app")         -- initial directory for RunScript
local scriptlevel = 0               -- greater than 0 if a user script is running

-- the default path for the script to run when 3D.lua starts up
pathsep = g.getdir("app"):sub(-1)
startup = g.getdir("app").."My-scripts"..pathsep.."3D-start.lua"

-- user settings are stored in this file
settingsfile = g.getdir("data").."3D.ini"

----------------------------------------------------------------------

function AddCount(item, counts, maxcount, bcount)
    local mincount = 0
    if bcount then mincount = 1 end
    local i = tonumber(item)
    if (i >= mincount) and (i <= maxcount) then
        counts[#counts+1] = i
    else
        if bcount then
            g.warn("Birth count ("..i..") must be from 1 to "..maxcount)
        else
            g.warn("Survival count ("..i..") must be from 0 to "..maxcount)
        end
        return false
    end
    return true
end

----------------------------------------------------------------------

function AddCountRange(low, high, counts, maxcount, bcount)
    local mincount = 0
    local what = "Survival"
    if bcount then
        mincount = 1
        what = "Birth"
    end
    local l = tonumber(low)
    local h = tonumber(high)

    -- check range is in ascending order
    if l > h then
        g.warn("End of "..what.." range ("..h..") must be higher than start ("..l..")")
        return false
    end

    -- check start of range is valid
    if (l < mincount or l > maxcount) then
        g.warn("Start of "..what.." range ("..l..") must be from "..mincount.." to "..maxcount)
        return false
    end

    -- check end of range is valid
    if (h < mincount or h > maxcount) then
        g.warn("End of "..what.." range ("..h..") must be from "..mincount.." to "..maxcount)
        return false
    end

    -- add range to counts
    for i = l, h do
        counts[#counts+1] = i
    end
    return true
end

----------------------------------------------------------------------

function AppendCounts(array, counts, maxcount, bcount)
    for _, item in ipairs(array) do
        if validint(item) then
            -- check for single number
            if not AddCount(item, counts, maxcount, bcount) then return false end
        else
            -- check for range e.g. 5..7
            local l, h = split(item, "..")
            if h == nil then h = "" end
            if #l > 0 and #h > 0 and validint(l) and validint(h) then
                if not AddCountRange(l, h, counts, maxcount, bcount) then return false end
            else
                g.warn("Illegal number in rule: "..item)
                return false
            end
        end
    end
    return true
end

--------------------------------------------------------------------------------

function AddCanonicalPart(canonstr, startrun, i)
    if #canonstr > 0 then canonstr = canonstr.."," end
    -- add the start value
    canonstr = canonstr..startrun
    -- check for a run
    if i > startrun + 1 then
        if i == startrun + 2 then
            -- run of two becomes a,b
            canonstr = canonstr..","..i-1
        else
            -- run of more than two becomes a..b
            canonstr = canonstr..".."..i-1
        end
    end
    return canonstr
end

--------------------------------------------------------------------------------

function CanonicalForm(counts, maxcount)
    local result = ""
    local startrun = -1
    for i = 0, maxcount do
        if counts[i] then
            if startrun == -1 then startrun = i end
        else
            if startrun ~= -1 then
                result = AddCanonicalPart(result, startrun, i)
                startrun = -1
            end
        end
    end
    -- check for unfinished run
    if startrun ~= -1 then
        result = AddCanonicalPart(result, startrun, maxcount+1)
    end
    return result
end

--------------------------------------------------------------------------------

function ParseRule(newrule)
    -- parse newrule and if ok set rulestring, survivals, births and NextGeneration
    if #newrule == 0 then
        newrule = DEFAULT_RULE
    else
        newrule = newrule:upper()
    end

    -- first check for BusyBoxes or BusyBoxesM
    if newrule == "BUSYBOXES" or newrule == "BB" then
        rulestring = "BusyBoxes"
        -- survivals and births are not used
        NextGeneration = NextGenBusyBoxes
        return true
    elseif newrule == "BUSYBOXESW" or newrule == "BBW" then
        rulestring = "BusyBoxesW"
        -- survivals and births are not used
        NextGeneration = NextGenBusyBoxes
        return true
    end

    if not newrule:find("^3D") then
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

    -- use 3D Moore neighborhood unless rule ends with a special letter
    local lastchar = newrule:sub(-1)
    local maxcount = 26
    if lastchar == "F" or lastchar == "V" then
        -- 6-cell face neighborhood (aka von Neumann neighborhood)
        lastchar = "F"
        maxcount = 6
        newrule = newrule:sub(1,-2)
    elseif lastchar == "C" then
        -- 8-cell corner neighborhood
        maxcount = 8
        newrule = newrule:sub(1,-2)
    elseif lastchar == "E" then
        -- 12-cell edge neighborhood
        maxcount = 12
        newrule = newrule:sub(1,-2)
    elseif lastchar == "H" then
        -- 12-cell hexahedral neighborhood
        maxcount = 12
        newrule = newrule:sub(1,-2)
    else
        lastchar = ""   -- for Moore neighborhood
    end

    local s, b = split(newrule,"/")
    if #s == 0 then s = {} else s = {split(s,",")} end
    if b == nil or #b == 0 then b = {} else b = {split(b,",")} end
    local news = {}
    local newb = {}
    if not AppendCounts(s, news, maxcount, false) then return false end
    if not AppendCounts(b, newb, maxcount, true) then return false end

    -- newrule is okay so set survivals and births
    survivals = {}
    births = {}
    for _, i in ipairs(news) do survivals[i] = true end
    for _, i in ipairs(newb) do births[i] = true end

    -- set rulestring to the canonical form
    rulestring = "3D"..CanonicalForm(survivals,maxcount).."/"..CanonicalForm(births,maxcount)..lastchar

    -- set NextGeneration to the appropriate NextGen* function
    if lastchar == "F" then
        NextGeneration = NextGen6Faces
    elseif lastchar == "C" then
        NextGeneration = NextGen8Corners
    elseif lastchar == "E" then
        NextGeneration = NextGen12Edges
    elseif lastchar == "H" then
        NextGeneration = NextGenHexahedral
    else
        NextGeneration = NextGenMoore
    end

    return true
end

--------------------------------------------------------------------------------

function ReadSettings()
    local f = io.open(settingsfile, "r")
    if f then
        while true do
            -- no need to worry about CRs here because file was created by WriteSettings
            local line = f:read("*l")
            if not line then break end
            local keyword, value = split(line,"=")
            -- look for a keyword used in WriteSettings
            if not value then
                -- ignore keyword
            elseif keyword == "startup" then startup = tostring(value)
            elseif keyword == "pattdir" then pattdir = tostring(value)
            elseif keyword == "scriptdir" then scriptdir = tostring(value)
            elseif keyword == "celltype" then celltype = tostring(value)
            elseif keyword == "perc" then randstring = tostring(value)
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
                    ParseRule(DEFAULT_RULE)
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
        f:write("perc=", randstring, "\n")
        f:write("axes="..tostring(showaxes), "\n")
        f:write("lines="..tostring(showlines), "\n")
        f:write("shading="..tostring(depthshading), "\n")
        f:close()
    end
end

----------------------------------------------------------------------

function SaveGollyState()
    local oldstate = {}
    oldstate.scroll = g.setoption("showscrollbars", 0)
    oldstate.time = g.setoption("showtimeline", 0)
    oldstate.tool = g.setoption("showtoolbar", 0)
    oldstate.status = g.setoption("showstatusbar", 0)
    oldstate.layer = g.setoption("showlayerbar", 0)
    oldstate.edit = g.setoption("showeditbar", 0)
    oldstate.tile = g.setoption("tilelayers", 0)
    oldstate.stack = g.setoption("stacklayers", 0)
    oldstate.files = g.setoption("showfiles", 0)
    oldstate.filesdir = g.getdir("files")
    return oldstate
end

----------------------------------------------------------------------

function RestoreGollyState(oldstate)
    ov("delete")
    g.setoption("showscrollbars", oldstate.scroll)
    g.setoption("showtimeline", oldstate.time)
    g.setoption("showtoolbar", oldstate.tool)
    g.setoption("showstatusbar", oldstate.status)
    g.setoption("showlayerbar", oldstate.layer)
    g.setoption("showeditbar", oldstate.edit)
    g.setoption("tilelayers", oldstate.tile)
    g.setoption("stacklayers", oldstate.stack)
    g.setdir("files", oldstate.filesdir)
    g.setoption("showfiles", oldstate.files)
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
    ovt{"line", x1, y, x2, y}
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
local function AddLineToBatch(startpt, endpt)
    -- use orthographic projection to transform line's start and end points
    local x, y, z = startpt[1], startpt[2], startpt[3]
    xyline[xyln] = (x*xixo + y*xiyo + z*xizo) + midx
    xyline[xyln + 1] = (x*yixo + y*yiyo + z*yizo) + midy
    xyln = xyln + 2
    x, y, z = endpt[1], endpt[2], endpt[3]
    xyline[xyln] = (x*xixo + y*xiyo + z*xizo) + midx
    xyline[xyln + 1] = (x*yixo + y*yiyo + z*yizo) + midy
    xyln = xyln + 2
end

----------------------------------------------------------------------

local function DrawBatchLines()
    if xyln > 2 then
        xyline[xyln] = nil
        ovt(xyline)
        xyln = 2
    end
end

----------------------------------------------------------------------

function DrawRearAxes()
    -- draw lattice lines and/or axes that are behind rotated reference cube
    -- assuming vertex order set in CreateCube
    local z1 = rotrefz[1]
    local z2 = rotrefz[2]
    local z3 = rotrefz[3]
    local z7 = rotrefz[7]

    if showlines then
        ov(LINE_COLOR)
        if z1 < z2 then
            -- front face of rotated refcube is visible
            for _, pt in ipairs(xylattice) do AddLineToBatch(pt[1], pt[2]) end
        end
        if z1 >= z7 then
            -- right face of rotated refcube is visible
            for _, pt in ipairs(yzlattice) do AddLineToBatch(pt[1], pt[2]) end
        end
        if z1 >= z3 then
            -- top face of rotated refcube is visible
            for _, pt in ipairs(xzlattice) do AddLineToBatch(pt[1], pt[2]) end
        end
        DrawBatchLines()
    end

    if showaxes then
        -- draw darker anti-aliased lines for rear axes
        ov("blend 1")
        ov("lineoption width 3")

        if z1 <  z2 or z1 >= z3 then ov(REARX_COLOR); DisplayLine(xaxes[1], xaxes[2]) end
        if z1 <  z2 or z1 >= z7 then ov(REARY_COLOR); DisplayLine(yaxes[1], yaxes[2]) end
        if z1 >= z7 or z1 >= z3 then ov(REARZ_COLOR); DisplayLine(zaxes[1], zaxes[2]) end

        if z1 <  z2 or z1 <  z3 then ov(REARX_COLOR); DisplayLine(xaxes[3], xaxes[4]) end
        if z1 >= z2 or z1 >= z7 then ov(REARY_COLOR); DisplayLine(yaxes[3], yaxes[4]) end
        if z1 <  z7 or z1 >= z3 then ov(REARZ_COLOR); DisplayLine(zaxes[3], zaxes[4]) end

        if z1 >= z2 or z1 <  z3 then ov(REARX_COLOR); DisplayLine(xaxes[5], xaxes[6]) end
        if z1 >= z2 or z1 <  z7 then ov(REARY_COLOR); DisplayLine(yaxes[5], yaxes[6]) end
        if z1 <  z3 or z1 <  z7 then ov(REARZ_COLOR); DisplayLine(zaxes[5], zaxes[6]) end

        if z1 >= z2 or z1 >= z3 then ov(REARX_COLOR); DisplayLine(xaxes[7], xaxes[8]) end
        if z1 <  z2 or z1 <  z7 then ov(REARY_COLOR); DisplayLine(yaxes[7], yaxes[8]) end
        if z1 >= z7 or z1 <  z3 then ov(REARZ_COLOR); DisplayLine(zaxes[7], zaxes[8]) end

        ov("lineoption width 1")
        ov("blend 0")
    end
end

----------------------------------------------------------------------

function DrawFrontAxes()
    -- draw lattice lines and/or axes that are in front of rotated reference cube
    -- assuming vertex order set in CreateCube
    local z1 = rotrefz[1]
    local z2 = rotrefz[2]
    local z3 = rotrefz[3]
    local z7 = rotrefz[7]

    if showlines then
        ov(LINE_COLOR)
        if z1 >= z2 then
            -- back face of rotated refcube is visible
            for _, pt in ipairs(xylattice) do AddLineToBatch(pt[1], pt[2]) end
        end
        if z1 < z7 then
            -- left face of rotated refcube is visible
            for _, pt in ipairs(yzlattice) do AddLineToBatch(pt[1], pt[2]) end
        end
        if z1 < z3 then
            -- bottom face of rotated refcube is visible
            for _, pt in ipairs(xzlattice) do AddLineToBatch(pt[1], pt[2]) end
        end
        DrawBatchLines()
    end

    if showaxes then
        -- draw brighter anti-aliased lines for front axes
        ov("blend 1")
        ov("lineoption width 3")

        if z1 >= z2 or z1 < z3 then ov(X_COLOR); DisplayLine(xaxes[1], xaxes[2]) end
        if z1 >= z2 or z1 < z7 then ov(Y_COLOR); DisplayLine(yaxes[1], yaxes[2]) end
        if z1 <  z7 or z1 < z3 then ov(Z_COLOR); DisplayLine(zaxes[1], zaxes[2]) end

        if z1 >= z2 or z1 >= z3 then ov(X_COLOR); DisplayLine(xaxes[3], xaxes[4]) end
        if z1 <  z2 or z1 <  z7 then ov(Y_COLOR); DisplayLine(yaxes[3], yaxes[4]) end
        if z1 >= z7 or z1 <  z3 then ov(Z_COLOR); DisplayLine(zaxes[3], zaxes[4]) end

        if z1 <  z2 or z1 >= z3 then ov(X_COLOR); DisplayLine(xaxes[5], xaxes[6]) end
        if z1 <  z2 or z1 >= z7 then ov(Y_COLOR); DisplayLine(yaxes[5], yaxes[6]) end
        if z1 >= z3 or z1 >= z7 then ov(Z_COLOR); DisplayLine(zaxes[5], zaxes[6]) end

        if z1 <  z2 or z1 <  z3 then ov(X_COLOR); DisplayLine(xaxes[7], xaxes[8]) end
        if z1 >= z2 or z1 >= z7 then ov(Y_COLOR); DisplayLine(yaxes[7], yaxes[8]) end
        if z1 <  z7 or z1 >= z3 then ov(Z_COLOR); DisplayLine(zaxes[7], zaxes[8]) end

        ov("lineoption width 1")
        ov("blend 0")
    end
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
    for i = 0, N do
        local offset = i*CELLSIZE
        xylattice[#xylattice+1] = {{o+offset,o,o}, {o+offset,endpt,o}}
        xylattice[#xylattice+1] = {{o,o+offset,o}, {endpt,o+offset,o}}
    end

    xzlattice = {}
    for i = 0, N do
        local offset = i*CELLSIZE
        xzlattice[#xzlattice+1] = {{o,o,o+offset}, {endpt,o,o+offset}}
        xzlattice[#xzlattice+1] = {{o+offset,o,o}, {o+offset,o,endpt}}
    end

    yzlattice = {}
    for i = 0, N do
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
    ov(color)

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

function DrawCubeEdges()
    if LEN > 4 then
        -- draw anti-aliased edges around visible face(s)
        if LEN == 5 then
            ovt{"rgba", 150, 150, 150, 255}
        elseif LEN == 6 then
            ovt{"rgba", 110, 110, 110, 255}
        elseif LEN == 7 then
            ovt{"rgba",  80, 80, 80, 255}
        else
            ovt{"rgba", 60, 60, 60, 255}
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
end

----------------------------------------------------------------------

function CreateLayers(clip)
    local adjust = depthrange / (maxdepth - mindepth + 1)
    local total = 0
    local rgb
    ov("target "..clip)
    ov("copy 0 0 0 0 "..clip.."1")
    ov("target "..clip.."1")
    ov("optimize "..clip.."1")
    for i = 2, maxdepth do
        total = total + adjust
        rgb = floor(total)
        ov("target "..clip)
        ov("copy 0 0 0 0 "..clip..i)
        ov("target "..clip..i)
        ov("replace *#-"..rgb.." *#-"..rgb.." *#-"..rgb.." *#")
        ov("optimize "..clip..i)
    end
    total = 0
    for i = 1, mindepth, -1 do
        total = total + adjust
        rgb = floor(total)
        ov("target "..clip)
        ov("copy 0 0 0 0 "..clip..i)
        ov("target "..clip..i)
        ov("replace *#+"..rgb.." *#+"..rgb.." *#+"..rgb.." *#")
        ov("optimize "..clip..i)
    end
    ov("target")
end

----------------------------------------------------------------------

local HALFCUBECLIP  -- half the wd/ht of the clip containing a live cube
lastCubeSize = -1

function CreateLiveCube()
    -- only create bitmaps if cell size has changed
    if CELLSIZE == lastCubeSize then return end
    lastCubeSize = CELLSIZE

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

    DrawCubeEdges()

    ov("optimize c")

    -- create faded versions of the clip if depth shading
    if depthshading then
        CreateLayers("c")
    end

    ov("target")
end

----------------------------------------------------------------------

lastSphereSize = -1

function CreateLiveSphere()
    -- only create bitmaps if cell size has changed
    if CELLSIZE == lastSphereSize then return end
    lastSphereSize = CELLSIZE

    -- create a clip containing one sphere that will be used later
    -- to draw all live cells
    local diameter = CELLSIZE+1                     -- so orthogonally adjacent spheres touch
    ov("create "..diameter.." "..diameter.." S")    -- s is used for selected cells
    ov("target S")
    ov("blend 1")

    local x = 0
    local y = 0
    local gray = 0      -- start with black outline
    local grayinc = 127
    local r = (diameter+1)//2
    if r > 2 then grayinc = 127/(r-2) end
    while true do
        local grayrgb = floor(gray)
        ovt{"rgba", grayrgb, grayrgb, grayrgb, 255}
        -- draw a solid circle by setting the line width to the radius
        ov("lineoption width "..r)
        ov("ellipse "..x.." "..y.." "..diameter.." "..diameter)
        diameter = diameter - 2
        r = r - 1
        if r < 2 then break end
        x = x + 1
        y = y + 1
        if gray == 0 then gray = 128 end
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

function DrawCellDepth(x, y, z, mx, my, addtobatch)
    local c, m = CELLSIZE, MIDCELL
    -- add live cell at given grid position
    x = x * c + m
    y = y * c + m
    z = z * c + m
    -- transform point
    local newx = (x*xixo + y*xiyo + z*xizo) + mx
    local newy = (x*yixo + y*yiyo + z*yizo) + my
    local newz = (x*zixo + y*ziyo + z*zizo)
    -- compute the depth layer
    local layer = depthlayers * (newz + zdepth) // zdepth2 | 0
    if addtobatch then
        -- add cell to batch
        if layer ~= layerlast then
            -- save the previous layer and switch to the new one
            layerindices[layerlast] = layerindex
            layerlast = layer
            layercurrent = layercoords[layer]
            layerindex = layerindices[layer]
        end
        -- add to the layer's list to draw
        layercurrent[layerindex] = newx
        layerindex = layerindex + 1
        layercurrent[layerindex] = newy
        layerindex = layerindex + 1
    else
        -- draw immediately
        local cmd = {"paste", newx, newy}
        if celltype == "cube" then
            cmd[4] = "c"..layer
        elseif celltype == "sphere" then
            cmd[4] = "S"..layer
        else -- celltype == "point"
            if not usingpointcol then
                usingpointcol = true
                ovt(pointcol)
            end
            cmd[1] = "set"
        end
        -- draw the cell
        ovt(cmd)
    end
end

----------------------------------------------------------------------

function DrawCell(x, y, z, mx, my, addtobatch)
    local c, m = CELLSIZE, MIDCELL
    -- add live cell at given grid position
    x = x * c + m
    y = y * c + m
    z = z * c + m
    -- transform point
    local newx = (x*xixo + y*xiyo + z*xizo) + mx
    local newy = (x*yixo + y*yiyo + z*yizo) + my
    if addtobatch then
        local xyi = xyindex
        xybatch[xyi] = newx
        xyi = xyi + 1
        xybatch[xyi] = newy
        xyindex = xyi + 1 -- update global index
    else
        -- draw immediately
        local cmd = {"paste", newx, newy}
        if celltype == "cube" then
            cmd[4] = "c"
        elseif celltype == "sphere" then
            cmd[4] = "S"
        else -- celltype == "point"
            if not usingpointcol then
                usingpointcol = true
                ovt(pointcol)
            end
            cmd[1] = "set"
        end
        -- draw the cell
        ovt(cmd)
    end
end

----------------------------------------------------------------------

local function DrawBatchLayer(depth, coordlist, length)
    -- ignore if layer is empty
    if length > 1 then
        coordlist[1] = "paste"
        -- select drawing method based on cell type
        if celltype == "cube" then
            length = length + 1
            coordlist[length] = "c"..depth
        elseif celltype == "sphere" then
            length = length + 1
            coordlist[length] = "S"..depth
        else -- celltype == "point" then
            ovt(pointcol)
            coordlist[1] = "set"
        end
        length = length + 1
        coordlist[length] = nil
        ovt(coordlist)
    end
end

----------------------------------------------------------------------

lastBatchSize = MAXN*MAXN*MAXN*2
lastDepthBatchSize = {}

local function DrawBatch()
    -- check for depth shading for cubes or spheres
    if depthshading and celltype ~= "point" then
        -- save current layer index (may have been updated in TestCell)
        layerindices[layerlast] = layerindex

        -- draw each layer in reverse order (furthest away first = darkest)
        for i = maxdepth, mindepth, -1 do
            local batchsize = layerindices[i]
            if batchsize > 2 then
                DrawBatchLayer(i, layercoords[i], batchsize - 1)
                -- reset the layer once drawn to after the command name
                if batchsize < lastDepthBatchSize[i] then
                    layercoords[i] = {}
                end
                lastDepthBatchSize[i] = batchsize
                layerindices[i] = 2
            end
        end
        layerlast = mindepth - 1
    else
        -- no depth shading so draw all
        DrawBatchLayer("", xybatch, xyindex - 1)
        -- reset the batch to after the command name
        if xyindex < lastBatchSize then
            xybatch = {}
        end
        lastBatchSize = xyindex
        xyindex = 2
    end
end
----------------------------------------------------------------------

local function DrawPoint(x, y, z)
    local c, m = CELLSIZE, MIDCELL
    -- draw mid point of cell at given grid position
    x = x * c + m
    y = y * c + m
    z = z * c + m
    -- transform point
    local newx = (x*xixo + y*xiyo + z*xizo) + midx + 0.5
    local newy = (x*yixo + y*yiyo + z*yizo) + midy + 0.5
    -- use orthographic projection
    ovt{"set", newx, newy}
end

----------------------------------------------------------------------

local function DrawActiveCell(x, y, z)
    local c, m = CELLSIZE, MIDCELL
    x = x * c + m
    y = y * c + m
    z = z * c + m
    -- transform point
    local newx = (x*xixo + y*xiyo + z*xizo) + midx - CELLSIZE + 0.5
    local newy = (x*yixo + y*yiyo + z*yizo) + midy - CELLSIZE + 0.5
    -- draw the clip created by CreateTranslucentCell
    ovt{"paste", newx, newy, "a"}
end

----------------------------------------------------------------------

local function DrawSelectedCell(x, y, z)
    local c, m = CELLSIZE, MIDCELL
    x = x * c + m
    y = y * c + m
    z = z * c + m
    -- transform point
    local newx = (x*xixo + y*xiyo + z*xizo) + midx - CELLSIZE + 0.5
    local newy = (x*yixo + y*yiyo + z*yizo) + midy - CELLSIZE + 0.5
    -- draw the clip created by CreateTranslucentCell
    ovt{"paste", newx, newy, "s"}
end

----------------------------------------------------------------------

local function DrawPasteCell(x, y, z)
    local c, m = CELLSIZE, MIDCELL
    x = x * c + m
    y = y * c + m
    z = z * c + m
    -- transform point
    local newx = (x*xixo + y*xiyo + z*xizo) + midx - CELLSIZE + 0.5
    local newy = (x*yixo + y*yiyo + z*yizo) + midy - CELLSIZE + 0.5
    -- draw the clip created by CreateTranslucentCell
    ovt{"paste", newx, newy, "p"}
end

----------------------------------------------------------------------

local function TestCell(editing, gridpos, x, y, z, mx, my)
    -- called from DisplayCells to test if given cell is in active plane,
    -- or if it's selected, or if it's a paste cell, and to draw it accordingly
    if editing then
        if active[gridpos] then
            if grid1[gridpos] then
                -- draw live cell within active plane
                DrawLiveCell(x, y, z, mx, my)
            end
            DrawActiveCell(x, y, z)
            if selected[gridpos] then
                DrawSelectedCell(x, y, z)
            end
        else
            -- cell is outside active plane
            if grid1[gridpos] then
                -- live cell so draw a point
                if not usingpointcol then
                    usingpointcol = true
                    ovt(pointcol)
                end
                DrawPoint(x, y, z)
            end
            if selected[gridpos] then
                -- draw translucent point
                ovt(selpointcol)
                usingpointcol = false
                DrawPoint(x, y, z)
            end
        end
    else
        -- active plane is not displayed
        if grid1[gridpos] then
            DrawLiveCell(x, y, z, mx, my, true)
        end
        if selected[gridpos] then
            DrawBatch()
            DrawSelectedCell(x, y, z)
        end
    end
    if pastepatt[gridpos] then
        DrawLiveCell(x, y, z, mx, my, true)
        DrawBatch()
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

    -- determine order to traverse x, y and z in the grid;
    -- note that we need to draw cells from back to front
    -- (assumes vertex order set in CreateCube)
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

    -- select the cell drawing functions based on whether depth shading is required
    if depthshading and celltype ~= "point" then
        zdepth = N*CELLSIZE*0.5
        zdepth2 = zdepth+zdepth
        DrawLiveCell = DrawCellDepth
    else
        DrawLiveCell = DrawCell
    end

    -- compute mid point
    local mx, my
    if celltype == "cube" then
        mx = midx - HALFCUBECLIP
        my = midy - HALFCUBECLIP
    elseif celltype == "sphere" then
        mx = midx - HALFCELL
        my = midy - HALFCELL
    else -- celltype == "point"
        mx = midx
        my = midy
    end
    -- add 0.5 so floor behaves like round
    mx = mx + 0.5
    my = my + 0.5

    local i, j
    local l_N = N
    local stepi, stepj = l_N*stepy, l_N*stepz
    if testcell then
        -- setup the point colors
        pointcol = {split(POINT_COLOR)}
        selpointcol = {split(SELPT_COLOR)}
        usingpointcol = false
        -- draw each cell
        j = l_N*fromz
        for z = fromz, toz, stepz do
            i = l_N*(fromy+j)
            for y = fromy, toy, stepy do
                for x = fromx, tox, stepx do
                    TestCell(editing, i+x, x, y, z, mx, my)
                end
                i = i+stepi
            end
            j = j+stepj
        end
    else
        -- only live cells need to be drawn
        local c, m = CELLSIZE, MIDCELL
        local xc, yc, zc
        local l_grid1 = grid1
        local l_xixo, l_xiyo, l_xizo = xixo, xiyo, xizo
        local l_yixo, l_yiyo, l_yizo = yixo, yiyo, yizo
        if depthshading and celltype ~= "point" then
            -- draw with depth shading
            local layer, zval
            local l_layerlast = layerlast
            local l_depthlayers = depthlayers
            local l_layerindex = layerindex
            local l_layercurrent = layercurrent
            local l_layerindices = layerindices
            local l_layercoords = layercoords
            local l_zixo, l_ziyo, l_zizo = zixo, ziyo, zizo
            local l_zdepth, l_zdepth2 = zdepth, zdepth2
            j = l_N*fromz
            for z = fromz, toz, stepz do
                i = l_N*(fromy+j)
                for y = fromy, toy, stepy do
                    for x = fromx, tox, stepx do
                        if l_grid1[i+x] then
                            -- add live cell at given grid position
                            xc = x * c + m
                            yc = y * c + m
                            zc = z * c + m
                            zval = xc*l_zixo + yc*l_ziyo + zc*l_zizo
                            -- compute the depth layer
                            -- floor divide // creates a real number e.g. 5.0 that can be used as an array index
                            layer = l_depthlayers * (zval + l_zdepth) // l_zdepth2
                            -- check if the layer has changed since the last call
                            if layer ~= l_layerlast then
                                -- save the previous layer and switch to the new one
                                l_layerindices[l_layerlast] = l_layerindex  -- first l_layerlast value is ignored
                                l_layerlast = layer
                                l_layercurrent = l_layercoords[layer]
                                l_layerindex = l_layerindices[layer]
                            end
                            -- add cell position to the layer's list to draw
                            l_layercurrent[l_layerindex] = ((xc*l_xixo + yc*l_xiyo + zc*l_xizo) + mx)
                            l_layerindex = l_layerindex + 1
                            l_layercurrent[l_layerindex] = ((xc*l_yixo + yc*l_yiyo + zc*l_yizo) + my)
                            l_layerindex = l_layerindex + 1
                        end
                    end
                    i = i+stepi
                end
                j = j+stepj
            end
            l_layerindices[l_layerlast] = l_layerindex
        else
            -- draw without depth shading
            local xyi = xyindex
            local xyb = xybatch
            j = l_N*fromz
            for z = fromz, toz, stepz do
                i = l_N*(fromy+j)
                for y = fromy, toy, stepy do
                    for x = fromx, tox, stepx do
                        if l_grid1[i+x] then
                            -- add live cell at given grid position
                            xc = x * c + m
                            yc = y * c + m
                            zc = z * c + m
                            -- transform point using othographic projection
                            -- and add to the list to draw
                            xyb[xyi] = (mx + xc*l_xixo + yc*l_xiyo + zc*l_xizo)
                            xyi = xyi + 1
                            xyb[xyi] = (my + xc*l_yixo + yc*l_yiyo + zc*l_yizo)
                            xyi = xyi + 1
                        end
                    end
                    i = i+stepi
                end
                j = j+stepj
            end
            xyindex = xyi
        end
    end
    DrawBatch()  -- complete any pending batch

    ov("blend 0")
end

----------------------------------------------------------------------

lastBusyCubeSize = {E = -1, O = -1}

function CreateBusyCube(clipname)
    -- only create bitmaps if cell size has changed
    if CELLSIZE == lastBusyCubeSize[clipname] then return end
    lastBusyCubeSize[clipname] = CELLSIZE

    -- create a clip containing a cube for odd/even cells
    -- largest size of a rotated cube with edge length L is sqrt(3)*L
    HALFCUBECLIP = round(sqrt(3) * LEN / 2.0)
    ov("create "..(HALFCUBECLIP*2).." "..(HALFCUBECLIP*2).." "..clipname)
    ov("target "..clipname)

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

    -- adjust grayscale colors using the replace command
    if clipname == "E" then
        ov(EVEN_CUBE)
    else
        ov(ODD_CUBE)
    end

    DrawCubeEdges()

    ov("optimize "..clipname)

    -- create faded versions of the clip if depth shading
    if depthshading then
        CreateLayers(clipname)
    end

    ov("target")
end

----------------------------------------------------------------------

lastBusySphereSize = { E = -1, O = -1 }

function CreateBusySphere(clipname)
    -- only create bitmaps if cell size has changed
    if CELLSIZE == lastBusySphereSize[clipname] then return end
    lastBusySphereSize[clipname] = CELLSIZE

    -- create a clip containing a sphere for odd/even cells
    local diameter = CELLSIZE+1
    local d1 = diameter
    ov("create "..diameter.." "..diameter.." "..clipname)
    ov("target "..clipname)
    ov("blend 1")

    local R, G, B
    if clipname == "E" then
        local _, red, green, blue = split(EVEN_COLOR)
        R = tonumber(red)
        G = tonumber(green)
        B = tonumber(blue)
    else
        local _, red, green, blue = split(ODD_COLOR)
        R = tonumber(red)
        G = tonumber(green)
        B = tonumber(blue)
    end

    local x = 0
    local y = 0
    local inc = 3
    if diameter < 50 then inc = 8 - diameter//10 end
    local r = (diameter+1)//2
    while true do
        ovt{"rgba", R, G, B, 255}
        -- draw a solid circle by setting the line width to the radius
        ov("lineoption width "..r)
        ov("ellipse "..x.." "..y.." "..diameter.." "..diameter)
        diameter = diameter - 2
        r = r - 1
        if r < 2 then break end
        x = x + 1
        y = y + 1
        R = R + inc
        G = G + inc
        B = B + inc
        if R > 255 then R = 255 end
        if G > 255 then G = 255 end
        if B > 255 then B = 255 end
    end

    -- draw black outline
    ovt{"rgba", 0, 0, 0, 255}
    ov("lineoption width 1")
    ov("ellipse 0 0 "..d1.." "..d1)
    ov("blend 0")
    ov("optimize "..clipname)

    -- create faded versions of the clip if depth shading
    if depthshading then
        CreateLayers(clipname)
    end

    ov("target")
end

----------------------------------------------------------------------

function DrawBusyCube(x, y, z, clipname)
    -- draw odd/even cube at given grid position
    local c, m = CELLSIZE, MIDCELL
    x = x * c + m
    y = y * c + m
    z = z * c + m
    -- transform point
    local newx = (x*xixo + y*xiyo + z*xizo) + midx - HALFCUBECLIP
    local newy = (x*yixo + y*yiyo + z*yizo) + midy - HALFCUBECLIP
    -- use orthographic projection
    ovt{"paste", newx, newy, clipname}
end

----------------------------------------------------------------------

function DrawBusyCubeDepth(x, y, z, clipname)
    -- draw odd/even cube at given grid position with shading
    local c, m = CELLSIZE, MIDCELL
    x = x * c + m
    y = y * c + m
    z = z * c + m
    -- transform point
    local newx = (x*xixo + y*xiyo + z*xizo) + midx - HALFCUBECLIP
    local newy = (x*yixo + y*yiyo + z*yizo) + midy - HALFCUBECLIP
    local newz = (x*zixo + y*ziyo + z*zizo)
    -- compute the depth layer
    local layer = depthlayers * (newz + zdepth) // zdepth2 | 0
    -- use orthographic projection
    ovt{"paste", newx, newy, clipname..layer}
end

----------------------------------------------------------------------

function DrawBusySphere(x, y, z, clipname)
    -- draw odd/even sphere at given grid position
    local c, m = CELLSIZE, MIDCELL
    x = x * c + m
    y = y * c + m
    z = z * c + m
    -- transform point
    local newx = (x*xixo + y*xiyo + z*xizo) + midx - HALFCELL -- clip wd = CELLSIZE + 1
    local newy = (x*yixo + y*yiyo + z*yizo) + midy - HALFCELL -- clip ht = CELLSIZE + 1
    -- use orthographic projection
    ovt{"paste", newx, newy, clipname}
end

----------------------------------------------------------------------

function DrawBusySphereDepth(x, y, z, clipname)
    -- draw odd/even sphere at given grid position with shading
    local c, m = CELLSIZE, MIDCELL
    x = x * c + m
    y = y * c + m
    z = z * c + m
    -- transform point
    local newx = (x*xixo + y*xiyo + z*xizo) + midx - HALFCELL -- clip wd = CELLSIZE+1
    local newy = (x*yixo + y*yiyo + z*yizo) + midy - HALFCELL -- clip ht = CELLSIZE+1
    local newz = (x*zixo + y*ziyo + z*zizo)
    -- compute the depth layer
    local layer = depthlayers * (newz + zdepth) // zdepth2 | 0
    -- use orthographic projection
    ovt{"paste", newx, newy, clipname..layer}
end

----------------------------------------------------------------------

function CreateBusyPoint(clipname, color)
    ov("create 1 1 "..clipname)
    ov("target "..clipname)
    -- set pixel to the given color
    ov(color)
    ovt{"set", 0, 0}
    ov("target")
end

----------------------------------------------------------------------

local function DrawBusyPoint(x, y, z, clipprefix)
    -- draw mid point of busy box at given grid position
    local c, m = CELLSIZE, MIDCELL
    x = x * c + m
    y = y * c + m
    z = z * c + m
    -- transform point using orthographic projection
    local newx = (x*xixo + y*xiyo + z*xizo) + midx
    local newy = (x*yixo + y*yiyo + z*yizo) + midy
    -- clipprefix is "E" or "O"
    ovt{"paste", newx, newy, clipprefix.."p"}
end

----------------------------------------------------------------------

local function TestBusyBox(editing, gridpos, x, y, z, clipname)
    -- called from DisplayBusyBoxes to test if given cell is in active plane,
    -- or if it's selected, or if it's a paste cell, and to draw it accordingly
    if editing then
        if active[gridpos] then
            if grid1[gridpos] then
                -- draw live cell within active plane
                DrawBusyBox(x, y, z, clipname)
            end
            DrawActiveCell(x, y, z)
            if selected[gridpos] then
                DrawSelectedCell(x, y, z)
            end
        else
            -- cell is outside active plane
            if grid1[gridpos] then
                DrawBusyPoint(x, y, z, clipname)
            end
            if selected[gridpos] then
                -- draw translucent point
                ovt(selpointcol)
                DrawPoint(x, y, z)
            end
        end
    else
        -- active plane is not displayed
        if grid1[gridpos] then
            DrawBusyBox(x, y, z, clipname)
        end
        if selected[gridpos] then
            DrawSelectedCell(x, y, z)
        end
    end
    if pastepatt[gridpos] then
        DrawBusyBox(x, y, z, clipname)
        DrawPasteCell(x, y, z)
    end
end

----------------------------------------------------------------------

function DisplayBusyBoxes(editing)
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

    -- determine order to traverse x, y and z in the grid;
    -- note that we need to draw cells from back to front
    -- (assumes vertex order set in CreateCube)
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

    -- select the cell drawing functions based on whether depth shading is required
    if depthshading then
        zdepth = N*CELLSIZE*0.5
        zdepth2 = zdepth+zdepth
        if celltype == "cube" then
            DrawBusyBox = DrawBusyCubeDepth
        elseif celltype == "sphere" then
            DrawBusyBox = DrawBusySphereDepth
        else -- celltype == "point"
            DrawBusyBox = DrawBusyPoint     -- shading is not done for points
        end
    else
        if celltype == "cube" then
            DrawBusyBox = DrawBusyCube
        elseif celltype == "sphere" then
            DrawBusyBox = DrawBusySphere
        else -- celltype == "point"
            DrawBusyBox = DrawBusyPoint
        end
    end

    -- clip names for cubes/spheres ("p" will be appended if drawing points)
    local evenclip = "E"
    local oddclip = "O"

    local i, j, evencell
    local stepi, stepj = N*stepy, N*stepz
    if testcell then
        j = N*fromz
        for z = fromz, toz, stepz do
            i = N*(fromy+j)
            for y = fromy, toy, stepy do
                evencell = (fromx + y + z) % 2 == 0
                for x = fromx, tox, stepx do
                    if evencell then
                        TestBusyBox(editing, i+x, x, y, z, evenclip)
                    else
                        TestBusyBox(editing, i+x, x, y, z, oddclip)
                    end
                    evencell = not evencell
                end
                i = i+stepi
            end
            j = j+stepj
        end
    else
        -- only live cells need to be drawn
        j = N*fromz
        for z = fromz, toz, stepz do
            i = N*(fromy+j)
            for y = fromy, toy, stepy do
                evencell = (fromx + y + z) % 2 == 0
                for x = fromx, tox, stepx do
                    if grid1[i+x] then
                        if evencell then
                            DrawBusyBox(x, y, z, evenclip)
                        else
                            DrawBusyBox(x, y, z, oddclip)
                        end
                    end
                    evencell = not evencell
                end
                i = i+stepi
            end
            j = j+stepj
        end
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
    mbar.enableitem(3, 3, bool)     -- Next Step
    mbar.enableitem(3, 4, bool)     -- Reset
    mbar.enableitem(3, 6, bool)     -- Set Rule
    -- View menu:
    mbar.enableitem(4, 3, bool)     -- Set Grid Size

    -- disable/enable unsafe buttons
    ssbutton.enable(bool)
    s1button.enable(bool)
    resetbutton.enable(bool)
    undobutton.enable(bool)
    redobutton.enable(bool)

    -- disable/enable unsafe radio buttons
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
        mbar.enableitem(3, 3, popcount > 0)     -- Next Step
        mbar.enableitem(3, 4, gencount > startcount)    -- Reset
    end

    mbar.radioitem(4, 5, celltype == "cube")
    mbar.radioitem(4, 6, celltype == "sphere")
    mbar.radioitem(4, 7, celltype == "point")
    mbar.tickitem(4, 9, showaxes)
    mbar.tickitem(4, 10, showlines)
    mbar.tickitem(4, 11, depthshading)

    mbar.show(0, 0, ovwd, mbarht)
end

--------------------------------------------------------------------------------

function DrawToolBar()
    ovt{"rgba", 230, 230, 230, 255}
    ovt{"fill", 0, 0, ovwd, toolbarht}

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
    local biggap = gap * 3

    ssbutton.show(x, y)
    x = x + ssbutton.wd + gap
    s1button.show(x, y)
    x = x + s1button.wd + gap
    resetbutton.show(x, y)
    x = x + resetbutton.wd + gap
    fitbutton.show(x, y)
    x = x + fitbutton.wd + biggap
    undobutton.show(x, y)
    x = x + undobutton.wd + gap
    redobutton.show(x, y)
    x = x + redobutton.wd + biggap
    drawbox.show(x, y, currcursor == drawcursor)
    x = x + drawbox.wd + gap
    selectbox.show(x, y, currcursor == selectcursor)
    x = x + selectbox.wd + gap
    movebox.show(x, y, currcursor == movecursor)

    -- show slider to right of radio buttons
    stepslider.show(x + movebox.wd + biggap, y, stepsize)

    -- show stepsize at right end of slider
    ov(op.black)
    local oldfont
    if g.os() == "Linux" then
        oldfont = ov("font 10 default")
    else
        oldfont = ov("font 10 default-bold")
    end
    local oldbg = ov("textoption background 230 230 230 255")
    local _, _ = op.maketext("Step="..stepsize)
    op.pastetext(stepslider.x + stepslider.wd + 2, y + 1)
    ov("textoption background "..oldbg)
    ov("font "..oldfont)

    -- last 2 buttons are at right end of tool bar
    x = ovwd - gap - exitbutton.wd
    exitbutton.show(x, y)
    x = x - gap - helpbutton.wd
    helpbutton.show(x, y)
end

----------------------------------------------------------------------

function UpdateWindowTitle()
    -- set window title if it has changed
    local newtitle = string.format("%s [%s]", pattname, rulestring)
    if dirty then newtitle = "*"..newtitle end
    if g.os() ~= "Mac" then newtitle = newtitle.." - Golly" end
    if newtitle ~= currtitle then
        g.settitle(newtitle)
        currtitle = newtitle
    end
end

----------------------------------------------------------------------

function Refresh(update)
    if scriptlevel > 0 and not update then
        -- user scripts need to call Update() when they want to refresh
        return
    end

    -- turn off event checking temporarily to avoid partial updates of overlay
    -- (eg. due to user resizing window while a pattern is generating)
    g.check(false)

    -- fill overlay with background color
    ov(BACK_COLOR)
    ovt{"fill"}

    -- get Z coordinates of the vertices of a rotated reference cube
    -- (for doing various depth tests)
    for i = 1, 8 do
        local _, _, z = TransformPoint(refcube[i])
        rotrefz[i] = z
    end

    if showaxes or showlines then DrawRearAxes() end

    local editing = currcursor ~= movecursor
    if popcount > 0 or pastecount > 0 or selcount > 0 or editing then
        if pastecount > 0 then
            -- paste cells will be translucent
            CreateTranslucentCell("p", PASTE_COLOR)
        end
        if selcount > 0 then
            -- selected cells will be translucent
            CreateTranslucentCell("s", SELECT_COLOR)
        end
        if editing then
            -- cells in active plane will be translucent
            CreateTranslucentCell("a", ACTIVE_COLOR)
        end
        if rulestring:find("^BusyBoxes") then
            if celltype == "cube" then
                CreateBusyCube("E")
                CreateBusyCube("O")
            elseif celltype == "sphere" then
                CreateBusySphere("E")
                CreateBusySphere("O")
            end
            if celltype == "point" or editing then
                -- 1st char must match the above clip names
                CreateBusyPoint("Ep", EVEN_COLOR)
                CreateBusyPoint("Op", ODD_COLOR)
            end
            DisplayBusyBoxes(editing)
        else
            if celltype == "cube" then
                CreateLiveCube()
            elseif celltype == "sphere" then
                CreateLiveSphere()
            end
            DisplayCells(editing)
        end
    end

    if showaxes or showlines then DrawFrontAxes() end

    -- show info in top left corner
    local info =
        "Generation = "..gencount.."\n"..
        "Population = "..popcount
    if selcount > 0 then
        info = info.."\nSelected cells = "..selcount
    end
    if editing then
        -- show cell coords of mouse if it's inside the active plane
        info = info.."\nx,y,z = "..activecell
    end
    ov(INFO_COLOR)
    local _, ht = op.maketext(info)
    op.pastetext(10, toolbarht + 10)
    if message then
        ov(MSG_COLOR)
        op.maketext(message)
        op.pastetext(10, toolbarht + 10 + ht + 10)
    end

    if toolbarht > 0 then DrawToolBar() end

    -- the overlay is 100% opaque and covers entire viewport
    -- so we can call ov("update") rather than slower g.update()
    ov("update")

    UpdateWindowTitle()

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
        ssbutton.customcolor = STOP_COLOR
        ssbutton.darkcustomcolor = SELSTOP_COLOR
        ssbutton.setlabel("Stop", false)
        mbar.setitem(3, 1, "Stop Generating")
    else
        ssbutton.customcolor = START_COLOR
        ssbutton.darkcustomcolor = SELSTART_COLOR
        ssbutton.setlabel("Start", false)
        mbar.setitem(3, 1, "Start Generating")
        if stopgen > 0 then
            -- terminate NextStep
            stopgen = 0
        end
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

    -- save current step size
    state.savestep = stepsize

    -- save current pattern
    state.savedirty = dirty
    state.savename = pattname
    state.savegencount = gencount
    state.savepopcount = popcount
    state.savecells = {}
    if popcount > 0 then
        for k,_ in pairs(grid1) do state.savecells[k] = 1 end
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

    -- restore grid size and active plane if necessary
    if N ~= state.saveN then
        N = state.saveN
        MIDGRID = (N+1-(N%2))*HALFCELL
        MIDCELL = HALFCELL-MIDGRID
        CreateAxes()
        -- active plane also depends on N
        SetActivePlane(state.saveplane, state.savepos)
    elseif activeplane ~= state.saveplane or activepos ~= state.savepos then
        SetActivePlane(state.saveplane, state.savepos)
    end

    -- restore cursor (determines whether active plane is displayed)
    currcursor = state.savecursor
    if not arrow_cursor then ov("cursor "..currcursor) end

    -- restore rule if necessary
    if rulestring ~= state.saverule then
        ParseRule(state.saverule)
    end

    -- restore step size
    stepsize = state.savestep

    -- restore pattern
    dirty = state.savedirty
    pattname = state.savename
    gencount = state.savegencount
    popcount = state.savepopcount
    grid1 = {}
    if popcount > 0 then
        for k,_ in pairs(state.savecells) do grid1[k] = 1 end
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

function SameState(state)
    -- return true if given state matches the current state
    if N ~= state.saveN then return false end
    if activeplane ~= state.saveplane then return false end
    if activepos ~= state.savepos then return false end
    if currcursor ~= state.savecursor then return false end
    if rulestring ~= state.saverule then return false end
    if dirty ~= state.savedirty then return false end
    if pattname ~= state.savename then return false end
    if gencount ~= state.savegencount then return false end
    if popcount ~= state.savepopcount then return false end
    if selcount ~= state.saveselcount then return false end
    if pastecount ~= state.savepcount then return false end
    
    -- note that we don't compare stepsize with state.savestep
    -- (we don't call RememberCurrentState when the user changes the step size)

    for k,_ in pairs(state.savecells) do if not grid1[k] then return false end end
    for k,_ in pairs(state.saveselected) do if not selected[k] then return false end end
    for k,_ in pairs(state.savepaste) do if not pastepatt[k] then return false end end

    return true
end

----------------------------------------------------------------------

function ClearUndoRedo()
    -- this might be called if a user script is running (eg. if it calls NewPattern)
    undostack = {}
    redostack = {}
    dirty = false
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

--------------------------------------------------------------------------------

function CheckIfGenerating()
    if generating and popcount > 0 and gencount > startcount then
        -- NextGeneration will be called soon
        RememberCurrentState()
    end
end

----------------------------------------------------------------------

function InitLiveBoundary()
    -- check for cells on grid edge
    liveedge = (minx == 0 or miny == 0 or minz == 0 or maxx == N-1 or maxy == N-1 or maxz == N-1)

    -- reset boundary
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
    popcount = 0
    InitLiveBoundary()
    -- remove selection
    selcount = 0
    selected = {}
    InitSelectionBoundary()
    -- remove paste pattern
    pastecount = 0
    pastepatt = {}
    collectgarbage()    -- might help avoid long delay when script exits (only on Mac OS 10.13???)
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

        if refresh then
            CheckIfGenerating()
            Refresh()
        end
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
        CheckIfGenerating()
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

function SaveCells()
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

function RestoreCells(livecells)
    -- restore pattern saved earlier by SaveCells
    -- (note that grid must currently be empty)
    if popcount > 0 then
        g.warn("Bug in RestoreCells: grid is not empty!")
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

function AllDead()
    -- this function is called at the start of every NextGen* function
    if popcount == 0 then
        StopGenerating()
        message = "All cells are dead."
        Refresh()
        return true         -- return from NextGen*
    else
        if gencount == startcount then
            -- remember starting state for later use in Reset()
            if scriptlevel > 0 then
                -- can't use undostack if user script is running
                startstate = SaveState()
            else
                -- starting state is on top of undostack
                startstate = undostack[#undostack]
            end
        end
        popcount = 0        -- incremented in NextGen*
        InitLiveBoundary()  -- updated in NextGen*
        return false        -- calculate the next generation
    end
end

----------------------------------------------------------------------

function DisplayGeneration(newgrid)
    -- this function is called at the end of every NextGen* function
    gencount = gencount + 1

    -- save the new grid
    grid1 = newgrid

    -- stopgen is > 0 if NextStep was called
    if popcount == 0 or gencount == stopgen then
        StopGenerating()
    end

    if gencount % stepsize == 0 or popcount == 0 then
        Refresh()
    end
end

----------------------------------------------------------------------

function UpdateBoundary(xlive, ylive, zlive, newpop)
    if newpop > 0 then
        local xlist = {}
        local ylist = {}
        local zlist = {}
        local ind = 1
        -- convert axis live cells into ordered list of keys
        for k, _ in pairs(xlive) do
            xlist[ind] = k
            ind = ind + 1
        end
        ind = 1
        for k, _ in pairs(ylive) do
            ylist[ind] = k
            ind = ind + 1
        end
        ind = 1
        for k, _ in pairs(zlive) do
            zlist[ind] = k
            ind = ind + 1
        end
        -- sort into ascending order
        table.sort(xlist)
        table.sort(ylist)
        table.sort(zlist)
        -- min is first value in ordered list, max is last value
        minx, maxx = xlist[1], xlist[#xlist]
        miny, maxy = ylist[1], ylist[#ylist]
        minz, maxz = zlist[1], zlist[#zlist]
    end
    -- save the population count
    popcount = newpop
end

----------------------------------------------------------------------

function NextGenMooreNoWrap()
    -- calculate and display the next generation for rules using the 3D Moore neighborhood
    local source = grid1
    local dest = {}
    local xlive, ylive, zlive = {}, {}, {}

    -- compute neighbor counts
    local count1 = {}
    local N = N
    local NN = N * N

    local offset = N
    for k,_ in pairs(source) do
        count1[k] = (count1[k] or 0) + 1
        count1[k+offset] = (count1[k+offset] or 0) + 1
        count1[k-offset] = (count1[k-offset] or 0) + 1
    end

    local count2 = {}
    offset = 1
    for k,v in pairs(count1) do
        count2[k] = (count2[k] or 0) + v
        count2[k+offset] = (count2[k+offset] or 0) + v
        count2[k-offset] = (count2[k-offset] or 0) + v
    end

    count1 = {}
    offset = NN
    for k,v in pairs(count2) do
        count1[k] = (count1[k] or 0) + v
        count1[k+offset] = (count1[k+offset] or 0) + v
        count1[k-offset] = (count1[k-offset] or 0) + v
    end

    -- apply rule to neighbour counts to create next generation
    local newpop = 0
    for k,v in pairs(count1) do
        if (source[k] and survivals[v-1]) or (births[v] and not source[k]) then
            -- create a live cell in grid2
            dest[k] = 1
            newpop = newpop + 1
            -- don't set dirty flag here!
            -- update boundary
            xlive[k % N] = 1
            ylive[k // N % N] = 1
            zlive[k // NN] = 1
        end
    end
    UpdateBoundary(xlive, ylive, zlive, newpop)
    DisplayGeneration(dest)
end

----------------------------------------------------------------------

function NextGenMooreWrap()
    -- calculate and display the next generation for rules using the 3D Moore neighborhood
    local source = grid1
    local dest = {}
    local xlive, ylive, zlive = {}, {}, {}

    -- compute neighbor counts
    local N = N
    local NN = N * N
    local NNN = NN * N

    local count1 = {}
    local NNmN = NN - N
    for k,_ in pairs(source) do
        count1[k] = (count1[k] or 0) + 1
        local y = k % NN
        local k2 = k + (y + N) % NN - y
        count1[k2] = (count1[k2] or 0) + 1
        k2 = k + (y + NNmN) % NN - y
        count1[k2] = (count1[k2] or 0) + 1
    end

    local count2 = {}
    local Nm1 = N - 1
    for k,v in pairs(count1) do
        count2[k] = (count2[k] or 0) + v
        local x = k % N
        local k2 = k + (x + 1) % N - x
        count2[k2] = (count2[k2] or 0) + v
        k2 = k + (x + Nm1) % N - x
        count2[k2] = (count2[k2] or 0) + v
    end

    count1 = {}
    local NNNmNN = NNN - NN
    for k,v in pairs(count2) do
        count1[k] = (count1[k] or 0) + v
        local k2 = (k + NN) % NNN
        count1[k2] = (count1[k2] or 0) + v
        k2 = (k + NNNmNN) % NNN
        count1[k2] = (count1[k2] or 0) + v
    end

    -- apply rule to neighbour counts to create next generation
    local newpop = 0
    for k,v in pairs(count1) do
        if (source[k] and survivals[v-1]) or (births[v] and not source[k]) then
            -- create a live cell in grid2
            dest[k] = 1
            newpop = newpop + 1
            -- don't set dirty flag here!
            -- update boundary
            xlive[k % N] = 1
            ylive[k // N % N] = 1
            zlive[k // NN] = 1
        end
    end
    UpdateBoundary(xlive, ylive, zlive, newpop)
    DisplayGeneration(dest)
end

----------------------------------------------------------------------

function NextGenMoore()
    if AllDead() then return end

    -- check if there are cells on the edge of the grid
    if liveedge then
        NextGenMooreWrap()
    else
        NextGenMooreNoWrap()
    end
end

----------------------------------------------------------------------

function NextGen6FacesNoWrap()
    -- calculate and display the next generation for rules using the 6-cell face neighborhood
    -- (aka the von Neumann neighborhood)
    local grid2 = {}
    local lcount = {}   -- neighbor counts (0..6) for live cells
    local ecount = {}   -- neighbor counts (1..6) for adjacent empty cells
    local xlive, ylive, zlive = {}, {}, {}
    local N = N
    local NN = N * N

    for k,_ in pairs(grid1) do
        lcount[k] = 0

        -- calculate the positions of the 6 cells next to each face of this cell
        local xp1 = k + 1
        local xm1 = k - 1
        local yp1 = k + N
        local ym1 = k - N
        local zp1 = k + NN
        local zm1 = k - NN
        if grid1[xp1] then lcount[k] = lcount[k] + 1 else ecount[xp1] = (ecount[xp1] or 0) + 1 end
        if grid1[xm1] then lcount[k] = lcount[k] + 1 else ecount[xm1] = (ecount[xm1] or 0) + 1 end
        if grid1[yp1] then lcount[k] = lcount[k] + 1 else ecount[yp1] = (ecount[yp1] or 0) + 1 end
        if grid1[ym1] then lcount[k] = lcount[k] + 1 else ecount[ym1] = (ecount[ym1] or 0) + 1 end
        if grid1[zp1] then lcount[k] = lcount[k] + 1 else ecount[zp1] = (ecount[zp1] or 0) + 1 end
        if grid1[zm1] then lcount[k] = lcount[k] + 1 else ecount[zm1] = (ecount[zm1] or 0) + 1 end
    end

    -- use lcounts and survivals to put live cells in grid2 (currently empty)
    local newpop = 0
    for k,v in pairs(lcount) do
        if survivals[v] then
            -- create a survivor cell in grid2
            grid2[k] = 1
            newpop = newpop + 1
            -- don't set dirty flag here!
            -- update boundary
            xlive[k % N] = 1
            ylive[k // N % N] = 1
            zlive[k // NN] = 1
        end
    end

    -- use ecounts and births to put live cells in grid2
    for k,v in pairs(ecount) do
        if births[v] then
            -- create a birth cell in grid2
            grid2[k] = 1
            newpop = newpop + 1
            -- don't set dirty flag here!
            -- update boundary
            xlive[k % N] = 1
            ylive[k // N % N] = 1
            zlive[k // NN] = 1
        end
    end
    UpdateBoundary(xlive, ylive, zlive, newpop)
    DisplayGeneration(grid2)
end

----------------------------------------------------------------------

function NextGen6FacesWrap()
    -- calculate and display the next generation for rules using the 6-cell face neighborhood
    -- (aka the von Neumann neighborhood)
    local grid2 = {}
    local lcount = {}   -- neighbor counts (0..6) for live cells
    local ecount = {}   -- neighbor counts (1..6) for adjacent empty cells
    local xlive, ylive, zlive = {}, {}, {}
    local N = N
    local NN = N * N

    for k,_ in pairs(grid1) do
        local x = k % N
        local y = k // N % N
        local z = k // NN
        lcount[k] = 0

        local Ny = N*y
        local NNz = NN*z
        local NypNNz = Ny + NNz
        local xpNNz = x + NNz
        local xpNy = x + Ny

        -- calculate the positions of the 6 cells next to each face of this cell
        local xp1 = (x+1)%N + NypNNz
        local xm1 = (x-1)%N + NypNNz
        local yp1 = N*((y+1)%N) + xpNNz
        local ym1 = N*((y-1)%N) + xpNNz
        local zp1 = NN*((z+1)%N) + xpNy
        local zm1 = NN*((z-1)%N) + xpNy

        if grid1[xp1] then lcount[k] = lcount[k] + 1 else ecount[xp1] = (ecount[xp1] or 0) + 1 end
        if grid1[xm1] then lcount[k] = lcount[k] + 1 else ecount[xm1] = (ecount[xm1] or 0) + 1 end
        if grid1[yp1] then lcount[k] = lcount[k] + 1 else ecount[yp1] = (ecount[yp1] or 0) + 1 end
        if grid1[ym1] then lcount[k] = lcount[k] + 1 else ecount[ym1] = (ecount[ym1] or 0) + 1 end
        if grid1[zp1] then lcount[k] = lcount[k] + 1 else ecount[zp1] = (ecount[zp1] or 0) + 1 end
        if grid1[zm1] then lcount[k] = lcount[k] + 1 else ecount[zm1] = (ecount[zm1] or 0) + 1 end
    end

    -- use lcounts and survivals to put live cells in grid2 (currently empty)
    local newpop = 0
    for k,v in pairs(lcount) do
        if survivals[v] then
            -- create a survivor cell in grid2
            grid2[k] = 1
            newpop = newpop + 1
            -- don't set dirty flag here!
            -- update boundary
            xlive[k % N] = 1
            ylive[k // N % N] = 1
            zlive[k // NN] = 1
        end
    end

    -- use ecounts and births to put live cells in grid2
    for k,v in pairs(ecount) do
        if births[v] then
            -- create a birth cell in grid2
            grid2[k] = 1
            newpop = newpop + 1
            -- don't set dirty flag here!
            -- update boundary
            xlive[k % N] = 1
            ylive[k // N % N] = 1
            zlive[k // NN] = 1
        end
    end
    UpdateBoundary(xlive, ylive, zlive, newpop)
    DisplayGeneration(grid2)
end


----------------------------------------------------------------------

function NextGen6Faces()
    if AllDead() then return end

    -- check if there are cells on the edge of the grid
    if liveedge then
        NextGen6FacesWrap()
    else
        NextGen6FacesNoWrap()
    end
end

----------------------------------------------------------------------

function NextGen8CornersNoWrap()
    -- calculate and display the next generation for rules using the 8-cell corner neighborhood
    local grid2 = {}
    local lcount = {}   -- neighbor counts (0..8) for live cells
    local ecount = {}   -- neighbor counts (1..8) for adjacent empty cells
    local xlive, ylive, zlive = {}, {}, {}
    local N = N
    local NN = N * N

    for k,_ in pairs(grid1) do
        lcount[k] = 0

        -- calculate the positions of the 8 cells touching each corner of this cell
        local ppp = k + 1 + N + NN
        local mmm = k - 1 - N - NN
        local ppm = k + 1 + N - NN
        local mmp = k - 1 - N + NN
        local mpp = k - 1 + N + NN
        local pmm = k + 1 - N - NN
        local pmp = k + 1 - N + NN
        local mpm = k - 1 + N - NN

        if grid1[ppp] then lcount[k] = lcount[k] + 1 else ecount[ppp] = (ecount[ppp] or 0) + 1 end
        if grid1[mmm] then lcount[k] = lcount[k] + 1 else ecount[mmm] = (ecount[mmm] or 0) + 1 end
        if grid1[ppm] then lcount[k] = lcount[k] + 1 else ecount[ppm] = (ecount[ppm] or 0) + 1 end
        if grid1[mmp] then lcount[k] = lcount[k] + 1 else ecount[mmp] = (ecount[mmp] or 0) + 1 end
        if grid1[mpp] then lcount[k] = lcount[k] + 1 else ecount[mpp] = (ecount[mpp] or 0) + 1 end
        if grid1[pmm] then lcount[k] = lcount[k] + 1 else ecount[pmm] = (ecount[pmm] or 0) + 1 end
        if grid1[pmp] then lcount[k] = lcount[k] + 1 else ecount[pmp] = (ecount[pmp] or 0) + 1 end
        if grid1[mpm] then lcount[k] = lcount[k] + 1 else ecount[mpm] = (ecount[mpm] or 0) + 1 end
    end

    -- use lcounts and survivals to put live cells in grid2 (currently empty)
    local newpop = 0
    for k,v in pairs(lcount) do
        if survivals[v] then
            -- create a survivor cell in grid2
            grid2[k] = 1
            newpop = newpop + 1
            -- don't set dirty flag here!
            -- update boundary
            xlive[k % N] = 1
            ylive[k // N % N] = 1
            zlive[k // NN] = 1
        end
    end

    -- use ecounts and births to put live cells in grid2
    for k,v in pairs(ecount) do
        if births[v] then
            -- create a birth cell in grid2
            grid2[k] = 1
            newpop = newpop + 1
            -- don't set dirty flag here!
            -- update boundary
            xlive[k % N] = 1
            ylive[k // N % N] = 1
            zlive[k // NN] = 1
        end
    end
    UpdateBoundary(xlive, ylive, zlive, newpop)
    DisplayGeneration(grid2)
end

----------------------------------------------------------------------

function NextGen8CornersWrap()
    -- calculate and display the next generation for rules using the 8-cell corner neighborhood
    local grid2 = {}
    local lcount = {}   -- neighbor counts (0..8) for live cells
    local ecount = {}   -- neighbor counts (1..8) for adjacent empty cells
    local xlive, ylive, zlive = {}, {}, {}
    local N = N
    local NN = N * N

    for k,_ in pairs(grid1) do
        local x = k % N
        local y = k // N % N
        local z = k // NN
        lcount[k] = 0

        local xp1 = (x+1)%N
        local xm1 = (x-1)%N
        local yp1 = N*((y+1)%N)
        local ym1 = N*((y-1)%N)
        local zp1 = NN*((z+1)%N)
        local zm1 = NN*((z-1)%N)

        -- calculate the positions of the 8 cells touching each corner of this cell
        local ppp = xp1 + yp1 + zp1
        local mmm = xm1 + ym1 + zm1
        local ppm = xp1 + yp1 + zm1
        local mmp = xm1 + ym1 + zp1
        local mpp = xm1 + yp1 + zp1
        local pmm = xp1 + ym1 + zm1
        local pmp = xp1 + ym1 + zp1
        local mpm = xm1 + yp1 + zm1

        if grid1[ppp] then lcount[k] = lcount[k] + 1 else ecount[ppp] = (ecount[ppp] or 0) + 1 end
        if grid1[mmm] then lcount[k] = lcount[k] + 1 else ecount[mmm] = (ecount[mmm] or 0) + 1 end
        if grid1[ppm] then lcount[k] = lcount[k] + 1 else ecount[ppm] = (ecount[ppm] or 0) + 1 end
        if grid1[mmp] then lcount[k] = lcount[k] + 1 else ecount[mmp] = (ecount[mmp] or 0) + 1 end
        if grid1[mpp] then lcount[k] = lcount[k] + 1 else ecount[mpp] = (ecount[mpp] or 0) + 1 end
        if grid1[pmm] then lcount[k] = lcount[k] + 1 else ecount[pmm] = (ecount[pmm] or 0) + 1 end
        if grid1[pmp] then lcount[k] = lcount[k] + 1 else ecount[pmp] = (ecount[pmp] or 0) + 1 end
        if grid1[mpm] then lcount[k] = lcount[k] + 1 else ecount[mpm] = (ecount[mpm] or 0) + 1 end
    end

    -- use lcounts and survivals to put live cells in grid2 (currently empty)
    local newpop = 0
    for k,v in pairs(lcount) do
        if survivals[v] then
            -- create a survivor cell in grid2
            grid2[k] = 1
            newpop = newpop + 1
            -- don't set dirty flag here!
            -- update boundary
            xlive[k % N] = 1
            ylive[k // N % N] = 1
            zlive[k // NN] = 1
        end
    end

    -- use ecounts and births to put live cells in grid2
    for k,v in pairs(ecount) do
        if births[v] then
            -- create a birth cell in grid2
            grid2[k] = 1
            newpop = newpop + 1
            -- don't set dirty flag here!
            -- update boundary
            xlive[k % N] = 1
            ylive[k // N % N] = 1
            zlive[k // NN] = 1
        end
    end
    UpdateBoundary(xlive, ylive, zlive, newpop)
    DisplayGeneration(grid2)
end

----------------------------------------------------------------------

function NextGen8Corners()
    if AllDead() then return end

    -- check if there are cells on the edge of the grid
    if liveedge then
        NextGen8CornersWrap()
    else
        NextGen8CornersNoWrap()
    end
end

----------------------------------------------------------------------

function NextGen12EdgesNoWrap()
    -- calculate and display the next generation for rules using the 12-cell edge neighborhood
    local grid2 = {}
    local lcount = {}   -- neighbor counts (0..12) for live cells
    local ecount = {}   -- neighbor counts (1..12) for adjacent empty cells
    local xlive, ylive, zlive = {}, {}, {}
    local N = N
    local NN = N * N

    for k,_ in pairs(grid1) do
        lcount[k] = 0

        -- calculate the positions of the 12 cells next to each edge of this cell
        local xpp = k + N + NN
        local xmm = k - N - NN
        local xpm = k + N - NN
        local xmp = k - N + NN

        local pyp = k + 1 + NN
        local mym = k - 1 - NN
        local pym = k + 1 - NN
        local myp = k - 1 + NN

        local ppz = k + 1 + N
        local mmz = k - 1 - N
        local pmz = k + 1 - N
        local mpz = k - 1 + N

        if grid1[xpp] then lcount[k] = lcount[k] + 1 else ecount[xpp] = (ecount[xpp] or 0) + 1 end
        if grid1[xmm] then lcount[k] = lcount[k] + 1 else ecount[xmm] = (ecount[xmm] or 0) + 1 end
        if grid1[xpm] then lcount[k] = lcount[k] + 1 else ecount[xpm] = (ecount[xpm] or 0) + 1 end
        if grid1[xmp] then lcount[k] = lcount[k] + 1 else ecount[xmp] = (ecount[xmp] or 0) + 1 end

        if grid1[pyp] then lcount[k] = lcount[k] + 1 else ecount[pyp] = (ecount[pyp] or 0) + 1 end
        if grid1[mym] then lcount[k] = lcount[k] + 1 else ecount[mym] = (ecount[mym] or 0) + 1 end
        if grid1[pym] then lcount[k] = lcount[k] + 1 else ecount[pym] = (ecount[pym] or 0) + 1 end
        if grid1[myp] then lcount[k] = lcount[k] + 1 else ecount[myp] = (ecount[myp] or 0) + 1 end

        if grid1[ppz] then lcount[k] = lcount[k] + 1 else ecount[ppz] = (ecount[ppz] or 0) + 1 end
        if grid1[mmz] then lcount[k] = lcount[k] + 1 else ecount[mmz] = (ecount[mmz] or 0) + 1 end
        if grid1[pmz] then lcount[k] = lcount[k] + 1 else ecount[pmz] = (ecount[pmz] or 0) + 1 end
        if grid1[mpz] then lcount[k] = lcount[k] + 1 else ecount[mpz] = (ecount[mpz] or 0) + 1 end
    end

    -- use lcounts and survivals to put live cells in grid2 (currently empty)
    local newpop = 0
    for k,v in pairs(lcount) do
        if survivals[v] then
            -- create a survivor cell in grid2
            grid2[k] = 1
            newpop = newpop + 1
            -- don't set dirty flag here!
            -- update boundary
            xlive[k % N] = 1
            ylive[k // N % N] = 1
            zlive[k // NN] = 1
        end
    end

    -- use ecounts and births to put live cells in grid2
    for k,v in pairs(ecount) do
        if births[v] then
            -- create a birth cell in grid2
            grid2[k] = 1
            newpop = newpop + 1
            -- don't set dirty flag here!
            -- update boundary
            xlive[k % N] = 1
            ylive[k // N % N] = 1
            zlive[k // NN] = 1
        end
    end
    UpdateBoundary(xlive, ylive, zlive, newpop)
    DisplayGeneration(grid2)
end
----------------------------------------------------------------------

function NextGen12EdgesWrap()
    -- calculate and display the next generation for rules using the 12-cell edge neighborhood
    local grid2 = {}
    local lcount = {}   -- neighbor counts (0..12) for live cells
    local ecount = {}   -- neighbor counts (1..12) for adjacent empty cells
    local xlive, ylive, zlive = {}, {}, {}
    local N = N
    local NN = N * N

    for k,_ in pairs(grid1) do
        local x = k % N
        local y = k // N % N
        local z = k // NN
        lcount[k] = 0

        local xp1 = (x+1)%N
        local xm1 = (x-1)%N
        local yp1 = N*((y+1)%N)
        local ym1 = N*((y-1)%N)
        local zp1 = NN*((z+1)%N)
        local zm1 = NN*((z-1)%N)
        local Ny = N*y
        local NNz = NN*z

        -- calculate the positions of the 12 cells next to each edge of this cell
        local xpp = x + yp1 + zp1
        local xmm = x + ym1 + zm1
        local xpm = x + yp1 + zm1
        local xmp = x + ym1 + zp1

        local pyp = xp1 + Ny + zp1
        local mym = xm1 + Ny + zm1
        local pym = xp1 + Ny + zm1
        local myp = xm1 + Ny + zp1

        local ppz = xp1 + yp1 + NNz
        local mmz = xm1 + ym1 + NNz
        local pmz = xp1 + ym1 + NNz
        local mpz = xm1 + yp1 + NNz

        if grid1[xpp] then lcount[k] = lcount[k] + 1 else ecount[xpp] = (ecount[xpp] or 0) + 1 end
        if grid1[xmm] then lcount[k] = lcount[k] + 1 else ecount[xmm] = (ecount[xmm] or 0) + 1 end
        if grid1[xpm] then lcount[k] = lcount[k] + 1 else ecount[xpm] = (ecount[xpm] or 0) + 1 end
        if grid1[xmp] then lcount[k] = lcount[k] + 1 else ecount[xmp] = (ecount[xmp] or 0) + 1 end

        if grid1[pyp] then lcount[k] = lcount[k] + 1 else ecount[pyp] = (ecount[pyp] or 0) + 1 end
        if grid1[mym] then lcount[k] = lcount[k] + 1 else ecount[mym] = (ecount[mym] or 0) + 1 end
        if grid1[pym] then lcount[k] = lcount[k] + 1 else ecount[pym] = (ecount[pym] or 0) + 1 end
        if grid1[myp] then lcount[k] = lcount[k] + 1 else ecount[myp] = (ecount[myp] or 0) + 1 end

        if grid1[ppz] then lcount[k] = lcount[k] + 1 else ecount[ppz] = (ecount[ppz] or 0) + 1 end
        if grid1[mmz] then lcount[k] = lcount[k] + 1 else ecount[mmz] = (ecount[mmz] or 0) + 1 end
        if grid1[pmz] then lcount[k] = lcount[k] + 1 else ecount[pmz] = (ecount[pmz] or 0) + 1 end
        if grid1[mpz] then lcount[k] = lcount[k] + 1 else ecount[mpz] = (ecount[mpz] or 0) + 1 end
    end

    -- use lcounts and survivals to put live cells in grid2 (currently empty)
    local newpop = 0
    for k,v in pairs(lcount) do
        if survivals[v] then
            -- create a survivor cell in grid2
            grid2[k] = 1
            newpop = newpop + 1
            -- don't set dirty flag here!
            -- update boundary
            xlive[k % N] = 1
            ylive[k // N % N] = 1
            zlive[k // NN] = 1
        end
    end

    -- use ecounts and births to put live cells in grid2
    for k,v in pairs(ecount) do
        if births[v] then
            -- create a birth cell in grid2
            grid2[k] = 1
            newpop = newpop + 1
            -- don't set dirty flag here!
            -- update boundary
            xlive[k % N] = 1
            ylive[k // N % N] = 1
            zlive[k // NN] = 1
        end
    end
    UpdateBoundary(xlive, ylive, zlive, newpop)
    DisplayGeneration(grid2)
end

----------------------------------------------------------------------

function NextGen12Edges()
    if AllDead() then return end

    -- check if there are cells on the edge of the grid
    if liveedge then
        NextGen12EdgesWrap()
    else
        NextGen12EdgesNoWrap()
    end
end

----------------------------------------------------------------------

function NextGenHexahedralNoWrap()
    -- calculate and display the next generation for rules using the 12-cell hexahedral neighborhood
    local grid2 = {}
    local lcount = {}   -- neighbor counts (0..12) for live cells
    local ecount = {}   -- neighbor counts (1..12) for adjacent empty cells
    local xlive, ylive, zlive = {}, {}, {}
    local N = N
    local NN = N * N

    for k,_ in pairs(grid1) do
        lcount[k] = 0

        -- calculate the positions of the 12 neighboring cells (using the top offsets given
        -- on page 872 in http://www.complex-systems.com/pdf/01-5-1.pdf)
        local xym = k - NN
        local xyp = k + NN
        local xpm = k + N - NN
        local xpz = k + N
        local xmp = k - N + NN
        local xmz = k - N
        local pym = k + 1 - NN
        local pyz = k + 1
        local myp = k - 1 + NN
        local myz = k - 1
        local pmz = k + 1 - N
        local mpz = k - 1 + N

        if grid1[xym] then lcount[k] = lcount[k] + 1 else ecount[xym] = (ecount[xym] or 0) + 1 end
        if grid1[xyp] then lcount[k] = lcount[k] + 1 else ecount[xyp] = (ecount[xyp] or 0) + 1 end
        if grid1[xpm] then lcount[k] = lcount[k] + 1 else ecount[xpm] = (ecount[xpm] or 0) + 1 end
        if grid1[xpz] then lcount[k] = lcount[k] + 1 else ecount[xpz] = (ecount[xpz] or 0) + 1 end

        if grid1[xmp] then lcount[k] = lcount[k] + 1 else ecount[xmp] = (ecount[xmp] or 0) + 1 end
        if grid1[xmz] then lcount[k] = lcount[k] + 1 else ecount[xmz] = (ecount[xmz] or 0) + 1 end
        if grid1[pym] then lcount[k] = lcount[k] + 1 else ecount[pym] = (ecount[pym] or 0) + 1 end
        if grid1[pyz] then lcount[k] = lcount[k] + 1 else ecount[pyz] = (ecount[pyz] or 0) + 1 end

        if grid1[myp] then lcount[k] = lcount[k] + 1 else ecount[myp] = (ecount[myp] or 0) + 1 end
        if grid1[myz] then lcount[k] = lcount[k] + 1 else ecount[myz] = (ecount[myz] or 0) + 1 end
        if grid1[pmz] then lcount[k] = lcount[k] + 1 else ecount[pmz] = (ecount[pmz] or 0) + 1 end
        if grid1[mpz] then lcount[k] = lcount[k] + 1 else ecount[mpz] = (ecount[mpz] or 0) + 1 end
    end

    -- use lcounts and survivals to put live cells in grid2 (currently empty)
    local newpop = 0
    for k,v in pairs(lcount) do
        if survivals[v] then
            -- create a survivor cell in grid2
            grid2[k] = 1
            newpop = newpop + 1
            -- don't set dirty flag here!
            -- update boundary
            xlive[k % N] = 1
            ylive[k // N % N] = 1
            zlive[k // NN] = 1
        end
    end

    -- use ecounts and births to put live cells in grid2
    for k,v in pairs(ecount) do
        if births[v] then
            -- create a birth cell in grid2
            grid2[k] = 1
            newpop = newpop + 1
            -- don't set dirty flag here!
            -- update boundary
            xlive[k % N] = 1
            ylive[k // N % N] = 1
            zlive[k // NN] = 1
        end
    end
    UpdateBoundary(xlive, ylive, zlive, newpop)
    DisplayGeneration(grid2)
end

----------------------------------------------------------------------

function NextGenHexahedralWrap()
    -- calculate and display the next generation for rules using the 12-cell hexahedral neighborhood
    local grid2 = {}
    local lcount = {}   -- neighbor counts (0..12) for live cells
    local ecount = {}   -- neighbor counts (1..12) for adjacent empty cells
    local xlive, ylive, zlive = {}, {}, {}
    local N = N
    local NN = N * N

    for k,_ in pairs(grid1) do
        local x = k % N
        local y = k // N % N
        local z = k // NN
        lcount[k] = 0

        local xp1 = (x+1)%N
        local xm1 = (x-1)%N
        local yp1 = N*((y+1)%N)
        local ym1 = N*((y-1)%N)
        local zp1 = NN*((z+1)%N)
        local zm1 = NN*((z-1)%N)
        local Ny = N*y
        local NNz = NN*z

        -- calculate the positions of the 12 neighboring cells (using the top offsets given
        -- on page 872 in http://www.complex-systems.com/pdf/01-5-1.pdf)
        local xym = x + Ny + zm1
        local xyp = x + Ny + zp1
        local xpm = x + yp1 + zm1
        local xpz = x + yp1 + NNz
        local xmp = x + ym1 + zp1
        local xmz = x + ym1 + NNz
        local pym = xp1 + Ny + zm1
        local pyz = xp1 + Ny + NNz
        local myp = xm1 + Ny + zp1
        local myz = xm1 + Ny + NNz
        local pmz = xp1 + ym1 + NNz
        local mpz = xm1 + yp1 + NNz

        if grid1[xym] then lcount[k] = lcount[k] + 1 else ecount[xym] = (ecount[xym] or 0) + 1 end
        if grid1[xyp] then lcount[k] = lcount[k] + 1 else ecount[xyp] = (ecount[xyp] or 0) + 1 end
        if grid1[xpm] then lcount[k] = lcount[k] + 1 else ecount[xpm] = (ecount[xpm] or 0) + 1 end
        if grid1[xpz] then lcount[k] = lcount[k] + 1 else ecount[xpz] = (ecount[xpz] or 0) + 1 end

        if grid1[xmp] then lcount[k] = lcount[k] + 1 else ecount[xmp] = (ecount[xmp] or 0) + 1 end
        if grid1[xmz] then lcount[k] = lcount[k] + 1 else ecount[xmz] = (ecount[xmz] or 0) + 1 end
        if grid1[pym] then lcount[k] = lcount[k] + 1 else ecount[pym] = (ecount[pym] or 0) + 1 end
        if grid1[pyz] then lcount[k] = lcount[k] + 1 else ecount[pyz] = (ecount[pyz] or 0) + 1 end

        if grid1[myp] then lcount[k] = lcount[k] + 1 else ecount[myp] = (ecount[myp] or 0) + 1 end
        if grid1[myz] then lcount[k] = lcount[k] + 1 else ecount[myz] = (ecount[myz] or 0) + 1 end
        if grid1[pmz] then lcount[k] = lcount[k] + 1 else ecount[pmz] = (ecount[pmz] or 0) + 1 end
        if grid1[mpz] then lcount[k] = lcount[k] + 1 else ecount[mpz] = (ecount[mpz] or 0) + 1 end
    end

    -- use lcounts and survivals to put live cells in grid2 (currently empty)
    local newpop = 0
    for k,v in pairs(lcount) do
        if survivals[v] then
            -- create a survivor cell in grid2
            grid2[k] = 1
            newpop = newpop + 1
            -- don't set dirty flag here!
            -- update boundary
            xlive[k % N] = 1
            ylive[k // N % N] = 1
            zlive[k // NN] = 1
        end
    end

    -- use ecounts and births to put live cells in grid2
    for k,v in pairs(ecount) do
        if births[v] then
            -- create a birth cell in grid2
            grid2[k] = 1
            newpop = newpop + 1
            -- don't set dirty flag here!
            -- update boundary
            xlive[k % N] = 1
            ylive[k // N % N] = 1
            zlive[k // NN] = 1
        end
    end
    UpdateBoundary(xlive, ylive, zlive, newpop)
    DisplayGeneration(grid2)
end

----------------------------------------------------------------------

function NextGenHexahedral()
    if AllDead() then return end

    -- check if there are cells on the edge of the grid
    if liveedge then
        NextGenHexahedralWrap()
    else
        NextGenHexahedralNoWrap()
    end
end

----------------------------------------------------------------------

function NextGenBusyBoxes()
    -- calculate and display the next generation for the BusyBoxes rule
    -- (see http://www.busyboxes.org/faq.html)
    if N%2 == 1 then
        -- BusyBoxes requires an even numbered grid size
        SetGridSize(N+1)
    end

    if AllDead() then return end

    -- the algorithm used below is a slightly modified (and corrected!)
    -- version of the kernel code in Ready's Salt 3D example
    -- (see Patterns/CellularAutomata/Salt/salt3D_circular330.vti);
    -- it uses a rule based on 28 cells in a 7x7 neighborhood for each live cell
    local coords = {
        -- 1 to 4 are the coordinates for the 4 potential swap sites:
        {1,1},{-1,1},{-1,-1},{1,-1},
        -- 5 to 12 are activators:
        {2,-1},{2,1},{1,2},{-1,2},{-2,1},{-2,-1},{-1,-2},{1,-2},
        -- 13 to 28 are inhibitors:
        {-2,-3},{0,-3},{2,-3},
        {-3,-2},{3,-2},
        {0,-1},
        {-3,0},{-1,0},{1,0},{3,0},
        {0,1},
        {-3,2},{3,2},
        {-2,3},{0,3},{2,3}
    }
    -- numbers are indices into the coords array
    local activators = { {5,8}, {7,10}, {9,12}, {6,11} }
    local inhibitors = { {18,25,22,27,20,28, 7,10, 9,12, 6,11},
                         {18,24,19,27,21,26, 5,8,  9,12, 6,11},
                         {16,23,14,19,13,21, 5,8,  7,10, 6,11},
                         {20,15,14,22,17,23, 5,8,  7,10, 9,12} }

    local mirror_mode = rulestring:sub(-1) ~= "W"
    local phase = gencount % 6
    local grid2 = {}
    local xlive, ylive, zlive = {}, {}, {}
    local N = N
    local NN = N * N

    local newpop = 0
    for k,_ in pairs(grid1) do
        local x = k % N
        local y = k // N % N
        local z = k // NN
        if (x + y + z) % 2 == phase % 2 then
            -- this live cell has the right parity so get values for its 28 neighbors
            local val = {}
            local sx, sy, sz
            for i = 1, 28 do
                if phase == 0 or phase == 3 then
                    -- use XY plane
                    sx = (x + coords[i][1]) % N
                    sy = (y + coords[i][2]) % N
                    sz = z
                elseif phase == 1 or phase == 4 then
                    -- use YZ plane
                    sx = x
                    sy = (y + coords[i][1]) % N
                    sz = (z + coords[i][2]) % N
                else
                    -- phase == 2 or 5 so use XZ plane
                    sx = (x + coords[i][1]) % N
                    sy = y
                    sz = (z + coords[i][2]) % N
                end
                val[i] = grid1[sx + N * (sy + N * sz)]
            end

            -- find the potential swaps
            local numswaps = 0
            local swapi
            for i = 1, 4 do
                -- if either activator is a live cell then the swap is possible,
                -- but if any inhibitor is a live cell then the swap is forbidden
                if (val[activators[i][1]] or val[activators[i][2]])
                    and not (val[inhibitors[i][1]] or val[inhibitors[i][2]] or
                             val[inhibitors[i][3]] or val[inhibitors[i][4]] or
                             val[inhibitors[i][5]] or val[inhibitors[i][6]] or
                             val[inhibitors[i][7]] or val[inhibitors[i][8]] or
                             val[inhibitors[i][9]] or val[inhibitors[i][10]] or
                             val[inhibitors[i][11]] or val[inhibitors[i][12]]) then
                    numswaps = numswaps + 1
                    if numswaps > 1 then break end
                    swapi = i   -- index of swap location in coords array (1..4)
                end
            end

            -- if only one swap, and only to an empty cell, then do it
            if numswaps == 1 and not val[swapi] then
                -- calculate the swap position
                local newx, newy, newz
                if phase == 0 or phase == 3 then
                    -- use XY plane
                    newx = x + coords[swapi][1]
                    newy = y + coords[swapi][2]
                    newz = z
                elseif phase == 1 or phase == 4 then
                    -- use YZ plane
                    newx = x
                    newy = y + coords[swapi][1]
                    newz = z + coords[swapi][2]
                else
                    -- phase == 2 or 5 so use XZ plane
                    newx = x + coords[swapi][1]
                    newy = y
                    newz = z + coords[swapi][2]
                end
                -- if using mirror mode then don't wrap
                if mirror_mode and
                    ( newx < 0 or newx >= N or
                      newy < 0 or newy >= N or
                      newz < 0 or newz >= N ) then
                    -- swap position is outside grid so don't do it
                    grid2[k] = 1
                    newpop = newpop + 1
                    -- don't set dirty flag here!
                else
                    -- do the swap, wrapping if necessary
                    x = newx % N
                    y = newy % N
                    z = newz % N
                    grid2[x + N * (y + N * z)] = 1
                    newpop = newpop + 1
                    -- don't set dirty flag here!
                end
            else
                -- don't swap this live cell
                grid2[k] = 1
                newpop = newpop + 1
                -- don't set dirty flag here!
            end
        else
            -- live cell with wrong parity
            grid2[k] = 1
            newpop = newpop + 1
            -- don't set dirty flag here!
        end

        -- update boundary
        xlive[x] = 1
        ylive[y] = 1
        zlive[z] = 1
    end
    UpdateBoundary(xlive, ylive, zlive, newpop)
    DisplayGeneration(grid2)
end

----------------------------------------------------------------------

function NewPattern()
    pattname = "untitled"
    SetCursor(drawcursor)
    gencount = 0
    startcount = 0
    stepsize = 1
    StopGenerating()
    ClearCells()
    ClearUndoRedo()
    -- avoid unnecessary work if user script calls NewPattern
    if scriptlevel == 0 then
        SetActivePlane()        -- restore default active plane
        InitialView()           -- calls Refresh
    end
end

----------------------------------------------------------------------

function ReadPattern(filepath)
    local f = io.open(filepath,"r")
    if not f then return "Failed to open file:\n"..filepath end

    -- Lua's f:read("*l") doesn't detect CR as EOL so we do this ugly stuff:
    -- read entire file and convert any CRs to LFs
    local all = f:read("*a"):gsub("\r", "\n").."\n"
    local nextline = all:gmatch("(.-)\n")

    f:close()

    local line = nextline()
    if line == nil or not line:find("^3D") then
        return "Invalid RLE3 file (first line must start with 3D)."
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
    local comments = ""

    while true do
        line = nextline()
        if not line then break end
        local ch = line:sub(1,1)
        if #ch == 0 then
            -- ignore blank line
        elseif ch == "#" then
            comments = comments..line.."\n"
        elseif ch == "x" then
            -- parse header
            wd, ht, dp, trule = line:match("x=(.+) y=(.+) z=(.+) rule=(.+)$")
            wd = tonumber(wd)
            ht = tonumber(ht)
            dp = tonumber(dp)
            if wd and ht and dp then
                if max(wd, ht, dp) > tsize then
                    return "The pattern size is bigger than the grid size!"
                end
                x = x0
                y = y0
                z = z0
                -- check that pattern is positioned within given grid size
                if x < 0 or x + wd > tsize or
                   y < 0 or y + ht > tsize or
                   z < 0 or z + dp > tsize then
                    return "The pattern is positioned outside the grid!"
                end
            else
                return "Bad number in header line:\n"..line
            end
            local saverule = rulestring
            if not ParseRule(trule) then
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
                        return "Unexpected character: "..ch
                    end
                    runcount = 0
                end
            end
        end
    end

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
    return nil, newpattern, comments
end

----------------------------------------------------------------------

function UpdateCurrentGrid(newpattern)
    -- called by OpenPattern/OpenClipboard
    N = newpattern.newsize
    MIDGRID = (N+1-(N%2))*HALFCELL
    MIDCELL = HALFCELL-MIDGRID
    CreateAxes()
    ClearCells()        -- resets grid1, popcount and selection info

    grid1 = newpattern.newgrid
    popcount = newpattern.newpop
    minx = newpattern.newminx
    miny = newpattern.newminy
    minz = newpattern.newminz
    maxx = newpattern.newmaxx
    maxy = newpattern.newmaxy
    maxz = newpattern.newmaxz
    -- note that ClearCells has set minimal_live_bounds = true

    ParseRule(newpattern.newrule)   -- sets rulestring, survivals, births, NextGeneration
    gencount = newpattern.newgens
    startcount = gencount           -- for Reset
    stepsize = 1
    StopGenerating()
    SetCursor(movecursor)
    ClearUndoRedo()         -- dirty = false
    if scriptlevel == 0 then
        SetActivePlane()
        InitialView()       -- calls Refresh
    end
end

----------------------------------------------------------------------

function OpenPattern(filepath)
    if filepath then
        local err, newpattern, comments = ReadPattern(filepath)
        if err then
            g.warn(err, false)
        else
            -- pattern ok so use info in newpattern to update current grid;
            -- set pattname to file name at end of filepath
            pattname = filepath:match("^.+"..pathsep.."(.+)$")
            if #comments > 0 then
                message = comments
            end
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
        local err, newpattern, comments = ReadPattern(filepath)
        if err then
            g.warn(err, false)
        else
            -- pattern ok so use info in newpattern to update current grid
            pattname = "clipboard"
            if #comments > 0 then
                message = comments
            end
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

    -- Lua's f:read("*l") doesn't detect CR as EOL so we do this ugly stuff:
    -- read entire file and convert any CRs to LFs
    local all = f:read("*a"):gsub("\r", "\n").."\n"
    local nextline = all:gmatch("(.-)\n")

    while true do
        local line = nextline()
        if not line then break end
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
        RememberCurrentState()  -- #undostack is > 0
        EnableControls(false)   -- disable most menu items and buttons
    end

    scriptlevel = scriptlevel + 1
    local status, err = pcall(func)
    scriptlevel = scriptlevel - 1

    if scriptlevel == 0 then
        -- note that if the script called NewPattern/RandomPattern/OpenPattern
        -- or any other function that called ClearUndoRedo then the undostack
        -- is empty and dirty should be false
        if #undostack == 0 then
            -- a later SetCell call might have set dirty to true, so reset it
            dirty = false
            if gencount > startcount then
                -- script called Step after NewPattern/RandomPattern/OpenPattern
                -- so push startstate onto undostack so user can Reset/Undo
                undostack[1] = startstate
                startstate.savedirty = false
            end
        elseif SameState(undostack[#undostack]) then
            -- script didn't change the current state so pop undostack
            table.remove(undostack)
        end
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
        CheckIfGenerating()
        Refresh()               -- calls DrawToolBar
    end
end

----------------------------------------------------------------------

function RunScript(filepath)
    if filepath then
        local f = io.open(filepath, "r")
        if f then
            -- Lua's f:read("*l") doesn't detect CR as EOL so we do this ugly stuff:
            -- read entire file and convert any CRs to LFs
            local all = f:read("*a"):gsub("\r", "\n").."\n"
            local nextline = all:gmatch("(.-)\n")
            local line1 = nextline()
            f:close()
            if not (line1 and line1:find("3D.lua")) then
                g.warn("3D.lua was not found on first line of script.", false)
                return
            end
        else
            g.warn("Script file could not be opened:\n"..filepath, false)
            return
        end
        local func, msg = loadfile(filepath)
        if func then
            CallScript(func, false)
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
    local cliptext = g.getclipstr()
    local eol = cliptext:find("[\n\r]")
    if not (eol and cliptext:sub(1,eol):find("3D.lua")) then
        g.warn("3D.lua was not found on first line of clipboard.", false)
        return
    end
    local func, msg = load(cliptext)
    if func then
        CallScript(func, true)
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

function RandomPattern(percentage, fill, sphere)
    local function getperc()
        local initstring = randstring
        ::try_again::
        local s = g.getstring("Enter density as a percentage (from 0 to 100).\n"..
                              "Append \"f\" to fill the grid.\n"..
                              "Append \"s\" to create a sphere.",
                              initstring, "Random pattern")
        initstring = s
        fill = s:find("f")
        sphere = s:find("s")
        s = s:gsub("[fs]","")
        if s:find("[^%d]") then
            g.warn("Only digits and the letters f and s are allowed.\nTry again.")
            goto try_again
        end
        if validint(s) and (tonumber(s) >= 0) and (tonumber(s) <= 100) then
            perc = tonumber(s)
            randstring = initstring
        else
            g.warn("Percentage must be an integer from 0 to 100.\nTry again.")
            goto try_again
        end
    end

    if percentage then
        perc = percentage
        if perc < 0 then perc = 0 end
        if perc > 100 then perc = 100 end
    else
        -- prompt user for the percentage and fill/sphere options;
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
    stepsize = 1
    StopGenerating()
    ClearCells()

    local minval, maxval
    if fill or N < 8 then
        minval = 0
        maxval = N-1
    else
        minval = N//8
        maxval = (N-1) - minval
    end
    if sphere then
        local mid = N//2
        if N % 2 == 0 then mid = mid - 0.5 end  -- ensure symmetry for even N
        local rsq = (maxval-minval+1)/2.0
        rsq = round(rsq*rsq)
        for z = minval, maxval do
            local dz = z-mid
            local dz2 = round(dz*dz)
            for y = minval, maxval do
                local dy = y-mid
                local dy2 = round(dy*dy)
                for x = minval, maxval do
                    local dx = x-mid
                    local d = round(dx*dx) + dy2 + dz2
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

    ClearUndoRedo()
    if scriptlevel == 0 then
        SetActivePlane()
        InitialView()       -- calls Refresh
    end
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
    local livecells = SaveCells()

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
    local clipcount = RestoreCells(livecells)
    if clipcount > 0 then
        dirty = true
    else
        -- RestoreCells sets dirty true if there are live cells,
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

    CheckIfGenerating()
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
                              "and b values are the neighbor counts for birth.\n" ..
                              "\n" ..
                              "Contiguous counts can be specified as a range,\n" ..
                              "so a rule like 3D4,5,6,7,9/4,5,7 can be entered as\n" ..
                              "3D4..7,9/4,5,7 (this is the canonical version).\n" ..
                              "\n" ..
                              "Append F for the 6-cell face neighborhood,\n" ..
                              "or C for the 8-cell corner neighborhood,\n" ..
                              "or E for the 12-cell edge neighborhood,\n" ..
                              "or H for the 12-cell hexahedral neighborhood.\n" ..
                              "\n" ..
                              "Another rule you might like to try is BusyBoxes\n" ..
                              "(just enter \"bb\", or \"bbw\" to wrap edges).\n",
                              newrule, "Set rule")
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
        if newrule:find("^BusyBoxes") and N%2 == 1 then
            -- BusyBoxes requires an even numbered grid size
            SetGridSize(N+1)
            -- above calls RememberCurrentState()
        else
            RememberCurrentState()
        end
        rulestring = newrule
        CheckIfGenerating()
        Refresh()
    end
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
        CheckIfGenerating()
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
        CheckIfGenerating()
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
        CheckIfGenerating()
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
        CheckIfGenerating()
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
        local fmidx = minselx + (maxselx - minselx) / 2
        local cells = {}
        local NN = N*N
        for k,_ in pairs(selected) do
            local x = k % N
            local y = (k // N) % N
            local z = k // NN
            cells[#cells+1] = {round(fmidx*2) - x, y, z, grid1[k]}
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
        CheckIfGenerating()
        Refresh()
    end
end

----------------------------------------------------------------------

function FlipSelectionY()
    if selcount > 0 then
        RememberCurrentState()
        -- reflect selected cells' Y coords across XZ plane thru middle of selection
        MinimizeSelectionBoundary()
        local fmidy = minsely + (maxsely - minsely) / 2
        local cells = {}
        local NN = N*N
        for k,_ in pairs(selected) do
            local x = k % N
            local y = (k // N) % N
            local z = k // NN
            cells[#cells+1] = {x, round(fmidy*2) - y, z, grid1[k]}
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
        CheckIfGenerating()
        Refresh()
    end
end

----------------------------------------------------------------------

function FlipSelectionZ()
    if selcount > 0 then
        RememberCurrentState()
        -- reflect selected cells' Z coords across XY plane thru middle of selection
        MinimizeSelectionBoundary()
        local fmidz = minselz + (maxselz - minselz) / 2
        local cells = {}
        local NN = N*N
        for k,_ in pairs(selected) do
            local x = k % N
            local y = (k // N) % N
            local z = k // NN
            cells[#cells+1] = {x, y, round(fmidz*2) - z, grid1[k]}
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
        CheckIfGenerating()
        Refresh()
    end
end

----------------------------------------------------------------------

function RotateSelectionX()
    if selcount > 0 then
        RememberCurrentState()
        -- rotate selection clockwise about its X axis by 90 degrees
        MinimizeSelectionBoundary()
        local fmidy = minsely + (maxsely - minsely) // 2
        local fmidz = minselz + (maxselz - minselz) // 2
        local y0 = fmidy - fmidz
        local z0 = fmidz + fmidy + (maxselz - minselz) % 2    -- avoids drift
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
        CheckIfGenerating()
        Refresh()
    end
end

----------------------------------------------------------------------

function RotateSelectionY()
    if selcount > 0 then
        RememberCurrentState()
        -- rotate selection clockwise about its Y axis by 90 degrees
        MinimizeSelectionBoundary()
        local fmidx = minselx + (maxselx - minselx) // 2
        local fmidz = minselz + (maxselz - minselz) // 2
        local x0 = fmidx + fmidz + (maxselx - minselx) % 2    -- avoids drift
        local z0 = fmidz - fmidx
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
        CheckIfGenerating()
        Refresh()
    end
end

----------------------------------------------------------------------

function RotateSelectionZ()
    if selcount > 0 then
        RememberCurrentState()
        -- rotate selection clockwise about its Z axis by 90 degrees
        MinimizeSelectionBoundary()
        local fmidx = minselx + (maxselx - minselx) // 2
        local fmidy = minsely + (maxsely - minsely) // 2
        local x0 = fmidx - fmidy
        local y0 = fmidy + fmidx + (maxsely - minsely) % 2    -- avoids drift
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
        CheckIfGenerating()
        Refresh()
    end
end

----------------------------------------------------------------------

function CancelPaste()
    if pastecount > 0 then
        RememberCurrentState()
        pastecount = 0
        pastepatt = {}
        CheckIfGenerating()
        Refresh()
    end
end

----------------------------------------------------------------------

function Read2DPattern(filepath)
    -- call g.load via pcall to catch any error
    local cellarray
    local function loadcells()
        cellarray = g.load(filepath)
        -- if g.load detected an error then force pcall to abort
        g.doevent("")
    end
    local status, err = pcall(loadcells)
    if err then
        g.continue("")
        return err
    end

    -- copy cells from cellarray into a temporary grid
    local tsize = MAXN
    local trule = rulestring
    local tgens = 0
    local tpop = 0
    local tminx = math.maxinteger
    local tminy = math.maxinteger
    local tminz = 0
    local tmaxx = math.mininteger
    local tmaxy = math.mininteger
    local tmaxz = 0
    local tgrid = {}

    -- determine if cellarray is one-state or multi-state
    local len = #cellarray
    local inc = 2
    if (len & 1) == 1 then
        inc = 3
        -- ignore padding int if present
        if len % 3 == 1 then len = len - 1 end
    end

    local M = tsize-1
    for i = 1, len, inc do
        local x = cellarray[i]
        local y = M - cellarray[i+1]    -- invert y coord
        if x < 0 or x >= tsize or
           y < 0 or y >= tsize then
            return "too big"            -- detected by caller
        end
        -- note that z is 0
        tgrid[x + tsize * y] = 1
        tpop = tpop + 1
        -- update boundary (tminz = tmaxz = 0)
        if x < tminx then tminx = x end
        if y < tminy then tminy = y end
        if x > tmaxx then tmaxx = x end
        if y > tmaxy then tmaxy = y end
    end

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

function Paste()
    -- if a paste pattern already exists then cancel it
    local savedstate = false
    if pastecount > 0 then
        CancelPaste()
        savedstate = true
    end

    -- if the clipboard contains a valid RLE3 or 2D pattern then create a paste pattern
    local filepath = CopyClipboardToFile()
    if not filepath then
        return false
    end

    local err, newpattern = ReadPattern(filepath)
    if err then
        -- invalid RLE3 pattern so try loading a 2D pattern
        err, newpattern = Read2DPattern(filepath)
        if err then
            if err == "too big" then
                message = "2D pattern in clipboard is too big."
            else
                message = "Clipboard does not contain a valid pattern."
            end
            Refresh()
            return false
        end
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
        message = "Clipboard pattern is too big ("..pwd.." x "..pht.." x "..pdp..")."
        Refresh()
        return false
    end

    if not savedstate then RememberCurrentState() end

    -- set pastecount and pastepatt
    pastecount = newpattern.newpop
    pastepatt = {}
    minpastex = math.maxinteger
    minpastey = math.maxinteger
    minpastez = math.maxinteger
    maxpastex = math.mininteger
    maxpastey = math.mininteger
    maxpastez = math.mininteger
    local P = newpattern.newsize
    local PP = P*P
    if currcursor ~= movecursor and min(pwd,pht,pdp) == 1 then
        -- put 1-cell thick paste pattern in middle of active plane,
        -- rotating pattern if necessary to match orientation
        local xx, xy, xz = 1, 0, 0
        local yx, yy, yz = 0, 1, 0
        local zx, zy, zz = 0, 0, 1
        local mid = N//2
        local xoffset = 0
        local yoffset = 0
        local zoffset = 0
        if pdp == 1 then
            if activeplane == "XY" then
                -- no rotation needed
                xoffset = (N - pwd + 1) // 2
                yoffset = (N - pht + 1) // 2
                zoffset = mid + activepos
            elseif activeplane == "YZ" then
                -- rotate pattern about its Y axis
                xx, xy, xz =  0,  0,  1
                yx, yy, yz =  0,  1,  0
                zx, zy, zz = -1,  0,  0
                xoffset = mid + activepos
                yoffset = (N - pht + 1) // 2
                zoffset = (N - pwd + 1) // 2 + pwd - 1
            else -- activeplane == "XZ"
                -- rotate pattern about its X axis
                xx, xy, xz =  1,  0,  0
                yx, yy, yz =  0,  0, -1
                zx, zy, zz =  0,  1,  0
                xoffset = (N - pwd + 1) // 2
                yoffset = mid + activepos
                zoffset = (N - pht + 1) // 2
            end
        elseif pht == 1 then
            if activeplane == "XY" then
                -- rotate pattern about its X axis
                xx, xy, xz =  1,  0,  0
                yx, yy, yz =  0,  0, -1
                zx, zy, zz =  0,  1,  0
                xoffset = (N - pwd + 1) // 2
                yoffset = (N - pdp + 1) // 2 + pdp - 1
                zoffset = mid + activepos
            elseif activeplane == "YZ" then
                -- rotate pattern about its Z axis
                xx, xy, xz =  0, -1,  0
                yx, yy, yz =  1,  0,  0
                zx, zy, zz =  0,  0,  1
                xoffset = mid + activepos
                yoffset = (N - pwd + 1) // 2
                zoffset = (N - pdp + 1) // 2
            else -- activeplane == "XZ"
                -- no rotation needed
                xoffset = (N - pwd + 1) // 2
                yoffset = mid + activepos
                zoffset = (N - pdp + 1) // 2
            end
        else -- pwd == 1
            if activeplane == "XY" then
                -- rotate pattern about its Y axis
                xx, xy, xz =  0,  0,  1
                yx, yy, yz =  0,  1,  0
                zx, zy, zz = -1,  0,  0
                xoffset = (N - pdp + 1) // 2
                yoffset = (N - pht + 1) // 2
                zoffset = mid + activepos
            elseif activeplane == "YZ" then
                -- no rotation needed
                xoffset = mid + activepos
                yoffset = (N - pht + 1) // 2
                zoffset = (N - pdp + 1) // 2
            else -- activeplane == "XZ"
                -- rotate pattern about its Z axis
                xx, xy, xz =  0, -1,  0
                yx, yy, yz =  1,  0,  0
                zx, zy, zz =  0,  0,  1
                xoffset = (N - pht + 1) // 2 + pht - 1
                yoffset = mid + activepos
                zoffset = (N - pdp + 1) // 2
            end
        end
        for k,_ in pairs(newpattern.newgrid) do
            -- move pattern to origin
            local x = (k % P)      - minpx
            local y = (k // P % P) - minpy
            local z = (k // PP)    - minpz

            -- do the rotation (if any)
            local newx = x*xx + y*xy + z*xz
            local newy = x*yx + y*yy + z*yz
            local newz = x*zx + y*zy + z*zz

            -- now shift to middle of active plane
            x = newx + xoffset
            y = newy + yoffset
            z = newz + zoffset

            pastepatt[ x + N * (y + N * z) ] = 1
            -- update paste boundary
            if x < minpastex then minpastex = x end
            if y < minpastey then minpastey = y end
            if z < minpastez then minpastez = z end
            if x > maxpastex then maxpastex = x end
            if y > maxpastey then maxpastey = y end
            if z > maxpastez then maxpastez = z end
        end
    else
        -- put paste pattern in middle of grid
        local xoffset = minpx - (N - pwd + 1) // 2
        local yoffset = minpy - (N - pht + 1) // 2
        local zoffset = minpz - (N - pdp + 1) // 2
        for k,_ in pairs(newpattern.newgrid) do
            -- newpattern.newgrid[k] is a live cell
            local x = (k % P)      - xoffset
            local y = (k // P % P) - yoffset
            local z = (k // PP)    - zoffset
            pastepatt[ x + N * (y + N * z) ] = 1
            -- update paste boundary
            if x < minpastex then minpastex = x end
            if y < minpastey then minpastey = y end
            if z < minpastez then minpastez = z end
            if x > maxpastex then maxpastex = x end
            if y > maxpastey then maxpastey = y end
            if z > maxpastez then maxpastez = z end
        end
    end
    CheckIfGenerating()
    Refresh()
    return true
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
        CheckIfGenerating()
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
        CheckIfGenerating()
        Refresh()
    end
end

----------------------------------------------------------------------

function FlipPasteX()
    if pastecount > 0 then
        RememberCurrentState()
        -- reflect X coords across YZ plane thru middle of paste pattern
        local fmidx = minpastex + (maxpastex - minpastex) / 2
        local cells = {}
        local NN = N*N
        for k,_ in pairs(pastepatt) do
            local x = k % N
            local y = (k // N) % N
            local z = k // NN
            cells[#cells+1] = {round(fmidx*2) - x, y, z}
        end
        -- flip doesn't change paste boundary
        pastepatt = {}
        for _,xyz in ipairs(cells) do
            local x, y, z = xyz[1], xyz[2], xyz[3]
            pastepatt[x + N * (y + N * z)] = 1
        end
        CheckIfGenerating()
        Refresh()
    end
end

----------------------------------------------------------------------

function FlipPasteY()
    if pastecount > 0 then
        RememberCurrentState()
        -- reflect Y coords across XZ plane thru middle of paste pattern
        local fmidy = minpastey + (maxpastey - minpastey) / 2
        local cells = {}
        local NN = N*N
        for k,_ in pairs(pastepatt) do
            local x = k % N
            local y = (k // N) % N
            local z = k // NN
            cells[#cells+1] = {x, round(fmidy*2) - y, z}
        end
        -- flip doesn't change paste boundary
        pastepatt = {}
        for _,xyz in ipairs(cells) do
            local x, y, z = xyz[1], xyz[2], xyz[3]
            pastepatt[x + N * (y + N * z)] = 1
        end
        CheckIfGenerating()
        Refresh()
    end
end

----------------------------------------------------------------------

function FlipPasteZ()
    if pastecount > 0 then
        RememberCurrentState()
        -- reflect Z coords across XY plane thru middle of paste pattern
        local fmidz = minpastez + (maxpastez - minpastez) / 2
        local cells = {}
        local NN = N*N
        for k,_ in pairs(pastepatt) do
            local x = k % N
            local y = (k // N) % N
            local z = k // NN
            cells[#cells+1] = {x, y, round(fmidz*2) - z}
        end
        -- flip doesn't change paste boundary
        pastepatt = {}
        for _,xyz in ipairs(cells) do
            local x, y, z = xyz[1], xyz[2], xyz[3]
            pastepatt[x + N * (y + N * z)] = 1
        end
        CheckIfGenerating()
        Refresh()
    end
end

----------------------------------------------------------------------

function RotatePasteX()
    if pastecount > 0 then
        RememberCurrentState()
        -- rotate paste pattern clockwise about its X axis by 90 degrees
        local fmidy = minpastey + (maxpastey - minpastey) // 2
        local fmidz = minpastez + (maxpastez - minpastez) // 2
        local y0 = fmidy - fmidz
        local z0 = fmidz + fmidy + (maxpastez - minpastez) % 2    -- avoids drift
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
        CheckIfGenerating()
        Refresh()
    end
end

----------------------------------------------------------------------

function RotatePasteY()
    if pastecount > 0 then
        RememberCurrentState()
        -- rotate paste pattern clockwise about its Y axis by 90 degrees
        local fmidx = minpastex + (maxpastex - minpastex) // 2
        local fmidz = minpastez + (maxpastez - minpastez) // 2
        local x0 = fmidx + fmidz + (maxpastex - minpastex) % 2    -- avoids drift
        local z0 = fmidz - fmidx
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
        CheckIfGenerating()
        Refresh()
    end
end

----------------------------------------------------------------------

function RotatePasteZ()
    if pastecount > 0 then
        RememberCurrentState()
        -- rotate paste pattern clockwise about its Z axis by 90 degrees
        local fmidx = minpastex + (maxpastex - minpastex) // 2
        local fmidy = minpastey + (maxpastey - minpastey) // 2
        local x0 = fmidx - fmidy
        local y0 = fmidy + fmidx + (maxpastey - minpastey) % 2    -- avoids drift
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
        CheckIfGenerating()
        Refresh()
    end
end

----------------------------------------------------------------------

function ChoosePasteAction(mousex, mousey)
    -- show red pop-up menu at mousex,mousey and let user choose a paste action
    pastemenu.setbgcolor(PASTE_MENU)
    pastemenu.show(mousex, mousey, ovwd, ovht)
end

----------------------------------------------------------------------

function ChooseSelectionAction(mousex, mousey)
    -- show green pop-up menu at mousex,mousey and let user choose a selection action
    selmenu.setbgcolor(SELECT_MENU)
    selmenu.show(mousex, mousey, ovwd, ovht)
end

----------------------------------------------------------------------

function ZoomIn()
    if CELLSIZE < MAXSIZE then
        -- zoom in by increasing size of cells
        CELLSIZE = CELLSIZE+1
        HALFCELL = CELLSIZE/2.0
        MIDGRID = (N+1-(N%2))*HALFCELL
        MIDCELL = HALFCELL-MIDGRID
        LEN = CELLSIZE-BORDER*2
        CreateAxes()
        Refresh()
    end
end

----------------------------------------------------------------------

function ZoomOut()
    if CELLSIZE > MINSIZE then
        -- zoom out by decreasing size of cells
        CELLSIZE = CELLSIZE-1
        HALFCELL = CELLSIZE/2.0
        MIDGRID = (N+1-(N%2))*HALFCELL
        MIDCELL = HALFCELL-MIDGRID
        LEN = CELLSIZE-BORDER*2
        CreateAxes()
        Refresh()
    end
end

----------------------------------------------------------------------

function OpenFile(filepath)
    if filepath:find("%.rle3$") then
        OpenPattern(filepath)
    elseif filepath:find("%.lua$") then
        RunScript(filepath)
    else
        g.warn("Unexpected file:\n"..filepath.."\n\n"..
               "3D.lua can only handle files ending with .rle3 or .lua.", false)
    end
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

    -- NextGeneration does nothing (except display a message) if popcount is 0
    if popcount > 0 then
        RememberCurrentState()
    end

    NextGeneration()

    if gencount % stepsize == 0 or popcount == 0 then
        -- NextGeneration called Refresh()
    else
        Refresh()
    end
end

----------------------------------------------------------------------

function NextStep()
    if popcount == 0 or stepsize == 1 then
        Step1()
    else
        StopGenerating()
        StartStop()
        -- stop at next multiple of stepsize (or if pattern becomes empty)
        stopgen = gencount + stepsize - (gencount % stepsize)
    end
end

----------------------------------------------------------------------

function Faster()
    if stepsize < 100 then
        stepsize = stepsize + 1
        Refresh()
    end
end

----------------------------------------------------------------------

function Slower()
    if stepsize > 1 then
        stepsize = stepsize - 1
        Refresh()
    end
end

----------------------------------------------------------------------

function SetStepSize(newval)
    if newval >= 1 and newval <= 100 then
        stepsize = newval
        Refresh()
    end
end

--------------------------------------------------------------------------------

function StepChange(newval)
    -- called if stepslider position has changed
    stepsize = newval
    Refresh()
end

----------------------------------------------------------------------

function Reset()
    if gencount > startcount then
        -- restore the starting state
        if scriptlevel > 0 then
            -- Reset was called by user script so don't modify undo/redo stacks
            RestoreState(startstate)
        else
            -- unwind undostack until gencount == startcount
            repeat
                -- push current state onto redostack
                redostack[#redostack+1] = SaveState()
                -- pop state off undostack and restore it
                RestoreState( table.remove(undostack) )
            until gencount == startcount
        end
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

function FitGrid(display)
    if display == nil then display = true end

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

    if display then Refresh() end
end

----------------------------------------------------------------------

-- getters for user scripts

function GetGeneration() return gencount end
function GetGridSize() return N end
function GetPercentage() return perc end
function GetPopulation() return popcount end
function GetRule() return rulestring end
function GetCellType() return celltype end
function GetStepSize() return stepsize end
function GetBarHeight() return toolbarht end

----------------------------------------------------------------------

-- for user scripts
function GetCells(selectedonly)
    -- return an array of live cell coordinates;
    -- if selected is true then only selected live cells will be returned
    local cellarray = {}
    if popcount > 0 then
        if selectedonly then
            cellarray = GetSelectedLiveCells()
        else
            -- get all live cells
            local mid = N//2
            local NN = N*N
            for k,_ in pairs(grid1) do
                -- grid1[k] is a live cell
                local x = k % N
                local y = (k // N) % N
                local z = k // NN
                cellarray[#cellarray+1] = {x-mid, y-mid, z-mid}
            end
        end
    end
    return cellarray
end

----------------------------------------------------------------------

-- for user scripts
function PutCells(cellarray, xoffset, yoffset, zoffset)
    -- use given array of live cell coordinates and offsets to set cells in grid
    -- (any cells outside the grid are silently clipped)
    xoffset = xoffset or 0
    yoffset = yoffset or 0
    zoffset = zoffset or 0
    local mid = N//2
    local midx = mid + xoffset
    local midy = mid + yoffset
    local midz = mid + zoffset
    for _, xyz in ipairs(cellarray) do
        local x = xyz[1] + midx
        local y = xyz[2] + midy
        local z = xyz[3] + midz
        if x >= 0 and x < N and
           y >= 0 and y < N and
           z >= 0 and z < N then
            SetCellState(x, y, z, 1)
        end
    end
end

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
function Step(n)
    n = n or 1
    while popcount > 0 and n > 0 do
        NextGeneration()
        n = n - 1
        g.doevent("")   -- null op to let user abort script
    end
end

----------------------------------------------------------------------

-- for user scripts
function SetRule(newrule)
    newrule = newrule or DEFAULT_RULE
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
    state = state or 1  -- default state is 1
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
<dd>&nbsp;&nbsp;&nbsp;&nbsp; <a href="#moore"><b>Moore neighborhood</b></a></dd>
<dd>&nbsp;&nbsp;&nbsp;&nbsp; <a href="#face"><b>Face neighborhood</b></a></dd>
<dd>&nbsp;&nbsp;&nbsp;&nbsp; <a href="#corner"><b>Corner neighborhood</b></a></dd>
<dd>&nbsp;&nbsp;&nbsp;&nbsp; <a href="#edge"><b>Edge neighborhood</b></a></dd>
<dd>&nbsp;&nbsp;&nbsp;&nbsp; <a href="#hex"><b>Hexahedral neighborhood</b></a></dd>
<dd>&nbsp;&nbsp;&nbsp;&nbsp; <a href="#busy"><b>Busy Boxes</b></a></dd>
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
The following keyboard shortcuts are provided (but see <a href="#shortcuts">below</a>
for how you can write a script to create new shortcuts or override any of the supplied
shortcuts):

<p>
<center>
<table cellspacing=1 border=2 cols=2 width="90%">
<tr><td align=right> Keys &nbsp;</td><td>&nbsp; Actions </td></tr>
<tr><td align=right> enter &nbsp;</td><td>&nbsp; start/stop generating pattern </td></tr>
<tr><td align=right> space &nbsp;</td><td>&nbsp; advance pattern by one generation </td></tr>
<tr><td align=right> tab &nbsp;</td><td>&nbsp; advance pattern to next multiple of step size </td></tr>
<tr><td align=right> - &nbsp;</td><td>&nbsp; decrease step size </td></tr>
<tr><td align=right> = &nbsp;</td><td>&nbsp; increase step size </td></tr>
<tr><td align=right> 1 &nbsp;</td><td>&nbsp; reset step size to 1 </td></tr>
<tr><td align=right> arrows &nbsp;</td><td>&nbsp; rotate about X/Y screen axes </td></tr>
<tr><td align=right> alt-arrows &nbsp;</td><td>&nbsp; rotate about Z screen axis </td></tr>
<tr><td align=right> ctrl-N &nbsp;</td><td>&nbsp; create a new, empty pattern </td></tr>
<tr><td align=right> 5 &nbsp;</td><td>&nbsp; create a new, random pattern </td></tr>
<tr><td align=right> ctrl-O &nbsp;</td><td>&nbsp; open a selected pattern file </td></tr>
<tr><td align=right> ctrl-S &nbsp;</td><td>&nbsp; save the current pattern in a file </td></tr>
<tr><td align=right> shift-O &nbsp;</td><td>&nbsp; open pattern in clipboard </td></tr>
<tr><td align=right> shift-R &nbsp;</td><td>&nbsp; run script in clipboard </td></tr>
<tr><td align=right> ctrl-R &nbsp;</td><td>&nbsp; reset to the starting pattern </td></tr>
<tr><td align=right> Z &nbsp;</td><td>&nbsp; undo </td></tr>
<tr><td align=right> shift-Z &nbsp;</td><td>&nbsp; redo </td></tr>
<tr><td align=right> ctrl-X &nbsp;</td><td>&nbsp; cut </td></tr>
<tr><td align=right> ctrl-C &nbsp;</td><td>&nbsp; copy </td></tr>
<tr><td align=right> ctrl-V &nbsp;</td><td>&nbsp; show paste pattern </td></tr>
<tr><td align=right> alt-V &nbsp;</td><td>&nbsp; cancel paste pattern </td></tr>
<tr><td align=right> ctrl-B &nbsp;</td><td>&nbsp; do the paste using OR mode </td></tr>
<tr><td align=right> ctrl-shift-B &nbsp;</td><td>&nbsp; do the paste using XOR mode </td></tr>
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
<tr><td align=right> shift-L &nbsp;</td><td>&nbsp; toggle axes </td></tr>
<tr><td align=right> alt-D &nbsp;</td><td>&nbsp; toggle depth shading </td></tr>
<tr><td align=right> T &nbsp;</td><td>&nbsp; toggle the menu bar and tool bar </td></tr>
<tr><td align=right> G &nbsp;</td><td>&nbsp; change the grid size </td></tr>
<tr><td align=right> R &nbsp;</td><td>&nbsp; change the rule </td></tr>
<tr><td align=right> , &nbsp;</td><td>&nbsp; move active plane closer (in initial view) </td></tr>
<tr><td align=right> . &nbsp;</td><td>&nbsp; move active plane further away (in initial view) </td></tr>
<tr><td align=right> shift-A &nbsp;</td><td>&nbsp; cycle active plane's orientation (XY/XZ/YZ) </td></tr>
<tr><td align=right> D &nbsp;</td><td>&nbsp; switch cursor to draw mode </td></tr>
<tr><td align=right> S &nbsp;</td><td>&nbsp; switch cursor to select mode </td></tr>
<tr><td align=right> M &nbsp;</td><td>&nbsp; switch cursor to move mode </td></tr>
<tr><td align=right> C &nbsp;</td><td>&nbsp; cycle cursor mode (draw/select/move) </td></tr>
<tr><td align=right> shift-M &nbsp;</td><td>&nbsp; move pattern to middle of grid </td></tr>
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
All undo/redo history is deleted and the step size is reset to 1.
The active plane is displayed, ready to be edited using the pencil cursor.
</dd>

<a name="rand"></a><p><dt><b>Random Pattern...</b></dt>
<dd>
Create a new pattern randomly filled with live cells at a given density.
You have the option of filling the grid and creating a cube or a sphere.
All undo/redo history is deleted and the step size is reset to 1.
</dd>

<a name="open"></a><p><dt><b>Open Pattern...</b></dt>
<dd>
Open a selected <a href="#rle3">RLE3</a> pattern file.
All undo/redo history is deleted and the step size is reset to 1.
</dd>

<a name="openclip"></a><p><dt><b>Open Clipboard</b></dt>
<dd>
Open the <a href="#rle3">RLE3</a> pattern stored in the clipboard.
All undo/redo history is deleted and the step size is reset to 1.
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
the start of the pattern name) then you'll be asked if you want to save them.
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
If the clipboard contains a valid, non-empty pattern (<a href="#rle3">RLE3</a> or 2D) that fits
within the current grid then a paste pattern (comprised of red cells) will appear.
If the active plane is visible and the clipboard pattern is one cell thick (in any direction)
then the paste pattern appears in the middle of the active plane, otherwise it will appear
in the middle of the grid.  You can use any cursor to drag the paste pattern to any position
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
If the pattern isn't empty then advance to the next generation.
</dd>

<a name="nextstep"></a><p><dt><b>Next Step</b></dt>
<dd>
Advance the pattern to the next multiple of the current step size,
or until the pattern is empty.  Only the final generation is displayed.
</dd>

<a name="reset"></a><p><dt><b>Reset</b></dt>
<dd>
Restore the starting generation and step size.
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
If selected then live cells are displayed as cubes.
</dd>

<a name="spheres"></a><p><dt><b>Spheres</b></dt>
<dd>
If selected then live cells are displayed as spheres.
</dd>

<a name="points"></a><p><dt><b>Points</b></dt>
<dd>
If selected then live cells are displayed as points.
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
If ticked then lattice lines are displayed on the three faces of the grid
that intersect at the corner with minimum <a href="#coords">cell coordinates</a>
(the far, bottom left corner in the initial view).
</dd>

<a name="shading"></a><p><dt><b>Use Depth Shading</b></dt>
<dd>
If ticked then cubes and spheres are drawn slightly darker the further
away they are from the front of the screen.  Depth shading is not done
when displaying points.
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
-- for 3D.lua (make sure you copy this line)
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
Note that 3D.lua will only run a script if the clipboard or the file
has "3D.lua" somewhere in the first line.  This avoids nasty problems
that can occur if you run a script not written for 3D.lua.

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
-- a startup script for 3D.lua
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
<a href="#CheckWindowSize"><b>CheckWindowSize</b></a><br>
<a href="#ClearOutside"><b>ClearOutside</b></a><br>
<a href="#ClearSelection"><b>ClearSelection</b></a><br>
<a href="#CopySelection"><b>CopySelection</b></a><br>
<a href="#CutSelection"><b>CutSelection</b></a><br>
<a href="#DeselectCell"><b>DeselectCell</b></a><br>
<a href="#DoPaste"><b>DoPaste</b></a><br>
<a href="#DrawMode"><b>DrawMode</b></a><br>
<a href="#FitGrid"><b>FitGrid</b></a><br>
<a href="#FlipPaste"><b>FlipPaste</b></a><br>
<a href="#FlipSelection"><b>FlipSelection</b></a><br>
<a href="#GetBarHeight"><b>GetBarHeight</b></a><br>
<a href="#GetBounds"><b>GetBounds</b></a>
</td>
<td valign=top width=30> </td>
<td valign=top>
<a href="#GetCell"><b>GetCell</b></a><br>
<a href="#GetCells"><b>GetCells</b></a><br>
<a href="#GetCellType"><b>GetCellType</b></a><br>
<a href="#GetGeneration"><b>GetGeneration</b></a><br>
<a href="#GetGridSize"><b>GetGridSize</b></a><br>
<a href="#GetPasteBounds"><b>GetPasteBounds</b></a><br>
<a href="#GetPercentage"><b>GetPercentage</b></a><br>
<a href="#GetPopulation"><b>GetPopulation</b></a><br>
<a href="#GetRule"><b>GetRule</b></a><br>
<a href="#GetSelectionBounds"><b>GetSelectionBounds</b></a><br>
<a href="#GetStepSize"><b>GetStepSize</b></a><br>
<a href="#HandleKey"><b>HandleKey</b></a><br>
<a href="#InitialView"><b>InitialView</b></a><br>
<a href="#MoveMode"><b>MoveMode</b></a><br>
<a href="#NewPattern"><b>NewPattern</b></a>
</td>
<td valign=top width=30> </td>
<td valign=top>
<a href="#OpenPattern"><b>OpenPattern</b></a><br>
<a href="#Paste"><b>Paste</b></a><br>
<a href="#PasteExists"><b>PasteExists</b></a><br>
<a href="#PutCells"><b>PutCells</b></a><br>
<a href="#RandomPattern"><b>RandomPattern</b></a><br>
<a href="#Reset"><b>Reset</b></a><br>
<a href="#RestoreState"><b>RestoreState</b></a><br>
<a href="#Rotate"><b>Rotate</b></a><br>
<a href="#RotatePaste"><b>RotatePaste</b></a><br>
<a href="#RotateSelection"><b>RotateSelection</b></a><br>
<a href="#RunScript"><b>RunScript</b></a><br>
<a href="#SavePattern"><b>SavePattern</b></a><br>
<a href="#SaveState"><b>SaveState</b></a><br>
<a href="#SelectAll"><b>SelectAll</b></a><br>
<a href="#SelectCell"><b>SelectCell</b></a>
</td>
<td valign=top width=30> </td>
<td valign=top>
<a href="#SelectedCell"><b>SelectedCell</b></a><br>
<a href="#SelectionExists"><b>SelectionExists</b></a><br>
<a href="#SelectMode"><b>SelectMode</b></a><br>
<a href="#SetCell"><b>SetCell</b></a><br>
<a href="#SetCellType"><b>SetCellType</b></a><br>
<a href="#SetGridSize"><b>SetGridSize</b></a><br>
<a href="#SetMessage"><b>SetMessage</b></a><br>
<a href="#SetRule"><b>SetRule</b></a><br>
<a href="#SetStepSize"><b>SetStepSize</b></a><br>
<a href="#Step"><b>Step</b></a><br>
<a href="#Update"><b>Update</b></a><br>
<a href="#ZoomIn"><b>ZoomIn</b></a><br>
<a href="#ZoomOut"><b>ZoomOut</b></a>
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

<a name="CheckWindowSize"></a><p><dt><b>CheckWindowSize()</b></dt>
<dd>
If the Golly window size has changed then this function resizes the overlay.
Useful in scripts that allow user interaction.
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

<a name="GetBarHeight"></a><p><dt><b>GetBarHeight()</b></dt>
<dd>
Return the combined height of the menu bar and tool bar.
The value will be 0 if the user has turned them off (by hitting the "T" key)
or switched to full screen mode.
Useful in scripts that allow user interaction.
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

<a name="GetCells"></a><p><dt><b>GetCells(<i>selected</i>)</b></dt>
<dd>
Return an array of live cell coordinates in the format
{ {x1,y1,z1}, {x2,y2,z2}, ... {xn,yn,zn} }.
All coordinates are relative to the middle cell in the grid.
If there are no live cells then {} is returned.
If selected is true then only the coordinates of selected live cells will be returned.
</dd>

<a name="GetCellType"></a><p><dt><b>GetCellType()</b></dt>
<dd>
Return the current cell type: "cube", "sphere" or "point".
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
Return the percentage (0 to 100) given in the most recent File > Random Pattern dialog.
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

<a name="GetStepSize"></a><p><dt><b>GetStepSize()</b></dt>
<dd>
Return the current step size (1 to 100).
</dd>

<a name="HandleKey"></a><p><dt><b>HandleKey(<i>event</i>)</b></dt>
<dd>
Process the given keyboard event.
Useful in scripts that allow user interaction.
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
All undo/redo history is deleted and the step size is reset to 1.
</dd>

<a name="OpenPattern"></a><p><dt><b>OpenPattern(<i>filepath</i>)</b></dt>
<dd>
Open the specified RLE3 pattern file.  If the <i>filepath</i> is not supplied then
the user will be prompted to select a .rle3 file.
All undo/redo history is deleted and the step size is reset to 1.
</dd>

<a name="Paste"></a><p><dt><b>Paste()</b></dt>
<dd>
Return true if the clipboard contains a valid, non-empty pattern (RLE3 or 2D) that fits
within the current grid.  If the active plane exists and the clipboard pattern is
one cell thick (in any direction) then the paste pattern is located in the middle
of the active plane, otherwise it is located in the middle of the grid.
You can then call <a href="#FlipPaste">FlipPaste</a> or
<a href="#RotatePaste">RotatePaste</a> to modify the paste pattern.
Call <a href="#DoPaste">DoPaste</a> when you want to actually paste the pattern into the grid.
</dd>

<a name="PasteExists"></a><p><dt><b>PasteExists()</b></dt>
<dd>
Return true if a paste pattern exists.
</dd>

<a name="PutCells"></a><p><dt><b>PutCells(<i>cellarray, xoffset, yoffset, zoffset</i>)</b></dt>
<dd>
Use the given array of live cell coordinates and offsets to set cells in the grid.
The array must have the same format as the one returned by <a href="#GetCells">GetCells</a>.
Any cells outside the grid are silently clipped.
</dd>

<a name="RandomPattern"></a><p><dt><b>RandomPattern(<i>percentage, fill, sphere</i>)</b></dt>
<dd>
Create a new, random pattern with the given percentage density (0 to 100) of live cells.
If fill is true then the grid is filled, otherwise there will be a gap around
the random object (this makes it easier to detect an explosive rule).
If sphere is true then a spherical object is created rather than a cube.
If no parameters are supplied then the user will be prompted for them.
All undo/redo history is deleted and the step size is reset to 1.
</dd>

<a name="Reset"></a><p><dt><b>Reset()</b></dt>
<dd>
Restore the starting generation and step size.
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
Run the specified .lua file, but only if the string "3D.lua" occurs
somewhere in a comment on the first line of the file.
If the <i>filepath</i> is not supplied then the user will be prompted
to select a .lua file.
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
the step size, the selection, and the paste pattern.
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
If the state is not supplied then it defaults to 1.
The x,y,z coordinates are relative to the middle cell in the grid.
</dd>

<a name="SetCellType"></a><p><dt><b>SetCellType(<i>string</i>)</b></dt>
<dd>
Set the cell type to "cube", "sphere" or "point".
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
Switch to the given rule.  If <i>rule</i> is not supplied the default rule
is used (3D5..7/6).
</dd>

<a name="SetStepSize"></a><p><dt><b>SetStepSize(<i>newsize</i>)</b></dt>
<dd>
Set the step size to the given value (1 to 100).
</dd>

<a name="Step"></a><p><dt><b>Step(<i>n</i>)</b></dt>
<dd>
While the population is &gt; 0 calculate the next <i>n</i> generations.
If <i>n</i> is not supplied it defaults to 1.
</dd>

<a name="Update"></a><p><dt><b>Update()</b></dt>
<dd>
Update the display.  Note that 3D.lua automatically updates the display
when a script finishes, so there's no need to call Update() at the
end of a script.
</dd>

<a name="ZoomIn"></a><p><dt><b>ZoomIn()</b></dt>
<dd>
Zoom in by incrementing the cell size.
Useful in scripts that allow user interaction.
</dd>

<a name="ZoomOut"></a><p><dt><b>ZoomOut()</b></dt>
<dd>
Zoom out by decrementing the cell size.
Useful in scripts that allow user interaction.
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
-- for 3D.lua
local N = GetGridSize()
for c = -(N//2), (N-1)//2 do
    SetCell(c, c, c, 1)
end</pre></table></dd>

<p><a name="rules"></a><br>
<font size=+1><b>Supported rules</b></font>

<p>
3D.lua supports rules that use a number of different neighborhoods:

<p><ul>
<li>
The Moore neighborhood consists of the 26 cells that form a cube around a central cell.
<li>
The Face neighborhood consists of the 6 cells adjacent to the faces of a cube
(this is the 3D version of the von Neumann neighborhood).
<li>
The Corner neighborhood consists of the 8 cells adjacent to the corners of a cube.
<li>
The Edge neighborhood consists of the 12 cells adjacent to the edges of a cube.
<li>
The Hexahedral neighborhood simulates 12 spheres packed around a central sphere.
Because it is simulating a hexahedral tessellation in a cubic grid, this neighborhood
is not orthogonally symmetric, so flipping or rotating a pattern can change the way it evolves.
<li>
The Busy Boxes neighborhood is rather complicated.  Follow the links below if you
want to know all the gory details.
</ul>

<p>
Use Control > Set Rule to change the current rule.
You can quickly restore 3D.lua's default rule (3D5..7/6) by simply deleting
the current rule and hitting OK.

<p><a name="moore"></a><br>
<font size=+1><b>Moore neighborhood</b></font>

<p>
Rules in this neighborhood are strings of the form "3DS,S,S,.../B,B,B,...".
The S values are the counts of neighboring live cells required
for a live cell to survive in the next generation.
The B values are the counts of neighboring live cells required for
birth; ie. a dead cell will become a live cell in the next generation.
Each cell has 26 neighbors so the S counts are from 0 to 26
and the B counts are from 1 to 26 (birth on 0 is not allowed).
Note that the Moore neighborhood is the combination of the Face+Corner+Edge neighborhoods.

<p>
Contiguous counts can be specified as a range, so a rule like
3D4,5,6,7,9/4,5,7 can be entered as 3D4..7,9/4,5,7 (this is the
canonical version).

<p><a name="face"></a><br>
<font size=+1><b>Face neighborhood</b></font>

<p>
Rules in this neighborhood use the same syntax as the Moore neighborhood
but with "F" appended.  For example: 3D0..6/1,3F.
Each cell has 6 neighbors so the S counts are from 0 to 6 and the
B counts are from 1 to 6 (again, birth on 0 is not allowed).

<p><a name="corner"></a><br>
<font size=+1><b>Corner neighborhood</b></font>

<p>
Rules use the same syntax as the Moore neighborhood but with "C" appended.
Each cell has 8 neighbors so the S counts are from 0 to 8 and the
B counts are from 1 to 8.

<p><a name="edge"></a><br>
<font size=+1><b>Edge neighborhood</b></font>

<p>
Rules use the same syntax as the Moore neighborhood but with "E" appended.
Each cell has 12 neighbors so the S counts are from 0 to 12 and the
B counts are from 1 to 12.

<p><a name="hex"></a><br>
<font size=+1><b>Hexahedral neighborhood</b></font>

<p>
Rules use the same syntax as the Moore neighborhood but with "H" appended.
Each cell has 12 neighbors so the S counts are from 0 to 12 and the
B counts are from 1 to 12.

<p><a name="busy"></a><br>
<font size=+1><b>Busy Boxes</b></font>

<p>
Busy Boxes is a 3D reversible CA created by Ed Fredkin and Daniel B. Miller.
3D.lua supports two rules: BusyBoxes and BusyBoxesW.  The first rule uses a
"mirror" mode where gliders are reflected back when they reach a grid boundary.
The second rule is for "wrap" mode where gliders can cross a boundary and appear
in the opposite side of the grid.

<p>
Each cell in the grid is either odd or even, depending on whether the sum of the
cell's x,y,z coordinates is odd or even.
3D.lua uses cyan for odd cells and magenta for even cells (Fredkin and Miller use
blue and red but these colors are used by 3D.lua to display paste patterns
and the active plane).

<p>
Each generation of a Busy Boxes pattern is in one of six phases, numbered 0 to 5.
In even phases, only even (magenta) cells can move.
In odd phases, only odd (cyan) cells can move.
In phases 0 and 3, movement can only occur in the XY plane.
In phases 1 and 4 movement can only occur in the YZ plane.
In phases 2 and 5, movement can only occur in the XZ plane.
For each diagonally opposite pair of cells in the current plane, if a live cell exists
at either of the pair's knight move positions, then the states of the two cells can be swapped.
However, this only happens if there is no other possible swap for either cell.

<p>
For a pattern with a sparse or small population it's quite likely that no swaps
are possible in a particular phase.  Or the only valid swaps might be between
two empty cells or two live cells.  Either way, the pattern won't change.
At each generation, a live cell can only move into a diagonally opposite empty cell
(and only in the same orthogonal plane).
This also means that the initial population never changes.
For more details see the <a href="http://www.busyboxes.org/faq.html">Busy Boxes FAQ</a>
and the papers listed in the <a href="#refs">references</a>.

<p><a name="rle3"></a><br>
<font size=+1><b>RLE3 file format</b></font>

<p>
3D.lua can read and write patterns as text files with a .rle3 extension.
The file format is known as RLE3 and is a simple extension of the well-known
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

<p>
A Note About the Discovery of Many New Rules for the Game of Three-Dimensional Life<br>
<a href="http://wpmedia.wolfram.com/uploads/sites/13/2018/02/16-4-7.pdf">http://wpmedia.wolfram.com/uploads/sites/13/2018/02/16-4-7.pdf</a>

<p>
References for Busy Boxes:

<p>
Website: <a href="http://www.busyboxes.org">http://www.busyboxes.org</a>

<p>
Two State, Reversible, Universal Cellular Automata In Three Dimensions<br>
<a href="https://arxiv.org/ftp/nlin/papers/0501/0501022.pdf">https://arxiv.org/ftp/nlin/papers/0501/0501022.pdf</a>

<p>
Circular Motion of Strings in Cellular Automata, and Other Surprises<br>
<a href="https://arxiv.org/abs/1206.2060">https://arxiv.org/abs/1206.2060</a>

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
        CheckIfGenerating()
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
    ViewChanged(false)
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
    ViewChanged(false)
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
    InitDepthShading()
    ViewChanged(false)
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
        stepslider.hide()
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
    if dirty and scriptlevel == 0 then
        local answer = g.savechanges("Save your changes?",
                                     "If you don't save, the changes will be lost.")
        if answer == "yes" then
            SavePattern()
            if dirty then
                -- error occurred or user hit Cancel in g.savedialog
                return
            end
        elseif answer == "no" then
            g.exit()
        else -- answer == "cancel"
            return
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

        CheckIfGenerating()
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

        CheckIfGenerating()
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

        CheckIfGenerating()
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

    ov("font 11 default-bold")              -- font for info text

    -- set parameters for menu bar and tool bar buttons
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
end

----------------------------------------------------------------------

function CreateMenuBar()
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
    mbar.additem(3, "Next Step", NextStep)
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
end

----------------------------------------------------------------------

function CreateToolBar()
    -- create tool bar buttons
    ssbutton = op.button("Start", StartStop)
    s1button = op.button("+1", Step1)
    resetbutton = op.button("Reset", Reset)
    fitbutton = op.button("Fit", FitGrid)
    undobutton = op.button("Undo", Undo)
    redobutton = op.button("Redo", Redo)
    helpbutton = op.button("?", ShowHelp)
    exitbutton = op.button("X", ExitScript)

    -- create radio buttons and slider (don't shadow text)
    op.textshadowx = 0
    op.textshadowy = 0

    drawbox = op.radiobutton("Draw", op.black, DrawMode)
    selectbox = op.radiobutton("Select", op.black, SelectMode)
    movebox = op.radiobutton("Move", op.black, MoveMode)

    -- create a slider for adjusting stepsize
    stepslider = op.slider("", op.black, 100, 1, 100, StepChange)
end

----------------------------------------------------------------------

function CreatePopUpMenus()
    -- text in pop-up menus is shadowed
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

function CheckWindowSize()
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

function ViewChanged(rotate)
    -- cube needs recreating on rotate or depth shade toggle
    lastCubeSize = -1
    lastBusyCubeSize["E"] = -1
    lastBusyCubeSize["O"] = -1
    if not rotate then
        -- sphere only needs recreating on depth shade toggle
        lastSphereSize = -1
        lastBusySphereSize["E"] = -1
        lastBusySphereSize["O"] = -1
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

    -- check if the view changed
    if (xixo ~= anew) or (xiyo ~= bnew) or (xizo ~= cnew) or
       (yixo ~= dnew) or (yiyo ~= enew) or (yizo ~= fnew) or
       (zixo ~= gnew) or (ziyo ~= hnew) or (zizo ~= inew) then
        ViewChanged(true)
    end

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

function InitialView(display)
    if display == nil then display = true end

    -- initialize the transformation matrix
    xixo = 1.0; yixo = 0.0; zixo = 0.0
    xiyo = 0.0; yiyo = 1.0; ziyo = 0.0
    xizo = 0.0; yizo = 0.0; zizo = 1.0

    -- rotate to a nice view but don't call Refresh
    Rotate(160, 20, 0, false)
    -- user can hit the up arrow 4 times and the right arrow 4 times
    -- to see an untilted XY plane parallel with the screen

    FitGrid(display)    -- calls Refresh if display is true
end

----------------------------------------------------------------------

function InitDepthShading()
    -- initialize each depth shading layer
    local extradepth = round(depthlayers * sqrt(3))
    mindepth = -extradepth // 2
    maxdepth = depthlayers + extradepth // 2
    for i = mindepth, maxdepth do
        -- clear depth shading list
        layercoords[i] = {}
        -- set index to position after draw command
        layerindices[i] = 2
        lastDepthBatchSize[i] = MAXN * MAXN * MAXN * 2
    end
    layerlast = mindepth - 1
end

----------------------------------------------------------------------

function Initialize()
    CreateOverlay()
    CreateMenuBar()
    CreateToolBar()
    CreatePopUpMenus()
    CreateAxes()
    InitDepthShading()
    
    if #rulestring == 0 then
        -- first call must initialize rulestring, survivals, births and NextGeneration
        ParseRule(DEFAULT_RULE)
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
    end

    SetActivePlane()
    InitialView(false)      -- don't call Refresh

    -- run the user's startup script if it exists
    local f = io.open(startup, "r")
    if f then
        f:close()
        RunScript(startup)
        ClearUndoRedo()     -- don't want to undo startup script
    end

    -- note that startup script might have changed BACK_COLOR etc
    ov("textoption background "..BACK_COLOR:sub(6))
    ssbutton.customcolor = START_COLOR
    ssbutton.darkcustomcolor = SELSTART_COLOR

    Refresh()
end

----------------------------------------------------------------------

function HandleKey(event)
    local CMDCTRL = "cmd"
    if g.os() ~= "Mac" then CMDCTRL = "ctrl" end
    local _, key, mods = split(event)
    if     key == "return" and mods == "none" then StartStop()
    elseif key == "space"  and mods == "none" then Step1()
    elseif key == "tab"    and mods == "none" then NextStep()
    elseif key == "down"   and mods == "none" then Rotate(-5,  0,  0)
    elseif key == "up"     and mods == "none" then Rotate( 5,  0,  0)
    elseif key == "left"   and mods == "none" then Rotate( 0, -5,  0)
    elseif key == "right"  and mods == "none" then Rotate( 0,  5,  0)
    elseif key == "down"   and mods == "alt"  then Rotate( 0,  0, -5)
    elseif key == "right"  and mods == "alt"  then Rotate( 0,  0, -5)
    elseif key == "up"     and mods == "alt"  then Rotate( 0,  0,  5)
    elseif key == "left"   and mods == "alt"  then Rotate( 0,  0,  5)
    elseif key == "delete" and mods == "none" then ClearSelection()
    elseif key == "delete" and mods == "shift" then ClearOutside()
    elseif key == "=" and mods == "none" then Faster()
    elseif key == "-" and mods == "none" then Slower()
    elseif key == "1" and mods == "none" then SetStepSize(1)
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
    elseif key == "z" and (mods == "none" or mods == CMDCTRL) then Undo()
    elseif key == "z" and (mods == "shift" or mods == CMDCTRL.."shift") then Redo()
    elseif key == "x" and mods == CMDCTRL then CutSelection()
    elseif key == "c" and mods == CMDCTRL then CopySelection()
    elseif key == "v" and (mods == "none" or mods == CMDCTRL) then Paste()
    elseif key == "v" and mods == "alt" then CancelPaste()
    elseif key == "b" and mods == CMDCTRL then PasteOR()
    elseif key == "b" and mods == CMDCTRL.."shift" then PasteXOR()
    elseif key == "b" and mods == "none" then Rotate(0, 180, 0)
    elseif key == "[" and mods == "none" then ZoomOut()
    elseif key == "]" and mods == "none" then ZoomIn()
    elseif key == "i" and mods == "none" then InitialView()
    elseif key == "f" and mods == "none" then FitGrid()
    elseif key == "p" and mods == "none" then CycleCellType()
    elseif key == "l" and mods == "none" then ToggleLines()
    elseif key == "l" and mods == "shift" then ToggleAxes()
    elseif key == "d" and mods == "alt" then ToggleDepthShading()
    elseif key == "t" and mods == "none" then ToggleToolBar()
    elseif key == "," and mods == "none" then MoveActivePlane(activepos+1, true)
    elseif key == "." and mods == "none" then MoveActivePlane(activepos-1, true)
    elseif key == "a" and mods == "shift" then CycleActivePlane()
    elseif key == "c" and mods == "none" then CycleCursor()
    elseif key == "d" and mods == "none" then DrawMode()
    elseif key == "s" and mods == "none" then SelectMode()
    elseif key == "m" and mods == "none" then MoveMode()
    elseif key == "m" and mods == "shift" then MiddlePattern()
    elseif key == "h" and mods == "none" then ShowHelp()
    elseif key == "q" then ExitScript()
    else
        -- could be a keyboard shortcut (eg. for full screen)
        g.doevent(event)
    end
end

----------------------------------------------------------------------

function MouseDown(x, y, mods, mouseinfo)
    -- mouse button has been pressed
    mouseinfo.mousedown = true
    mouseinfo.prevx = x
    mouseinfo.prevy = y
    if pastecount > 0 then
        -- paste pattern can be dragged using any cursor
        mouseinfo.dragface = StartDraggingPaste(x, y)
        mouseinfo.drag_paste = #mouseinfo.dragface > 0
    end
    if mouseinfo.drag_paste then
        -- ignore currcursor
        RememberCurrentState()
    elseif currcursor == drawcursor then
        if mods == "none" then
            mouseinfo.drawing = StartDrawing(x, y)
        elseif mods == "shift" then
            mouseinfo.drag_active = StartDraggingPlane(x, y)
        end
    elseif currcursor == selectcursor then
        if mods == "none" then
            mouseinfo.selecting = StartSelecting(x, y)
        elseif mods == "shift" then
            mouseinfo.drag_active = StartDraggingPlane(x, y)
        end
    else
        -- currcursor == movecursor
        if mods == "none" then
            if selcount > 0 then
                mouseinfo.dragface = StartDraggingSelection(x, y)
                mouseinfo.drag_selection = #mouseinfo.dragface > 0
            end
        elseif mods == "alt" then
            mouseinfo.hand_erase = true
            EraseLiveCells(x, y, true)
        elseif mods == "shift" then
            mouseinfo.hand_select = true
            SelectLiveCells(x, y, true)
        end
    end
end

----------------------------------------------------------------------

function MouseUp(mouseinfo)
    -- mouse button has been released
    mouseinfo.mousedown = false
    if mouseinfo.drawing then
        mouseinfo.drawing = false
        CheckIfGenerating()
    elseif mouseinfo.selecting then
        mouseinfo.selecting = false
        CheckIfGenerating()
    elseif mouseinfo.drag_paste then
        mouseinfo.drag_paste = false
        CheckIfGenerating()
    elseif mouseinfo.drag_selection then
        mouseinfo.drag_selection = false
        StopDraggingSelection()
        CheckIfGenerating()
    elseif mouseinfo.drag_active then
        mouseinfo.drag_active = false
        CheckIfGenerating()
    elseif mouseinfo.hand_erase then
        mouseinfo.hand_erase = false
        CheckIfGenerating()
    elseif mouseinfo.hand_select then
        mouseinfo.hand_select = false
        CheckIfGenerating()
    end
end

----------------------------------------------------------------------

function CheckMousePosition(mousepos, mouseinfo)
    if #mousepos > 0 then
        local x, y = split(mousepos)
        x = tonumber(x)
        y = tonumber(y)
        if x ~= mouseinfo.prevx or y ~= mouseinfo.prevy then
            -- mouse has moved
            if mouseinfo.drawing then
                DrawCells(x, y)
            elseif mouseinfo.selecting then
                SelectCells(x, y)
            elseif mouseinfo.drag_paste then
                DragPaste(x, y, mouseinfo.prevx, mouseinfo.prevy, mouseinfo.dragface)
            elseif mouseinfo.drag_selection then
                DragSelection(x, y, mouseinfo.prevx, mouseinfo.prevy, mouseinfo.dragface)
            elseif mouseinfo.drag_active then
                DragActivePlane(x, y, mouseinfo.prevx, mouseinfo.prevy)
            elseif mouseinfo.hand_erase then
                EraseLiveCells(x, y, false)
            elseif mouseinfo.hand_select then
                SelectLiveCells(x, y, false)
            else
                -- rotate the view
                local deltax = x - mouseinfo.prevx
                local deltay = y - mouseinfo.prevy
                Rotate(round(-deltay/2.0), round(deltax/2.0), 0)
            end
            mouseinfo.prevx = x
            mouseinfo.prevy = y
        end
    elseif #activecell > 0 and currcursor ~= movecursor then
        activecell = ""
        Refresh()
    end
end

----------------------------------------------------------------------

function EventLoop()
    -- best to call Initialize here so any error is caught by xpcall
    Initialize()

    local mouseinfo = {
        mousedown = false,          -- mouse button is down?
        drawing = false,            -- draw/erase cells with pencil cursor?
        selecting = false,          -- (de)select cells with cross-hairs cursor?
        drag_paste = false,         -- drag paste pattern with any cursor?
        drag_selection = false,     -- drag selected cells with hand cursor?
        drag_active = false,        -- drag active plane with pencil/cross-hairs?
        hand_erase = false,         -- erase live cells with hand cursor?
        hand_select = false,        -- select live cells with hand cursor?
        dragface = "",              -- which paste/selection face is being dragged
        prevx = nil, prevy = nil    -- previous mouse position
    }

    while true do
        local event = g.getevent()
        if #event == 0 then
            if not mouseinfo.mousedown then
                if not generating then
                    g.sleep(5)      -- don't hog the CPU when idle
                end
                CheckWindowSize()   -- may need to resize the overlay
            end
        else
            if message and (event:find("^key") or event:find("^oclick") or event:find("^file")) then
                message = nil
                Refresh()           -- remove the most recent message
            end
            event = op.process(event)
            if #event == 0 then
                -- op.process handled the given event
            elseif event:find("^key") then
                -- don't do key action if mouse button is down (can clobber undo history)
                if not mouseinfo.mousedown then
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
                        MouseDown(x, y, mods, mouseinfo)
                    end
                end
            elseif event:find("^mup") then
                MouseUp(mouseinfo)
            elseif event:find("^ozoomout") then
                if not arrow_cursor then ZoomOut() end
            elseif event:find("^ozoomin") then
                if not arrow_cursor then ZoomIn() end
            elseif event:find("^file") then
                OpenFile(event:sub(6))
            end
        end

        local mousepos = ov("xy")
        if mouseinfo.mousedown then
            CheckMousePosition(mousepos, mouseinfo)
        else
            CheckCursor(mousepos)
            if generating then NextGeneration() end
        end
    end
end

----------------------------------------------------------------------

ReadSettings()
oldstate = SaveGollyState()

status, err = xpcall(EventLoop, gp.trace)
if err then g.continue(err) end
-- the following code is always executed

-- ensure the following code *completes*, even if user quits Golly
g.check(false)

RestoreGollyState(oldstate)
WriteSettings()
