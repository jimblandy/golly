-- This module is loaded if a script calls require "oplus".
-- It provides a high-level interface to the overlay commands.

local g = golly()
-- require "gplus.strict"
local ov = g.overlay

local gp = require "gplus"
local int = gp.int
local rect = gp.rect
local split = gp.split

local m = {}

--------------------------------------------------------------------------------

-- scripts can adjust these parameters:

m.buttonht = 24     -- height of buttons (also used for check boxes and sliders)
m.sliderwd = 16     -- width of slider button (best if even)
m.checkgap = 5      -- gap between check box button and its label
m.textgap = 10      -- gap between edge of button and its label

m.normalrgb = "rgba 40 128 255"         -- light blue buttons (alpha will be appended)
m.darkerrgb = "rgba 20 64 255"          -- darker blue when buttons are clicked
m.textrgba = "rgba 255 255 255 255"     -- white button labels and tick mark on check box
m.textfont = "font 12 default-bold"     -- font for labels
m.yoffset = -1                          -- for better y position of labels

if g.os() == "Linux" then
    m.yoffset = 0
    m.textfont = "font 12 default"
end

--------------------------------------------------------------------------------

-- synonyms for some common overlay commands:

-- opaque colors
m.white     = "rgba 255 255 255 255"
m.gray      = "rgba 128 128 128 255"
m.black     = "rgba 0 0 0 255"
m.red       = "rgba 255 0 0 255"
m.green     = "rgba 0 255 0 255"
m.blue      = "rgba 0 0 255 255"
m.cyan      = "rgba 0 255 255 255"
m.magenta   = "rgba 255 0 255 255"
m.yellow    = "rgba 255 255 0 255"

-- affine transformations
m.identity     = "transform  1  0  0  1"
m.flip         = "transform -1  0  0 -1"
m.flip_x       = "transform -1  0  0  1"
m.flip_y       = "transform  1  0  0 -1"
m.swap_xy      = "transform  0  1  1  0"
m.swap_xy_flip = "transform  0 -1 -1  0"
m.rcw          = "transform  0 -1  1  0"
m.rccw         = "transform  0  1 -1  0"
m.racw         = m.rccw
m.r180         = m.flip

-- themes
m.theme0 = "theme 255 255 255 255 255 255 0 0 0 0 0 0 0 0 0"
m.theme1 = "theme 0 255 255 255 255 255 0 0 255 0 0 47 0 0 0"
m.theme2 = "theme 255 144 0 255 255 0 160 0 0 32 0 0 0 0 0"
m.theme3 = "theme 0 255 255 255 255 255 0 128 0 0 24 0 0 0 0"
m.theme4 = "theme 255 255 0 255 255 255 128 0 128 0 47 0 0 32 128"
m.theme5 = "theme 176 176 176 255 255 255 104 104 104 16 16 16 0 0 0"
m.theme6 = "theme 0 0 0 0 0 0 255 255 255 255 255 255 255 255 255"
m.theme7 = "theme 0 0 255 0 0 0 0 255 255 240 240 240 255 255 255"
m.theme8 = "theme 240 240 240 240 240 240 240 240 240 240 240 240 0 0 0"
m.theme9 = "theme 240 240 240 240 240 240 160 0 0 160 0 0 0 0 0"
m.themes = {
    [-1] = "theme -1",
    [0] = m.theme0,
    [1] = m.theme1,
    [2] = m.theme2,
    [3] = m.theme3,
    [4] = m.theme4,
    [5] = m.theme5,
    [6] = m.theme6,
    [7] = m.theme7,
    [8] = m.theme8,
    [9] = m.theme9
}

--------------------------------------------------------------------------------

local darken_button = false     -- tell draw_button to use m.darkerrgb
local darken_slider = false     -- tell draw_slider to darken the slider button

local button_tables = {}        -- for detecting click in a button
local checkbox_tables = {}      -- for detecting click in a check box
local slider_tables = {}        -- for detecting click in a slider

--------------------------------------------------------------------------------

