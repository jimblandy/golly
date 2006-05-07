# Go to a requested generation.  The given generation can be an
# absolute number like 1,000,000 (commas are optional) or a number
# relative to the current generation like +9 or -6.  If the target
# generation is less than the current generation then we go back
# to the starting generation (0) and advance to the target.
# Authors: Andrew Trevorrow and Dave Greene, April 2006.

from glife import getstring, validint, getgenint
from time import time
import golly

# --------------------------------------------------------------------

def goto(gen):
   currgen = getgenint()
   if gen[0] == '+':
      n = int(gen[1:])
      newgen = currgen + n
   elif gen[0] == '-':
      n = int(gen[1:])
      if currgen > n:
         newgen = currgen - n
      else:
         newgen = 0
   else:
      newgen = int(gen)
   
   if newgen < currgen:
      # go back to gen 0 and then forwards to newgen; note that reset()
      # also restores rule and hashing option, so too bad if user
      # changed those options after gen 0 info was saved;
      # first save current location and scale
      midx, midy = golly.getpos()
      mag = golly.getmag()
      golly.reset()
      # restore location and scale
      golly.setpos(midx, midy)
      golly.setmag(mag)
      currgen = 0
   
   golly.show("Hit escape to abort...")
   oldsecs = time()
   while currgen < newgen:
      if golly.empty():
         golly.show("Pattern is empty.")
         return
      # using golly.step() as a faster method for getting to newgen
      # is left as an exercise for the reader :)
      golly.run(1)
      currgen += 1
      newsecs = time()
      if newsecs - oldsecs >= 1.0:     # do an update every sec
         oldsecs = newsecs
         golly.update()
   golly.show("")

# --------------------------------------------------------------------

gen = getstring("Go to this generation:")
if len(gen) == 0:
   golly.show("")
elif not validint(gen):
   golly.error('Sorry, but "' + gen + '" is not a valid integer.')
else:
   goto(gen.replace(",",""))
