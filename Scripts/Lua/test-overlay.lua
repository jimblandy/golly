-- Test all the overlay commands.

local g = golly()
-- require "gplus.strict"
local gp = require "gplus"
local split = gp.split
local int = gp.int
local op = require "oplus"

math.randomseed(os.time())  -- init seed for math.random
local rand = math.random

local ov = g.overlay

local wd, ht        -- overlay's current width and height (set by create_overlay)
local toggle = 0    -- for toggling alpha blending
local textmode = 0  -- for toggling multiline mode

--------------------------------------------------------------------------------

local function ms(t)
    return string.format("%.1f", 1000 * t).."ms"
end

--------------------------------------------------------------------------------

local function create_overlay(w, h)
    wd = w
    ht = h
    ov("create "..w.." "..h)
    ov("fill")              -- fill overlay with opaque white
    ov("rgba 0 0 255 64")   -- 25% translucent blue
    ov("fill 1 1 -2 -2")    -- non +ve wd/ht are treated as insets
    
    ov(op.yellow)
    
    -- rects completely outside overlay are ignored
    ov("fill -1 -1 1 1")
    ov("fill 1000000 1000000 1 1")
    -- rects partially outside overlay are clipped
    ov("fill -20 "..(h-20).." 40 40")
    ov("fill "..(w-20).." -20 40 40")
    
    g.setoption("showoverlay", 1)
    g.update()
end

--------------------------------------------------------------------------------

local function show_help()
    ov(op.black)
    ov("fill")
    ov(op.white)
    local oldfont = ov("font 10 mono-bold")   -- use a mono-spaced font
    local w, h = op.multiline("helpclip",
[[
Special keys and their actions:

b -- test alpha blending
c -- test cursors
C -- create overlay that covers layer
e -- test error messages
f -- test filling lots of rectangles
g -- show pixel value under mouse
h -- display this help
i -- test loading image from file
l -- test drawing lots of lines
m -- test multiline text
p -- test overlay positions
s -- test setting lots of pixels
t -- test text and transforms
v -- test copy and paste
w -- test saving overlay to file

Click and drag to draw.
Option-click to flood.
]]
    )
    local oldblend = ov("blend 1")
    ov("paste 5 5 helpclip")
    ov("freeclip helpclip")
    ov("blend "..oldblend)
    ov("font "..oldfont)
end

--------------------------------------------------------------------------------

local pos = 0
local function test_positions()
    pos = pos + 1
    if pos == 1 then ov("position middle") end
    if pos == 2 then ov("position topright") end
    if pos == 3 then ov("position bottomright") end
    if pos == 4 then ov("position bottomleft") end
    if pos == 5 then ov("position topleft") ; pos = 0 end
end

--------------------------------------------------------------------------------

local curs = 0
local function test_cursors()
    curs = curs + 1
    if curs == 1 then ov("cursor pencil") end
    if curs == 2 then ov("cursor pick") end
    if curs == 3 then ov("cursor cross") end
    if curs == 4 then ov("cursor hand") end
    if curs == 5 then ov("cursor zoomin") end
    if curs == 6 then ov("cursor zoomout") end
    if curs == 7 then ov("cursor arrow") end
    if curs == 8 then ov("cursor current") end
    if curs == 9 then ov("cursor hidden") ; curs = 0 end
end

--------------------------------------------------------------------------------

local function test_get()
    local xy = ov("xy")
    if #xy > 0 then
        local x, y = split(xy)
        g.show("pixel at "..x..","..y.." = "..ov("get "..x.." "..y))
    else
        g.show("mouse is outside overlay")
    end
end

--------------------------------------------------------------------------------

local function test_set()
    ov(op.blue)
    ov("fill")
    ov(op.green)
    -- pixels outside overlay are ignored
    ov("set -1 -1")
    ov("set "..wd.." "..ht)
    local maxx = wd-1
    local maxy = ht-1
    local t1 = os.clock()
    for i = 1, 1000000 do
        ov("set "..rand(0,maxx).." "..rand(0,maxy))
        -- ov(string.format("set %d %d",rand(0,maxx),rand(0,maxy))) -- slower
    end
    g.show("Time to set one million pixels: "..ms(os.clock()-t1))
