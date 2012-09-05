from glife.base import *

herschel_ghost = pattern ("""
***
.
.*.*
""")

# A Herschel conduit is represented by its pattern and
# the transformation it applies to the Herschel.
# Hence the additional .transform field.

hc64 = block (14, -8) + block (9, -13) + block (13, -15) + block (7, -19)
hc64.transform = (-9, -9, rccw)

hc77 = block (9, -13) + eater (10, 0, swap_xy) + eater (-7, -12, swap_xy_flip)
hc77.transform = (10, -25, flip_x)

hc112 = block (16, -10) + block (-3, -11) + aircraft_carrier (13, -3, swap_xy) + \
        eater (10, 1) + eater (-3, -14, flip) + eater (9, -16, flip_y)
hc112.transform = (35, -12, rcw)

hc117 = eater (10, 1) + eater (13, -9, swap_xy) + eater (0, -12, swap_xy_flip) + block (8, -22) + snake (15, -19)
hc117.transform = (6, -40, identity)

hc119 = block (-14, -3) + block (-13, -8) + block (-19, -9) + eater (-17, -2, rcw)
hc119.transform = (-12, -20, flip_x)

hc156 = eater (10, 0, swap_xy) + eater (12, -9, swap_xy) + eater (0, -12, swap_xy_flip) + eater (17, -21, flip_y) + \
        snake (21, -5, rccw) + block (24, -15) + tub (11, -24)
hc156.transform = (43, -17, rcw)

hc158 = eater (-2, -13, flip) + eater (-3, -8, rcw) + eater (20, -19, rccw) + pattern ("""
.....**
.....*.*
.......*
.......*.*.**
..**.*.*.**.*
..**.**.*
........*
..**.***
...*.*
...*.*
**.**
*.*
..*
..**
""", 14, -12)
hc158.transform = (7, -27, flip_x)

hc190 = eater (-3, -8, rcw) + eater (-10, -12, swap_xy) + eater (-3, -12, swap_xy_flip) + \
        eater (-8, -17) + eater (11, -27, flip_y) + block (2, -25) + snake (5, -31, rccw) + \
        pattern ("""
..**.*
..**.***
........*
..**.***
...*.*
...*.*
**.**
*.*
..*
..**
""", 14, -8)
hc190.transform = (-16, -22, rccw)
