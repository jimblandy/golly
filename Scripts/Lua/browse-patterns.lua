-- Browse through patterns in a folder (and optionally subfolders)
--
-- Keyboard shortcuts:
--   Page Down  - next pattern
--   Page Up    - previous pattern
--   Home       - select a new folder to browse
--   O          - toggle options display
--   Control-I  - toggle pattern information display
--   Control-T  - toggle autofit
--   Esc        - exit at current pattern
--
-- Author:
--   Chris Rowett (crowett@gmail.com)

local g = golly()
local gp = require "gplus"
local gpl = require "gplus.lifeviewer"
local floor = math.floor
local op = require "oplus"
-- require "gplus.strict"

-- overlay
local ov = g.overlay
local ovt = g.ovtable
local viewwd, viewht = g.getview(g.getlayer())
local overlaycreated = false

local pathsep = g.getdir("app"):sub(-1)
local controls = "[Page Up] previous, [Page Down] next, [Home] select folder, [O] options, [Esc] exit."

-- pattern list
local patterns = {}           -- Array of patterns
local numpatterns = 0         -- Number of patterns
local numsubs = 0             -- Number of subdirectories
local whichpattern = 1        -- Current pattern index

-- settings are saved in this file
local settingsfile = g.getdir("data").."browse-patterns.ini"

-- gui buttons
local prevbutton      -- Previous button
local nextbutton      -- Next button
local folderbutton    -- Folder button
local viewerbutton    -- Viewer button
local exitbutton      -- Exit button
local helpbutton      -- Help button
local startcheck      -- AutoStart checkbox
local fitcheck        -- AutoFit checkbox
local speedslider     -- AutoAdvance speed slider
local optionsbutton   -- Options button
local subdircheck     -- Include subdirectories checkbox
local keepspeedcheck  -- Keep speed checkbox
local loopcheck       -- Loop checkbox
local infocheck       -- Show Info checkbox

-- position
local guiht        = 32          -- height of toolbar
local guiwd        = 0           -- computed width of toolbar (from control widths)
local optht        = 0           -- height of options panel
local gapx         = 10          -- horitzonal gap between controls
local gapy         = 4           -- vertical gap between controls
local maxsliderval = 22          -- maxmimum slider value
local sliderpower  = 1.32        -- slider power
local textrect     = "textrect"  -- clip name for clipping info text

-- style
local toolbarbgcolor = "0 0 0 192"

-- flags
local loadnew      = false  -- whether to load a new pattern
local exitnow      = false  -- whether to exit
local showoptions  = false  -- whether options are displayed
local refreshgui   = true   -- whether the gui needs to be refreshed

-- settings
local autostart    = 1   -- whether to autostart playback on pattern load
local autofit      = 1   -- whether to switch on autofit on pattern load
local keepspeed    = 0   -- whether to maintain speed between patterns
local subdirs      = 1   -- whether to include subdirectories
local looping      = 1   -- whether to loop pattern list
local advancespeed = 10  -- advance speed (0 for manual)
local showinfo     = 1   -- show info if present
local firsttime    = 0   -- whether first time help has been displayed

local currentstep     = g.getstep()  -- current step
local patternloadtime = 0            -- pattern load time (ms)

-- pattern information
local infonum     = 0       -- number of info clips
local infowd      = {}      -- width of pattern info clips
local infoht      = {}      -- height of pattern info clips
local infox       = 0       -- info text x coordinate
local infoclip    = "info"  -- info clip prefix name
local infotime    = 0       -- time last information frame drawn
local infochunk   = 256     -- number of characters per info clip
local infowdtotal = 0       -- total width of info clips
local infospeed   = 9       -- scrolling speed (ms to move one pixel)

-- file extensions to load
local matchlist = { rle = true, mcl = true, mc = true, lif = true, gz = true }

-- remaining time before auto advance
local remaintime = 0

-- temporary file path
local temppath = g.getdir("temp")

--------------------------------------------------------------------------------

local function setfont()
    op.textfont = "font 10 default-bold"
    if g.os() == "Mac" then
        op.yoffset = -1
    end
    if g.os() == "Linux" then
        op.textfont = "font 10 default"
    end
    ov(op.textfont)
