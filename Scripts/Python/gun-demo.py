# Glider guns used in this script are 'aligned', which means gliders start in the
# origin and go SE.
# Based on gun_demo.py from PLife (http://plife.sourceforge.net/).

from glife.base import *
from glife.gun24 import *
from glife.gun30 import *
from glife.gun46 import *

rule ()  # Life

def mirror_gun (glider_gun):
    """mirror an 'aligned' glider gun and preserve its timing"""
    return glider_gun[2] (-1, 0, swap_xy)

def compose_lwss_gun (glider_gun, A = -1, B = -1, C = -1):
    """construct an lwss gun using 3 copies of an 'aligned' glider gun.
    A, B and C are distances from the glider collision point to
    NE, NW and SW guns respectively"""
    if (A < 0): A = 40
    if (B < 0): B = A;
    if (C < 0): C = A;

    m = min (A, B, C)
    a = A - m
    b = B - m
    c = C - m
    return \
           glider_gun[4 * a] ( A, -A - 3, flip_x) + \
           glider_gun[4 * b] (-B + 2, -B + 1) + \
           glider_gun[4 * c + 1] (-C + 6,  C, flip_y)

lwss_eater = eater (4, 2, swap_xy)

all = \
        compose_lwss_gun (mirror_gun (gun24), 12, 11, 12) (0, -100) + lwss_eater (100, -100) + \
        compose_lwss_gun (gun24, 11, 13, 4) + lwss_eater (100, 0) + \
        compose_lwss_gun (gun30, 13, 11, 4) (0,  70) + lwss_eater (100,  70) + \
        compose_lwss_gun (gun46, 22, 13, 3) (0, 130) + lwss_eater (100, 130)

all.display ("gun-demo")
