# Oscar is an OSCillation AnalyzeR.
# It uses Gabriel Nivasch's "keep minima" algorithm:
#
# For each generation, calculate a hash value for the pattern.  Keep all of
# the record-breaking minimal hashes in a list, with the oldest first.
# For example, after 5 generations the saved hash values might be:
# 
# 8 12 16 24 25,
# 
# If the next hash goes down to 13 then the list can be shortened:
# 
# 8 12 13.
# 
# When the current hash matches one of the saved hashes, it is highly likely
# the pattern is oscillating.  By keeping a corresponding list of generation
# counts we can calculate the period.  We also keep lists of population
# counts and bounding boxes that are used to reduce the chance of spurious
# oscillator detection due to hash collisions.  The bounding box info also
# allows us to detect spaceships/knightships.

from glife import *
import golly as g
from time import clock

# ------------------------------------------------------------------------------

# initialize lists
hashlist = []        # for pattern hash values
genlist = []         # corresponding generation counts
poplist = []         # corresponding population counts
boxlist = []         # corresponding bounding boxes

# ------------------------------------------------------------------------------

def bigs2int(bigs):
   # convert a bigint string like "1,234,..." to an int < 10^9
   return int( bigs.replace(",", "")[-9:] )

# ------------------------------------------------------------------------------

def same_rects(r1, r2):
   # return True if given rectangles are identical
   return (r1.x == r2.x) and (r1.wd == r2.wd) and \
          (r1.y == r2.y) and (r1.ht == r2.ht)

# ------------------------------------------------------------------------------

def show_spaceship_speed(period, deltax, deltay):
   if period == 1:
      g.show("Spaceship detected (speed = c)")
   elif (deltax == deltay) or (deltax == 0) or (deltay == 0):
      speed = ""
      if (deltax == 0) or (deltay == 0):
         # orthogonal spaceship
         if (deltax > 1) or (deltay > 1):
            speed += str(deltax + deltay)
      else:
         # diagonal spaceship (deltax == deltay)
         if deltax > 1:
            speed += str(deltax)
      g.show("Spaceship detected (speed = " + str(speed) + "c/" +str(period) + ")")
   else:
      # deltax != deltay and both > 0
      speed = str(deltay) + "," + str(deltax)
      g.show("Knightship detected (speed = " + str(speed) + "c/" + str(period) + ")")

# ------------------------------------------------------------------------------

def oscillation_detected():
   # first get current pattern's bounding box
   prect = g.getrect()
   pbox = rect(prect)
   if pbox.empty:
      g.show("The pattern is empty.")
      return True
   
   # get current pattern and create hash of "normalized" version -- ie. shift
   # its top left corner to 0,0 -- so we can detect spaceships and knightships
   patt = pattern( g.getcells(prect) )
   h = hash( tuple( patt(-pbox.left, -pbox.top) ) )
   
   # determine where to insert h into hashlist
   pos = 0
   listlen = len(hashlist)
   while pos < listlen:
      if h > hashlist[pos]:
         pos += 1
      elif h < hashlist[pos]:
         # shorten lists and append info below
         del hashlist[pos : listlen]
         del genlist[pos : listlen]
         del poplist[pos : listlen]
         del boxlist[pos : listlen]
         break
      else:
         # h == hashlist[pos] so pattern is probably oscillating, but just in
         # case this is a hash collision we also compare pop count and box size
         if (g.getpop() == poplist[pos]) and \
            (pbox.wd == boxlist[pos].wd) and \
            (pbox.ht == boxlist[pos].ht):
            period = bigs2int(g.getgen()) - bigs2int(genlist[pos])
            if period == 1:
               if same_rects(pbox, boxlist[pos]):
                  g.show("The pattern is stable.")
               else:
                  show_spaceship_speed(1, 0, 0)
            elif same_rects(pbox, boxlist[pos]):
               g.show("Oscillator detected (period = " + str(period) + ")")
            else:
               deltax = abs(boxlist[pos].x - pbox.x)
               deltay = abs(boxlist[pos].y - pbox.y)
               show_spaceship_speed(period, deltax, deltay)
            return True
         else:
            # look at next matching hash value or insert if no more
            pos += 1
   
   # store hash/gen/pop/box info at same position in various lists
   hashlist.insert(pos, h)
   genlist.insert(pos, g.getgen())
   poplist.insert(pos, g.getpop())
   boxlist.insert(pos, pbox)

   return False

# ------------------------------------------------------------------------------

def fit_if_not_visible():
   # fit pattern in viewport if not empty and not completely visible
   r = rect(g.getrect())
   if (not r.empty) and (not r.visible()): g.fit()

# ------------------------------------------------------------------------------

g.setoption("showstatusbar", True)
g.show("Checking for oscillation... (hit escape to abort)")

oldsecs = clock()
while not oscillation_detected():
   g.run(1)
   newsecs = clock()
   if newsecs - oldsecs >= 1.0:     # show pattern every second
      oldsecs = newsecs
      fit_if_not_visible()
      g.update()

fit_if_not_visible()
