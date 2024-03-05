-- Based on bricklayer.py from PLife (http://plife.sourceforge.net/).

local g = golly()
local gp = require "gplus"
local pattern = gp.pattern

g.new("bricklayer")
-- best to create empty universe before setting rule
-- to avoid converting an existing pattern (slow if large)
g.setrule("B3/S23")

local p22_half = pattern("2o$bo$bobo$2b2o3bo$6bob2o$5bo4bo$6bo3bo$7b3o!")
local p22 = p22_half + p22_half.t(26, 9, gp.flip)

local gun22 = p22 + p22[1].t(-18, 11)
local gun154 = gun22[27] + gun22[5].t(49, 12, gp.rcw) + gun22.t(5, 53, gp.flip_y)

local p7_reflector = pattern([[
2b2o5b2o$2b2o5bo$7bobo$7b2o$3b2o$3bobo$4bo3$13bo$10bo2b3o3b2o$3b5o
2b3o3bo2bo$b3obob3o4b2obobo$o4bo4b4obobo2b2o$2ob2ob4o3bobob2obo$
4bobobobobo2bo2bobo$4bobobo2bo2bob3ob2o$3b2obob4o6bobo$7bo4b6o2b
o$9bo2bo2bo4b2o$8b2o!]])
local pre_lom = pattern("2bo$2ob2o$2ob2o2$b2ob2o$b2ob2o$3bo!")

local all = gun154[210].t(-52, -38) + gun154[254].t(52, -38, gp.flip_x) +
    p7_reflector.t(8, 23) + pre_lom.t(-3, 30)

all.display("") -- don't change name
