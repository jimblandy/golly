from glife.base import *

rule()   # use Life rule to evolve phases

__half = bheptomino (0, 2, flip_x) + bheptomino (0, -2, flip) + block (16, -4) + block (16, 3)

gun46_double = __half (7, -2) + __half (-8, 2, flip_x)

gun46 = __half[1] (1, -7) + __half (-13, -4, flip_x) # aligned version shooting SE