local function draw_button(x, y, w, h)
    local oldblend = ov("blend 0")
    local buttrect = " "..x.." "..y.." "..w.." "..h
    
    -- copy rect under button to temp_bg
    ov("copy"..buttrect.." temp_bg")
    
    -- clear rect under button
    local oldrgba = ov("rgba 0 0 0 0")
    ov("fill"..buttrect)
    
    local buttonrgb = m.normalrgb
    if darken_button then
        buttonrgb = m.darkerrgb
    end
    
    -- draw button with rounded corners
    ov(buttonrgb.." 255") -- opaque
    ov("fill"..buttrect)
    
    -- draw one rounded corner then copy and paste with flips to draw the rest
    ov("rgba 0 0 0 0")
    ov("set "..x.." "..y)
    ov(buttonrgb.." 48")
    ov("set "..(x+1).." "..y)
    ov("set "..x.." "..(y+1))
    ov(buttonrgb.." 128")
    ov("set "..(x+2).." "..y)
    ov("set "..x.." "..(y+2))
    ov(buttonrgb.." 200")
    ov("set "..(x+1).." "..(y+1))
    ov("copy "..x.." "..y.." 3 3 temp_corner")
    ov(m.flip_x)
    ov("paste "..(x+w-1).." "..y.." temp_corner")
    ov(m.flip_y)
    ov("paste "..x.." "..(y+h-1).." temp_corner")
    ov(m.flip)
    ov("paste "..(x+w-1).." "..(y+h-1).." temp_corner")
    ov(m.identity)
    
    -- copy rect to temp_button
    ov("copy"..buttrect.." temp_button")
    
    -- paste temp_bg back to rect
    ov("paste "..x.." "..y.." ".." temp_bg")
    
    -- turn on blending and paste temp_button
    ov("blend 1")
    ov("paste "..x.." "..y.." ".." temp_button")
    
    ov("rgba "..oldrgba)
    ov("blend "..oldblend)
end

--------------------------------------------------------------------------------

function m.button(label, onclick)
    -- return a table that makes it easier to create and use buttons
    local b = {}
    
    if type(label) ~= "string" then
    	error("1st arg of button must be a string", 2)
    end
    if type(onclick) ~= "function" then
    	error("2nd arg of button must be a function", 2)
    end
    
    b.onclick = onclick     -- remember click handler
    b.shown = false         -- b.show hasn't been called
    b.ht = m.buttonht;

	b.setlabel = function (newlabel, changesize)
        local oldrgba = ov(m.textrgba)
        local oldfont = ov(m.textfont)
        local w, h = split(ov("text "..b.clipname.." "..newlabel))
        ov("font "..oldfont)
        ov("rgba "..oldrgba)
        b.labelwd = tonumber(w);
        b.labelht = tonumber(h);
        if changesize then
            -- use label size to set button width
            b.wd = b.labelwd + 2*m.textgap;
        end
	end
    
    -- create text for label with a unique clip name
    b.clipname = tostring(b).."+button"
    b.clipname = string.gsub(b.clipname, " ", "")   -- remove any spaces
    b.setlabel(label, true)

	b.show = function (x, y)
	    b.x = x
	    b.y = y
	    if b.shown then
	        -- remove old button in case it is moving to new position
	        b.hide()
	    end
	    -- remember position and save background pixels
	    b.background = b.clipname.."+bg"
	    ov("copy "..b.x.." "..b.y.." "..b.wd.." "..b.ht.." "..b.background)
        -- draw the button at the given location
        draw_button(x, y, b.wd, b.ht)
        -- draw the label
        local oldblend = ov("blend 1")
        x = int(x + (b.wd - b.labelwd) / 2)
        y = int(y + m.yoffset + (b.ht - b.labelht) / 2)
        ov("paste "..x.." "..y.." "..b.clipname)
        ov("blend "..oldblend)
        
        -- store this table using the button's rectangle as key
        b.rect = rect({b.x, b.y, b.wd, b.ht})
        button_tables[b.rect] = b
        b.shown = true
	end

	b.hide = function ()
	    if b.shown then
            -- restore background pixels saved in b.show
            local oldblend = ov("blend 0")
            ov("paste "..b.x.." "..b.y.." "..b.background)
            ov("blend "..oldblend)
            
            -- remove the table entry
            button_tables[b.rect] = nil
            b.shown = false
        end
	end

    b.refresh = function ()
        -- redraw button
        b.show(b.x, b.y)
        g.update()
    end

	return b
