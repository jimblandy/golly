-- Browse through patterns in a folder (and optionally subfolders)
-- Page Up/Page Down to cycle through patterns
-- Home to select a new folder to browse
-- O to display options
-- Esc to exit at current pattern
-- Author: Chris Rowett (crowett@gmail.com)
-- Build 11

local g = golly()
local gp = require "gplus"
local floor = math.floor
local op = require "oplus"
-- require "gplus.strict"
local ov = g.overlay
local viewwd, viewht = g.getview(g.getlayer())
local pathsep = g.getdir("app"):sub(-1)
local busy = "Finding patterns, please wait..."
local controls = "[Page Up] previous, [Page Down] next, [Home] select folder, [O] options, [Esc] exit."
local patterns = {}
local numpatterns = 0
local whichpattern = 1
local overlaycreated = false

-- settings are saved in this file
local settingsfile = g.getdir("data").."browse-patterns.ini"

-- gui buttons
local prevbutton      -- Previous button
local nextbutton      -- Next button
local folderbutton    -- Folder button
local exitbutton      -- Exit button
local startcheck      -- AutoStart checkbox
local fitcheck        -- AutoFit checkbox
local speedslider     -- AutoAdvance speed slider
local optionsbutton   -- Options button
local subdircheck     -- Include subdirectories checkbox
local keepspeedcheck  -- Keep speed checkbox
local loopcheck       -- Loop checkbox
local closebutton     -- Close options button

-- position
local guiht        = 32    -- height of toolbar
local guiwd        = 0     -- computed width of toolbar (from control widths)
local gapx         = 10    -- horitzonal gap between controls
local gapy         = 4     -- vertical gap between controls
local sectiongapy  = 16    -- vertical gap between sections
local slidertextx  = 0     -- text position following slider
local maxsliderval = 22    -- maxmimum slider value (2^n seconds)
local sliderpower  = 1.32  -- slider power
local remainx      = 0     -- remaining time position

-- style
local toolbarbgcolor = "rgba 0 0 0 192"

-- flags
local loadnew     = false  -- whether to load a new pattern
local exitnow     = false  -- whether to exit
local showoptions = false  -- whether options are displayed

-- settings
local autostart    = 0   -- whether to autostart playback on pattern load
local autofit      = 0   -- whether to switch on autofit on pattern load
local keepspeed    = 0   -- whether to maintain speed between patterns
local subdirs      = 1   -- whether to include subdirectories
local looping      = 1   -- whether to loop pattern list
local advancespeed = 0   -- advance speed (0 for manual)

-- current step
local currentstep = g.getstep()
local patternloadtime = 0

-- file extensions to load
local matchlist = { rle = true, mcl = true, mc = true, lif = true, gz = true }

-- remaining time before auto advance
local remaintime = 0

--------------------------------------------------------------------------------

local function savesettings()
    local f = io.open(settingsfile, "w")
    if f then
        f:write(tostring(autostart).."\n")
        f:write(tostring(autofit).."\n")
        f:write(tostring(keepspeed).."\n")
        f:write(tostring(subdirs).."\n")
        f:write(tostring(looping).."\n")
        f:write(tostring(advancespeed).."\n")
        f:close()
    end
end

--------------------------------------------------------------------------------

local function loadsettings()
    local f = io.open(settingsfile, "r")
    if f then
        autostart    = tonumber(f:read("*l")) or 0
        autofit      = tonumber(f:read("*l")) or 0
        keepspeed    = tonumber(f:read("*l")) or 0
        subdirs      = tonumber(f:read("*l")) or 1
        looping      = tonumber(f:read("*l")) or 1
        advancespeed = tonumber(f:read("*l")) or 0
        f:close()
    end
end

--------------------------------------------------------------------------------

