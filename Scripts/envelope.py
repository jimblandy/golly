# Use multiple layers to create a history of the current pattern.
# The "envelope" layer remembers all live cells.
# Author: Andrew Trevorrow (andrew@trevorrow.com), December 2006.

from glife import pattern
import golly as g

if g.empty(): g.exit("There is no pattern.")

genprefix = "gen "
envelopename = "envelope"
currindex = g.getlayer()

if currindex > 1 and g.getname(currindex).startswith(genprefix) \
                 and g.getname(currindex-1) == envelopename:
   # continue from where we left off
   pass

elif currindex+1 < g.numlayers() \
                 and g.getname(currindex) == envelopename \
                 and g.getname(currindex+1).startswith(genprefix):
   # switch from envelopename layer to genprefix layer and continue
   g.setlayer(currindex+1)

elif currindex+2 < g.numlayers() \
                 and g.getname(currindex+1) == envelopename \
                 and g.getname(currindex+2).startswith(genprefix):
   # switch from starting layer to genprefix layer and continue
   g.setlayer(currindex+2)

else:
   # start a new envelope using pattern in current layer
   if g.numlayers() + 1 > g.maxlayers():
      g.exit("You need to delete a couple of layers.")
   if g.numlayers() + 2 > g.maxlayers():
      g.exit("You need to delete a layer.")
   
   # get starting pattern from current layer
   startpatt = pattern( g.getcells(g.getrect()) )
   
   envelope = g.addlayer()    # create layer for remembering all live cells
   startpatt.put(0,0)      # copy starting pattern into this layer
   
   curr = g.addlayer()     # create layer for generating pattern
   startpatt.put(0,0)      # copy starting pattern into this layer

# draw stacked layers using same location and scale
g.setoption("stacklayers", 1)

def main():
   while True:
      g.dokey( g.getkey() )
      g.run(1)
      if g.empty(): break
      
      # copy current pattern to envelope layer;
      # we temporarily disable event checking so thumb scrolling
      # and other mouse events won't cause confusing changes
      currpatt = pattern( g.getcells(g.getrect()) )
      g.check(0)
      g.setlayer(envelope)
      currpatt.put(0,0)
      g.setlayer(curr)
      g.check(1)
      
      exp = g.getstep()
      if exp > 0:
         step = g.getbase()**exp
      else:
         step = 1
      if int(g.getgen()) % step == 0:
         # display all 3 layers (start, envelope, curr)
         g.update()

try:
   main()
finally:
   # name the current and envelope layers so user can run script
   # again and continue from where it was stopped
   g.setlayer(curr)
   g.setname(genprefix + g.getgen(','))
   g.setname(envelopename, g.getlayer() - 1)
