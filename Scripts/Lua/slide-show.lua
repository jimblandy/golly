-- Display all patterns in Golly's Patterns folder.
-- Author: Andrew Trevorrow (andrew@trevorrow.com), Apr 2016.

local g = golly()
local gp = require "gplus"

local pathsep = g.getdir("app"):sub(-1)
local message = "Hit space to continue or escape to exit the slide show..."

--------------------------------------------------------------------------------

local function walk(dir)
    local files = g.getfiles(dir)
    for _, name in ipairs(files) do
        if name:sub(1,1) == "." then
            -- ignore hidden files (like .DS_Store on Mac)
        elseif name:sub(-1) == pathsep then
            -- name is a subdirectory
            walk(dir..name)
        else
            g.new("")
            g.setalgo("QuickLife")      -- nicer to start from this algo
            local fullname = dir..name
            g.open(fullname, false)     -- don't add file to Open/Run Recent submenu
            g.update()
            if name:sub(-4) == ".lua" or name:sub(-3) == ".py" then
                -- reshow message in case it was changed by script
                g.show(message)
            end
            while true do
                local event = g.getevent()
                if event == "key space none" then
                    break
                end
                g.doevent(event)        -- allow keyboard/mouse interaction
            end
        end
    end
end

--------------------------------------------------------------------------------

function slideshow()
    local oldalgo = g.getalgo()
    local oldrule = g.getrule()

    g.show(message)
    walk(g.getdir("app").."Patterns"..pathsep)

    -- if all patterns have been displayed then restore original algo and rule
    -- (don't do this if user hits escape in case they want to explore pattern)
    g.new("untitled")
    g.setalgo(oldalgo)
    g.setrule(oldrule)
end

--------------------------------------------------------------------------------

-- show status bar but hide other info to maximize viewport
local oldstatus = g.setoption("showstatusbar", 1)
local oldtoolbar = g.setoption("showtoolbar", 0)
local oldlayerbar = g.setoption("showlayerbar", 0)
local oldeditbar = g.setoption("showeditbar", 0)
local oldfiles = g.setoption("showfiles", 0)

local status, err = xpcall(slideshow, gp.trace)
if err then g.continue(err) end

-- this code is always executed, even after escape/error;
-- clear message line in case there was no escape/error
g.show("")
-- restore original state
g.setoption("showstatusbar", oldstatus)
g.setoption("showtoolbar", oldtoolbar)
g.setoption("showlayerbar", oldlayerbar)
g.setoption("showeditbar", oldeditbar)
g.setoption("showfiles", oldfiles)
