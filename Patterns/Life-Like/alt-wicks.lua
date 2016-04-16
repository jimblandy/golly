-- Based on alt_wicks.py from PLife (http://plife.sourceforge.net/).

local g = golly()
local gp = require "gplus"
local pattern = gp.pattern

g.new("alt-wicks")
-- best to create empty universe before setting rule
-- to avoid converting an existing pattern (slow if large)
g.setrule("B3/S135")

local head_a_c3 = pattern([[
23bo6bo$5bo3b2o2bobobo4b2o2b6ob2obobo$3b2o2bo6bobobo3bob3o2b2obo2b
obobo$2b2o4bobo2bo2bobobob3o4bo$4bo2bo2bo2b3obo11bo$2obo8b3ob2o2b
o2b2obo$4bo2bo2bo2b3obo11bo$2b2o4bobo2bo2bobobob3o4bo$3b2o2bo6bo
bobo3bob3o2b2obo2bobobo$5bo3b2o2bobobo4b2o2b6ob2obobo$23bo6bo!
]], -5, 39, gp.rccw)

local tail_a_c5 = pattern([[
5bobo$6bo$4bo3bo$5bobo$4bo3bo$o11bo$3bo5bo$2obo5bob2o$bo9bo2$2bo7b
o$3bo5bo$2bo7bo!]], -6, 0)

local head_b_c5 = pattern([[
2bobo3bobo$bo3bobo3bo$2bobo3bobo$b2ob2ob2ob2o$2b3o3b3o$2b3o3b3o$3b
2o3b2o$2b4ob4o$bo9bo$4bo3bo$5bobo$2obo5bob2o$2o2bo3bo2b2o$bobo5b
obo$2b2obobob2o2$6bo$2bobo3bobo$2b2o2bo2b2o$3b2o3b2o!]], -6, 0)

local converter_bc_c5 = pattern([[
6bobo$6bobo2$4b3ob3o$4bo5bo$4b2o3b2o$4bobobobo$3bo2bobo2bo$5bo3bo
$4bobobobo$bo3b2ob2o3bo$2bo3bobo3bo$b2o3bobo3b2o$3bob2ob2obo$2b2o
7b2o2$4bo5bo$4bo5bo$ob2obo3bob2obo$o13bo2$3b2o5b2o$5bo3bo2$4b3ob
3o$2b5ob5o3$2bo3bobo3bo$3bobo3bobo$2bo3bobo3bo!]], -7, 0)

local tail_c_c5 = pattern([[
2bo9bo$bobobo3bobobo$4b3ob3o$6bobo$3b2ob3ob2o$3bo2bobo2bo$3bobo3b
obo$2bo2b5o2bo$bo2b7o2bo$bo3bobobo3bo$6obob6o$2o11b2o$2b3obobob3o
$b2obobobobob2o$5bo3bo$4b3ob3o$3b3o3b3o$5bo3bo$5b2ob2o$obobo5bob
obo$bobo3bo3bobo$bo2bo5bo2bo$5bo3bo$6bobo$5bo3bo!]], -7, 0)

local A = head_a_c3 + tail_a_c5.t(0, -13)

local B = (head_b_c5[84] + converter_bc_c5.t(0, -31))[100] + tail_c_c5.t(0, -56)

local all = A.t(-25, 0) + B.t(24, 0)

all.display("") -- don't change name
