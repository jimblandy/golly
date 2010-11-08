# smaller variant of David Bell's p59 glider loop from 2006 May 28
# Author: Dave Greene, 30 May 2006

import golly
from glife.base import *
from glife.text import *
from glife.herschel import *

golly.new("p59 track")
rule()

fast_eater = pattern ("2o$bo2b2o$bobobo$2bo2$4bo$3bobo$4bo$b3o$bo!", -9, -2)

# create a list specifying half of a length-6608 Herschel track
path = ( hc112, ) + 14 * ( hc77, )
path += path + 12 * ( hc77, )

T = (0, 0, identity)
p59_track = pattern(herschel)

golly.show("Building p59 Herschel track -- hang on a minute...")
for h in 2 * path:
       p59_track += h (*T) + fast_eater (*T)
       T = compose (h.transform, T)

# add 111 more Herschels to template Herschel track, 59 generations apart
for i in range(111):
       p59_track=p59_track[59] + herschel

p59_track +=pattern("o!", -10, 589) # get rid of eater

# add 9 gliders in same spot, 59 generations apart
for i in range(9):
       p59_track = (p59_track + pattern("2o$b2o$o!", -16, 595))[59]

golly.show("")
all = p59_track
for i in range(3):
       all = all(-720, 450, rccw) + p59_track

all += make_text ('59') (-142, 596)
all.display ("")