local function findpatterns(dir)
    local files = g.getfiles(dir)
    for _, name in ipairs(files) do
        if name:sub(1,1) == "." then
            -- ignore hidden files (like .DS_Store on Mac)
        elseif name:sub(-1) == pathsep then
            -- name is a subdirectory
            if subdirs == 1 then
                findpatterns(dir..name)
            end
        else
            -- check the file is the right type to display
            local index = name:match'^.*()%.'
            if index then
                if matchlist[name:sub(index+1)] then
                    -- add to list of patterns
                    numpatterns = numpatterns + 1
                    patterns[numpatterns] = dir..name
                end
            end
        end
    end
end

--------------------------------------------------------------------------------

local function getpatternlist(dir)
    numpatterns = 0
    g.show(busy)
    findpatterns(dir)
    if numpatterns == 0 then
        g.note("No patterns found in:\n\n"..dir)
    end
end

--------------------------------------------------------------------------------

local function previouspattern()
    local new = whichpattern - 1
    if new < 1 then
        if looping == 1 then
            new = numpatterns
        else
            new = 1
        end
    end
    if new ~= whichpattern then
        whichpattern = new
        loadnew = true
    end
end

--------------------------------------------------------------------------------

local function nextpattern()
    local new = whichpattern + 1
    if new > numpatterns then
        if looping == 1 then
            new = 1
        else
            new = numpatterns
        end
    end
    if new ~= whichpattern then
        whichpattern = new
        loadnew = true
    end
end

--------------------------------------------------------------------------------

local function exitbrowser()
    loadnew = true
    exitnow = true
end

--------------------------------------------------------------------------------

local function toggleautostart()
    autostart = 1 - autostart
    savesettings()
end

--------------------------------------------------------------------------------

local function toggleautofit()
    autofit = 1 - autofit
    savesettings()
end

--------------------------------------------------------------------------------

local function selectfolder()
    -- remove the filename from the supplied path
    local current = patterns[whichpattern]
    local dirname = current:sub(1, current:find(pathsep.."[^"..pathsep.."/]*$"))

    -- ask for a folder
    local dirname = g.opendialog("Choose a folder", "dir", dirname)
    if dirname ~= "" then
        getpatternlist(dirname)
        whichpattern = 1
        loadnew = true
    end
end

--------------------------------------------------------------------------------

local function drawspeed(x, y)
    -- convert speed into labael
    local message = "Manual"
    if advancespeed > 0 then
        local time = sliderpower ^ (maxsliderval - advancespeed)
        if time < 60 then
            if time < 10 then
                message = string.format("%.1f", time).."s"
            else
                message = floor(time).."s"
            end
        else
            message = floor(time / 60).."m"..floor(time % 60).."s"
        end
    end

    -- update the label
    ov("textoption background 0 0 0 0")
    local wd, ht = op.maketext(message, "label", op.white, 2, 2, op.black)
    ov("blend 1")
    op.pastetext(x, y, op.identity, "label")
    ov("blend 0")
end

--------------------------------------------------------------------------------

local function togglespeed()
    keepspeed = 1 - keepspeed
    savesettings()
end

--------------------------------------------------------------------------------

local function toggleloop()
    looping = 1 - looping
    savesettings()
end

--------------------------------------------------------------------------------

local function togglesubdirs()
    subdirs = 1 - subdirs
    savesettings()
end

--------------------------------------------------------------------------------

local function toggleoptions()
    showoptions = not showoptions
end

--------------------------------------------------------------------------------

