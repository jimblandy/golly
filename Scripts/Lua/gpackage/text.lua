-- This module is loaded if a script calls require "gpackage.text".

local g = gollylib()

local m = {}

--------------------------------------------------------------------------------

-- create a mono-spaced ASCII font

local mfont = {}
mfont[' '] = g.parse("")
mfont['!'] = g.parse("2bo$2bo$2bo$2bo$2bo2$2bo!")
mfont['"'] = g.parse("bobo$bobo$bobo!")
mfont['#'] = g.parse("bobo$bobo$5o$bobo$5o$bobo$bobo!")
mfont['$'] = g.parse("b3o$obobo$obo$b3o$2bobo$obobo$b3o!")
mfont['%'] = g.parse("2o2bo$2o2bo$3bo$2bo$bo$o2b2o$o2b2o!")
mfont['&'] = g.parse("b2o$o2bo$o2bo$b2o$o2bo$o2bo$b2obo!")
mfont["'"] = g.parse("2bo$2bo$2bo!")
mfont['('] = g.parse("3bo$2bo$2bo$2bo$2bo$2bo$3bo!")
mfont[')'] = g.parse("bo$2bo$2bo$2bo$2bo$2bo$bo!")
mfont['*'] = g.parse("$obobo$b3o$5o$b3o$obobo!")
mfont['+'] = g.parse("$2bo$2bo$5o$2bo$2bo!")
mfont[','] = g.parse("6$2bo$2bo$bo!")
mfont['-'] = g.parse("3$5o!")
mfont['.'] = g.parse("6$2bo!")
mfont['/'] = g.parse("3bo$3bo$2bo$2bo$2bo$bo$bo!")
mfont['0'] = g.parse("b3o$o3bo$o2b2o$obobo$2o2bo$o3bo$b3o!")
mfont['1'] = g.parse("2bo$b2o$2bo$2bo$2bo$2bo$b3o!")
mfont['2'] = g.parse("b3o$o3bo$4bo$3bo$2bo$bo$5o!")
mfont['3'] = g.parse("b3o$o3bo$4bo$2b2o$4bo$o3bo$b3o!")
mfont['4'] = g.parse("3bo$2b2o$bobo$o2bo$5o$3bo$3bo!")
mfont['5'] = g.parse("5o$o$o$b3o$4bo$o3bo$b3o!")
mfont['6'] = g.parse("b3o$o$o$4o$o3bo$o3bo$b3o!")
mfont['7'] = g.parse("5o$4bo$3bo$2bo$bo$o$o!")
mfont['8'] = g.parse("b3o$o3bo$o3bo$b3o$o3bo$o3bo$b3o!")
mfont['9'] = g.parse("b3o$o3bo$o3bo$b4o$4bo$4bo$b3o!")
mfont[':'] = g.parse("2$2bo4$2bo!")
mfont[';'] = g.parse("2$2bo4$2bo$2bo$bo!")
mfont['<'] = g.parse("$3bo$2bo$bo$2bo$3bo!")
mfont['='] = g.parse("2$5o2$5o!")
mfont['>'] = g.parse("$bo$2bo$3bo$2bo$bo!")
mfont['?'] = g.parse("b3o$o3bo$4bo$2b2o$2bo2$2bo!")
mfont['@'] = g.parse("b3o$o3bo$ob3o$obobo$ob2o$o$b3o!")
mfont['A'] = g.parse("b3o$o3bo$o3bo$5o$o3bo$o3bo$o3bo!")
mfont['B'] = g.parse("4o$o3bo$o3bo$4o$o3bo$o3bo$4o!")
mfont['C'] = g.parse("b3o$o3bo$o$o$o$o3bo$b3o!")
mfont['D'] = g.parse("4o$o3bo$o3bo$o3bo$o3bo$o3bo$4o!")
mfont['E'] = g.parse("5o$o$o$3o$o$o$5o!")
mfont['F'] = g.parse("5o$o$o$3o$o$o$o!")
mfont['G'] = g.parse("b3o$o3bo$o$o2b2o$o3bo$o3bo$b3o!")
mfont['H'] = g.parse("o3bo$o3bo$o3bo$5o$o3bo$o3bo$o3bo!")
mfont['I'] = g.parse("b3o$2bo$2bo$2bo$2bo$2bo$b3o!")
mfont['J'] = g.parse("2b3o$3bo$3bo$3bo$3bo$o2bo$b2o!")
mfont['K'] = g.parse("o3bo$o2bo$obo$2o$obo$o2bo$o3bo!")
mfont['L'] = g.parse("o$o$o$o$o$o$5o!")
mfont['M'] = g.parse("o3bo$2ob2o$obobo$obobo$o3bo$o3bo$o3bo!")
mfont['N'] = g.parse("o3bo$2o2bo$obobo$o2b2o$o3bo$o3bo$o3bo!")
mfont['O'] = g.parse("b3o$o3bo$o3bo$o3bo$o3bo$o3bo$b3o!")
mfont['P'] = g.parse("4o$o3bo$o3bo$4o$o$o$o!")
mfont['Q'] = g.parse("b3o$o3bo$o3bo$o3bo$obobo$o2bo$b2obo!")
mfont['R'] = g.parse("4o$o3bo$o3bo$4o$o2bo$o3bo$o3bo!")
mfont['S'] = g.parse("b3o$o3bo$o$b3o$4bo$o3bo$b3o!")
mfont['T'] = g.parse("5o$2bo$2bo$2bo$2bo$2bo$2bo!")
mfont['U'] = g.parse("o3bo$o3bo$o3bo$o3bo$o3bo$o3bo$b3o!")
mfont['V'] = g.parse("o3bo$o3bo$o3bo$o3bo$o3bo$bobo$2bo!")
mfont['W'] = g.parse("o3bo$o3bo$o3bo$obobo$obobo$2ob2o$o3bo!")
mfont['X'] = g.parse("o3bo$o3bo$bobo$2bo$bobo$o3bo$o3bo!")
mfont['Y'] = g.parse("o3bo$o3bo$bobo$2bo$2bo$2bo$2bo!")
mfont['Z'] = g.parse("5o$4bo$3bo$2bo$bo$o$5o!")
mfont['['] = g.parse("2b2o$2bo$2bo$2bo$2bo$2bo$2b2o!")
mfont['\\'] = g.parse("bo$bo$2bo$2bo$2bo$3bo$3bo!")
mfont[']'] = g.parse("b2o$2bo$2bo$2bo$2bo$2bo$b2o!")
mfont['^'] = g.parse("2bo$bobo$o3bo!")
mfont['_'] = g.parse("6$5o!")
mfont['`'] = g.parse("o$bo!")
mfont['a'] = g.parse("2$b4o$o3bo$o3bo$o3bo$b4o!")
mfont['b'] = g.parse("o$o$4o$o3bo$o3bo$o3bo$4o!")
mfont['c'] = g.parse("2$b4o$o$o$o$b4o!")
mfont['d'] = g.parse("4bo$4bo$b4o$o3bo$o3bo$o3bo$b4o!")
mfont['e'] = g.parse("2$b3o$o3bo$5o$o$b4o!")
mfont['f'] = g.parse("2b2o$bo2bo$bo$3o$bo$bo$bo!")
mfont['g'] = g.parse("2$b3o$o3bo$o3bo$o3bo$b4o$4bo$b3o!")
mfont['h'] = g.parse("o$o$ob2o$2o2bo$o3bo$o3bo$o3bo!")
mfont['i'] = g.parse("$2bo2$2bo$2bo$2bo$2b2o!")
mfont['j'] = g.parse("$3bo2$3bo$3bo$3bo$3bo$o2bo$b2o!")
mfont['k'] = g.parse("o$o$o2bo$obo$3o$o2bo$o3bo!")
mfont['l'] = g.parse("b2o$2bo$2bo$2bo$2bo$2bo$2b2o!")
mfont['m'] = g.parse("2$bobo$obobo$obobo$o3bo$o3bo!")
mfont['n'] = g.parse("2$4o$o3bo$o3bo$o3bo$o3bo!")
mfont['o'] = g.parse("2$b3o$o3bo$o3bo$o3bo$b3o!")
mfont['p'] = g.parse("2$4o$o3bo$o3bo$o3bo$4o$o$o!")
mfont['q'] = g.parse("2$b4o$o3bo$o3bo$o3bo$b4o$4bo$4bo!")
mfont['r'] = g.parse("2$ob2o$2o2bo$o$o$o!")
mfont['s'] = g.parse("2$b4o$o$b3o$4bo$4o!")
mfont['t'] = g.parse("$2bo$5o$2bo$2bo$2bo$3b2o!")
mfont['u'] = g.parse("2$o3bo$o3bo$o3bo$o3bo$b4o!")
mfont['v'] = g.parse("2$o3bo$o3bo$o3bo$bobo$2bo!")
mfont['w'] = g.parse("2$o3bo$o3bo$obobo$2ob2o$o3bo!")
mfont['x'] = g.parse("2$o3bo$bobo$2bo$bobo$o3bo!")
mfont['y'] = g.parse("2$o3bo$o3bo$o3bo$o3bo$b4o$4bo$b3o!")
mfont['z'] = g.parse("2$5o$3bo$2bo$bo$5o!")
mfont['{'] = g.parse("3bo$2bo$2bo$bo$2bo$2bo$3bo!")
mfont['|'] = g.parse("2bo$2bo$2bo$2bo$2bo$2bo$2bo!")
mfont['}'] = g.parse("bo$2bo$2bo$3bo$2bo$2bo$bo!")
mfont['~'] = g.parse("2$bo$obobo$3bo!")

