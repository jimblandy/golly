-- Use multiple layers to create a history of the current pattern.
-- The "envelope" layer remembers all live cells.
-- Author: Andrew Trevorrow (andrew@trevorrow.com), Apr 2016.

local g = golly()
local gp = require "gplus"

if g.empty() then g.exit("There is no pattern.") end

local currindex = g.getlayer()
local startindex
local envindex
local startname = "starting pattern"
local envname = "envelope"

if currindex > 1 and g.getname(currindex - 1) == startname
                 and g.getname(currindex - 2) == envname then
    -- continue from where we left off
    startindex = currindex - 1
    envindex = currindex - 2

elseif (currindex + 2) < g.numlayers()
                 and g.getname(currindex + 1) == startname
                 and g.getname(currindex)     == envname then
    -- switch from envelope layer to current layer and continue
    currindex = currindex + 2
    g.setlayer(currindex)
    startindex = currindex - 1
    envindex = currindex - 2

elseif (currindex + 1) < g.numlayers()
                 and g.getname(currindex)     == startname
                 and g.getname(currindex - 1) == envname then
    -- switch from starting layer to current layer and continue
    currindex = currindex + 1
    g.setlayer(currindex)
    startindex = currindex - 1
    envindex = currindex - 2

else
    -- start a new envelope using pattern in current layer
    if g.numlayers() + 1 > g.maxlayers() then
        g.exit("You need to delete a couple of layers.")
    end
    if g.numlayers() + 2 > g.maxlayers() then
        g.exit("You need to delete a layer.")
    end
    
    -- get current layer's starting pattern
    local startpatt = g.getcells(g.getrect())
    
    envindex = g.addlayer()         -- create layer for remembering all live cells
    g.setcolors({-1,100,100,100})   -- set all live states to darkish gray
    g.putcells(startpatt)           -- copy starting pattern into this layer
    
    startindex = g.addlayer()       -- create layer for starting pattern
    g.setcolors({-1,0,255,0})       -- set all live states to green
    g.putcells(startpatt)           -- copy starting pattern into this layer
    
    -- move currindex to above the envelope and starting pattern
    g.movelayer(currindex, envindex)
    g.movelayer(envindex, startindex)
    currindex = startindex
    startindex = currindex - 1
    envindex = currindex - 2
    
    -- name the starting and envelope layers so user can run script
    -- again and continue from where it was stopped
    g.setname(startname, startindex)
    g.setname(envname, envindex)
end

--------------------------------------------------------------------------------

function envelope()
    -- draw stacked layers using same location and scale
    g.setoption("stacklayers", 1)
    
    g.show("Hit escape key to stop script...")
    while true do
        g.run(1)
        if g.empty() then
            g.show("Pattern died out.")
            return
        end
        
        -- copy current pattern to envelope layer
        -- we temporarily disable event checking so thumb scrolling
        -- and other mouse events won't cause confusing changes
        local currpatt = g.getcells(g.getrect())
        g.check(false)
        g.setlayer(envindex)
        g.putcells(currpatt)
        g.setlayer(currindex)
        g.check(true)
        
        local step = 1
        local expo = g.getstep()
        if expo > 0 then
            step = g.getbase()^expo
        end
        if g.getgen() % step == 0 then
            -- display all 3 layers (envelope, start, current)
            g.update()
        end
    end
end

--------------------------------------------------------------------------------

-- show status bar but hide layer & edit bars (faster, and avoids flashing)
local oldstatus = g.setoption("showstatusbar", 1)
local oldlayerbar = g.setoption("showlayerbar", 0)
local oldeditbar = g.setoption("showeditbar", 0)

local status, err = xpcall(envelope, gp.trace)
if err then g.continue(err) end
-- the following code is executed even if error occurred or user aborted script

-- restore original state of status/layer/edit bars
g.setoption("showstatusbar", oldstatus)
g.setoption("showlayerbar", oldlayerbar)
g.setoption("showeditbar", oldeditbar)