end

--------------------------------------------------------------------------------

local function test_copy_paste()
    local t1 = os.clock()
    -- tile overlay
    local tilewd = 144
    local tileht = 133
    ov("copy 20 20 "..tilewd.." "..tileht.." tile")
    for y = 0, ht, tileht do
        for x = 0, wd, tilewd do
            ov("paste "..x.." "..y.." tile")
        end
    end
    -- do a simple animation
    local x = 20
    local y = 20
    local boxwd = 105
    local boxht = 83
    ov("copy 0 0 0 0 background")
    ov("rgba 255 255 255 128")
    ov("fill 0 0 "..boxwd.." "..boxht)
    ov("copy 0 0 "..boxwd.." "..boxht.." box")
    while x < wd and y < ht do
        x = x+1
        y = y+1
        ov("paste 0 0 background")
        ov("blend 1")
        ov("paste "..x.." "..y.." box")
        ov("blend 0")
        ov("update")
        -- above is much faster than g.update() but to avoid display glitches
        -- the overlay must cover the current layer and all pixels must be opaque
    end
    g.show("Time to test copy and paste: "..ms(os.clock()-t1))
    ov("freeclip background")
    ov("freeclip box")
end

--------------------------------------------------------------------------------

local function test_save()
    ov("save 0 0 "..wd.." "..ht.." test-save.png")
    g.show("overlay has been saved in test-save.png")
end

--------------------------------------------------------------------------------

local function test_load()
    -- center image in current overlay by 1st loading completely outside it
    -- so we get the image dimensions without changing the overlay
    local imgsize = ov("load "..wd.." "..ht.." test-alpha.png")
    local iw, ih = split(imgsize)
    ov("load "..int((wd-iw)/2).." "..int((ht-ih)/2).." test-alpha.png")
    g.show("Image width and height: "..imgsize)
end

--------------------------------------------------------------------------------

local function test_lines()
    ov(op.blue)
    ov("fill")
    ov(op.red)
    local maxx = wd-1
    local maxy = ht-1
    local t1 = os.clock()
    for i = 1, 1000 do
        ov("line "..rand(0,maxx).." "..rand(0,maxy).." "..rand(0,maxx).." "..rand(0,maxy))
    end
    g.show("Time to draw one thousand lines: "..ms(os.clock()-t1))
end

--------------------------------------------------------------------------------

local function maketext(s)
    -- convert given string to text in current font and return
    -- its width and height etc for later use by pastetext
    local w, h, descent, leading = split(ov("text temp "..s))
    return tonumber(w), tonumber(h), tonumber(descent), tonumber(leading)
end

--------------------------------------------------------------------------------

local function pastetext(x, y, transform)
    transform = transform or op.identity
    -- text background is transparent so paste needs to use alpha blending
    local oldblend = ov("blend 1")
    local oldtransform = ov(transform)
    ov("paste "..x.." "..y.." temp")
    ov("transform "..oldtransform)
    ov("blend "..oldblend)
end

--------------------------------------------------------------------------------

local function test_multiline_text()
    local oldfont = ov("font 10 mono-bold")   -- use a mono-spaced font
    local oldblend = ov("blend 0")
    ov(op.white) -- white background
    ov("fill")
    ov(op.black) -- black text
    ov("blend 1")

    local textstr =
