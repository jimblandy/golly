-- Browse through patterns in a folder (and subfolders)
-- Page Up/Page Down to cycle through patterns
-- Home to reload current pattern
-- Esc to exit at current pattern
-- Author: Chris Rowett (crowett@gmail.com)

local g = golly()
local gp = require "gplus"

require "gplus.strict"
local pathsep = g.getdir("app"):sub(-1)
local busy = "Finding patterns, please wait..."
local controls = "Page Up for previous, Page Down for next, Home to reload current, Esc to exit."
local patterns = {}
local numpatterns = 0

--------------------------------------------------------------------------------

local function findpatterns(dir)
    local files = g.getfiles(dir)
    for _, name in ipairs(files) do
        if name:sub(1,1) == "." then
            -- ignore hidden files (like .DS_Store on Mac)
        elseif name:sub(-1) == pathsep then
            -- name is a subdirectory
            findpatterns(dir..name)
        else
            -- check it isn't a script
            if not (name:sub(-4) == ".lua" or name:sub(-3) == ".py") then
                -- add to list of patterns
                numpatterns = numpatterns + 1
                patterns[numpatterns] = dir..name
            end
        end
    end
end

--------------------------------------------------------------------------------

local function browsepatterns()
    local whichpattern = 1
    local generating = false
    local now = g.millisecs()
    local target = 15

    -- loop until escape pressed
    while true do
        g.new("")
        g.setalgo("QuickLife")      -- nicer to start from this algo
        local fullname = patterns[whichpattern]
        g.open(fullname, false)     -- don't add file to Open/Run Recent submenu
        g.show("Pattern "..whichpattern.." of "..numpatterns..". "..controls)
        g.update()
        generating = false

        -- decode key presses
        while true do
            local event = g.getevent()
            if event == "key pagedown none" then
                whichpattern = whichpattern + 1
                if whichpattern > numpatterns then
                    whichpattern = 1
                end
                break
            elseif event == "key pageup none" then
                whichpattern = whichpattern - 1
                if whichpattern < 1 then
                    whichpattern = numpatterns
                end
                break
            elseif event == "key home none" then
                break
            elseif event == "key enter none" or event == "key return none" then
                generating = not generating
            elseif event == "key space none" then
                if generating then
                    generating = false
                else
                    g.run(1)
                end
            elseif event == "key r ctrl" then
                generating = false
                g.reset()
            elseif event == "key tab none" then
                if generating then
                    generating = false
                else
                    g.step()
                end
            else
               g.doevent(event)
            end

            -- run the pattern
            if generating then
                local base = g.getbase()
                local step = g.getstep()
                if step < 0 then
                    delay = 2 ^ -step * 125
                    step = 0
                else
                    delay = 15
                end
                if delay ~= target then
                    target = delay
                end
                local t = g.millisecs()
                if t - now > target then
                    g.run(base ^ step)
                    now = t
                    target = delay
                end
            end
            g.update()
        end
    end
end

--------------------------------------------------------------------------------

function browse()
    -- try to get the current open pattern folder
    local pathname = g.getpath()
    local dirname = ""
    if pathname == "" then
        -- ask for a folder
        dirname = g.opendialog("Choose a folder", "dir", g.getdir("app"))
    else
        -- remove the file name
        dirname = pathname:sub(1, pathname:find(pathsep.."[^"..pathsep.."/]*$"))
    end
    if dirname ~= "" then
        g.show(busy)
        findpatterns(dirname)
        if numpatterns > 0 then
            -- browse patterns
            browsepatterns(pathname)
        end
    end
end

--------------------------------------------------------------------------------

local status, err = xpcall(browse, gp.trace)
if err then g.continue(err) end

-- this code is always executed, even after escape/error;
-- clear message line in case there was no escape/error
g.show("")
