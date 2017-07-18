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

local textclip = "textclip" -- default clip name for m.maketext and m.pastetext

--------------------------------------------------------------------------------

function m.pastetext(x, y, transform, clipname)
    -- set optional parameter defaults
    transform = transform or m.identity
    clipname  = clipname or textclip
    -- apply transform and paste text clip
    local oldtransform = ov(transform)
    ov("paste "..x.." "..y.." "..clipname)
    -- restore settings
    ov("transform "..oldtransform)

    return clipname
end

--------------------------------------------------------------------------------

function m.maketext(s, clipname, textcol, shadowx, shadowy, shadowcol)
    local oldrgba = ov(m.white)
    -- set optional paramter defaults
    clipname  = clipname or textclip
    textcol   = textcol or "rgba "..oldrgba
    shadowx   = shadowx or 0
    shadowy   = shadowy or 0
    shadowcol = shadowcol or m.black
    local w, h, d
    -- check if shadow required
    if shadowx == 0 and shadowy == 0 then
        ov(textcol)
        w, h, d = split(ov("text "..clipname.." "..s))
    else
        -- build shadow clip
        ov(shadowcol)
        local oldbg = ov("textoption background 0 0 0 0")
        local oldblend
        if oldbg == "0 0 0 0" then
            oldblend = ov("blend 1")
        else
            oldblend = ov("blend 0")
            ov("textoption background "..oldbg)
        end
        local tempclip = clipname.."_temp"
        w, h, d  = split(ov("text "..tempclip.." "..s))
        -- compute paste location based on shadow offset
        local tx = 0
        local ty = 0
        local sx = 0
        local sy = 0
        if shadowx < 0 then
            tx = -shadowx
        else
            sx = shadowx
        end
        if shadowy < 0 then
            ty = -shadowy
        else
            sy = shadowy
        end
        -- size result clip to fit text and shadow
        w = tonumber(w) + math.abs(shadowx)
        h = tonumber(h) + math.abs(shadowy)
        ov("create ".." "..w.." "..h.." "..clipname)
        -- paste shadow onto result
        local oldtarget = ov("target "..clipname)
        if oldbg ~= "0 0 0 0" then
            ov("rgba "..oldbg)
            ov("fill")
        end
        m.pastetext(sx, sy, nil, tempclip)
        -- build normal text clip
        ov(textcol)
        if oldbg ~= "0 0 0 0" then
            ov("textoption background 0 0 0 0")
            ov("blend 1")
        end
        ov("text "..tempclip.." "..s)
        -- paste normal onto result
        
        m.pastetext(tx, ty, nil, tempclip)
        -- restore settings
        ov("textoption background "..oldbg)
        ov("delete "..tempclip)
        ov("target "..oldtarget)
        ov("blend "..oldblend)
    end
    -- add index
    ov("optimize "..clipname)

    -- restore color
    ov("rgba "..oldrgba)

    return tonumber(w), tonumber(h), tonumber(d)
end

--------------------------------------------------------------------------------

-- scripts can adjust these parameters:

m.buttonht = 24     -- height of buttons (also used for check boxes and sliders)
m.sliderwd = 16     -- width of slider button (best if even)
m.checkgap = 5      -- gap between check box button and its label
m.textgap = 10      -- gap between edge of button and its label
m.radius = 3        -- curvature of button corners
m.border = 0        -- thickness of button border (no border if 0)

m.buttonrgba = "rgba 40 128 255 255"    -- light blue buttons
m.darkerrgba = "rgba 20 64 255 255"     -- darker blue when buttons are clicked
m.borderrgba = m.white                  -- white border around buttons (if m.border > 0)
m.textrgba = m.white                    -- white button labels and tick mark on check box
m.textfont = "font 12 default-bold"     -- font for labels
m.textshadox = 0                        -- label shadow x offset
m.textshadoy = 0                        -- label shadow y offset
m.textshadowrgba = m.black              -- black label color
m.yoffset = 0                           -- for better y position of labels

if g.os() == "Windows" then
    m.yoffset = -1
end

if g.os() == "Linux" then
    m.textfont = "font 12 default"
end

--------------------------------------------------------------------------------

