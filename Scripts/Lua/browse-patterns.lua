-- Browse through patterns in a folder (and subfolders)
-- Page Up/Page Down to cycle through patterns
-- Home to reload current pattern
-- Esc to exit at current pattern
-- Author: Chris Rowett (crowett@gmail.com)
-- Build 2

local g = golly()
local gp = require "gplus"
local int = gp.int
local op = require "oplus"
require "gplus.strict"
local ov = g.overlay
local viewwd, viewht = g.getview(g.getlayer())
local guiht = 24
local pathsep = g.getdir("app"):sub(-1)
local busy = "Finding patterns, please wait..."
local controls = "Page Up for previous, Page Down for next, Home to reload current, Esc to exit."
local patterns = {}
local numpatterns = 0
local whichpattern = 1

-- gui buttons
local prevbutton
local nextbutton
local reloadbutton
local exitbutton

-- whether to load a new pattern
local loadnew = false

-- whether to eit
local exitnow = false

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

local function reloadpattern()
    loadnew = true
end

--------------------------------------------------------------------------------

local function previouspattern()
    whichpattern = whichpattern - 1
    if whichpattern < 1 then
        whichpattern = numpatterns
    end
    loadnew = true
end

--------------------------------------------------------------------------------

local function nextpattern()
    whichpattern = whichpattern + 1
    if whichpattern > numpatterns then
        whichpattern = 1
    end
    loadnew = true
end

--------------------------------------------------------------------------------

local function exitbrowser()
    loadnew = true
    exitnow = true
end

--------------------------------------------------------------------------------

local function createoverlay()
    -- single text line at the top of the display
    ov("create "..viewwd.." "..guiht)
    op.buttonht = 20
    op.textgap = 8
    op.textfont = "font 10 default-bold"
    if g.os() == "Mac" then
        op.yoffset = -1
    end
    if g.os() == "Linux" then
        op.textfont = "font 10 default"
    end

    -- create gui buttons
    prevbutton = op.button("Previous", previouspattern)
    nextbutton = op.button("Next", nextpattern)
    reloadbutton = op.button("Reload", reloadpattern)
    exitbutton = op.button("Exit", exitbrowser)

    -- draw overlay
    ov(op.white)
    ov("fill 0 0 "..viewwd.." "..guiht)
    local gap = 10
    local y = int((guiht - prevbutton.ht) / 2)
    local x = gap
    prevbutton.show(x, y)
    x = x + prevbutton.wd + gap
    nextbutton.show(x, y)
    x = x + nextbutton.wd + gap
    reloadbutton.show(x, y)
    x = x + reloadbutton.wd + gap
    exitbutton.show(x, y)
end

--------------------------------------------------------------------------------

local function browsepatterns(startpattern)
    local generating = false
    local now = g.millisecs()
    local target = 15
    local delay = target

    -- if start pattern is supplied then find it in the list
    whichpattern = 1
    if startpattern ~= "" then
        local i = 1
        while i <= numpatterns do
            if patterns[i] == startpattern then
                whichpattern = i
                break
            end
            i = i + 1
        end
    end

    -- loop until escape pressed or exit clicked
    exitnow = false
    while not exitnow do
        g.new("")
        g.setalgo("QuickLife")      -- nicer to start from this algo
        local fullname = patterns[whichpattern]
        g.open(fullname, false)     -- don't add file to Open/Run Recent submenu
        g.show("Pattern "..whichpattern.." of "..numpatterns..". "..controls)
        g.update()
        generating = false

        -- decode key presses
        loadnew = false
        while not loadnew do
            local event = op.process(g.getevent())
            if event == "key pagedown none" then
                nextpattern()
            elseif event == "key pageup none" then
                previouspattern()
            elseif event == "key home none" then
                reloadpattern()
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
                    -- convert negative steps into delays
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
            -- display gui
            createoverlay()

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
ov("delete")