end

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
        f:write(tostring(showinfo).."\n")
        f:write(tostring(firsttime).."\n")
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
        showinfo     = tonumber(f:read("*l")) or 0
        firsttime    = tonumber(f:read("*l")) or 0
        f:close()
    end
end

--------------------------------------------------------------------------------

local function findpatterns(dir)
    g.show("Searching for patterns in "..dir)
    g.update()
    local files = g.getfiles(dir)
    for _, name in ipairs(files) do
        if name:sub(1,1) == "." then
            -- ignore hidden files (like .DS_Store on Mac)
        elseif name:sub(-1) == pathsep then
            -- name is a subdirectory
            numsubs = numsubs + 1
            if subdirs == 1 then
                findpatterns(dir..name)
            end
        else
            -- check the file is the right type to display
            local index = name:match'^.*()%.'
            if index then
                if matchlist[name:sub(index+1):lower()] then
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
    local result = true

    -- save current list
    local currentpatterns = numpatterns

    -- search for patterns in the specified folder
    numpatterns = 0
    numsubs = 0
    findpatterns(dir)
    if numpatterns == 0 then
        if numsubs == 0 or subdirs == 1 then
            g.note("No patterns found in:\n\n"..dir)
        else
            g.note("Only subdirectories found in:\n\n"..dir.."\n\nInclude subdirectories option not selected.")
        end
        -- restore current list
        numpatterns = currentpatterns
        result = false
    end

    return result
end

--------------------------------------------------------------------------------

local function getremainingtime()
    local time = sliderpower ^ (maxsliderval - advancespeed)
    local remain = remaintime / 1000
    return time, remain
end

--------------------------------------------------------------------------------

local function updatenextbutton()
    if advancespeed > 0 then
        local time, remain = getremainingtime()
        local clipname = "nextcopy"
        local x = nextbutton.x
        local y = nextbutton.y
        local ht = nextbutton.ht
        local wd = floor(nextbutton.wd * ((time - remain) / time))
        if wd > 0 then
            if wd > nextbutton.wd then
                wd = nextbutton.wd
            end
            -- shade a proportion of the next button in green
            ov("copy "..x.." "..y.." "..wd.." "..ht.." "..clipname)
            ov("target "..clipname)
            ov("rgba 40 192 0 255")
            ov("replace 40 128 255 255")
            ov("target")
            ov("blend 0")
            ovt{"paste", x, y, clipname}
        end
    end
end

--------------------------------------------------------------------------------

local function drawspeed(x, y)
    -- convert speed into labael
    local message = "Manual"
    if advancespeed > 0 then
        -- convert the slider position into minutes and seconds
        local time, remain = getremainingtime()
        if time < 60 then
            if time < 10 then
                message = string.format("%.1f", time).."s"
            else
                message = floor(time).."s"
            end
        else
            message = floor(time / 60).."m"..floor(time % 60).."s"
        end

        -- convert remaining ms into minutes and seconds
        message = message.."  (next in "
        if remain >= 60 then
            message = message..floor(remain / 60).."m"..floor(remain % 60).."s"
        else
            if remain < 10 then
                remain = floor(remaintime / 100) / 10
                if remain < 0 then remain = 0 end
                message = message..string.format("%.1f", remain).."s"
            else
                if remain < 0 then remain = 0 end
                message = message..floor(remain).."s"
            end
        end
        message = message..")"
    end

    -- update the label
    if showoptions then
        op.maketext(message, "label", op.white, 2, 2, op.black)
        ov("blend 1")
        op.pastetext(x, y, op.identity, "label")
        ov("blend 0")
    end
end

--------------------------------------------------------------------------------

