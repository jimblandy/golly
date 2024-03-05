-- Creates a large set of pentadecathlon + glider collisions.
-- Based on pd_glider.py from PLife (http://plife.sourceforge.net/).

local g = golly()
local gp = require "gplus"
local gpo = require "gplus.objects"

g.new("pd-glider")
-- best to create empty universe before setting rule
-- to avoid converting an existing pattern (slow if large)
g.setrule("B3/S23")

local function collision(i, j)
    return gpo.pentadecathlon + gpo.glider[i + 11].t(-8 + j, -10, gp.flip)
end

local all = gp.pattern()
for i = -7, 7 do
    for j = -9, 9 do
        all = all + collision(i, j).t(100 * i, 100 * j)
    end
end

all.display("") -- don't change name
