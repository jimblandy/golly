-- Run the current selection for a given number of steps and
-- create an animated GIF file using the current layer's colors.
-- Based on giffer.pl by Tony Smith.
-- Conversion to Lua by Andrew Trevorrow and Scorbie.

local g = golly()
local gp = require "gplus"
local getcell = g.getcell
local int = gp.int
local chr = string.char

local r = g.getselrect()
if #r == 0 then g.exit("There is no selection.") end

local x, y, width, height = table.unpack(r)
if width >= 65536 or height >= 65536 then
    g.exit("The width and height of the selection must be less than 65536.")
end

-- get the parameters given last time
local inifilename = g.getdir("data").."giffer.ini"
local oldparams = "100 1"
local f = io.open(inifilename, "r")
if f then
    oldparams = f:read("*l") or ""
    f:close()
end

local prompt = [[
Enter the number of frames, and the pause
time between each frame (in centisecs):
]]
local answer = g.getstring(prompt, oldparams, "Create animated GIF")
local frames, pause = gp.split(answer)

-- validate given parameters
frames = frames or 100
pause = pause or 1
if not gp.validint(frames) then g.exit("Bad frames value: "..frames) end
if not gp.validint(pause) then g.exit("Bad pause value: "..pause) end
frames = tonumber(frames)
pause = tonumber(pause)

-- given parameters are valid so save them for next run
f = io.open(inifilename, "w")
if f then
    f:write(answer)
    f:close()
end

local depth = g.numstates()
local bitdepth = 0
while depth > 2^(bitdepth+1) do bitdepth = bitdepth + 1 end
local codesize = 2
if bitdepth > 0 then codesize = bitdepth + 1 end

--------------------------------------------------------------------------------

local function getframedata()
    local data = {}
    for row = y, y+height-1 do
        for col = x, x+width-1 do
            data[#data+1] = chr( getcell(col, row) )
        end
    end
    return data
end

--------------------------------------------------------------------------------

local function compress(data)
    local t = {}
    for i = 0, depth-1 do t[chr(i)] = i end
    local curr = 2^codesize
    local cc = 2^codesize
    local used = cc + 1
    local bits = codesize + 1
    local size = codesize + 1
    local mask = 2^size - 1
    local output = ""
    local code = ""
    for _, nextch in ipairs(data) do
        if t[code .. nextch] then
            code = code .. nextch
        else
            used = used + 1
            t[code .. nextch] = used
            curr = curr + (t[code] << bits)
            bits = bits + size
            while bits >= 8 do
                output = output .. chr(curr & 255)
                curr = curr >> 8
                bits = bits - 8
            end
            if used > mask then
                if size < 12 then
                    size = size + 1
                    mask = mask*2 + 1
                else
                    curr = curr + (cc << bits)
                    bits = bits + size
                    while bits >= 8 do
                        output = output .. chr(curr & 255)
                        curr = curr >> 8
                        bits = bits - 8
                    end
                    -- reset t
                    t = {}
                    for j = 0, depth-1 do t[chr(j)] = j end
                    used = cc + 1
                    size = codesize + 1
                    mask = 2^size - 1
                end
            end
            code = nextch
        end
    end
    curr = curr + (t[code] << bits)
    bits = bits + size
    while bits >= 8 do
        output = output .. chr(curr & 255)
        curr = curr >> 8
        bits = bits - 8
    end
    output = output .. chr(curr)
    local subbed = ""
    while #output > 255 do
        subbed = subbed .. chr(255) .. output:sub(1,255)
        output = output:sub(256)
    end
    return subbed .. chr(#output) .. output .. chr(0)
end

--------------------------------------------------------------------------------

-- This function packs a list of +ve integers into a binary string using
-- little-endian ordering.  The format parameter is a string composed of
-- ASCII digits which specify the size in bytes of the corresponding value.
-- Based on code at http://lua-users.org/wiki/ReadWriteFormat.

local function mypack(format, ...)
    local result = ""
    local values = {...}
    for i = 1, #format do
        local size = tonumber(format:sub(i,i))
        local value = values[i]
        local str = ""
        for j = 1, size do
            str = str .. chr(value % 256)
            value = math.floor(value / 256)
        end
        result = result .. str
    end
    return result
end

--------------------------------------------------------------------------------

-- For information on GIF format:
-- http://www.matthewflickinger.com/lab/whatsinagif/bits_and_bytes.asp

local function savegiffile(gifpath)
    local header = "GIF89a"
    local screendesc = mypack("22111", width, height, 0xF0 + bitdepth, 0, 0)

    local colortable = ""
    for state = 0, depth-1 do
        local s, r, g, b = table.unpack(g.getcolors(state))
        colortable = colortable .. mypack("111", r, g, b)
    end
    local fullength = 6 * 2^bitdepth
    while #colortable < fullength do
        colortable = colortable .. mypack("111", 0, 0, 0)
    end

    -- this application extension allows looping
    local applicext = "\x21\xFF\x0BNETSCAPE2.0" .. mypack("1121", 3, 1, 0, 0)

    local controlext = "\x21\xF9" .. mypack("11211", 4, 0, pause, 0, 0)
    local imagedesc = "," .. mypack("222211", 0, 0, width, height, 0, codesize)

    local gif = io.open(gifpath,"wb")
    if not gif then g.exit("Unable to create GIF file: "..gifpath) end

    gif:write(header)
    gif:write(screendesc)
    gif:write(colortable)
    gif:write(applicext)

    for f = 1, frames do
        -- write the data for this frame
        gif:write(controlext)
        gif:write(imagedesc)
        gif:write(compress(getframedata()))
        g.show("Frame: "..f.."/".. frames)
        g.step()
        g.update()
    end

    gif:write("\x3B")
    gif:close()

    g.show("GIF animation saved in "..gifpath)
end

--------------------------------------------------------------------------------

-- set initial directory for the save dialog
local initdir = g.getdir("app")
local savedirfile = g.getdir("data").."gifdir.ini"
local f = io.open(savedirfile, "r")
if f then
    initdir = f:read("*l") or ""
    f:close()
end

-- remove any existing extension from layer name and append .gif
local initfile = gp.split(g.getname(),"%.")..".gif"

-- prompt for file name and location
local gifpath = g.savedialog("Save as GIF file", "GIF (*.gif)|*.gif",
                             initdir, initfile)
if #gifpath == 0 then g.exit() end

-- save directory for next time
f = io.open(savedirfile, "w")
if f then
    local pathsep = g.getdir("app"):sub(-1)
    f:write(gifpath:gsub("[^"..pathsep.."]+$",""))
    f:close()
end

savegiffile(gifpath)