end

--------------------------------------------------------------------------------

local function draw_checkbox(x, y, w, h, ticked)
    draw_button(x, y, w, h)
    if ticked then
        -- draw a tick mark (needs improvement!!!)
        local oldrgba = ov(m.textrgba)
        ov("line "..int(x+w/2).." "..(y+h-5).." "..(x+6).." "..(y+h-8))
        ov("line "..int(x+w/2).." "..(y+h-5).." "..(x+w-6).." "..(y+5))
        ov("rgba 255 255 255 128")
        local oldblend = ov("blend 1")
        ov("line "..int(x+w/2).." "..(y+h-6).." "..(x+6).." "..(y+h-9))
        ov("line "..int(x+w/2).." "..(y+h-6).." "..(x+6).." "..(y+h-10))
        ov("line "..int(x+w/2-1).." "..(y+h-5).." "..(x+w-7).." "..(y+5))
        ov("line "..int(x+w/2+1).." "..(y+h-6).." "..(x+w-5).." "..(y+5))
        ov("rgba "..oldrgba)
        ov("blend "..oldblend)
    end
end

--------------------------------------------------------------------------------

function m.checkbox(label, labelrgba, onclick)
    -- return a table that makes it easier to create and use check boxes
    local c = {}
    
    if type(label) ~= "string" then
    	error("1st arg of checkbox must be a string", 2)
    end
    if type(labelrgba) ~= "string" then
    	error("2nd arg of checkbox must be a string", 2)
    end
    if type(onclick) ~= "function" then
    	error("3rd arg of checkbox must be a function", 2)
    end
    
    -- create text for label with a unique clip name
    c.clipname = tostring(c).."+checkbox"
    c.clipname = string.gsub(c.clipname, " ", "")   -- remove any spaces
    local oldrgba = ov(labelrgba)
    local oldfont = ov(m.textfont)
    local w, h = split(ov("text "..c.clipname.." "..label))
    ov("font "..oldfont)
    ov("rgba "..oldrgba)
    
    -- use label size to set check box size
    c.labelwd = tonumber(w);
    c.labelht = tonumber(h);
    c.wd = m.buttonht + m.checkgap + c.labelwd;
    c.ht = m.buttonht;
    
    c.onclick = onclick     -- remember click handler
    c.shown = false         -- c.show hasn't been called

	c.show = function (x, y, ticked)
	    c.x = x
	    c.y = y
	    c.ticked = ticked
	    if c.shown then
	        -- remove old check box in case it is moving to new position
	        c.hide()
	    end
	    -- remember position and save background pixels
	    c.background = c.clipname.."+bg"
	    ov("copy "..c.x.." "..c.y.." "..c.wd.." "..c.ht.." "..c.background)
        -- draw the check box (excluding label) at the given location
        draw_checkbox(x+1, y+1, c.ht-2, c.ht-2, ticked)
        -- draw the label
        local oldblend = ov("blend 1")
        ov("paste "..(x+c.ht+m.checkgap).." "..int(y+m.yoffset+(c.ht-c.labelht)/2).." "..c.clipname)
        ov("blend "..oldblend)
        -- store this table using the check box's rectangle as key
        c.rect = rect({c.x, c.y, c.wd, c.ht})
        checkbox_tables[c.rect] = c
        c.shown = true
	end

	c.hide = function ()
	    if c.shown then
            -- restore background pixels saved in c.show
            local oldblend = ov("blend 0")
            ov("paste "..c.x.." "..c.y.." "..c.background)
            ov("blend "..oldblend)
            -- remove the table entry
            checkbox_tables[c.rect] = nil
            c.shown = false
        end
	end

    c.refresh = function ()
        -- redraw checkbox
        c.show(c.x, c.y, c.ticked)
        g.update()
    end
    
	return c