local function drawinfo()
    -- move the first option page control down since it won't have moved if show info
    -- in on and the first viewed pattern didn't have any pattern comments
    startcheck.y = guiht + guiht + gapy

    -- compute how far to move the text based on time since last update
    local now = g.millisecs()
    local dx = now - infotime
    infotime = now
    infox = infox - dx / infospeed
    if infox < -infowdtotal then
        infox = guiwd
    end

    -- draw the info text
    ov("target "..textrect)
    ov("blend 0")
    ov("rgba "..toolbarbgcolor)
    ovt{"fill"}
    ov("blend 1")
    local x = floor(infox)
    local y = infoht[1]
    for i = 1, infonum do
        op.pastetext(x, floor((guiht - y) / 2), op.identity, infoclip..i)
        x = x + infowd[i] - 2  -- make subsequent clips overlap slightly to remove text gap
    end
    ov("target")
    ov("blend 0")
    ovt{"paste", 0, guiht, textrect}
end

--------------------------------------------------------------------------------

local function drawgui()
    -- check whether to draw info
    if showinfo == 1 and infowdtotal ~= 0 then
        drawinfo()
    end

    -- check if the gui needs to be refreshed
    if refreshgui or (showoptions and advancespeed > 0) then
        -- mark gui as refreshed
        refreshgui = false

        -- draw toolbar background
        ov("blend 0")
        ov("rgba "..toolbarbgcolor)
        ovt{"fill", 0, 0, guiwd, guiht}

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
        viewerbutton.show(x, y)
        x = x + viewerbutton.wd + gapx
        helpbutton.show(x, y)
        x = x + helpbutton.wd + gapx
        exitbutton.show(x, y)

        -- compute offset for options if info is displayed
        local infoy = 0
        if showinfo == 1 and infowdtotal ~= 0 then
            infoy = guiht
        end

        -- check for options
        if showoptions then
            -- draw options background
            ovt{"fill", 0, (guiht + infoy), guiwd, optht}
            ov("rgba 0 0 0 0")
            ovt{"fill", 0, (guiht + infoy + optht), guiwd, infoy}

            -- draw option controls
            x = gapx
            y = guiht + gapy + infoy
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
            y = y + fitcheck.ht + gapy
            if showinfo == 1 then
                infocheck.show(x, y, true)
            else
                infocheck.show(x, y, false)
            end
            y = y + infocheck.ht + gapy
            if subdirs == 1 then
                subdircheck.show(x, y, true)
            else
                subdircheck.show(x, y, false)
            end
            y = y + subdircheck.ht + gapy
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
        else
            -- not showing options so clear the area under the toolbar
            local oldrgba = ov("rgba 0 0 0 0")
            ovt{"fill", 0, (guiht + infoy), guiwd, optht}
            ov("rgba "..oldrgba)
        end
    end

    -- update the next button
    updatenextbutton()
end

--------------------------------------------------------------------------------

local function showhelp()
    local helptext =
[[
Pattern Browser

Overview:

The Pattern Browser displays a slideshow of patterns in the current pattern's folder.

You can change folder, include subfolders, adjust slideshow speed and other settings
with the controls and options below.

Controls:

Button    Key        Function
Next      Page Down  go to next pattern.
Previous  Page Up    go to previous pattern.
Folder    Home       choose a new folder to browse.
Options   O          toggle options panel to set slideshow speed and other settings.
Viewer    V          view the current pattern in LifeViewer.
?         ?          display this help.
X         Esc        exit Pattern Browser.
          Ctrl-I     toggle scrolling pattern information.
          Ctrl-T     toggle autofit during playback.

                          (click or hit any key to close help) ]]

    -- draw help text
    ov("font 11 mono-bold")
    ov(op.black)
    local w, h = gp.split(ov("text temp "..helptext))
    w = tonumber(w) + 20
    h = tonumber(h) + 20
    local x = floor((viewwd - w) / 2)
    local y = floor((viewht - h) / 2)
    ov(op.gray)
    ovt{"fill", x, y, w, h}
    ov("rgba 255 253 217 255") -- pale yellow (matches Help window)
    ovt{"fill", (x+2), (y+2), (w-4), (h-4)}
    local oldblend = ov("blend 1")
    ovt{"paste", (x+10), (y+10), "temp"}
    ov("blend "..oldblend)
    ov("update")


    -- save current time
    local now = g.millisecs()

    -- wait for a key or click
    while true do
        local event = g.getevent()
        if event:find("^key") or event:find("^oclick") then
            break
        end
    end

    -- clear help
    ov("rgba 0 0 0 0")
    ov("blend 0")
    ovt{"fill", x, y, w, h}
    refreshgui = true

    -- reset font
    setfont()

    -- ensure scrolling info continues from where it paused
    infotime = g.millisecs()

    -- adjust pattern load time
    patternloadtime = patternloadtime + (g.millisecs() - now)
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
    refreshgui = true