local function drawgui()
    -- draw gui background
    local optht = startcheck.ht + fitcheck.ht + keepspeedcheck.ht + subdircheck.ht + loopcheck.ht + speedslider.ht
    optht = optht + 24 * 3 + gapy * 6 + sectiongapy * 3 + closebutton.ht

    -- clear gui
    ov("blend 0")
    ov("rgba 0 0 0 0")
    ov("fill")

    -- draw toolbar background
    ov(toolbarbgcolor)
    ov("fill 0 0 "..guiwd.." "..guiht)
    
    -- draw main buttons
    local y = floor((guiht - prevbutton.ht) / 2)
    local x = gapx
    prevbutton.show(x, y)
    x = x + prevbutton.wd + gapx
    nextbutton.show(x, y)
    x = x + nextbutton.wd + gapx
    folderbutton.show(x, y)
    x = x + folderbutton.wd + gapx
    optionsbutton.show(x, y)
    x = x + optionsbutton.wd + gapx
    exitbutton.show(x, y)

    -- check for options
    if showoptions then
        -- draw options background
        ov("fill 0 "..guiht.." "..viewwd.." "..optht)

        -- draw option controls
        x = gapx
        y = guiht + gapy
        ov("blend 1")
        op.pastetext(x, y, op.identity, "playback")
        ov("blend 0")
        y = y + 24
        if autostart == 1 then
            startcheck.show(x, y, true)
        else
            startcheck.show(x, y, false)
        end
        y = y + startcheck.ht + gapy
        if autofit == 1 then
            fitcheck.show(x, y, true)
        else
            fitcheck.show(x, y, false)
        end
        y = y + fitcheck.ht + sectiongapy
        ov("blend 1")
        op.pastetext(x, y, op.identity, "folder")
        ov("blend 0")
        y = y + 24
        if subdirs == 1 then
            subdircheck.show(x, y, true)
        else
            subdircheck.show(x, y, false)
        end
        y = y + subdircheck.ht + sectiongapy
        ov("blend 1")
        if advancespeed == 0 then
            op.pastetext(x, y, op.identity, "advancemanual")
        else
            -- convert remaining ms into minutes and seconds
            op.pastetext(x, y, op.identity, "advanceauto")
            local remain = floor(remaintime / 1000)
            local message = "in "
            if remain >= 60 then
                message = message..floor(remain / 60).."m"..(remain % 60).."s"
            else
                if remain < 10 then
                    remain = remaintime / 1000
                    if remain < 0 then remain = 0 end
                    message = message..string.format("%.1f", remain).."s"
                else
                    if remain < 0 then remain = 0 end
                    message = message..remain.."s"
                end
            end
            op.maketext(message, "remain", op.white, 2, 2, op.black)
            op.pastetext(x + remainx, y, op.identity, "remain")
        end
        ov("blend 0")
        y = y + 24
        speedslider.show(x, y, advancespeed)
        x = x + speedslider.wd + gapx
        drawspeed(x, y)
        y = y + speedslider.ht + gapy
        x = gapx
        if looping == 1 then
            loopcheck.show(x, y, true)
        else
            loopcheck.show(x, y, false)
        end
        y = y + loopcheck.ht + gapy
        if keepspeed == 1 then
            keepspeedcheck.show(x, y, true)
        else
            keepspeedcheck.show(x, y, false)
        end
        y = y + keepspeedcheck.ht + sectiongapy
        closebutton.show(x, y)
    end
end

--------------------------------------------------------------------------------

local function updatespeed(newval)
    advancespeed = newval
    savesettings()
    drawgui()
end

--------------------------------------------------------------------------------

