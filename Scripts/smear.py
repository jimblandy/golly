# Use multiple layers to create a "smear" history of the current pattern
# where one layer remembers all cells that become alive.
# Author: Andrew Trevorrow (andrew@trevorrow.com), December 2006.

from glife import pattern
import golly as g

if g.empty(): g.exit("There is no pattern.")

currname = "current"
smearname = "smear"
currindex = g.getlayer()

if currindex > 1 and g.getname(currindex) == currname \
                 and g.getname(currindex-1) == smearname:
   # continue from where we left off
   pass

elif currindex+1 < g.numlayers() \
                 and g.getname(currindex) == smearname \
                 and g.getname(currindex+1) == currname:
   # switch from smearname layer to currname layer and continue
   g.setlayer(currindex+1)

elif currindex+2 < g.numlayers() \
                 and g.getname(currindex+1) == smearname \
                 and g.getname(currindex+2) == currname:
   # switch from starting layer to currname layer and continue
   g.setlayer(currindex+2)

else:
   # start a new smear using pattern in current layer
   if g.numlayers() + 1 > g.maxlayers():
      g.exit("You need to delete a couple of layers.")
   if g.numlayers() + 2 > g.maxlayers():
      g.exit("You need to delete a layer.")
   
   # get starting pattern from current layer
   startpatt = pattern( g.getcells(g.getrect()) )
   
   smear = g.addlayer()    # create layer for remembering all live cells
   startpatt.put(0,0)      # copy starting pattern into this layer
   
   curr = g.addlayer()     # create layer for generating pattern
   startpatt.put(0,0)      # copy starting pattern into this layer

# draw all layers using same location & scale
g.setoption("stacklayers", 1)

def main():
   while True:
      g.dokey( g.getkey() )
      g.run(1)
      if g.empty(): break
      
      # copy current pattern to smear layer;
      # we temporarily disable event checking so thumb scrolling
      # and other mouse events won't cause confusing changes
      currpatt = pattern( g.getcells(g.getrect()) )
      g.check(0)
      g.setlayer(smear)
      currpatt.put(0,0)
      g.setlayer(curr)
      g.check(1)
      
      exp = g.getstep()
      if exp > 0:
         step = g.getbase()**exp
      else:
         step = 1
      if int(g.getgen()) % step == 0:
         # display all 3 layers (start, smear, curr)
         g.update()

try:
   main()
finally:
   # name the current and smear layers so user can run script
   # again and continue from where it was stopped
   g.setlayer(curr)
   g.setname(currname)
   g.setname(smearname, g.getlayer() - 1)

'''
Ideas and questions:
- each layer is a separate universe with its own rule, viewport,
  selection, window title, etc
- allow up to 10 layers (0..9)???
  if we allow more (unlimited?) then still only allow up to 10
  different colors (use mod 10).
- int = getlayer() returns index of current layer
- int = addlayer() inserts new layer after current layer
  and returns index of new layer (ie. getlayer() + 1)
- dellayer() deletes current layer and sets curr layer to prev index,
  or 0 if only 1 layer remains
- int = numlayers() returns number of existing layers
- need movelayer(oldindex,newindex)???
- need setname(str,index) and str = getname(index)???

Need a new Layer menu with these items:

Add Layer            (creates new empty layer)
Clone Layer          (creates new layer with shared algo???)
Duplicate Layer      (like Add Layer but copies pattern, etc???)
Delete Layer
Move Layer...        (maybe no need if we can drag layer buttons???)
Name Layer...        (change name seen in window's title bar???)
-----
Delete Other Layers  (delete all layers except current one)
-----
Show Layer Bar       (check item -- show/hide layer bar)
Synchronize Views    (check item -- keep all viewports in sync)
Synchronize Cursors  (check item -- keep all cursors in sync)
-----
Stack Layers         (radio item -- use current pos and mag)
Tile Layers          (radio item -- tile all viewports)
-----
0: name
1: name              (current layer is ticked)
2: name

If "Show Layer Bar" is ticked, display bitmap buttons in a bar
at the top of the viewport:

 _____  _____    _____  _____  _____
|  +  ||  -  |  |  0  ||**1**||  2  |
 -----  -----    -----  -----  -----

First 2 buttons are for Add Layer and Delete Layer.

User can:
- click on a button to change current layer
- click and drag button to move layer to new location???
'''