local darken_button = false     -- tell draw_button to use m.darkerrgba
local darken_slider = false     -- tell draw_slider to darken the slider button

local button_tables = {}        -- for detecting click in a button
local checkbox_tables = {}      -- for detecting click in a check box
local slider_tables = {}        -- for detecting click in a slider

--------------------------------------------------------------------------------

function m.fill_ellipse(x, y, w, h, borderwd, fillrgba)
    -- draw an ellipse with the given border width (if > 0) using the current color
    -- and fill it with the given color (if fillrgba isn't "")

    if borderwd == 0 then
        if #fillrgba > 0 then
            -- just draw solid anti-aliased ellipse using fillrgba
            local oldwidth = ov("lineoption width "..int(math.min(w,h)/2 + 0.5))
            local oldblend = ov("blend 1")
            local oldrgba = ov(fillrgba)
            ov("ellipse "..x.." "..y.." "..w.." "..h)
            ov("rgba "..oldrgba)
            ov("blend "..oldblend)
            ov("lineoption width "..oldwidth)
        end
        return
    end

    if w <= borderwd*2 or h <= borderwd*2 then
        -- no room to fill so just draw anti-aliased ellipse using current color
        local oldwidth = ov("lineoption width "..int(math.min(w,h)/2 + 0.5))
        local oldblend = ov("blend 1")
        ov("ellipse "..x.." "..y.." "..w.." "..h)
        ov("blend "..oldblend)
        ov("lineoption width "..oldwidth)
        return
    end

    local oldblend = ov("blend 1")

    if #fillrgba > 0 then
        -- draw smaller filled ellipse using fillrgba
        local oldrgba = ov(fillrgba)
        local smallw = w - borderwd*2
        local smallh = h - borderwd*2
        local oldwidth = ov("lineoption width "..int(math.min(smallw,smallh)/2 + 0.5))
        ov("ellipse "..(x+borderwd).." "..(y+borderwd).." "..smallw.." "..smallh)
        ov("rgba "..oldrgba)
        ov("lineoption width "..oldwidth)
    end

    -- draw outer ellipse using given borderwd
    local oldwidth = ov("lineoption width "..borderwd)
    ov("ellipse "..x.." "..y.." "..w.." "..h)
    
    -- restore line width and blend state
    ov("lineoption width "..oldwidth)
    ov("blend "..oldblend)
end

--------------------------------------------------------------------------------

function m.round_rect(x, y, w, h, radius, borderwd, fillrgba)
    -- draw a rounded rectangle using the given radius for the corners
    -- with a border in the current color using the given width (if > 0)
    -- and filled with the given color (if fillrgba isn't "")

    if radius == 0 then
        -- draw a non-rounded rectangle (possibly translucent)
        local oldblend = ov("blend 1")
        if borderwd > 0 then
            -- draw border lines using current color
            ov("fill "..x.." "..y.." "..w.." "..borderwd)
            ov("fill "..x.." "..(y+h-borderwd).." "..w.." "..borderwd)
            ov("fill "..x.." "..(y+borderwd).." "..borderwd.." "..(h-borderwd*2))
            ov("fill "..(x+w-borderwd).." "..(y+borderwd).." "..borderwd.." "..(h-borderwd*2))
        end
        if #fillrgba > 0 then
            -- draw interior of rectangle
            local oldrgba = ov(fillrgba)
            ov("fill "..(x+borderwd).." "..(y+borderwd).." "..(w-borderwd*2).." "..(h-borderwd*2))
            ov("rgba "..oldrgba)
        end
        ov("blend "..oldblend)
        return
    end
    
    if radius > w/2 then radius = int(w/2) end
    if radius > h/2 then radius = int(h/2) end

    -- construct rounded rectangle in top left corner of overlay
    ov("copy 0 0 "..w.." "..h.." rectbg")
    local oldrgba = ov("rgba 0 0 0 0")
    local oldblend = ov("blend 0")
    ov("fill 0 0 "..w.." "..h)
    ov("rgba "..oldrgba)
    
    -- create bottom right quarter circle in top left corner of overlay
    m.fill_ellipse(-radius, -radius, radius*2, radius*2, borderwd, fillrgba)
    ov("copy 0 0 "..radius.." "..radius.." qcircle")
    
    -- draw corners
    ov("paste "..(w-radius).." "..(h-radius).." qcircle")
    ov(m.flip_y)
    ov("paste "..(w-radius).." "..(radius-1).." qcircle")
    ov(m.flip_x)
    ov("paste "..(radius-1).." "..(h-radius).." qcircle")
    ov(m.flip)
    ov("paste "..(radius-1).." "..(radius-1).." qcircle")
    ov(m.identity)
    ov("delete qcircle")
    
    if #fillrgba > 0 then
        -- draw non-corner portions of rectangle
        ov(fillrgba)
        if radius < w/2 then
            ov("fill "..radius.." 0 "..(w-radius*2).." "..h)
        end
        if radius < h/2 then
            ov("fill 0 "..radius.." "..radius.." "..(h-radius*2))
            ov("fill "..(w-radius).." "..radius.." "..radius.." "..(h-radius*2))
        end
        ov("rgba "..oldrgba)
    end

    if borderwd > 0 then
        -- draw border lines using current color
        if radius < w/2 then
            ov("fill "..radius.." 0 "..(w-radius*2).." "..borderwd)
            ov("fill "..radius.." "..(h-borderwd).." "..(w-radius*2).." "..borderwd)
        end
        if radius < h/2 then
            ov("fill 0 "..radius.." "..borderwd.." "..(h-radius*2))
            ov("fill "..(w-borderwd).." "..radius.." "..borderwd.." "..(h-radius*2))
        end
    end
    
    -- save finished rectangle in a clip
    ov("copy 0 0 "..w.." "..h.." roundedrect")
    
    -- restore top left corner of overlay and draw rounded rectangle
    ov("paste 0 0 rectbg")
    ov("delete rectbg")
    ov("blend 1")
    ov("paste "..x.." "..y.." roundedrect")
    ov("delete roundedrect")
    
    -- restore blend setting
    ov("blend "..oldblend)
end

--------------------------------------------------------------------------------

local function draw_button(x, y, w, h)
    local oldblend = ov("blend 0")
    local buttrect = " "..x.." "..y.." "..w.." "..h
    
    -- copy rect under button to temp_bg
    ov("copy"..buttrect.." temp_bg")
    
    -- clear rect under button
    local oldrgba = ov("rgba 0 0 0 0")
    ov("fill"..buttrect)
    
    local butt_rgba = m.buttonrgba
    if darken_button then
        butt_rgba = m.darkerrgba
    end
    
    -- draw button with rounded corners
    if m.border > 0 then
        ov(m.borderrgba)
    end
    m.round_rect(x, y, w, h, m.radius, m.border, butt_rgba)
    
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
        local oldfont = ov(m.textfont)
        local w, h = m.maketext(newlabel, b.labelclip, m.textrgba, m.textshadowx, m.textshadowy, m.textshadowrgba)
        ov("font "..oldfont)
        b.labelwd = tonumber(w);
        b.labelht = tonumber(h);
        if changesize then
            -- use label size to set button width
            b.wd = b.labelwd + 2*m.textgap;
        end
        b.savetextrgba = m.textrgba
        b.savetextfont = m.textfont
        b.savelabel = newlabel
	end
    
    -- create text for label with a unique clip name
    b.labelclip = tostring(b).."+button"
    b.labelclip = string.gsub(b.labelclip, " ", "")   -- remove any spaces
    b.setlabel(label, true)

	b.show = function (x, y)
	    b.x = x
	    b.y = y
	    if b.shown then
	        -- remove old button in case it is moving to new position
	        b.hide()
	    end
	    -- remember position and save background pixels
	    b.background = b.labelclip.."+bg"
	    ov("copy "..b.x.." "..b.y.." "..b.wd.." "..b.ht.." "..b.background)
        
        -- draw the button at the given location
        draw_button(x, y, b.wd, b.ht)
        
        -- if m.textrgba or m.textfont has changed then recreate b.labelclip
        if b.savetextrgba ~= m.textrgba or b.savetextfont ~= m.textfont then
            b.setlabel(b.savelabel, false)
        end
        
        -- draw the label
        local oldblend = ov("blend 1")
        x = int(x + (b.wd - b.labelwd) / 2)
        y = int(y + m.yoffset + (b.ht - b.labelht) / 2)
        ov("paste "..x.." "..y.." "..b.labelclip)
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
        -- draw a tick mark
        local oldrgba = ov(m.textrgba)
        local oldblend = ov("blend 1")
        local oldwidth = ov("lineoption width 4")
        ov("line "..int(x+w/2).." "..int(y+h*0.75).." "..int(x+w*0.75).." "..int(y+h*0.25))
        ov("line "..int(x+w/2+1).." "..int(y+h*0.75).." "..int(x+w*0.25).." "..int(y+h*0.6))
        ov("lineoption width "..oldwidth)
        ov("blend "..oldblend)
        ov("rgba "..oldrgba)
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
    local oldfont = ov(m.textfont)
    local w, h = m.maketext(label, c.clipname, labelrgba, m.textshadowx, m.textshadowy, m.textshadowrgba)
    ov("font "..oldfont)
    
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
    local oldfont = ov(m.textfont)
    local w, h
    if #label == 0 then
        w, h = split(ov("text "..s.clipname.." "..label.." "))
        w, h = m.maketext(label.." ", s.clipname, labelrgba, m.textshadowx, m.textshadowy, m.textshadowrgba)
        w = 0
    else
        w, h = m.maketext(label, s.clipname, labelrgba, m.textshadowx, m.textshadowy, m.textshadowrgba)
    end
    ov("font "..oldfont)
    
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
    
    local t0 = g.millisecs()
    
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

    -- pause to ensure darkened button is seen
    while g.millisecs() - t0 < 16 do end
    
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
            local range = slider.maxval - slider.minval + 1
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
                            g.update()
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
        return (rule:sub(1, 3) == "MAP" and rule:len() == 25) or rule:sub(-1) == "H"
    elseif algo == "RuleLoader" then
        return rule:lower():find("hex") ~= nil
        -- !!! or maybe look in the .rule file and see if the TABLE section specifies
        -- neighborhood:hexagonal or the ICONS section specifies hexagons
    end
    
    return false
end

--------------------------------------------------------------------------------

function m.minbox(clipname, wd, ht)
    -- find the minimal bounding box of non-transparent pixels in given clip
    
    local xmin, ymin, xmax, ymax, minwd, minht

    -- find the top edge (ymin)
    local oldtarget = ov("target "..clipname)
    for row = 0, ht-1 do
        for col = 0, wd-1 do
            local _, _, _, a = split(ov("get "..col.." "..row))
            if a ~= "0" then
                ymin = row
                goto found_top
            end
        end
    end
    
    -- only get here if clip has no non-transparent pixels
    xmin, ymin, minwd, minht = 0, 0, 0, 0
    goto finish
    
    ::found_top::
    -- get here if clip has at least one non-transparent pixel
    
    -- find the bottom edge (ymax)
    for row = ht-1, ymin, -1 do
        for col = 0, wd-1 do
            local _, _, _, a = split(ov("get "..col.." "..row))
            if a ~= "0" then
                ymax = row
                goto found_bottom
            end
        end
    end
    ::found_bottom::
    
    -- find the left edge (xmin)
    for col = 0, wd-1 do
        for row = ymin, ymax do
            local _, _, _, a = split(ov("get "..col.." "..row))
            if a ~= "0" then
                xmin = col
                goto found_left
            end
        end
    end
    ::found_left::
    
    -- find the right edge (xmax)
    for col = wd-1, xmin, -1 do
        for row = ymin, ymax do
            local _, _, _, a = split(ov("get "..col.." "..row))
            if a ~= "0" then
                xmax = col
                goto found_right
            end
        end
    end
    ::found_right::
    
    -- all edges have been found
    minwd = xmax - xmin + 1
    minht = ymax - ymin + 1
    
    ::finish::
    
    -- return the bounding box info
    ov("target "..oldtarget)
    return xmin, ymin, minwd, minht
end

--------------------------------------------------------------------------------

return m
