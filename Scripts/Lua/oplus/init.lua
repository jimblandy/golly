-- This module is loaded if a script calls require "oplus".
-- It provides a high-level interface to the overlay commands.

local g = golly()
-- require "gplus.strict"
local ov = g.overlay
local ovt = g.ovtable

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

-- opaque colors for ovtable command
m.twhite    = {"rgba", 255, 255, 255, 255}
m.tgray     = {"rgba", 128, 128, 128, 255}
m.tblack    = {"rgba", 0, 0, 0, 255}
m.tred      = {"rgba", 255, 0, 0, 255}
m.tgreen    = {"rgba", 0, 255, 0, 255}
m.tblue     = {"rgba", 0, 0, 255, 255}
m.tcyan     = {"rgba", 0, 255, 255, 255}
m.tmagenta  = {"rgba", 255, 0, 255, 255}
m.tyellow   = {"rgba", 255, 255, 0, 255}

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

-- scripts can adjust these parameters:
-- check boxes and radio buttons are collectively known as select buttons

m.buttonht = 24     -- height of buttons (also used for select buttons and sliders)
m.sliderwd = 16     -- width of slider button (best if even)
m.selectgap = 5     -- gap between select button and its label
m.textgap = 10      -- gap between edge of button and its label
m.radius = 3        -- curvature of button corners
m.border = 0        -- thickness of button border (no border if 0)

m.buttonrgba = "rgba 40 128 255 255"    -- light blue buttons
m.darkerrgba = "rgba 20 64 255 255"     -- darker blue when buttons are clicked
m.borderrgba = m.white                  -- white border around buttons (if m.border > 0)
m.textrgba = m.white                    -- white button labels, marks on select buttons
m.distext = "rgba 100 192 255 255"      -- lighter blue for disabled button labels and marks on select buttons
m.textfont = "font 12 default-bold"     -- font for labels
m.textshadowx = 0                       -- label shadow x offset
m.textshadowy = 0                       -- label shadow y offset
m.textshadowrgba = m.black              -- black label shadow color
m.buttonshadowx = 0                     -- button shadow x offset
m.buttonshadowy = 0                     -- button shadow y offset
m.buttonshadowrgba = m.black            -- black button shadow color
m.yoffset = 0                           -- for better y position of labels

m.menubg = "rgba 40 128 255 255"        -- light blue background for menu bar and items
m.selcolor = "rgba 20 64 255 255"       -- darker background for selected menu/item
m.discolor = "rgba 100 192 255 255"     -- lighter blue for disabled items and separator lines
m.menufont = "font 12 default-bold"     -- font for menu and item labels
m.menutext = m.white                    -- white text for menu and item labels
m.menugap = 10                          -- horizontal space around each menu label
m.itemgap = 2                           -- vertical space above and below item labels

if g.os() == "Windows" then
    m.yoffset = -1
elseif g.os() == "Linux" then
    m.textfont = "font 12 default"
    m.menufont = "font 12 default"
end

--------------------------------------------------------------------------------

local darken_button = false     -- tell draw_button to use m.darkerrgba
local darken_slider = false     -- tell draw_slider to darken the slider button

local button_tables = {}        -- for detecting click in a button
local selectbutton_tables = {}  -- for detecting click in a select button
local slider_tables = {}        -- for detecting click in a slider
local menubar_tables = {}       -- for detecting click in a menu bar

local selmenu = 0               -- index of selected menu (if > 0)
local selitem = 0               -- index of selected item in selmenu (if > 0)

local textclip = "textclip"     -- default clip name for m.maketext and m.pastetext

local item_normal = 1           -- normal menu item
local item_tick   = 2           -- tick (multi-select) menu item
local item_radio  = 3           -- radio (single-select) menu item

--------------------------------------------------------------------------------

function m.pastetext(x, y, transform, clipname)
    -- set optional parameter defaults
    transform = transform or m.identity
    clipname  = clipname or textclip
    -- apply transform and paste text clip
    local oldtransform = ov(transform)
    ovt{"paste", x, y, clipname}
    -- restore settings
    ov("transform "..oldtransform)

    return clipname
end

--------------------------------------------------------------------------------

function m.maketext(s, clipname, textcol, shadowx, shadowy, shadowcol)
    local oldrgba = ov(m.white)
    -- set optional parameter defaults
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
            ovt{"fill"}
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

function m.draw_line(x1, y1, x2, y2)
    ovt{"line", x1, y1, x2, y2}
end

--------------------------------------------------------------------------------

