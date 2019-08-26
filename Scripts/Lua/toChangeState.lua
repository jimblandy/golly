-- toChangeState.lua
--   adapted in August 2019 from change-state.py by Andrew Trevorrow,
--   and toChangeColors.py by Dave Greene
-- Switch all cells of a clicked-on color in the selected pattern to the current brush color

local g = golly()
local gp = require "gplus"
local split = gp.split
local oldsecs
local newsecs
local oldeditbar = g.setoption("showeditbar",1)

local function savestate(filename, state)
    local f = io.open(filename, "w")
    if f then
        f:write(state)
        f:close()
    else
        g.warn("Can't save target state in filename:\n"..filename)
    end
end

local ChangeStateINIFileName = g.getdir("data").."changestate.ini"
local targetstate = "0"
local f = io.open(ChangeStateINIFileName, "r")
if f then
    targetstate = f:read("*l") or ""
    f:close()
end
if targetstate==nil then
    targetstate = "0"
    savestate(ChangeStateINIFileName, targetstate)
end

limits = "selection"
seltype = " selected"
r = g.getselrect()
if #r==0 then
    limits = "pattern's bounding box"
    seltype = ""
    r = g.getrect()
    if #r==0 then
        g.exit("No pattern to change.")
    end
end

-- set fill limits to the chosen rectangle
minx, miny, sizex, sizey = table.unpack(r)
maxx = minx + sizex - 1
maxy = miny + sizey - 1

area = g.getcells(r)
if g.numstates() == 2 then
    g.exit("Rule must be multistate.  Use 'Clear' editing tools and invert.lua for cell changes in two-state patterns.")
end

saveoldstate = g.setoption("drawingstate",targetstate)
newstate = targetstate
oldstate = newstate

-- wait for user to click a cell
g.show("Click a cell to change all" .. seltype .. " cells of that type to target state "..newstate.. 
       ". Change drawing state on Edit toolbar to adjust target state. Esc to cancel.")
while true do
    event = g.getevent()
    -- user may change drawing state at any time
    newstate = g.getoption("drawingstate")
    if newstate ~= oldstate then
        g.show("Click a cell to change all" .. seltype .. " cells of that type to target state "..newstate.. 
               ". Change drawing state on Edit toolbar to adjust target state. Esc to cancel.")
        oldstate = newstate
        savestate(ChangeStateINIFileName, newstate)
    end
 
    if event:sub(1,5)==("click") then
        -- event is a string like "click 10 20 left none"
        evt, xstr, ystr, button, mods = split(event)
        x = tonumber(xstr)
        y = tonumber(ystr)
        if x < minx or x > maxx or y < miny or y > maxy then
            g.warn("Click inside the "..limits..".  To change the region affected by the script, hit Esc to cancel, then adjust the selection.")
        else
            oldstate = g.getcell(x, y)
            if oldstate == newstate then
                g.warn("The clicked cell must have a different state\n"
                      .."from the current drawing state, "..newstate..".")
            else
                break
            end
        end
    else
        g.doevent(event)
    end
end
 
-- tell Golly to handle all further keyboard/mouse events
g.getevent(false)

if oldstate == 0 then
    -- special handling for state-0 cells, which don't show up in cell lists
    --   so they must be tested individually by scanning the entire bounding box
    changed = 0
    total = sizex * sizey
    oldsecs = os.clock()
    for row = miny, maxy do
        -- might be large pattern so show percentage done each second
        newsecs = os.clock()
        if newsecs - oldsecs >= 1.0 then
            oldsecs = newsecs
            g.show("Changing cell states: "..string.format("%.1f", 100 * (row - miny) * sizex / total).."%")
            g.update()
        end
        -- allow keyboard interaction
        g.doevent( g.getevent() )
     
        for col = minx, maxx do
            if g.getcell(col, row) == oldstate then
                g.setcell(col, row, newstate)
            end
            changed = changed + 1
        end
    end
    g.show("Changed "..changed.." state "..oldstate.." cells to state "..newstate..".")
else
    -- faster handling for changing nonzero states to another state (including zero)
    oldsecs = os.clock()
    numcellsx3 = #area
    count = 0
    for i=1, numcellsx3-2, 3 do
        -- might be large pattern so show percentage done each second
        newsecs = os.clock()
        if newsecs - oldsecs >= 1.0 then
            oldsecs = newsecs
            g.show("Changing cell states: "..string.format("%.1f",100 * i / numcellsx3).."%")
            g.update()
        end
        -- allow keyboard interaction
        g.doevent( g.getevent() )
        
        if area[i+2]==oldstate then
            g.setcell(area[i],area[i+1],newstate)
            count = count + 1
        end
    end
    g.show("Changed "..count.." state "..oldstate.." cells to state "..newstate..".")
end

-- return to previous Edit Bar setting
g.setoption("showeditbar", oldeditbar)
