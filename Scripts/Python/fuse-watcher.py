# Maintain a 3c/6 and a 36c/75 fuse in the viewport
# (at least until the speed difference moves them too far apart)
# Author: Dave Greene, 17 May 2006, updated 22 October 2006.

import golly as g
from glife import *
from time import time

# ----------------------------------------------------------------

rule ()  # Life

scorpion = pattern("bo$oboboo$obobobo$boo3bo$3b3o$3bo!")
# scorpion fuse by David Bell, 8 April 2000
scorpion_fuse = pattern("""
36b3o$$34bo5bo$12boo20bo5bo$10bo4bo18bo5bo$$29boo6boo16bo$10bo4bo4bo7b
o8boo9bo5b3o$12boo6bo5bobbo18b3obbobbo$20bo4bo3bo5bo11bobbo3bo$bo27bo
4bobo10bo$3o7bo5b3o6b3oboobbo13b3o$obo5booboo12b3obobbo5bo9bobo$8boob
oo11bo4bobboo$9b3o13boobbo3boboobo7bo$20boobboo5b4o11bo$21b3obo4b3ob3o
10bobo$21boobbobob3obboobo9b3o$26bobobboo$24boo4bo5boo$25bo3bo$25bobbo
$26boo26boo6bo$54boo5bobo$62boo!""")

fuse1 = scorpion_fuse
for fuse1_end in range (1, 20):
  fuse1 += scorpion(-12 * fuse1_end - 1, 11)
fuse1_end+=1

HH_fuse_seg = pattern("oo$obo$$3o$$3o$$obo$oo!")
HH_fuse_cap = pattern("3$boo$o$boo!")
# 3c/6 fuse by Hartmut Holzwart, 19 May 2005
HH_fuse = pattern("""
13b3oboo9bobbo$12bo4boo3bo4bobboo$bb5obboo11b3oboob3o$b4oboo3b3obbo3b
oobobb3o$o3boo3bobobboo3boobbobboo3bo$oobo5b4obbobbo12bo$4bo3bobboo3bo
bbo$4o4boo5b8o$14boo$4o4boo5b8o$4bo3bobboo3bobbo$oobo5b4obbobbo12bo$o
3boo3bobobboo3boobbobboo3bo$b4oboo3b3obbo3boobobb3o$bb5obboo11b3oboob
3o$12bo4boo3bo4bobboo$13b3oboo9bobbo!""")

fuse2 = HH_fuse
for fuse2_end in range (1, 80):
  fuse2 += HH_fuse_seg(-3 * fuse2_end, 4)
fuse2_end += 1
fuse2 += HH_fuse_cap(-3 * fuse2_end,4)
fuse2_offset=50

g.new("fuse-watcher")
g.putcells(fuse1, 0, 0, 1, 0, 0, 1)
g.putcells(fuse2, 0, fuse2_offset,1, 0, 0, 1)
view_x = "0"
view_y = "35"
g.setpos(str(view_x),str(view_y))
g.setmag(4)
while not g.visrect([-10, -10, 90, 90]):
  g.setmag( g.getmag() - 1 )
    
g.show("Press left and right arrows or +, -, *, /, or f to control the pattern,"\
+ " or any other key to exit.  Mouse wheel also zooms.")

# annoying melodramatic pause
g.update()
oldsecs = time()
while True:
  newsecs = time()
  if newsecs - oldsecs >= 1:
    break

# now start running the timer
ticks=0
# The 'target' variables below hold the generation numbers
# (monitored by the 'ticks' variable) at which various things
# are supposed to happen:
# lengthening fuse1 and fuse2, cleaning up after fuse1, moving the view,
# and displaying screen updates [even without hashing, things go much
# faster if the viewport is not redrawn every generation.]
# The associated 'step' variables specify how long to wait before
# repeating a particular event.
fuse1_step=24
fuse1_target=0
fuse1_cleanup_step=75
fuse1_cleanup_target=750
fuse2_step=6
fuse2_target=0
view_step=2
view_target=0
view_dx=-1
view_dy=0
view_dx_offset=0
view_dy_offset=0
display_step=1
display_target=0

while True:
  ch = g.getkey()
  if len(ch) > 0:
    if ch=="[" or ch==chr(28):
      view_dx_offset=-50
    elif ch=="]" or ch==chr(29):
      view_dx_offset=50
    elif ch=="+" or ch=="=": # override defaults here to get smaller step size
      display_step *= 2
      view_dx=-display_step/2
    elif ch=="-" or ch=="?":
      if display_step != 1:
        display_step /= 2
        view_dx=-display_step/2
    elif ch=="*" or ch=="/" or ch=="f":
      g.dokey(ch) # just pass these keys through and let Golly handle them
    else:
      break
  oldsecs = time()
  g.run(1)
  if ticks==fuse1_target:
    g.putcells(scorpion,-12*fuse1_end-1, 11, 1, 0, 0, 1)
    fuse1_end+=1
    fuse1_target+=fuse1_step
  if ticks==fuse1_cleanup_target:
    g.select([400-36*fuse1_cleanup_target/75,-5,25,35])
    g.clear(0)
    g.select([]) # comment this out to see the clear operation occurring
    fuse1_cleanup_target+=fuse1_cleanup_step
  if ticks==fuse2_target:
    g.putcells(HH_fuse_seg,-3*fuse2_end, 4+fuse2_offset, 1, 0, 0, 1)
    # the line below is specific to this fuse --
    # should really have more generic resetcells() code to remove the end cap
    g.setcell(-3*fuse2_end, 11+fuse2_offset-3, 0)
    fuse2_end+=1
    g.putcells(HH_fuse_cap,-3 * fuse2_end,4 + fuse2_offset, 1, 0, 0, 1)
    fuse2_target+=fuse2_step
  if ticks==view_target:
    view_x, view_y = g.getpos() # user may have moved the view!
    view_x=str(int(view_x)+view_dx+view_dx_offset)
    view_y=str(int(view_y)+view_dy+view_dy_offset)
    view_dx_offset=0
    view_dy_offset=0
    view_target+=view_step
  if ticks==display_target:
    g.setpos(view_x,view_y)
    g.update()
    display_target+=display_step
  ticks+=1
g.show("") # clear the script message before exiting