[[
"To be or not to be, that is the question;
Whether 'tis nobler in the mind to suffer
The slings and arrows of outrageous fortune,
Or to take arms against a sea of troubles,
And by opposing, end them. To die, to sleep;
No more; and by a sleep to say we end
The heart-ache and the thousand natural shocks
That flesh is heir to — 'tis a consummation
Devoutly to be wish'd. To die, to sleep;
To sleep, perchance to dream. Ay, there's the rub,
For in that sleep of death what dreams may come,
When we have shuffled off this mortal coil,
Must give us pause. There's the respect
That makes calamity of so long life,
For who would bear the whips and scorns of time,
Th'oppressor's wrong, the proud man's contumely,
The pangs of despised love, the law's delay,
The insolence of office, and the spurns
That patient merit of th'unworthy takes,
When he himself might his quietus make
With a bare bodkin? who would fardels bear,
To grunt and sweat under a weary life,
But that the dread of something after death,
The undiscovered country from whose bourn
No traveller returns, puzzles the will,
And makes us rather bear those ills we have
Than fly to others that we know not of?
Thus conscience does make cowards of us all,
And thus the native hue of resolution
Is sicklied o'er with the pale cast of thought,
And enterprises of great pitch and moment
With this regard their currents turn awry,
And lose the name of action.
Soft you now! The fair Ophelia! Nymph,

in thy orisons be all my sins remember'd."

Test non-ASCII: áàâäãåçéèêëíìîïñóòôöõúùûüæøœÿ
                ÁÀÂÄÃÅÇÉÈÊËÍÌÎÏÑÓÒÔÖÕÚÙÛÜÆØŒŸ
]]

    local t1 = os.clock()
    toggle = 1 - toggle
    if toggle > 0 then
        -- test oplus multiline
        local w, h = op.multiline("multi", textstr)
        ov("paste 0 0 multi")
        ov("freeclip multi")
        g.show("Time to test multiline text: oplus "..ms(os.clock() - t1))
    else
        -- test native multiline
        maketext(textstr)
        pastetext(0, 0)
        g.show("Time to test multiline text: native "..ms(os.clock() - t1))
    end

    ov("font "..oldfont)
    ov("blend "..oldblend)
end

--------------------------------------------------------------------------------

local function test_text()
    local t1 = os.clock()

    local oldfont, oldblend, w, h, descent, leading, nextx
    
    ov(op.white) -- white background
    ov("fill")
    ov(op.black) -- black text
    
    maketext("FLIP Y")
    pastetext(20, 30)
    pastetext(20, 30, op.flip_y)

    maketext("FLIP X")
    pastetext(110, 30)
    pastetext(110, 30, op.flip_x)

    maketext("FLIP BOTH")
    pastetext(210, 30)
    pastetext(210, 30, op.flip)

    maketext("ROTATE CW")
    pastetext(20, 170)
    pastetext(20, 170, op.rcw)

    maketext("ROTATE ACW")
    pastetext(20, 140)
    pastetext(20, 140, op.racw)

    maketext("SWAP XY")
    pastetext(150, 170)
    pastetext(150, 170, op.swap_xy)

    maketext("SWAP XY FLIP")
    pastetext(150, 140)
    pastetext(150, 140, op.swap_xy_flip)

    oldfont = ov("font 7 default")
    w, h, descent, leading = maketext("tiny")
    pastetext(300, 30 - h + descent)
    nextx = 300 + w + 5

    ov("font "..oldfont)    -- restore previous font
    w, h, descent, leading = maketext("normal")
    pastetext(nextx, 30 - h + descent)
    nextx = nextx + w + 5
    
    ov("font 20 default-bold")
    w, h, descent, leading = maketext("Big")
    pastetext(nextx, 30 - h + descent)
    
    ov("font 10 default-bold")
    w = maketext("bold")
    pastetext(300, 40)
    nextx = 300 + w + 5
    
    ov("font 10 default-italic")
    maketext("italic")
    pastetext(nextx, 40)
    
    ov("font 10 mono")
    w, h, descent, leading = maketext("mono")
    pastetext(300, 80 - h + descent)
    nextx = 300 + w + 5
    
    ov("font 12")   -- just change font size
    w, h, descent, leading = maketext("mono12")
    pastetext(nextx, 80 - h + descent)
    
    ov("font 10 mono-bold")
    w = maketext("mono-bold")
    pastetext(300, 90)
    
    ov("font 10 mono-italic")
    maketext("mono-italic")
    pastetext(300, 105)
    
    ov("font 10 roman")
    maketext("roman")
    pastetext(300, 130)
    
    ov("font 10 roman-bold")
    w = maketext("roman-bold")
    pastetext(300, 145)
    
    ov("font 10 roman-italic")
    maketext("roman-italic")
    pastetext(300, 160)
    
    ov("font "..oldfont)    -- restore previous font

    ov(op.red)
    w, h, descent, leading = maketext("RED")
    pastetext(300, 200 - h + descent)
    nextx = 300 + w + 5

    ov(op.green)
    w, h, descent, leading = maketext("GREEN")
    pastetext(nextx, 200 - h + descent)
    nextx = nextx + w + 5

    ov(op.blue)
    w, h, descent, leading = maketext("BLUE")
    pastetext(nextx, 200 - h + descent)

    ov(op.yellow)
    w, h = maketext("Yellow on black [] gjpqy")
    ov(op.black)
    ov("fill 300 210 "..w.." "..h)
    pastetext(300, 210)

    ov(op.yellow)       ov("fill 0   250 100 100")
    ov(op.cyan)         ov("fill 100 250 100 100")
    ov(op.magenta)      ov("fill 200 250 100 100")
    ov("rgba 0 0 0 0")  ov("fill 300 250 100 100")
    
    ov(op.black)
    maketext("The quick brown fox jumps over 123 dogs.")
    pastetext(10, 270)

    ov(op.white)
    maketext("SPOOKY")
    pastetext(310, 270)

    oldfont = ov("font 120 default-bold")
    ov("rgba 255 0 0 40")   -- translucent red text
    w, h, descent = maketext("Golly")
    pastetext(10, 10)
    oldblend = ov("blend 1")
    
    -- draw box around text
    ov("line 10 10 "..(w-1+10).." 10")
    ov("line 10 10 10 "..(h-1+10))
    ov("line "..(w-1+10).." 10 "..(w-1+10).." "..(h-1+10))
    ov("line 10 "..(h-1+10).." "..(w-1+10).." "..(h-1+10))
    -- show baseline
    ov("line 10 "..(h-1+10-descent).." "..(w-1+10).." "..(h-1+10-descent))
    
    -- draw minimal bounding rect over text
    local xoff, yoff, minwd, minht = op.minbox("temp", w, h)
    ov("rgba 0 0 255 20")
    ov("fill "..(xoff+10).." "..(yoff+10).." "..minwd.." "..minht)
    
    -- restore blend state and font
    ov("blend "..oldblend)
    ov("font "..oldfont)
    ov(op.black)

    g.show("Time to test text: "..ms(os.clock()-t1))
