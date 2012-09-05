from glife.base import *

rule()   # use Life rule to evolve phases

__gun30 = queenbee + block (12, 2) + (queenbee[5] + block (12, 2)) (-9, 2, flip_x)
__gun30_asym = queenbee + eater (10, 1, rccw) + (queenbee[5] + block (12, 2)) (-9, 2, flip_x)

gun30   = __gun30 (1, -7)               # aligned gun shooting SE
gun30_a = __gun30_asym (1, -7)          # aligned gun shooting SE

gun60 = gun30 + gun30_a[26] (-4, 11, flip_y)