--------------------------------------------------------------------------------

-- return a rect which is the minimal bounding box of the given pattern
-- note that the pattern is two-state so we don't have to worry about
-- getting a multi-state cell array here

local function getminbox(cells)
    local len = #cells
    if len < 2 then return {} end
    
    local minx = cells[1]
    local miny = cells[2]
    local maxx = minx
    local maxy = miny
    for x = 1, len, 2 do
        if cells[x] < minx then minx = cells[x] end
        if cells[x] > maxx then maxx = cells[x] end
    end
    for y = 2, len, 2 do
        if cells[y] < miny then miny = cells[y] end
        if cells[y] > maxy then maxy = cells[y] end
    end
    
    return {minx, miny, maxx - minx + 1, maxy - miny + 1}
end

--------------------------------------------------------------------------------

-- convert given string to a cell array using above mono-spaced font
-- and also return the array's width and height

function m.maketext(s)
    local p = {}
    local x = 0
    for ch in string.gmatch(s, ".[\128-\191]*") do
        if not mfont[ch] then ch = '?' end
        local symbol = g.transform(mfont[ch], x, 0)
        p = g.join(p, symbol)
        x = x + 6
    end
    local tbox = getminbox(p)
    return p, tbox[3], tbox[4]
end

--------------------------------------------------------------------------------

return m
