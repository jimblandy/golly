# Use multiple layers to create a history of the current pattern.
# The "envelope" layer remembers all live cells.
# Author: Andrew Trevorrow (andrew@trevorrow.com), December 2006.
# Updated for better compatibility with envelope.pl, June 2007.
# Updated to handle multi-state patterns, Aug 2008.

import golly as g

if g.empty(): g.exit("There is no pattern.")

currname = "current"
envname = "envelope"
currindex = g.getlayer()
envindex = 0

multistate = g.numstates() > 2
curralgo = g.getalgo()
currrule = g.getrule()

if currindex > 1 and g.getname(currindex) == currname \
                 and g.getname(currindex - 1) == envname :
   # continue from where we left off
   envindex = currindex - 1

elif currindex + 1 < g.numlayers() \
                 and g.getname(currindex) == envname \
                 and g.getname(currindex + 1) == currname :
   # switch from envelope layer to currname layer and continue
   g.setlayer(currindex + 1)
   envindex = currindex
   currindex += 1

elif currindex + 2 < g.numlayers() \
                 and g.getname(currindex + 1) == envname \
                 and g.getname(currindex + 2) == currname :
   # switch from starting layer to currname layer and continue
   g.setlayer(currindex + 2)
   envindex = currindex + 1
   currindex += 2

else:
   # start a new envelope using pattern in current layer
   if g.numlayers() + 1 > g.maxlayers():
      g.exit("You need to delete a couple of layers.")
   if g.numlayers() + 2 > g.maxlayers():
      g.exit("You need to delete a layer.")
   
   # get starting pattern from current layer
   startpatt = g.getcells(g.getrect())
   startstep = g.getstep()
   
   envindex = g.addlayer()    # create layer for remembering all live cells
   g.putcells(startpatt)      # copy starting pattern into this layer
   if multistate:
      # convert all cell states to 1
      g.setalgo("QuickLife")
      g.setalgo(curralgo)
      g.setrule(currrule)
   
   currindex = g.addlayer()   # create layer for generating pattern
   g.putcells(startpatt)      # copy starting pattern into this layer
   g.setstep(startstep)
   
   # name the current and envelope layers so user can run script
   # again and continue from where it was stopped
   g.setname(currname)
   g.setname(envname, envindex)

# draw stacked layers using same location and scale
g.setoption("stacklayers", 1)

g.show("Hit escape key to stop script...")
while True:
   g.dokey( g.getkey() )
   g.run(1)
   if g.empty():
      g.show("Pattern died out.")
      break
   
   # copy current pattern to envelope layer;
   # we temporarily disable event checking so thumb scrolling
   # and other mouse events won't cause confusing changes
   currpatt = g.getcells(g.getrect())
   g.check(0)
   g.setlayer(envindex)
   if multistate:
      # restore original algo+rule so putcells won't fail
      g.setalgo(curralgo)
      g.setrule(currrule)
   g.putcells(currpatt)
   if multistate:
      # convert all cell states to 1 and leave envelope layer
      # in qlife so envelope color is (probably) different
      g.setalgo("QuickLife")
   g.setlayer(currindex)
   g.check(1)
   
   step = 1
   exp = g.getstep()
   if exp > 0:
      step = g.getbase()**exp
   if int(g.getgen()) % step == 0:
      # display all 3 layers (start, envelope, current)
      g.update()