end

--------------------------------------------------------------------------------

local function showinviewer()
    gpl.launch()
end

--------------------------------------------------------------------------------

local function selectfolder()
    -- remove the filename from the supplied path
    local current = patterns[whichpattern]
    local dirname = current:sub(1, current:find(pathsep.."[^"..pathsep.."/]*$"))

    -- ask for a folder
    dirname = g.opendialog("Choose a folder", "dir", dirname)
    if dirname ~= "" then
        if getpatternlist(dirname) then
            whichpattern = 1
            loadnew = true
        else
            g.show("Pattern "..whichpattern.." of "..numpatterns..". "..controls)
        end
    end
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
    if showoptions then
        optionsbutton.setlabel("Close", false)
    else
        optionsbutton.setlabel("Options", false)
    end
    refreshgui = true
end

--------------------------------------------------------------------------------

local function toggleinfo()
    showinfo = 1 - showinfo
    savesettings()
    refreshgui = true
    drawgui()

    -- need to refresh again since options panel will move if open
    refreshgui = true
end

--------------------------------------------------------------------------------

local function updatespeed(newval)
    advancespeed = newval
    savesettings()
    patternloadtime = g.millisecs()
    local targettime = (sliderpower ^ (maxsliderval - advancespeed)) * 1000
    remaintime = targettime - (g.millisecs() - patternloadtime)
    refreshgui = true
    drawgui()
end

--------------------------------------------------------------------------------

local function createoverlay()
    -- create overlay for gui
    ov("create "..viewwd.." "..viewht)
    overlaycreated = true
    op.buttonht = 20
    op.textgap = 8

    -- set font
    setfont()

    -- create gui buttons
    op.textshadowx = 2
    op.textshadowy = 2
    prevbutton = op.button("Previous", previouspattern)
    nextbutton = op.button("Next", nextpattern)
    exitbutton = op.button("X", exitbrowser)
    helpbutton = op.button("?", showhelp)
    folderbutton = op.button("Folder", selectfolder)
    viewerbutton = op.button("Viewer", showinviewer)
    startcheck = op.checkbox("Start playback on pattern load", op.white, toggleautostart)
    fitcheck = op.checkbox("Fit pattern to display", op.white, toggleautofit)
    speedslider = op.slider("Advance: ", op.white, 81, 0, maxsliderval, updatespeed)
    optionsbutton = op.button("Options", toggleoptions)
    subdircheck = op.checkbox("Include subdirectories", op.white, togglesubdirs)
    keepspeedcheck = op.checkbox("Maintain step speed across patterns", op.white, togglespeed)
    loopcheck = op.checkbox("Loop pattern list", op.white, toggleloop)
    infocheck = op.checkbox("Show pattern information", op.white, toggleinfo)

    -- get the size of the gui controls
    optht = startcheck.ht + fitcheck.ht + infocheck.ht + subdircheck.ht + speedslider.ht
    optht = optht + loopcheck.ht + keepspeedcheck.ht + 8 * gapy
    guiwd = prevbutton.wd + nextbutton.wd + folderbutton.wd + viewerbutton.wd + optionsbutton.wd + helpbutton.wd + exitbutton.wd + 8 * gapx

    -- create the clip for clipping info text
    ov("create "..guiwd.." "..guiht.." "..textrect)

    -- draw the overlay
    drawgui()
end

--------------------------------------------------------------------------------