function m.fill_rect(x, y, wd, ht)
    ovt{"fill", x, y, wd, ht}
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
            ovt{"fill", x, y, w, borderwd}
            ovt{"fill", x, (y+h-borderwd), w, borderwd}
            ovt{"fill", x, (y+borderwd), borderwd, (h-borderwd*2)}
            ovt{"fill", (x+w-borderwd), (y+borderwd), borderwd, (h-borderwd*2)}
        end
        if #fillrgba > 0 then
            -- draw interior of rectangle
            local oldrgba = ov(fillrgba)
            ovt{"fill", (x+borderwd), (y+borderwd), (w-borderwd*2), (h-borderwd*2)}
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
    ovt{"fill", 0, 0, w, h}
    ov("rgba "..oldrgba)

    -- create bottom right quarter circle in top left corner of overlay
    m.fill_ellipse(-radius, -radius, radius*2, radius*2, borderwd, fillrgba)
    ov("copy 0 0 "..radius.." "..radius.." qcircle")

    -- draw corners
    ovt{"paste", (w-radius), (h-radius), "qcircle"}
    ov(m.flip_y)
    ovt{"paste", (w-radius), (radius-1), "qcircle"}
    ov(m.flip_x)
    ovt{"paste", (radius-1), (h-radius), "qcircle"}
    ov(m.flip)
    ovt{"paste", (radius-1), (radius-1), "qcircle"}
    ov(m.identity)
    ov("delete qcircle")

    if #fillrgba > 0 then
        -- draw non-corner portions of rectangle
        ov(fillrgba)
        if radius < w/2 then
            ovt{"fill", radius, 0, (w-radius*2), h}
        end
        if radius < h/2 then
            ovt{"fill", 0, radius, radius, (h-radius*2)}
            ovt{"fill", (w-radius), radius, radius, (h-radius*2)}
        end
        ov("rgba "..oldrgba)
    end

    if borderwd > 0 then
        -- draw border lines using current color
        if radius < w/2 then
            ovt{"fill", radius, 0, (w-radius*2), borderwd}
            ovt{"fill", radius, (h-borderwd), (w-radius*2), borderwd}
        end
        if radius < h/2 then
            ovt{"fill", 0, radius, borderwd, (h-radius*2)}
            ovt{"fill", (w-borderwd), radius, borderwd, (h-radius*2)}
        end
    end

    -- save finished rectangle in a clip
    ov("copy 0 0 "..w.." "..h.." roundedrect")

    -- restore top left corner of overlay and draw rounded rectangle
    ovt{"paste", 0, 0, "rectbg"}
    ov("delete rectbg")
    ov("blend 1")
    ovt{"paste", x, y, "roundedrect"}
    ov("delete roundedrect")

    -- restore blend setting
    ov("blend "..oldblend)
end

--------------------------------------------------------------------------------

local function draw_buttonlayer(x, y, w, h, color)
    local oldblend = ov("blend 0")
    local buttrect = " "..x.." "..y.." "..w.." "..h

    -- copy rect under button to temp_bg
    ov("copy"..buttrect.." temp_bg")

    -- clear rect under button
    local oldrgba = ov("rgba 0 0 0 0")
    ovt{"fill", x, y, w, h}

    -- draw button with rounded corners
    if m.border > 0 then
        ov(m.borderrgba)
    end
    m.round_rect(x, y, w, h, m.radius, m.border, color)

    -- copy rect to temp_button
    ov("copy"..buttrect.." temp_button")

    -- paste temp_bg back to rect
    ovt{"paste", x, y, "temp_bg"}

    -- turn on blending and paste temp_button
    ov("blend 1")
    ovt{"paste", x, y, "temp_button"}

    ov("rgba "..oldrgba)
    ov("blend "..oldblend)
end

--------------------------------------------------------------------------------

local function draw_button(x, y, w, h, color, darkcolor)
    if m.buttonshadowx ~= 0 or m.buttonshadowy ~= 0 then
        draw_buttonlayer(x + m.buttonshadowx, y + m.buttonshadowy, w, h, m.buttonshadowrgba)
    end

    local butt_rgba = color or m.buttonrgba
    if darken_button then
        butt_rgba = darkcolor or m.darkerrgba
    end

    draw_buttonlayer(x, y, w, h, butt_rgba)
end

--------------------------------------------------------------------------------

function m.button(label, onclick)
    -- return a table that makes it easy to create and use buttons
    local b = {}

    if type(label) ~= "string" then
        error("1st arg of button must be a string", 2)
    end
    if type(onclick) ~= "function" then
        error("2nd arg of button must be a function", 2)
    end

    b.onclick = onclick     -- remember click handler
    b.shown = false         -- b.show hasn't been called
    b.enabled = true
    b.ht = m.buttonht;
    b.customcolor = nil
    b.darkcustomcolor = nil

    b.setlabel = function (newlabel, changesize)
        local oldfont = ov(m.textfont)
        local oldtextbg = ov("textoption background 0 0 0 0")
        local w, h
        if b.enabled then
            w, h = m.maketext(newlabel, b.labelclip, m.textrgba, m.textshadowx, m.textshadowy, m.textshadowrgba)
        else
            w, h = m.maketext(newlabel, b.labelclip, m.distext)   -- no shadow if disabled
        end
        ov("textoption background "..oldtextbg)
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
            -- remove old button from button_tables
            b.hide()
        end
        -- remember position and save background pixels
        b.background = b.labelclip.."+bg"
        ov("copy "..b.x.." "..b.y.." "..b.wd.." "..b.ht.." "..b.background)

        -- draw the button at the given location
        draw_button(x, y, b.wd, b.ht, b.customcolor, b.darkcustomcolor)

        -- if m.textrgba or m.textfont has changed then recreate b.labelclip
        if b.savetextrgba ~= m.textrgba or b.savetextfont ~= m.textfont then
            b.setlabel(b.savelabel, false)
        end

        -- draw the label
        local oldblend = ov("blend 1")
        x = int(x + (b.wd - b.labelwd) / 2)
        y = int(y + m.yoffset + (b.ht - b.labelht) / 2)
        ovt{"paste", x, y, b.labelclip}
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
            ovt{"paste", b.x, b.y, b.background}
            ov("blend "..oldblend)

            -- remove the table entry
            button_tables[b.rect] = nil
            b.shown = false
        end
    end

    b.enable = function (bool)
        if b.enabled ~= bool then
            b.enabled = bool
            b.setlabel(b.savelabel, false)
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

