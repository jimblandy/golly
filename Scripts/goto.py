# Go to a requested generation.  The given generation can be an
# absolute number like 1,000,000 (commas are optional) or a number
# relative to the current generation like +9 or -6.  If the target
# generation is less than the current generation then we go back
# to the starting generation (normally 0) and advance to the target.
# Authors: Andrew Trevorrow and Dave Greene, April 2006.
# Updated Sept-Oct 2006 -- XRLE support and reusable default value.

from glife import getstring, validint
from time import time
import os
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
         golly.error("Can't go back any further; pattern was saved "\
         + "at generation " + str(currgen) + ".")
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
GotoINIFileName = os.path.join(golly.appdir() + "Scripts","goto.ini")
previousgen=prevdefault=""
if os.access(GotoINIFileName, os.R_OK):
   f=open(GotoINIFileName, 'r')
   previousgen=f.readline()
   f.close()
   # should maybe change validint in __init__.py
   # (add "if len(s)==0: return False")
   # to avoid this initial conditional...
   if len(previousgen):
      if not validint(previousgen):
         previousgen=""
if previousgen!="":
   prevdefault=" (default is " + previousgen + ")"

gen = getstring("Go to what generation" + prevdefault + "?")
if len(gen) == 0:
   gen = previousgen
if len(gen) == 0:
   golly.show("")
elif gen=="+" or gen=="-":
   previousgen="" # provides a way to clear the default
elif not validint(gen):
   golly.error('Sorry, but "' + gen + '" is not a valid integer.')
else:
   previousgen=gen
   goto(gen.replace(",",""))
if os.access(GotoINIFileName, os.W_OK) or not os.access(GotoINIFileName, os.F_OK):
   try:
      f=open(GotoINIFileName, 'w')
      f.write(previousgen)
      f.close()
   except:
      golly.show("Unable to save defaults to " + GotoINIFileName)
