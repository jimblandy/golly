# Based on breeder.life from Xlife's pattern collection.

from glife import *
rule ()  # Life
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
flotilla=(((((((breederrake11(314,242)[4] + breederrake11(312,201,flip_y))[101]\
+ breederrake00(368,286))[33] + breederrake10(359,158,flip_y))[306]\
+ breederrake10(406,137,flip_y))[129] + breederrake01(349,273))[141]\
+ breederrake10(428,336) + breederrake10(426,107,flip_y))[223]\
+ breederrake00(473,371))[1] + breederrake00(472,71,flip_y)
startup=flotilla(-287,-280) + glider(256,76,flip_x) + glider[1](252,-198,swap_xy)\
                            + glider(177,-120,flip) + glider(167,-4,flip_x)

startup[18](-300,0).display("breeder.life")