local function draw_selectbutton(x, y, w, h, ticked, enabled, multi)
    draw_button(x, y, w, h)
    if multi then
        -- draw a radio button
        local optcol = m.distext
        if ticked and enabled then optcol = m.textrgba end
        local x1 = int(x+w/6)
        local y1 = int(y+w/6)
        local w1 = w - int(w/3)
        local h1 = h - int(h/3)
        if enabled and (m.textshadowx > 0 or m.textshadowy > 0) then
            m.fill_ellipse(x1+m.textshadowx, y1+m.textshadowy, w1, h1, 0, m.textshadowrgba)
        end
        if enabled or ticked then
            m.fill_ellipse(x1, y1, w1, h1, 0, optcol)
        end
    else
        if ticked then
            -- draw a tick mark
            local oldrgba
            if enabled then
                oldrgba = ov(m.textrgba)
            else
                oldrgba = ov(m.distext)
            end
            local oldblend = ov("blend 1")
            local oldwidth = ov("lineoption width 4")

            local x1 = int(x+w/2)
            local y1 = int(y+h*0.75)
            local x2 = int(x+w*0.75)
            local y2 = int(y+h*0.25)
            local x3 = int(x+w/2+1)
            local y3 = int(y+h*0.75)
            local x4 = int(x+w*0.25)
            local y4 = int(y+h*0.6)

            if enabled and (m.textshadowx > 0 or m.textshadowy > 0) then
                local oldcol = ov(m.textshadowrgba)
                ovt{"line", x1+m.textshadowx, y1+m.textshadowy, x2+m.textshadowx, y2+m.textshadowy}
                ovt{"line", x3+m.textshadowx, y3+m.textshadowy, x4+m.textshadowx, y4+m.textshadowy}
                ov("rgba "..oldcol)
            end
            ovt{"line", x1, y1, x2, y2}
            ovt{"line", x3, y3, x4, y4}
            ov("lineoption width "..oldwidth)
            ov("blend "..oldblend)
            ov("rgba "..oldrgba)
        end
    end
end

--------------------------------------------------------------------------------

function m.radiobutton(label, labelrgba, onclick)
    return m.selectbutton(label, labelrgba, onclick, true)
end

--------------------------------------------------------------------------------

function m.checkbox(label, labelrgba, onclick)
    return m.selectbutton(label, labelrgba, onclick, false)
end

--------------------------------------------------------------------------------

function m.selectbutton(label, labelrgba, onclick, multi)
    -- return a table that makes it easy to create and use check boxes and radio buttons
    local c = {}
    local what
    if multi then what = "checkbox" else what = "radiobutton" end

    if type(label) ~= "string" then
        error("1st arg of "..what.." must be a string", 2)
    end
    if type(labelrgba) ~= "string" then
        error("2nd arg of "..what.." must be a string", 2)
    end
    if type(onclick) ~= "function" then
        error("3rd arg of "..what.." must be a function", 2)
    end

    -- create text for label with a unique clip name
    c.clipname = tostring(c).."+"..what
    c.clipname = string.gsub(c.clipname, " ", "")   -- remove any spaces
    local oldfont = ov(m.textfont)
    local oldtextbg = ov("textoption background 0 0 0 0")
    local w, h = m.maketext(label, c.clipname, labelrgba, m.textshadowx, m.textshadowy, m.textshadowrgba)
    ov("textoption background "..oldtextbg)
    ov("font "..oldfont)

    -- use label size to set select button size
    c.labelwd = tonumber(w);
    c.labelht = tonumber(h);
    c.wd = m.buttonht + m.selectgap + c.labelwd;
    c.ht = m.buttonht;

    c.onclick = onclick     -- remember click handler
    c.shown = false         -- c.show hasn't been called
    c.enabled = true
    c.multi = multi

    c.show = function (x, y, ticked)
        c.x = x
        c.y = y
        c.ticked = ticked
        if c.shown then
            -- remove old select button from selectbutton_tables
            c.hide()
        end
        -- remember position and save background pixels
        c.background = c.clipname.."+bg"
        ov("copy "..c.x.." "..c.y.." "..c.wd.." "..c.ht.." "..c.background)

        -- draw the select button (excluding label) at the given location
        draw_selectbutton(x+1, y+1, c.ht-2, c.ht-2, ticked, c.enabled, c.multi)

        -- draw the label
        local oldblend = ov("blend 1")
        ovt{"paste", (x+c.ht+m.selectgap), int(y+m.yoffset+(c.ht-c.labelht)/2), c.clipname}
        ov("blend "..oldblend)

        -- store this table using the select button's rectangle as key
        c.rect = rect({c.x, c.y, c.wd, c.ht})
        selectbutton_tables[c.rect] = c
        c.shown = true
    end

    c.hide = function ()
        if c.shown then
            -- restore background pixels saved in c.show
            local oldblend = ov("blend 0")
            ovt{"paste", c.x, c.y, c.background}
            ov("blend "..oldblend)
            -- remove the table entry
            selectbutton_tables[c.rect] = nil
            c.shown = false
        end
    end

    c.enable = function (bool)
        c.enabled = bool
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
    ovt{"fill", x, (midy-3), w, 6}
    ov("rgba 110 110 110 255")
    ovt{"fill", (x+1), (midy-2), (w-2), 1}
    ov("rgba 120 120 120 255")
    ovt{"fill", (x+1), (midy-1), (w-2), 1}
    ov("rgba 130 130 130 255")
    ovt{"fill", (x+1), midy, (w-2), 1}
    ov("rgba 140 140 140 255")
    ovt{"fill", (x+1), (midy+1), (w-2), 1}

    if darken_slider then darken_button = true end

    -- draw slider button on top of horizontal bar
    draw_button(x + barpos - int(m.sliderwd/2), y, m.sliderwd, h)
    ov("rgba "..oldrgba)

    if darken_slider then darken_button = false end

    -- draw the label
    local oldblend = ov("blend 1")
    ovt{"paste", s.x, int(y+m.yoffset+(h-s.labelht)/2), s.clipname}
    ov("blend "..oldblend)
