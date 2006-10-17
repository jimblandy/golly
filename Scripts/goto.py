# Go to a requested generation.  The given generation can be an
# absolute number like 1,000,000 (commas are optional) or a number
# relative to the current generation like +9 or -6.  If the target
# generation is less than the current generation then we go back
# to the starting generation (normally 0) and advance to the target.
# Authors: Andrew Trevorrow and Dave Greene, April 2006.

from glife import getstring, validint
from time import time
import golly

# --------------------------------------------------------------------

def goto(gen):
   currgen = int(golly.getgen())
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
      # try to go back to starting gen (not necessarily 0) and
      # then forwards to newgen; note that reset() also restores
      # rule and hashing option, so too bad if user changed
      # those options after the starting info was saved;
      # first save current location and scale
      midx, midy = golly.getpos()
      mag = golly.getmag()
      golly.reset()
      # restore location and scale
      golly.setpos(midx, midy)
      golly.setmag(mag)
      # current gen might be > 0 if user loaded a pattern file
      # that set the gen count
      currgen = int(golly.getgen())
      if newgen < currgen:
         golly.error("Can't go back any further.")
         return
   
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
      golly.dokey( golly.getkey() )    # allow keyboard interaction
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