end

--------------------------------------------------------------------------------

local function test_fill()
    ov(op.white)
    ov("fill")
    
    toggle = 1 - toggle
    if toggle > 0 then
        ov("blend 1") -- turn on alpha blending
    end
    
    local maxx = wd-1
    local maxy = ht-1
    local t1 = os.clock()
    for i = 1, 1000 do
        ov("rgba "..rand(0,255).." "..rand(0,255).." "..rand(0,255).." "..rand(0,255))
        ov("fill "..rand(0,maxx).." "..rand(0,maxy).." "..rand(100).." "..rand(100))
    end
    g.show("Time to fill one thousand rectangles: "..ms(os.clock()-t1))
    ov("rgba 0 0 0 0")
    ov("fill 10 10 100 100") -- does nothing when alpha blending is on

    if toggle > 0 then
        ov("blend 0") -- turn off alpha blending
    end
end

--------------------------------------------------------------------------------

local function test_blending()
    ov(op.white)
    ov("fill")
    
    toggle = 1 - toggle
    if toggle > 0 then
        ov("blend 1") -- turn on alpha blending
    end
    
    ov("rgba 0 255 0 128") -- 50% translucent green
    for i = 0,9 do
        ov("line "..(20+i).." 20 "..(20+i).." 119")
    end
    for i = 0,9 do
        ov("line 20 "..(130+i).." 139 "..(130+i))
    end
    ov("line 20 145 20 145")    -- single pixel
    ov("set 22 145")
    ov("line 20 149 140 150")
    ov("line 20 150 140 270")
    ov("fill 40 20 100 100")
    
    ov("rgba 255 0 0 128") -- 50% translucent red
    ov("fill 80 60 100 100")
    
    ov("rgba 0 0 255 128") -- 50% translucent blue
    ov("fill 120 100 100 100")
    
    if toggle > 0 then
        ov("blend 0") -- turn off alpha blending
    end