end

--------------------------------------------------------------------------------

local function draw_slider(s, barpos)
    local x = s.startbar
    local y = s.y
    local w = s.barwidth
    local h = s.ht
    
    -- draw horizontal bar
    local oldrgba = ov("rgba 100 100 100 255")
    local midy = int(y+h/2)
    ov("fill "..x.." "..(midy-3).." "..w.." 6")
    ov("rgba 110 110 110 255")
    ov("fill "..(x+1).." "..(midy-2).." "..(w-2).." 1")
    ov("rgba 120 120 120 255")
    ov("fill "..(x+1).." "..(midy-1).." "..(w-2).." 1")
    ov("rgba 130 130 130 255")
    ov("fill "..(x+1).." "..midy.." "..(w-2).." 1")
    ov("rgba 140 140 140 255")
    ov("fill "..(x+1).." "..(midy+1).." "..(w-2).." 1")
    
    if darken_slider then darken_button = true end
    
    -- draw slider button on top of horizontal bar
    draw_button(x + barpos - int(m.sliderwd/2), y, m.sliderwd, h)
    ov("rgba "..oldrgba)

    if darken_slider then darken_button = false end
    
    -- draw the label
    local oldblend = ov("blend 1")
    ov("paste "..s.x.." "..int(y+m.yoffset+(h-s.labelht)/2).." "..s.clipname)
    ov("blend "..oldblend)
end

--------------------------------------------------------------------------------

function m.slider(label, labelrgba, barwidth, minval, maxval, onclick)
    -- return a table that makes it easier to create and use sliders
    local s = {}
    
    if type(label) ~= "string" then
    	error("1st arg of slider must be a string", 2)
    end
    if type(labelrgba) ~= "string" then
    	error("2nd arg of slider must be a string", 2)
    end
    if type(onclick) ~= "function" then
    	error("6th arg of slider must be a function", 2)
    end
    
    if barwidth <= 0 then
        error("slider width must be > 0", 2)
    end
    if minval >= maxval then
        error("minimum slider value must be < maximum value", 2)
    end

    s.barwidth = barwidth   -- width of the slider bar
    s.minval = minval
    s.maxval = maxval
    
    -- create text for label with a unique clip name
    s.clipname = tostring(s).."+slider"
    s.clipname = string.gsub(s.clipname, " ", "")   -- remove any spaces
    local oldrgba = ov(labelrgba)
    local oldfont = ov(m.textfont)
    local w, h
    if #label == 0 then
        w, h = split(ov("text "..s.clipname.." "..label.." "))
        w = 0
    else
        w, h = split(ov("text "..s.clipname.." "..label))
    end
    ov("font "..oldfont)
    ov("rgba "..oldrgba)
    
    -- set total slider size (including label)
    s.labelwd = tonumber(w);
    s.labelht = tonumber(h);
    s.wd = s.labelwd + m.sliderwd + s.barwidth;
    s.ht = m.buttonht;
    
    s.onclick = onclick     -- remember click handler
    s.shown = false         -- s.show hasn't been called

	s.show = function (x, y, pos)
	    s.pos = int(pos)
	    if s.pos < s.minval then s.pos = s.minval end
	    if s.pos > s.maxval then s.pos = s.maxval end
	    if s.shown then
	        -- remove old slider in case it is moving to new position
	        s.hide()
	    end
	    -- remember slider location and save background pixels
	    s.x = x
	    s.y = y
	    s.background = s.clipname.."+bg"
	    ov("copy "..s.x.." "..s.y.." "..s.wd.." "..s.ht.." "..s.background)
        -- draw the slider and label at the given location
        s.startbar = x + s.labelwd + int(m.sliderwd/2)
        local barpos = int(s.barwidth * (s.pos - s.minval) / (s.maxval - s.minval))
        draw_slider(s, barpos)
        -- store this table using the slider button's rectangle as key
        s.rect = rect({s.startbar+barpos-int(m.sliderwd/2), s.y, m.sliderwd, s.ht})
        slider_tables[s.rect] = s
        s.shown = true
	end

	s.hide = function ()
	    if s.shown then
            -- restore background pixels saved in previous s.show
            local oldblend = ov("blend 0")
            ov("paste "..s.x.." "..s.y.." "..s.background)
            ov("blend "..oldblend)
            -- remove the table entry
            slider_tables[s.rect] = nil
            s.shown = false
        end
	end

    s.refresh = function ()
        -- redraw slider
        s.show(s.x, s.y, s.pos)
        g.update()
    end

	return s
