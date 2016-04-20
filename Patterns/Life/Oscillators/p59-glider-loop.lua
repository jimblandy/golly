-- Smaller variant of David Bell's p59 glider loop from 2006 May 28.
-- Author: Dave Greene, 30 May 2006.
-- Conversion to Lua by Andrew Trevorrow, Apr 2016.

local g = golly()
-- require "gplus.strict"
local gp = require "gplus"
local pattern = gp.pattern 
local gpo = require "gplus.objects"
local gpt = require "gplus.text"

g.new("p59 track")
g.setrule("B3/S23")

local fast_eater = pattern("2o$bo2b2o$bobobo$2bo2$4bo$3bobo$4bo$b3o$bo!", -9, -2)

-- create an array specifying half of a length-6608 Herschel track
local path = {}
path[1] = gpo.hc112
for i = 1, 14 do path[#path + 1] = gpo.hc77 end
for i = 1, #path do path[#path + 1] = path[i] end
for i = 1, 12 do path[#path + 1] = gpo.hc77 end

local T = {0, 0, gp.identity}
local p59_track = pattern(gpo.herschel)

g.show("Building p59 Herschel track -- hang on a minute...")
for i = 1, #path do path[#path + 1] = path[i] end
for _, h in ipairs(path) do
    p59_track = p59_track + h.t(T) + fast_eater.t(T)
    T = gp.compose(h.transform, T)
end

-- add 111 more Herschels to template Herschel track, 59 generations apart
for i = 1, 111 do
    p59_track = p59_track[59] + gpo.herschel
end
p59_track = p59_track + pattern("o!", -10, 589) -- get rid of eater

-- add 9 gliders in same spot, 59 generations apart
local glider = pattern("2o$b2o$o!", -16, 595)
for i = 1, 9 do
    p59_track = (p59_track + glider)[59]
end

g.show("")
local all = p59_track
for i = 1, 3 do
    all = all.t(-720, 450, gp.rccw) + p59_track
end

all = all + gpt.maketext('59').t(-142, 596)
all.display("")
