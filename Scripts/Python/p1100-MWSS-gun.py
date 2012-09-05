# Bill Gosper's pure-period p1100 double MWSS gun, circa 1984.

import golly as g
from glife import *

g.new("P1100 gun")
g.setalgo("HashLife")
g.setrule("B3/S23")

# update status bar now so we don't see different colors when
# g.show is called
g.update()

glider = pattern("bo$bbo$3o!")
block = pattern("oo$oo!")
eater = pattern("oo$bo$bobo$bboo!")
bhept = pattern("bbo$boo$oo$boo!")

twobits = eater + bhept(8,3)
half = twobits + twobits[1](51,0,flip_x)
centinal = half + half(0,16,flip_y)
passthrough = centinal[1](16,3,rcw) + centinal[19](52,0,rcw) \
   + centinal[81](55,51,rccw) + centinal[99](91,54,rccw)

# build the source signal -- the most compact set of gliders
# from which all other gliders and spaceships can be generated
MWSSrecipes = glider(5759,738,rccw) + glider(6325,667,flip_x) \
   + glider[3](5824,896,flip_x) + glider[2](5912,1264) \
   + glider[2](6135,1261,flip_x) + glider[1](5912,1490,flip_y) \
   + glider(6229,4717,flip_x) + glider[1](6229,5029,flip) \
   + glider[1](5920,5032,flip_y) + glider(6230,5188,flip) \
   + glider[3](6230,5306,flip) + glider[3](5959,5309,flip_y)

# suppress output MWSSes as long as gliders are being added
MWSSrecipes += pattern("o!",6095,-65) + pattern("o!",6075,228)

# add the first four centinals to guide the recipe gliders
all = centinal[44](6185,5096,flip_x) + centinal[73](5897,1066)
all += centinal[42](5782,690) + centinal[25](5793,897,rcw)

# generate the MWSSes for the glider-fanout ladder
for i in range(7):
    g.show("Building rung " + str(i+1) + " of ladder...")
    all = (all + MWSSrecipes)[1100]

# add the actual glider-fanout ladder -- six rungs
for i in range(6):
    all += glider(6030,1706+550*i,swap_xy_flip) \
       + centinal[15](6102,1585+550*i) \
       + block(6029,1721+550*i) + centinal[34](5996,1725+550*i,rccw) \
       + block(6087,1747+550*i) + centinal[87](6122,1745+550*i,rcw) \

# add the rest of the centinals to guide the ladder's output gliders
g.show("Adding centinal reflectors...")
all += centinal[88](5704,0) + centinal[29](6423,295,flip_x) \
   + centinal[74](5616,298) + centinal[40](6361,613,rcw) \
   + centinal[23](6502,620,flip_x) + centinal[36](5636,986) \
   + centinal[38](6370,1008,rcw) + centinal[19](5747,1347,rcw) \
   + centinal[67](5851,1516) + centinal(4061,2605,rccw) \
   + centinal[10](5376,3908,rccw) + centinal[77](8191,4407,flip_x) \
   + centinal[6](4988,4606) + centinal[34](6357,4608,flip_x) \
   + centinal[27](8129,4621,flip_x) + centinal[92](5159,5051) \
   + centinal[53](7991,5201,flip_x) + centinal[94](7038,5370,rccw) \
   + centinal[13](5591,5379,rccw) + centinal[3](5858,5428,rccw) \
   + centinal[87](7805,5511,flip_x) + centinal[98](401,5557,rccw) \
   + centinal[14](955,5561,rccw) + centinal[8](6592,5584,rccw) \
   + centinal[39](6933,5698,flip_x) + centinal[32](6230,5881) \
   + centinal[47](11676,5854,rccw) + centinal[68](0,5748,rccw) \
   + centinal[89](6871,5912,flip_x) + centinal[45](12095,6027,rccw) \
   + centinal[86](6209,6134) + centinal[55](6868,6357,flip_x) \
   + centinal[95](9939,6491,rccw) + centinal[23](8782,6548,rccw) \
   + centinal[58](3066,6572,rccw) + centinal[21](9326,6596,rccw) \
   + centinal[80](3628,6626,rccw) + centinal[45](6821,6528,flip_x) \
   + centinal[33](10373,6649,rccw) + centinal[16](2587,6685,rccw)

#  to change behavior at center, comment out one of the lines below
all += eater(6018,5924,rccw) + eater(6037,5943,rccw) # true p1100 gun
# all += block(6018,6787) # synch center to recreate original p1100x5

center = block(1081,6791) + block(2731,6791) + block(3831,6791) \
   + block(9108,6791) + block(10208,6791) + block(11308,6791) \
   + passthrough(8475,6737) + passthrough[39](3365,6737,flip_x) \
   + passthrough(9575,6737) + passthrough[39](2265,6737,flip_x) \
   + passthrough(10675,6737) + passthrough[39](1715,6737,flip_x)

# add asymmetric Equator to mirror-symmetric North and South
MWSSrecipes += MWSSrecipes(0,13583,flip_y)
all += all(0,13583,flip_y) + center

g.putcells(all)
g.fit()

# Different glider paths are different lengths, so incomplete
# glider recipes must be overwritten for a while to prevent disaster.
for i in range(46):
    g.show("Filling glider tracks -- " \
           + str(49500 - i*1100) + " ticks left.")
    g.update()
    g.putcells(MWSSrecipes)
    g.run(1100)
g.show("")

# reset gen count to 0
g.setgen("0")