local function createoverlay()
    -- single text line at the top of the display
    ov("create "..viewwd.." "..viewht)
    overlaycreated = true
    op.buttonht = 20
    op.textgap = 8
    op.textfont = "font 10 default-bold"
    if g.os() == "Mac" then
        op.yoffset = -1
    end
    if g.os() == "Linux" then
        op.textfont = "font 10 default"
    end

    -- create labels
    ov(op.textfont)
    ov(op.black)
    op.maketext("Playback", "playback", op.white, 2, 2, op.black)
    op.maketext("Folder", "folder", op.white, 2, 2, op.black)
    op.maketext("Advance Manually", "advancemanual", op.white, 2, 2, op.black)
    remainx = op.maketext("Advance Automatically ", "advanceauto", op.white, 2, 2, op.black)

    -- create gui buttons
    op.textshadowx = 2
    op.textshadowy = 2
    prevbutton = op.button("Previous", previouspattern)
    nextbutton = op.button("Next", nextpattern)
    exitbutton = op.button("Exit", exitbrowser)
    folderbutton = op.button("Folder", selectfolder)
    startcheck = op.checkbox("Start playback on pattern load", op.white, toggleautostart)
    fitcheck = op.checkbox("Fit pattern to display", op.white, toggleautofit)
    speedslider = op.slider("Speed: ", op.white, 81, 0, maxsliderval, updatespeed)
    optionsbutton = op.button("Options", toggleoptions)
    subdircheck = op.checkbox("Include subdirectories", op.white, togglesubdirs)
    keepspeedcheck = op.checkbox("Maintain step speed across patterns", op.white, togglespeed)
    loopcheck = op.checkbox("Loop patterns", op.white, toggleloop)
    closebutton = op.button("Close Options", toggleoptions)

    -- resize the overlay to fit the controls
    local optht = startcheck.ht + fitcheck.ht + keepspeedcheck.ht + subdircheck.ht + loopcheck.ht + speedslider.ht
    optht = optht + 24 * 3 + gapy * 6 + sectiongapy * 3 + closebutton.ht
    guiwd = prevbutton.wd + nextbutton.wd + folderbutton.wd + optionsbutton.wd + exitbutton.wd + 6 * gapx
    ov("resize "..guiwd.." "..(guiht + optht))

    -- draw the overlay
    drawgui(0)
end

--------------------------------------------------------------------------------

local function browsepatterns(startpattern)
    local generating = 0
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
        -- load the pattern
        currentstep = g.getstep()
        g.new("")
        g.setalgo("QuickLife")      -- nicer to start from this algo
        local fullname = patterns[whichpattern]
        g.open(fullname, false)     -- don't add file to Open/Run Recent submenu
        g.show("Pattern "..whichpattern.." of "..numpatterns..". "..controls)
        g.update()
        generating = autostart

        -- restore playback speed if requested
        if keepspeed == 1 then
            g.setstep(currentstep)
        end

        -- decode key presses
        loadnew = false
        while not loadnew do
            local event = op.process(g.getevent())
            if event == "key pagedown none" then
                nextpattern()
                patternloadtime = g.millisecs()
            elseif event == "key pageup none" then
                previouspattern()
                patternloadtime = g.millisecs()
            elseif event == "key home none" then
                selectfolder()
                patternloadtime = g.millisecs()
            elseif event == "key enter none" or event == "key return none" then
                generating = 1 - generating
            elseif event == "key o none" then
                toggleoptions()
            elseif event == "key space none" then
                if generating == 1 then
                    generating = 0
                else
                    g.run(1)
                end
            elseif event == "key r ctrl" then
                generating = 0
                g.reset()
            elseif event == "key tab none" then
                if generating == 1 then
                    generating = 0
                else
                    g.step()
                end
            else
               g.doevent(event)
            end

            -- run the pattern
            if generating == 1 then
                currentstep = g.getstep()
                if currentstep < 0 then
                    -- convert negative steps into delays
                    delay = sliderpower ^ -currentstep * 125
                    currentstep = 0
                else
                    delay = 15
                end
                if delay ~= target then
                    target = delay
                end
                local t = g.millisecs()
                if t - now > target then
                    g.run(g.getbase() ^ currentstep)
                    now = t
                    target = delay
                end
            end
            if autofit == 1 then
                -- get pattern bounding box
                local rect = g.getrect()
                if #rect ~= 0 then
                    -- check if entire bounding box is visible in the viewport
                    if not g.visrect(rect) then
                        g.fit()
                    end
                end
            end

            -- check for auto advance
            remaintime = 0
            if advancespeed > 0 then
                local targettime = (sliderpower ^ (maxsliderval - advancespeed)) * 1000
                remaintime = targettime - (g.millisecs() - patternloadtime)
                if remaintime < 0 then
                    nextpattern()
                    patternloadtime = g.millisecs()
                end
            end
            drawgui()
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
        loadsettings()
        getpatternlist(dirname)
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
if overlaycreated then ov("delete") end