end

--------------------------------------------------------------------------------

local function expand(x, y)
    -- do via some sort of scale command???!!!
end

--------------------------------------------------------------------------------

local function shrink(x, y)
    -- do via some sort of scale command???!!!
end

--------------------------------------------------------------------------------

local errnum = 0

local function test_errors()
    local function force_error()
        if errnum >= 14 then errnum = 0 end -- cycle back to 1st error
        errnum = errnum + 1
        if errnum == 1 then ov("xxx") end
        if errnum == 2 then ov("position yyy") end
        if errnum == 3 then ov("create -1 1") end
        if errnum == 4 then ov("create 1 -1") end
        if errnum == 5 then ov("rgba 1 2 3") end
        if errnum == 6 then ov("rgba -1 0 0 256") end
        if errnum == 7 then ov("load 0 0 unknown.png") end
        if errnum == 8 then ov("save -1 -1 2 2 foo.png") end
        if errnum == 9 then ov("save 0 0 1 1 unsupported.bmp") end
        if errnum == 10 then ov("copy -1 -1 2 2 foo") end
        if errnum == 11 then ov("copy 0 0 1 1 ") end
        if errnum == 12 then ov("paste 0 0 badname") end
        if errnum == 13 then ov("cursor xxx") end
        if errnum == 14 then ov("text foo") end
    end
    local status, err = pcall(force_error)
    if err then
        -- show error message now and not when script finishes
        g.warn(err)
        g.continue("")
    end
end

--------------------------------------------------------------------------------

local function main()
    g.show("Testing overlay (type h for help)...")
    create_overlay(500, 300)

    local mousedown = false
    local prevx, prevy
    
    while true do
        local event = g.getevent()
        if event:find("^key") then
            local _, ch, mods = split(event)
            if ch == 'e' then
                test_errors()
            elseif ch == 'b' then
                test_blending()
            elseif ch == 'c' and mods == 'none' then
                test_cursors()
            elseif ch == 'c' and mods == 'shift' then
                -- create an overlay covering entire layer
                create_overlay(g.getview(g.getlayer()))
            elseif ch == 'f' then
                test_fill()
            elseif ch == 'g' then
                test_get()
            elseif ch == 'i' then
                test_load()
            elseif ch == 'w' then
                test_save()
            elseif ch == 'p' then
                test_positions()
            elseif ch == 's' then
                test_set()
            elseif ch == 't' then
                test_text()
            elseif ch == 'm' then
                test_multiline_text()
            elseif ch == 'l' then
                test_lines()
            elseif ch == 'v' then
                test_copy_paste()
            elseif ch == 'h' then
                show_help()
            else
                g.doevent(event)
            end
            g.update()
        elseif event:find("^oclick") then
            local _, x, y, button, mods = split(event)
            ov(op.white)
            if mods:find("alt") then
                ov("flood "..x.." "..y)
            else
                ov("set "..x.." "..y)
                mousedown = true
                prevx = x
                prevy = y
            end
            g.update()
        elseif event:find("^mup") then
            mousedown = false
        elseif event:find("^ozoomin") then
            local _, x, y = split(event)
            expand(tonumber(x), tonumber(y))
        elseif event:find("^ozoomout") then
            local _, x, y = split(event)
            shrink(tonumber(x), tonumber(y))
        elseif #event > 0 then
            g.doevent(event)
        end
        
        local xy = ov("xy")
        if #xy > 0 and mousedown then
            local x, y = split(xy)
            if x ~= prevx or y ~= prevy then
                ov("line "..prevx.." "..prevy.." "..x.." "..y)
                prevx = x
                prevy = y
                g.update()
            end
        end
    end
end

--------------------------------------------------------------------------------

local oldoverlay = g.setoption("showoverlay", 1)
local oldbuttons = g.setoption("showbuttons", 0) -- disable translucent buttons

local status, err = pcall(main)
if err then g.continue(err) end
-- the following code is always executed

ov("delete")
g.setoption("showoverlay", oldoverlay)
g.setoption("showbuttons", oldbuttons)
