# Based on bricklayer.py from PLife (http://plife.sourceforge.net/).

from glife import *

rule ()  # Life

p22_half = pattern("2o$bo$bobo$2b2o3bo$6bob2o$5bo4bo$6bo3bo$7b3o!")
p22 = p22_half + p22_half(26, 9, flip)

gun22 = p22 + p22[1](-18, 11)
gun154 = gun22[27] + gun22[5](49, 12, rcw) + gun22(5, 53, flip_y)

p7_reflector = pattern("""
2b2o5b2o$2b2o5bo$7bobo$7b2o$3b2o$3bobo$4bo3$13bo$10bo2b3o3b2o$3b5o
2b3o3bo2bo$b3obob3o4b2obobo$o4bo4b4obobo2b2o$2ob2ob4o3bobob2obo$
4bobobobobo2bo2bobo$4bobobo2bo2bob3ob2o$3b2obob4o6bobo$7bo4b6o2b
o$9bo2bo2bo4b2o$8b2o!""")
pre_lom = pattern("2bo$2ob2o$2ob2o2$b2ob2o$b2ob2o$3bo!")

all = gun154[210](-52, -38) + gun154[254](52, -38, flip_x) + p7_reflector(8, 23) \
      + pre_lom(-3, 30)

### all.save ("bricklayer.lif", "David Bell, 29 Sep 2002")

all.display("bricklayer")
