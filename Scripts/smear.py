# test multiple layers

from glife import pattern
import golly as g

if g.empty():
   g.exit("There is no pattern.")

'''
if g.numlayers() + 2 > g.maxlayers():
   g.exit("You need to delete a couple of layers.")
'''

# get starting pattern from current layer
startpatt = pattern( g.getcells(g.getrect()) )

smear = g.addlayer()    # create layer for remembering all live cells
startpatt.put(0,0)      # copy starting pattern into this layer

curr = g.addlayer()     # create layer for generating pattern
startpatt.put(0,0)      # copy starting pattern into this layer

g.setoption("drawlayers",1)   # draw all layers (sync location & scale)
g.setoption("genlayers",0)    # but only generate current layer

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
      # assigned layer colors (for live cells) with alpha blending???
      g.update()

'''
Ideas and questions:
- each layer is a separate universe with its own rule, viewport,
  selection, window title, etc
- allow up to 10 layers (0..9)?
  if we allow more (unlimited?) then still only allow up to 10
  different colors (use mod 10).
- int = getlayer() returns index of current layer
- int = addlayer() inserts new layer after current layer
  and returns index of new layer (ie. getlayer() + 1)
- dellayer() deletes current layer and sets curr layer to prev index,
  or 0 if only 1 layer remains
- int = numlayers() returns number of existing layers
- need movelayer(oldindex,newindex)?

Need a new Layer menu with these items:

Add Layer            (creates new empty layer)
Duplicate Layer      (like Add Layer but copies pattern, etc???)
Delete Layer
Move Layer...
-----
Delete Other Layers  (delete all layers except current one)
-----
Draw All Layers      (check item -- use current pos and mag)
Generate All Layers  (ditto -- use current step base and exp)
-----
Next Layer           (select next layer???)
Previous Layer       (select previous layer???)
-----
Layer 0
Layer 1              (current layer is ticked)
Layer 2

Whenever there is more than 1 layer, display small buttons
at the top of the status bar:

         _____   _____   _____
Layers: |  0  | |**1**| |  2  |
         -----   -----   -----

User can:
- click on a button to change current layer
- click and drag button to move layer to new location
- alt/option-click on a button to add a new layer
- control-click on a button to delete layer
'''