end

--------------------------------------------------------------------------------

function m.slider(label, labelrgba, barwidth, minval, maxval, onclick)
    -- return a table that makes it easy to create and use sliders
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
    local oldtextbg = ov("textoption background 0 0 0 0")
    local w, h
    if #label == 0 then
        w, h = split(ov("text "..s.clipname.." "..label.." "))
        w, h = m.maketext(label.." ", s.clipname, labelrgba, m.textshadowx, m.textshadowy, m.textshadowrgba)
        w = 0
    else
        w, h = m.maketext(label, s.clipname, labelrgba, m.textshadowx, m.textshadowy, m.textshadowrgba)
    end
    ov("textoption background "..oldtextbg)
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
            -- remove old slider from slider_tables
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

        -- store this table using the slider bar's rectangle as key, including
        -- overlap of half button width at left and right edges of bar
        s.rect = rect({s.startbar-int(m.sliderwd/2), s.y, s.barwidth+m.sliderwd, s.ht})
        slider_tables[s.rect] = s
        s.shown = true
    end

    s.hide = function ()
        if s.shown then
            -- restore background pixels saved in previous s.show
            local oldblend = ov("blend 0")
            ovt{"paste", s.x, s.y, s.background}
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
        if event == "mup left" then break end
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
            if button.enabled and release_in_rect(r, button) then
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
    for r, checkbox in pairs(selectbutton_tables) do
        if x >= r.left and x <= r.right and y >= r.top and y <= r.bottom then
            if checkbox.enabled and release_in_rect(r, checkbox) then
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

            -- check if click is outside slider button
            local barpos = int(slider.barwidth * (slider.pos - slider.minval) / (slider.maxval - slider.minval))
            local buttrect = rect({slider.startbar+barpos-int(m.sliderwd/2), slider.y, m.sliderwd, slider.ht})
            if not (x >= buttrect.left and x <= buttrect.right and y >= buttrect.top and y <= buttrect.bottom) then
                -- move button to clicked position immediately
                prevx = math.maxinteger
            end

            -- track horizontal movement of mouse until button is released
            repeat
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
                        ovt{"paste", slider.x, slider.y, slider.background}
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
            until g.getevent() == "mup left"

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

local function DrawMenuBar(mbar)
    local oldrgba = ov(m.menubg)
    m.fill_rect(mbar.r.x, mbar.r.y, mbar.r.wd, mbar.r.ht)
    local oldblend = ov("blend 1")
    local xpos = mbar.r.x + m.menugap
    local ypos = mbar.r.y + m.yoffset + (mbar.r.ht - mbar.labelht) // 2
    for i = 1, #mbar.menus do
        local menu = mbar.menus[i]
        if i == selmenu then
            ov(m.selcolor)
            m.fill_rect(xpos, mbar.r.y, menu.labelwd + m.menugap*2, mbar.r.ht)
        end
        xpos = xpos + m.menugap
        ovt{"paste", xpos, ypos, menu.labelclip}
        xpos = xpos + menu.labelwd + m.menugap
    end
    ov("blend "..oldblend)
    ov("rgba "..oldrgba)
end

--------------------------------------------------------------------------------