end

--------------------------------------------------------------------------------

local function release_in_rect(r, t)
    -- return true if mouse button is released while in given rect
    local inrect = true
    
    -- draw darkened button
    darken_button = true
    t.refresh()
    
    while true do
        local event = g.getevent()
        if event:find("^mup left") then break end
        local xy = ov("xy")
        local wasinrect = inrect
        if #xy == 0 then
            inrect = false      -- mouse is outside overlay
        else
            local x, y = split(xy)
            x = tonumber(x)
            y = tonumber(y)
            inrect = x >= r.left and x <= r.right and y >= r.top and y <= r.bottom
        end
        if inrect ~= wasinrect then
            -- mouse has moved in/out of r
            darken_button = inrect
            t.refresh()
        end
    end
    
    if inrect then
        -- undarken button
        darken_button = false
        t.refresh()
    end
    
    return inrect
end

--------------------------------------------------------------------------------

local function click_in_button(x, y)
    for r, button in pairs(button_tables) do
        if x >= r.left and x <= r.right and y >= r.top and y <= r.bottom then
            if release_in_rect(r, button) then
                -- call this button's handler
                button.onclick()
            end
            return true
        end
    end
    return false
end

--------------------------------------------------------------------------------

local function click_in_checkbox(x, y)
    for r, checkbox in pairs(checkbox_tables) do
        if x >= r.left and x <= r.right and y >= r.top and y <= r.bottom then
            if release_in_rect(r, checkbox) then
                checkbox.show(checkbox.x, checkbox.y, not checkbox.ticked)
                -- call this checkbox's handler
                checkbox.onclick()
            end
            return true
        end
    end
    return false
end

--------------------------------------------------------------------------------

local function click_in_slider(x, y)
    for r, slider in pairs(slider_tables) do
        if x >= r.left and x <= r.right and y >= r.top and y <= r.bottom then
            -- draw darkened slider button
            darken_slider = true
            slider.show(slider.x, slider.y, slider.pos)
            g.update()
            
            local prevx = x
            local range = slider.maxval - slider.minval
            local maxx = slider.startbar + slider.barwidth
            
            -- track horizontal movement of mouse until button is released
            while true do
                local event = g.getevent()
                if event:find("^mup left") then break end
                local xy = ov("xy")
                if #xy > 0 then
                    local x, _ = split(xy)
                    -- check if slider position needs to change
                    x = tonumber(x)
                    if x < slider.startbar then x = slider.startbar end
                    if x > maxx then x = maxx end
                    
                    if x ~= prevx then
                        -- draw new position of slider button immediately;
                        -- first restore background pixels saved in previous slider.show
                        local oldblend = ov("blend 0")
                        ov("paste "..slider.x.." "..slider.y.." "..slider.background)
                        ov("blend "..oldblend)
                        draw_slider(slider, x - slider.startbar)
                        g.update()
                        
                        -- now check if slider value has to change
                        local newpos = int(slider.minval + range * (x - slider.startbar) / slider.barwidth)
                        if newpos < slider.minval then newpos = slider.minval end
                        if newpos > slider.maxval then newpos = slider.maxval end
                        if slider.pos ~= newpos then
                            slider.pos = newpos
                            -- call this slider's handler
                            slider.onclick(newpos)
                        end
                        
                        prevx = x
                    end
                end
            end
            
            -- undarken slider button
            darken_slider = false
            slider.show(slider.x, slider.y, slider.pos)
            g.update()
            return true
        end
    end
    return false
end

--------------------------------------------------------------------------------

