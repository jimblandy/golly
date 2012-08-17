# Based on breeder.life from Xlife's pattern collection.

from glife import *
import golly as g
rule ()  # Life

# ---------------------------------

def pause(t):
   g.update()
   sleep(t)

# ---------------------------------

def runn(n,t):
   while n>0:
      g.run(1)
      g.update()
      sleep(t)
      n=n-1

# ---------------------------------

halfbreeder = pattern("2bo$bo$2o3b2o$obo3b2o$bo3b2o$5bo!",0,1)
breeder=halfbreeder + halfbreeder(0,14,flip_y)
hwss=pattern("2b2o$o4bo$6bo$o5bo$b6o!")
mwss=pattern("2bo$o3bo$5bo$o4bo$b5o!")
lwss=pattern("o2bo$4bo$o3bo$b4o!",0,1)
glider=pattern("b2o$obo$2bo!",0,1)
breederpuffer = breeder(15,-45) + hwss(16,-30) + hwss(16,-46,flip_y)\
                                + hwss(5,-54) + hwss(5,-22,flip_y)
breederrake11 = breederpuffer(71,37) + lwss(66,20,flip_y)\
                + mwss(51,12,flip_y) + mwss(51,-14)
breederrake00 = breederrake11 + mwss(-9,10,flip_y) + mwss(-9,-12)
breederrake01 = breederrake11 + mwss(-9,-12)
breederrake10 = breederrake11 + mwss(-9,10,flip_y)

g.new("breeder.life")
g.select([400,50,600,350])
g.fitsel()
g.select(getminbox(breederrake11(314,242)))
pause(.5)
breederrake11.put(314,242)
pause(.5)
g.select(getminbox(breederrake11(312,201,flip_y)))
runn(4,.01)
breederrake11.put(312,201,flip_y)
pause(.5)
g.select(getminbox(breederrake00(368,286)))
runn(101,.01)
breederrake00.put(368,286)
pause(.5)
g.select(getminbox(breederrake10(359,158,flip_y)))
runn(33,.01)
breederrake10.put(359,158,flip_y)
pause(.5)
g.select(getminbox(breederrake10(406,137,flip_y)))
runn(306,.01)
breederrake10.put(406,137,flip_y)
pause(.5)
g.select(getminbox(breederrake01(349,273)))
runn(129,.01)
breederrake01.put(349,273)
pause(.5)
g.select(getminbox(breederrake10(428,336)))
runn(141,.01)
breederrake10.put(428,336)
pause(.5)
g.select(getminbox(breederrake10(426,107,flip_y)))
breederrake10.put(426,107,flip_y)
pause(.5)
g.select(getminbox(breederrake00(473,371)))
runn(223,.01)
breederrake00.put(473,371)
pause(.5)
g.select(getminbox(breederrake00(472,71,flip_y)))
runn(1,.01)
breederrake00.put(472,71,flip_y)
g.select([530,343,25,25])
pause(1)
glider.put(543,356,flip_x)
pause(1)
g.select([529,70,25,25])
pause(1)
glider[1].put(539,82,swap_xy)
pause(1)
g.select([454,144,25,25])
pause(1)
glider.put(464,160,flip)
pause(1)
g.select([440,266,25,25])
pause(1)
glider.put(454,276,flip_x)
pause(1)
g.select([])
pause(3)
runn(2000,0)