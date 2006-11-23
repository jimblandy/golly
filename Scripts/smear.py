# test multiple layers

from glife import pattern
import golly as g

if g.empty():
   g.exit("There is no pattern.")
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

g.setoption("drawlayers",1)   # draw all layers (sync location & scale)
g.setoption("genlayers",0)    # but only generate current layer

def main():
   while True:
      g.dokey( g.getkey() )
      g.run(1)
      if g.empty(): break
      
      # copy current pattern to smear layer
      currpatt = pattern( g.getcells(g.getrect()) )
      g.setlayer(smear)
      currpatt.put(0,0)
      g.setlayer(curr)
      
      exp = g.getstep()
      if exp > 0:
         step = g.getbase()**exp
      else:
         step = 1
      if int(g.getgen()) % step == 0:
         # display all 3 layers (start, smear, curr) using currently
         # assigned layer colors
         g.update()

try:
   main()
finally:
   g.setlayer(curr)

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
- need setname(str) and str = getname()???

Need a new Layer menu with these items:

Add Layer            (creates new empty layer)
Duplicate Layer      (like Add Layer but copies pattern, etc???)
Delete Layer
Move Layer...
Name Layer...        (change name seen in window's title bar???)
-----
Delete Other Layers  (delete all layers except current one)
-----
Synchronize Views    (check item -- keep all viewports in sync???)
Tile All Layers      (check item -- tile all viewports???)
Display All Layers   (check item -- use current pos and mag)
Generate All Layers  (check item -- use current step base and exp)
-----
Next Layer           (select next layer???)
Previous Layer       (select previous layer???)
Wrap                 (wrap around if ticked???)
-----
Layer 0: name
Layer 1: name        (current layer is ticked)
Layer 2: name

If "Show Layer Bar" is ticked, display bitmap buttons in a bar
at the top of the viewport:

 _____  _____    _____  _____  _____
|  +  ||  -  |  |  0  ||**1**||  2  |
 -----  -----    -----  -----  -----

First 2 buttons are for Add Layer and Delete Layer.

User can:
- click on a button to change current layer
- click and drag button to move layer to new location
'''