function m.process(event)
    if #event > 0 then
        if event:find("^oclick") then
            local _, x, y, butt, mods = split(event)
            if butt == "left" and mods == "none" then
                if click_in_button(tonumber(x), tonumber(y)) then return "" end
                if click_in_checkbox(tonumber(x), tonumber(y)) then return "" end
                if click_in_slider(tonumber(x), tonumber(y)) then return "" end
            end
        end
    end
    return event
end

--------------------------------------------------------------------------------

function m.hexrule()
    -- return true if the current rule uses a hexagonal neighborhood
    local rule = g.getrule()
    rule = rule:match("^(.+):") or rule     -- remove any ":*" suffix
    
    local algo = g.getalgo()
    if algo == "QuickLife" or algo == "HashLife" or algo == "Generations" then
        return rule:sub(-1) == "H"
    elseif algo == "RuleLoader" then
        return rule:lower():find("hex") ~= nil
        -- !!! or maybe look in the .rule file and see if the TABLE section specifies
        -- neighborhood:hexagonal or the ICONS section specifies hexagons
    end
    
    return false
end

--------------------------------------------------------------------------------

local clip_too_big = "minbox clip does not fit in overlay"

function m.minbox(clipname, wd, ht)
    -- find the minimal bounding box of non-transparent pixels in given clip
    
    local xmin, ymin, xmax, ymax, minwd, minht

    -- ensure clip name used in here is not the same as clipname
    local oldoverlay = clipname.."+oldoverlay"
    
    -- copy entire overlay and fill it with transparent pixels
    ov("copy 0 0 0 0 "..oldoverlay)
    local oldrgba = ov("rgba 0 0 0 0")
    ov("fill")
    ov("rgba "..oldrgba)
    
    -- paste given clip into the top left corner
    local oldtransform = ov(m.identity)
    local oldblend = ov("blend 0")
    ov("paste 0 0 "..clipname)
    
    -- find the top edge (ymin)
    for row = 0, ht-1 do
        for col = 0, wd-1 do
            local rgba = ov("get "..col.." "..row)
            if rgba == "" then
                error(clip_too_big, 2)
            else
                local _, _, _, a = split(rgba)
                if a ~= "0" then
                    ymin = row
                    goto found_top
                end
            end
        end
    end
    
    -- only get here if clip has no non-transparent pixels
    xmin = 0
    ymin = 0
    minwd = 0
    minht = 0
    goto finish
    
    ::found_top::
    -- get here if clip has at least one non-transparent pixel
    
    -- find the bottom edge (ymax)
    for row = ht-1, ymin, -1 do
        for col = 0, wd-1 do
            local rgba = ov("get "..col.." "..row)
            if rgba == "" then
                error(clip_too_big, 2)
            else
                local _, _, _, a = split(rgba)
                if a ~= "0" then
                    ymax = row
                    goto found_bottom
                end
            end
        end
    end
    ::found_bottom::
    
    -- find the left edge (xmin)
    for col = 0, wd-1 do
        for row = ymin, ymax do
            local rgba = ov("get "..col.." "..row)
            if rgba == "" then
                error(clip_too_big, 2)
            else
                local _, _, _, a = split(rgba)
                if a ~= "0" then
                    xmin = col
                    goto found_left
                end
            end
        end
    end
    ::found_left::
    
    -- find the right edge (xmax)
    for col = wd-1, xmin, -1 do
        for row = ymin, ymax do
            local rgba = ov("get "..col.." "..row)
            if rgba == "" then
                error(clip_too_big, 2)
            else
                local _, _, _, a = split(rgba)
                if a ~= "0" then
                    xmax = col
                    goto found_right
                end
            end
        end
    end
    ::found_right::
    
    -- all edges have been found
    minwd = xmax - xmin + 1
    minht = ymax - ymin + 1
    
    ::finish::
    
    -- restore original overlay, transform and blend state
    ov("paste 0 0 "..oldoverlay)
    ov("transform "..oldtransform)
    ov("blend "..oldblend)
    
    -- free the temporary clip memory
    ov("freeclip "..oldoverlay)
    
    -- return the bounding box info
    return xmin, ymin, minwd, minht
end

--------------------------------------------------------------------------------

return m
