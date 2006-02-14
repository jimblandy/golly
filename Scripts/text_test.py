from glife.text import *
from glife.gun256 import *

rule()

coe_rake_1 = hwss (-1, -10, flip_x) + lwss (8, 2, flip_x) + hwss[1] (0, 7, flip) + mwss[1] (8, -2, flip) + [3, -3]
coe_rake_2 = coe_rake_1 + hwss (-12, -8, flip_x) + hwss (-15, 7, flip)

all = pattern ()

all += coe_rake_1 (0, 200)
all += coe_rake_2 (0, 100)

all += make_text ('010-212-323-434-545-656-767-878-989-090') (-160, -200)
all += gun256_full

### all.save ("text_test.lif")

all.display ("text_test")