local function loadinfo()
     -- clear pattern info
     infowdtotal = 0
     refreshgui = true
     local lvcommands = false

     -- load pattern info
     local info = g.getinfo()
     if info ~= "" then
         -- format into a single line
         local clean = info:gsub("#CXRLE.-\n", ""):gsub("#[CDNO ]? *", ""):gsub(" +", " "):gsub(" *\n", " ")
         if clean ~= "" then
             -- check for LifeViewer comments
             if clean:find(" %[%[ ") then
                lvcommands = true
                clean = clean:gsub(" %[%[.*%]%] ", "")
             end

             -- split into a number of clips due to bitmap width limits
             infonum = floor((clean:len() - 1) / infochunk) + 1
             ov("blend 0")

             -- create the text clips
             for i = 1, infonum do
                 local i1 = (i - 1) * infochunk + 1
                 local i2 = i1 + infochunk - 1
                 infowd[i], infoht[i] = op.maketext(clean:sub(i1, i2), infoclip..i, op.white, 2, 2, op.black)
                 infowdtotal = infowdtotal + infowd[i]
             end
             infox = guiwd
         end
     end

     -- set viewer button colour based on script commands being present
     if lvcommands then
        viewerbutton.customcolor = "rgba 40 128 128 255"
     else
        viewerbutton.customcolor = nil
     end
     viewerbutton.setlabel("Viewer", false)
     infotime = g.millisecs()
end

--------------------------------------------------------------------------------

local function checkforresize()
    local newwd, newht = g.getview(g.getlayer())
    if newwd ~= viewwd or newht ~= viewht then
        -- hide controls so show draws correct background
        prevbutton.hide()
        nextbutton.hide()
        viewerbutton.hide()
        folderbutton.hide()
        exitbutton.hide()
        helpbutton.hide()
        startcheck.hide()
        fitcheck.hide()
        speedslider.hide()
        optionsbutton.hide()
        subdircheck.hide()
        keepspeedcheck.hide()
        loopcheck.hide()
        infocheck.hide()

        -- resize overlay
        if newwd < 1 then newwd = 1 end
        if newht < 1 then newht = 1 end
        viewwd = newwd
        viewht = newht
        ov("resize "..viewwd.." "..viewht)
        refreshgui = true
    end
end

--------------------------------------------------------------------------------

local function browsepatterns(startpattern)
    local generating
    local now = g.millisecs()
    local target = 15
    local delay

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
        loadinfo()
        g.show("Pattern "..whichpattern.." of "..numpatterns..". "..controls)
        g.update()
        generating = autostart

        -- reset pattern load time
        patternloadtime = g.millisecs()

        -- restore playback speed if requested
        if keepspeed == 1 then
            g.setstep(currentstep)
        end

        -- display first time help if needed
        if firsttime == 0 then
            firsttime = 1
            savesettings()
            showhelp()
        end

        -- decode key presses
        loadnew = false
        while not loadnew do
            -- check for window reisze
            checkforresize()

            -- check for event
            local event = op.process(g.getevent())

            -- process key events
            if event == "key pagedown none" then
                nextpattern()
            elseif event == "key pageup none" then
                previouspattern()
            elseif event == "key home none" then
                selectfolder()
            elseif event == "key return none" then
                generating = 1 - generating
            elseif event == "key o none" then
                toggleoptions()
            elseif event == "key i ctrl" then
                toggleinfo()
            elseif event == "key t ctrl" then
                toggleautofit()
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
            elseif event == "key ? none" then
                showhelp()
            elseif event == "key v none" then
                showinviewer()
            else
               -- pass event to Golly for processing
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

                -- check for autofit when running
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

local function browse()
    -- try to get the current open pattern folder
    local pathname = g.getpath()
    local dirname

    -- check for no saved pattern or a pattern from the clipboard
    if pathname == "" or pathname:sub(1, pathname:find(pathsep.."[^"..pathsep.."/]*$")) == temppath then
        -- ask for a folder
        dirname = g.opendialog("Choose a folder", "dir", g.getdir("app"))
    else
        -- remove the file name
        dirname = pathname:sub(1, pathname:find(pathsep.."[^"..pathsep.."/]*$"))
    end
    if dirname ~= "" then
        loadsettings()
        if getpatternlist(dirname) then
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
g.check(false)
g.show("")
if overlaycreated then ov("delete") end
