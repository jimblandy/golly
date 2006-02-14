from glife.base import *

rule()

def collision (i, j):
	return pentadecathlon + glider[i + 11] (-8 + j, -10, flip)

all = pattern ()

for i in range (-7, 8):
	for j in range (-9, 10):
		all += collision (i, j) (100 * i, 100 * j)

### all.save ("pd_glider.lif", "Exploring 'pentadecathlon + glider' collisions.")

all.display ("pd_glider")
