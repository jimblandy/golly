-- Bill Gosper's pure-period p1100 double MWSS gun, circa 1984.

local g = golly()
local gp = require "gplus"
local pattern = gp.pattern
local flip = gp.flip
local flip_x = gp.flip_x
local flip_y = gp.flip_y
local rccw = gp.rccw
local rcw = gp.rcw
local swap_xy_flip = gp.swap_xy_flip

g.new("P1100 gun")
g.setalgo("HashLife")
g.setrule("B3/S23")

-- update status bar now so we don't see different colors when g.show is called
g.update()

local glider = pattern("bo$bbo$3o!")
local block = pattern("oo$oo!")
local eater = pattern("oo$bo$bobo$bboo!")
local bhept = pattern("bbo$boo$oo$boo!")

local twobits = eater + bhept.t(8,3)
local half = twobits + twobits[1].t(51,0,flip_x)
local centinal = half + half.t(0,16,flip_y)
local passthrough = centinal[1].t(16,3,rcw) + centinal[19].t(52,0,rcw)
   + centinal[81].t(55,51,rccw) + centinal[99].t(91,54,rccw)

-- build the source signal -- the most compact set of gliders
-- from which all other gliders and spaceships can be generated
local MWSSrecipes = glider.t(5759,738,rccw) + glider.t(6325,667,flip_x)
    + glider[3].t(5824,896,flip_x) + glider[2].t(5912,1264)
    + glider[2].t(6135,1261,flip_x) + glider[1].t(5912,1490,flip_y)
    + glider.t(6229,4717,flip_x) + glider[1].t(6229,5029,flip)
    + glider[1].t(5920,5032,flip_y) + glider.t(6230,5188,flip)
    + glider[3].t(6230,5306,flip) + glider[3].t(5959,5309,flip_y)

-- suppress output MWSSes as long as gliders are being added
MWSSrecipes = MWSSrecipes + pattern("o!",6095,-65) + pattern("o!",6075,228)

-- add the first four centinals to guide the recipe gliders
local all = centinal[44].t(6185,5096,flip_x) + centinal[73].t(5897,1066)
all = all + centinal[42].t(5782,690) + centinal[25].t(5793,897,rcw)

-- generate the MWSSes for the glider-fanout ladder
for i = 1, 7 do
    g.show("Building rung " .. i .. " of ladder...")
    all = (all + MWSSrecipes)[1100]
end

-- add the actual glider-fanout ladder -- six rungs
for i = 0, 5 do
    all = all + glider.t(6030,1706+550*i,swap_xy_flip)
        + centinal[15].t(6102,1585+550*i)
        + block.t(6029,1721+550*i) + centinal[34].t(5996,1725+550*i,rccw)
        + block.t(6087,1747+550*i) + centinal[87].t(6122,1745+550*i,rcw)
end

-- add the rest of the centinals to guide the ladder's output gliders
g.show("Adding centinal reflectors...")
all = all + centinal[88].t(5704,0) + centinal[29].t(6423,295,flip_x)
   + centinal[74].t(5616,298) + centinal[40].t(6361,613,rcw)
   + centinal[23].t(6502,620,flip_x) + centinal[36].t(5636,986)
   + centinal[38].t(6370,1008,rcw) + centinal[19].t(5747,1347,rcw)
   + centinal[67].t(5851,1516) + centinal.t(4061,2605,rccw)
   + centinal[10].t(5376,3908,rccw) + centinal[77].t(8191,4407,flip_x)
   + centinal[6].t(4988,4606) + centinal[34].t(6357,4608,flip_x)
   + centinal[27].t(8129,4621,flip_x) + centinal[92].t(5159,5051)
   + centinal[53].t(7991,5201,flip_x) + centinal[94].t(7038,5370,rccw)
   + centinal[13].t(5591,5379,rccw) + centinal[3].t(5858,5428,rccw)
   + centinal[87].t(7805,5511,flip_x) + centinal[98].t(401,5557,rccw)
   + centinal[14].t(955,5561,rccw) + centinal[8].t(6592,5584,rccw)
   + centinal[39].t(6933,5698,flip_x) + centinal[32].t(6230,5881)
   + centinal[47].t(11676,5854,rccw) + centinal[68].t(0,5748,rccw)
   + centinal[89].t(6871,5912,flip_x) + centinal[45].t(12095,6027,rccw)
   + centinal[86].t(6209,6134) + centinal[55].t(6868,6357,flip_x)
   + centinal[95].t(9939,6491,rccw) + centinal[23].t(8782,6548,rccw)
   + centinal[58].t(3066,6572,rccw) + centinal[21].t(9326,6596,rccw)
   + centinal[80].t(3628,6626,rccw) + centinal[45].t(6821,6528,flip_x)
   + centinal[33].t(10373,6649,rccw) + centinal[16].t(2587,6685,rccw)

--  to change behavior at center, comment out one of the lines below
all = all + eater.t(6018,5924,rccw) + eater.t(6037,5943,rccw) -- true p1100 gun
-- all = all + block.t(6018,6787) -- synch center to recreate original p1100x5

local center = block.t(1081,6791) + block.t(2731,6791) + block.t(3831,6791)
   + block.t(9108,6791) + block.t(10208,6791) + block.t(11308,6791)
   + passthrough.t(8475,6737) + passthrough[39].t(3365,6737,flip_x)
   + passthrough.t(9575,6737) + passthrough[39].t(2265,6737,flip_x)
   + passthrough.t(10675,6737) + passthrough[39].t(1715,6737,flip_x)

-- add asymmetric Equator to mirror-symmetric North and South
MWSSrecipes = MWSSrecipes + MWSSrecipes.t(0,13583,flip_y)
all = all + all.t(0,13583,flip_y) + center

all.put()
g.fit()

-- Different glider paths are different lengths, so incomplete
-- glider recipes must be overwritten for a while to prevent disaster.
for i = 0, 45 do
    g.show("Filling glider tracks -- " .. (49500 - i*1100) .. " ticks left.")
    g.update()
    MWSSrecipes.put()
    g.run(1100)
end
g.show("")

-- reset gen count to 0
g.setgen("0")
