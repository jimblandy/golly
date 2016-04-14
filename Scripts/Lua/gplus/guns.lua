-- This module is loaded if a script calls require "gplus.guns".

local g = golly()

local gp = require "gplus"
local pattern = gp.pattern
local flip = gp.flip
local flip_x = gp.flip_x
local flip_y = gp.flip_y
local rccw = gp.rccw

local gpo = require "gplus.objects"
local block = gpo.block
local eater = gpo.eater
local queenbee = gpo.queenbee
local bheptomino = gpo.bheptomino

local m = {}

-- better to assume that caller has set Life rule to evolve phases
-- gp.setrule("B3/S23")

--------------------------------------------------------------------------------

-- an aligned gun shooting SE:

m.gun24 = pattern([[
23bo2bo$21b6o$17b2obo8bo$13b2obobobob8o2bo$11b3ob2o3bobo7b3o$10bo
4b3o2bo3bo3b2o$11b3o3b2ob4obo3bob2o$12bobo3bo5bo4bo2bo4b2obo$10b
o8bob2o2b2o2b2o5bob2obo$10b5ob4obo4b3o7bo4bo$15b2o4bo4bob3o2b2ob
ob2ob2o$12b5ob3o4b2ob2o3bobobobobo$11bo5b2o4b2obob2o5bo5bo$12b5o
6b2obo3bo3bobob2ob2o$2ob2o9b2o2bo5bobo4bo2b3obobo$bobobobob2o3b3o
bo6bo2bobo4b3o2bo$o2bo7bo6b2o3b3o8bobob2o$3o2bo4b2o11bo10bo$5b4o
bo17b2o4b2o$2b2obo6bo14bo3bo2b2o$bo4bo3bo16bo6b2o$b3obo4bo16bo3b
o2bo$11bo2bo3bo9b2o4bobob2o$b3obo4bo8b2o3bo10b3o2bo$bo4bo3bo7bo6b
o8b3obobo$2b2obo6bo10b3o8bobob2ob2o$5b4obo24bo5bo$3o2bo4b2o21bob
obobobo$o2bo7bo9b2o10b2obob2ob2o$bobobobob2o10bo8bo5bo4bo$2ob2o17b
3o6bo4bob2obo$24bo4b3o5b2obo!]], -32, -32)

--------------------------------------------------------------------------------

local lgun30 = queenbee + block.t(12, 2) +
               (queenbee[5] + block.t(12, 2)).t(-9, 2, flip_x)
local lgun30_asym = queenbee + eater.t(10, 1, rccw) +
                    (queenbee[5] + block.t(12, 2)).t(-9, 2, flip_x)

m.gun30   = lgun30.t(1, -7)          -- aligned gun shooting SE
m.gun30_a = lgun30_asym.t(1, -7)     -- aligned gun shooting SE

m.gun60 = m.gun30 + m.gun30_a[26].t(-4, 11, flip_y)

--------------------------------------------------------------------------------

local lhalf = bheptomino.t(0, 2, flip_x) + bheptomino.t(0, -2, flip) +
              block.t(16, -4) + block.t(16, 3)

m.gun46_double = lhalf.t(7, -2) + lhalf.t(-8, 2, flip_x)

m.gun46 = lhalf[1].t(1, -7) + lhalf.t(-13, -4, flip_x)  -- aligned version shooting SE

--------------------------------------------------------------------------------

return m