function m.menubar()
    -- return a table that makes it easy to create and use a menu bar
    local mbar = {}

    mbar.menus = {}
    mbar.r = {}             -- menu bar's bounding rectangle
    mbar.labelht = 0        -- height of label text
    mbar.itemht = 0         -- height of a menu item
    mbar.shown = false      -- mbar.show hasn't been called

    mbar.addmenu = function (menuname)
        -- append a menu to the menu bar
        local index = #mbar.menus+1
        local clipname = string.gsub(tostring(mbar)..index," ","")
        local oldfont = ov(m.menufont)
        local oldblend = ov("blend 1")
        local oldtextbg = ov("textoption background 0 0 0 0")
        local wd, ht = m.maketext(menuname, clipname, m.menutext, m.textshadowx, m.textshadowy, m.textshadowrgba)
        ov("textoption background "..oldtextbg)
        ov("blend "..oldblend)
        ov("font "..oldfont)
        mbar.labelht = ht
        mbar.itemht = ht + m.itemgap*2
        mbar.menus[index] = { labelwd=wd, labelclip=clipname, items={}, maxwd=0 }
    end

    local function check_width(menuindex, itemname)
        local oldfont = ov(m.menufont)
        local oldblend = ov("blend 1")
        local wd, _ = m.maketext(itemname, nil, m.menutext, m.textshadowx, m.textshadowy, m.textshadowrgba)
        ov("blend "..oldblend)
        ov("font "..oldfont)
        local itemwd = wd + m.menugap*2 + 20
        if itemwd > mbar.menus[menuindex].maxwd then
            mbar.menus[menuindex].maxwd = itemwd
        end
    end

    mbar.additem = function (menuindex, itemname, onclick, args)
        -- append an item to given menu
        args = args or {}
        check_width(menuindex, itemname)
        local items = mbar.menus[menuindex].items
        items[#items+1] = { name=itemname, f=onclick, fargs=args, enabled=(onclick ~= nil), type=item_normal, value=false }
    end

    mbar.setitem = function (menuindex, itemindex, newname)
        -- change name of given menu item
        check_width(menuindex, newname)
        mbar.menus[menuindex].items[itemindex].name = newname
    end

    mbar.enableitem = function (menuindex, itemindex, bool)
        mbar.menus[menuindex].items[itemindex].enabled = bool
    end

    mbar.tickitem = function (menuindex, itemindex, bool)
        -- tick/untick the given menu item
        mbar.menus[menuindex].items[itemindex].type = item_tick
        mbar.menus[menuindex].items[itemindex].value = bool
    end

    mbar.radioitem = function (menuindex, itemindex, bool)
        -- mark menu item as a radio item and set its value
        mbar.menus[menuindex].items[itemindex].type = item_radio
        mbar.menus[menuindex].items[itemindex].value = bool
    end

    mbar.show = function (x, y, wd, ht)
        if wd > 0 and ht > 0 then
            if mbar.shown then
                mbar.hide()     -- set menubar_tables[mbar.r] to nil
            end
            mbar.r = rect({x, y, wd, ht})
            DrawMenuBar(mbar)
            menubar_tables[mbar.r] = mbar
            mbar.shown = true
        end
    end

    mbar.hide = function ()
        if mbar.shown then
            menubar_tables[mbar.r] = nil
            mbar.shown = false
        end
    end

    mbar.refresh = function ()
        -- redraw menu bar
        mbar.show(mbar.r.x, mbar.r.y, mbar.r.wd, mbar.r.ht)
        g.update()
    end

    return mbar
end

--------------------------------------------------------------------------------

local function GetMenu(x, y, oldmenu, mbar)
    -- return menu index depending on given mouse location
    if y < mbar.r.y then return oldmenu end
    if y > mbar.r.y + mbar.r.ht - 1 then return oldmenu end

    if x < mbar.r.x + m.menugap then return 0 end
    if x > mbar.r.x + mbar.r.wd - 1 then return 0 end

    local endmenu = mbar.r.x + m.menugap
    for i = 1, #mbar.menus do
        endmenu = endmenu + mbar.menus[i].labelwd + m.menugap*2
        if x < endmenu then
            return i
        end
    end

    return 0
end

--------------------------------------------------------------------------------

local function GetItem(x, y, menuindex, mbar)
    -- return index of menu item at given mouse location
    if menuindex == 0 then return 0 end

    if y < mbar.r.y + mbar.r.ht then return 0 end

    local numitems = #mbar.menus[menuindex].items
    local ht = numitems * mbar.itemht
    if y > mbar.r.y + mbar.r.ht + ht then return 0 end

    local mleft = mbar.r.x + m.menugap
    for i = 2, menuindex do
        mleft = mleft + mbar.menus[i-1].labelwd + m.menugap*2
    end
    if x < mleft then return 0 end
    if x > mleft + mbar.menus[menuindex].maxwd then return 0 end

    -- x,y is somewhere in a menu item
    local itemindex = math.floor( (y - (mbar.r.y + mbar.r.ht)) / mbar.itemht ) + 1
    if itemindex > numitems then itemindex = numitems end

    return itemindex
end

--------------------------------------------------------------------------------

local function DrawMenuItems(mbar)
    -- draw drop-down window showing all items in the currently selected menu
    local numitems = #mbar.menus[selmenu].items
    if numitems == 0 then return end

    local oldrgba = ov(m.menubg)
    local x = mbar.r.x + m.menugap
    for i = 2, selmenu do
        x = x + mbar.menus[i-1].labelwd + m.menugap*2
    end
    local ht = numitems * mbar.itemht + 1
    local wd = mbar.menus[selmenu].maxwd
    local y = mbar.r.y + mbar.r.ht
    m.fill_rect(x, y, wd, ht)

    local oldfont = ov(m.menufont)
    local oldblend = ov("blend 1")

    -- draw translucent gray shadows
    ov("rgba 48 48 48 128")
    local shadowsize = 3
    m.fill_rect(x+shadowsize, y+ht, wd-shadowsize, shadowsize)
    m.fill_rect(x+wd, y, shadowsize, ht+shadowsize)

    x = x + m.menugap
    y = y + m.yoffset
    for i = 1, numitems do
        local item = mbar.menus[selmenu].items[i]
        if item.f == nil then
            -- item is a separator
            ov(m.discolor)
            m.draw_line(x-m.menugap, y+mbar.itemht//2, x-m.menugap+wd-1, y+mbar.itemht//2)
        else
            if i == selitem and item.enabled then
                ov(m.selcolor)
                m.fill_rect(x-m.menugap, y, wd, mbar.itemht)
            end
            local oldtextbg = ov("textoption background 0 0 0 0")
            if item.enabled then
                m.maketext(item.name, nil, m.menutext, m.textshadowx, m.textshadowy, m.textshadowrgba)
                m.pastetext(x, y + m.itemgap)
                ov(m.menutext)
            else
                m.maketext(item.name, nil, m.discolor)  -- no shadow if disabled
                m.pastetext(x, y + m.itemgap)
                ov(m.discolor)
            end
            ov("textoption background "..oldtextbg)
            if item.type == item_tick then
                if item.value then
                    -- draw tick mark at right edge
                    local x1 = x - m.menugap + wd - m.menugap
                    local y1 = y + 6
                    local x2 = x1 - 6
                    local y2 = y + mbar.itemht - 8
                    local oldwidth = ov("lineoption width 4")
                    if item.enabled and (m.textshadowx > 0 or m.textshadowy > 0) then
                        local oldcolor = ov(m.textshadowrgba)
                        m.draw_line(x1+m.textshadowx, y1+m.textshadowy, x2+m.textshadowx, y2+m.textshadowy)
                        m.draw_line(x2+m.textshadowx, y2+m.textshadowy, x2+m.textshadowx-5, y2+m.textshadowy-3)
                        ov("rgba "..oldcolor)
                    end
                    m.draw_line(x1, y1, x2, y2)
                    m.draw_line(x2, y2, x2-5, y2-3)
                    ov("lineoption width "..oldwidth)
                end
            elseif item.type == item_radio then
                -- draw radio button at right edge
                local size = mbar.itemht - 12
                local x1 = x - m.menugap + wd - m.menugap - size
                local y1 = y + 6
                if item.enabled and (m.textshadowx > 0 or m.textshadowy > 0) then
                    m.fill_ellipse(x1+m.textshadowx, y1+m.textshadowy, size, size, 0, m.textshadowrgba)
                end
                local optcol = m.distext
                if item.value and item.enabled then optcol = m.textrgba end
                if item.enabled or item.value then
                    m.fill_ellipse(x1, y1, size, size, 0, optcol)
                end
            end
        end
        y = y + mbar.itemht
    end

    ov("blend "..oldblend)
    ov("font "..oldfont)
    ov("rgba "..oldrgba)
end

--------------------------------------------------------------------------------

local function release_in_item(x, y, mbar)
    -- user clicked given point in menu bar so return the selected menu item
    -- on release, or nil if the item is disabled or no item is selected

    local t0 = g.millisecs()
    local MacOS = g.os() == "Mac"

    selitem = 0
    selmenu = GetMenu(x, y, 0, mbar)
    if selmenu == 0 and not MacOS then
        -- if initial click is not in any menu then ignore it on Windows/Linux
        return nil
    end

    -- save entire overlay (including menu bar) in bgclip
    local bgclip = string.gsub(tostring(mbar).."bg"," ","")
    ov("copy 0 0 0 0 "..bgclip)

    if selmenu > 0 then
        DrawMenuBar(mbar)       -- highlight selected menu
        DrawMenuItems(mbar)
        g.update()
    end

    local prevx = x
    local prevy = y
    local menuitem = nil
    -- on Windows/Linux we loop until an enabled item is clicked;
    -- on Mac we loop until an enabled or disabled item is clicked
    while true do
        -- loop until click or keypress
        while true do
            local event = g.getevent()
            if event == "mup left" then
                if g.millisecs() - t0 > 500 then
                    local oldmenu = selmenu
                    selmenu = GetMenu(x, y, selmenu, mbar)
                    if selmenu == 0 and not MacOS then selmenu = oldmenu end
                    break
                end
            elseif event:find("^oclick") then
                local _, sx, sy, butt, mods = split(event)
                if butt == "left" and mods == "none" then
                    x = tonumber(sx)
                    y = tonumber(sy)
                    local oldmenu = selmenu
                    selmenu = GetMenu(x, y, selmenu, mbar)
                    if selmenu == 0 and not MacOS then selmenu = oldmenu end
                    break
                end
            elseif event == "key enter none" or event == "key return none" then
                break
            end
            local xy = ov("xy")
            if #xy > 0 then
                x, y = split(xy)
                x = tonumber(x)
                y = tonumber(y)
                if x ~= prevx or y ~= prevy then
                    -- check if mouse moved into or out of a menu/item
                    local oldmenu = selmenu
                    local olditem = selitem
                    selmenu = GetMenu(x, y, selmenu, mbar)
                    if selmenu == 0 and not MacOS then selmenu = oldmenu end
                    selitem = GetItem(x, y, selmenu, mbar)
                    if selmenu ~= oldmenu or selitem ~= olditem then
                        ovt{"paste", 0, 0, bgclip}
                        DrawMenuBar(mbar)
                        if MacOS then
                            if selmenu > 0 then DrawMenuItems(mbar) end
                        else
                            DrawMenuItems(mbar)
                        end
                        g.update()
                    end
                    prevx = x
                    prevy = y
                end
            end
        end

        if MacOS then
            -- on Mac we can return nil if user clicked a disabled item
            menuitem = nil
            if selmenu > 0 then
                selitem = GetItem(x, y, selmenu, mbar)
                if selitem > 0 and mbar.menus[selmenu].items[selitem].enabled then
                    menuitem = mbar.menus[selmenu].items[selitem]
                end
            end
            break
        else
            -- Windows/Linux
            if selmenu > 0 then
                selitem = GetItem(x, y, selmenu, mbar)
                if selitem > 0 and mbar.menus[selmenu].items[selitem].enabled then
                    menuitem = mbar.menus[selmenu].items[selitem]
                    break
                elseif selitem == 0 then
                    break
                end
            end
        end
    end

    -- restore overlay and menu bar
    ovt{"paste", 0, 0, bgclip}
    g.update()
    ov("delete "..bgclip)
    selmenu = 0
    return menuitem
end

--------------------------------------------------------------------------------

local function click_in_menubar(x, y)
    for r, mbar in pairs(menubar_tables) do
        if x >= r.left and x <= r.right and y >= r.top and y <= r.bottom then
            local menuitem = release_in_item(x, y, mbar)
            if menuitem and menuitem.f then
                -- call this menu item's handler
                menuitem.f( table.unpack(menuitem.fargs) )
            end
            return true
        end
    end
    return false
end

--------------------------------------------------------------------------------

local function DrawPopUpMenu(p, chosenitem)
    -- draw pop-up window showing all items
    local numitems = #p.items
    if numitems == 0 then return end

    local oldfont = ov(m.menufont)
    local oldblend = ov("blend 1")
    local oldrgba = ov(p.bgcolor)

    local ht = p.menuht + 1
    local wd = p.menuwd
    local x = p.x
    local y = p.y
    m.fill_rect(x, y, wd, ht)

    -- draw translucent gray shadows
    ov("rgba 48 48 48 128")
    local shadowsize = 3
    m.fill_rect(x+shadowsize, y+ht, wd-shadowsize, shadowsize)
    m.fill_rect(x+wd, y+shadowsize, shadowsize, ht)

    x = x + m.menugap
    y = y + m.yoffset
    for i = 1, numitems do
        local item = p.items[i]
        if item.f == nil then
            -- item is a separator
            ov(p.discolor)
            m.draw_line(x-m.menugap, y+p.itemht//2, x-m.menugap+wd-1, y+p.itemht//2)
        else
            if i == chosenitem and item.enabled then
                ov(p.selcolor)
                m.fill_rect(x-m.menugap, y, wd, p.itemht)
            end
            local oldtextbg = ov("textoption background 0 0 0 0")
            if item.enabled then
                m.maketext(item.name, nil, m.menutext, m.textshadowx, m.textshadowy, m.textshadowrgba)
                m.pastetext(x, y + m.itemgap)
                ov(m.menutext)
            else
                m.maketext(item.name, nil, p.discolor)  -- no shadow if disabled
                m.pastetext(x, y + m.itemgap)
                ov(p.discolor)
            end
            ov("textoption background "..oldtextbg)
            if item.type == item_tick then
                if item.value then
                    -- draw tick mark at right edge
                    local x1 = x - m.menugap + wd - m.menugap
                    local y1 = y + 6
                    local x2 = x1 - 6
                    local y2 = y + p.itemht - 8
                    local oldwidth = ov("lineoption width 4")
                    if item.enabled and (m.textshadowx > 0 or m.textshadowy > 0) then
                        local oldcolor = ov(m.textshadowrgba)
                        m.draw_line(x1+m.textshadowx, y1+m.textshadowy, x2+m.textshadowx, y2+m.textshadowy)
                        m.draw_line(x2+m.textshadowx, y2+m.textshadowy, x2+m.textshadowx-5, y2+m.textshadowy-3)
                        ov("rgba "..oldcolor)
                    end
                    m.draw_line(x1, y1, x2, y2)
                    m.draw_line(x2, y2, x2-5, y2-3)
                    ov("lineoption width "..oldwidth)
                end
            elseif item.type == item_radio then
                -- draw radio button at right edge
                local size = p.itemht - 12
                local x1 = x - m.menugap + wd - m.menugap - size
                local y1 = y + 6
                if item.enabled and (m.textshadowx > 0 or m.textshadowy > 0) then
                    m.fill_ellipse(x1+m.textshadowx, y1+m.textshadowy, size, size, 0, m.textshadowrgba)
                end
                local optcol = m.distext
                if item.value and item.enabled then optcol = m.textrgba end
                if item.enabled or item.value then
                    m.fill_ellipse(x1, y1, size, size, 0, optcol)
                end
            end
        end
        y = y + p.itemht
    end

    ov("blend "..oldblend)
    ov("font "..oldfont)
    ov("rgba "..oldrgba)
end

--------------------------------------------------------------------------------

local function GetPopUpItem(x, y, p)
    -- return index of item at given mouse location
    if x <= p.x or y <= p.y then return 0 end
    local numitems = #p.items
    if y > p.y + p.menuht then return 0 end
    if x > p.x + p.menuwd then return 0 end

    -- x,y is somewhere in a menu item
    local itemindex = math.floor((y - p.y) / p.itemht) + 1
    if itemindex > numitems then itemindex = numitems end

    return itemindex
end

--------------------------------------------------------------------------------

local function choose_popup_item(p)
    -- return a chosen item from the given pop-up menu
    -- or nil if the item is disabled or no item is selected

    local t0 = g.millisecs()

    -- save entire overlay in bgclip
    local bgclip = string.gsub(tostring(p).."bg"," ","")
    ov("copy 0 0 0 0 "..bgclip)

    local chosenitem = 0
    DrawPopUpMenu(p, chosenitem)
    g.update()

    local x = p.x
    local y = p.y
    local prevx = x
    local prevy = y
    while true do
        local event = g.getevent()
        if event:find("^mup") then
            if g.millisecs() - t0 > 500 then
                break
            end
        elseif event:find("^oclick") then
            local _, sx, sy, butt, mods = split(event)
            if butt == "left" and mods == "none" then
                x = tonumber(sx)
                y = tonumber(sy)
                break
            end
        elseif event == "key enter none" or event == "key return none" then
            break
        end
        local xy = ov("xy")
        if #xy > 0 then
            x, y = split(xy)
            x = tonumber(x)
            y = tonumber(y)
            if x ~= prevx or y ~= prevy then
                -- check if mouse moved into or out of an item
                local olditem = chosenitem
                chosenitem = GetPopUpItem(x, y, p)
                if chosenitem ~= olditem then
                    ovt{"paste", 0, 0, bgclip}
                    DrawPopUpMenu(p, chosenitem)
                    g.update()
                end
                prevx = x
                prevy = y
            end
        end
    end

    chosenitem = GetPopUpItem(x, y, p)

    -- restore overlay
    ovt{"paste", 0, 0, bgclip}
    g.update()
    ov("delete "..bgclip)

    return chosenitem
end

--------------------------------------------------------------------------------

function m.popupmenu()
    -- return a table that makes it easy to create and use a pop-up menu
    local p = {}

    p.items = {}        -- array of items
    p.labelht = 0       -- height of label text
    p.itemht = 0        -- height of an item
    p.menuwd = 0        -- width of pop-up menu
    p.menuht = 0        -- height of pop-up menu
    p.x, p.y = 0, 0     -- top left location of pop-up menu

    -- default to menu bar colors
    p.bgcolor = m.menubg
    p.selcolor = m.selcolor
    p.discolor = m.discolor

    local function check_width(itemname)
        local oldfont = ov(m.menufont)
        local oldblend = ov("blend 1")
        local wd, ht = m.maketext(itemname, nil, m.menutext, m.textshadowx, m.textshadowy, m.textshadowrgba)
        ov("blend "..oldblend)
        ov("font "..oldfont)
        p.labelht = ht
        p.itemht = ht + m.itemgap*2
        local itemwd = wd + m.menugap*2 + 20
        if itemwd > p.menuwd then p.menuwd = itemwd end
    end

    p.additem = function (itemname, onselect, args)
        args = args or {}
        check_width(itemname)
        p.items[#p.items+1] = { name=itemname, f=onselect, fargs=args, enabled=(onselect ~= nil), type=item_normal, value=false }
        p.menuht = #p.items * p.itemht
    end

    p.enableitem = function (itemindex, bool)
        -- enable/disable the given item
        p.items[itemindex].enabled = bool
    end

    p.tickitem = function (itemindex, bool)
        -- tick/untick the given item
        p.items[itemindex].type = item_tick
        p.items[itemindex].value = bool
    end

    p.radioitem = function (itemindex, bool)
        -- set/clear the given item option
        -- mark menu item as radio item and set its value
        p.items[itemindex].type = item_radio
        p.items[itemindex].value = bool
    end

    p.setbgcolor = function (rgba)
        p.bgcolor = rgba
        local _,R,G,B,A = split(rgba)
        R = tonumber(R)
        G = tonumber(G)
        B = tonumber(B)
        A = tonumber(A)
        -- use a darker color when item is selected
        p.selcolor = "rgba "..math.max(0,R-48).." "..math.max(0,G-48).." "..math.max(0,B-48).." "..A
        -- use lighter color for disabled items and separator lines
        p.discolor = "rgba "..math.min(255,R+48).." "..math.min(255,G+48).." "..math.min(255,B+48).." "..A
    end

    p.show = function (x, y, ovwd, ovht)
        if x + p.menuwd > ovwd then x = x - p.menuwd - 2 end
        if y + p.menuht > ovht then y = ovht - p.menuht end
        p.x = x
        p.y = y
        local itemindex = choose_popup_item(p)
        if itemindex > 0 then
            local item = p.items[itemindex]
            if item and item.f and item.enabled then
                -- call this item's handler
                item.f( table.unpack(item.fargs) )
            end
        end
    end

    return p
end

--------------------------------------------------------------------------------

function m.process(event)
    if #event > 0 then
        if event:find("^oclick") then
            local _, x, y, butt, mods = split(event)
            if butt == "left" and mods == "none" then
                x = tonumber(x)
                y = tonumber(y)
                if click_in_button(x, y) then return "" end
                if click_in_checkbox(x, y) then return "" end
                if click_in_slider(x, y) then return "" end
                if click_in_menubar(x, y) then return "" end
            end
        end
    end
    return event
end

--------------------------------------------------------------------------------

function m.hexrule()
    -- return true if the current rule uses a hexagonal neighborhood
    return g.getgridtype() == "Hexagonal"
end

--------------------------------------------------------------------------------

function m.minbox(clipname, wd, ht)
    -- find the minimal bounding box of non-transparent pixels in given clip

    local xmin, ymin, xmax, ymax, minwd, minht

    -- find the top edge (ymin)
    local oldtarget = ov("target "..clipname)
    for row = 0, ht-1 do
        for col = 0, wd-1 do
            local _, _, _, a = ovt{"get", col, row}
            if a ~= 0 then
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
            local _, _, _, a = ovt{"get", col, row}
            if a ~= 0 then
                ymax = row
                goto found_bottom
            end
        end
    end
    ::found_bottom::

    -- find the left edge (xmin)
    for col = 0, wd-1 do
        for row = ymin, ymax do
            local _, _, _, a = ovt{"get", col, row}
            if a ~= 0 then
                xmin = col
                goto found_left
            end
        end
    end
    ::found_left::

    -- find the right edge (xmax)
    for col = wd-1, xmin, -1 do
        for row = ymin, ymax do
            local _, _, _, a = ovt{"get", col, row}
            if a ~= 0 then
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
