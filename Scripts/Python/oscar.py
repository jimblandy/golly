# Oscar is an OSCillation AnalyzeR for use with Golly.
# Author: Andrew Trevorrow (andrew@trevorrow.com), March 2006.
# Modified to handle B0-and-not-S8 rules, August 2009.

# This script uses Gabriel Nivasch's "keep minima" algorithm.
# For each generation, calculate a hash value for the pattern.  Keep all of
# the record-breaking minimal hashes in a list, with the oldest first.
# For example, after 5 generations the saved hash values might be:
#
#   8 12 16 24 25,
#
# If the next hash goes down to 13 then the list can be shortened:
#
#   8 12 13.
#
# When the current hash matches one of the saved hashes, it is highly likely
# the pattern is oscillating.  By keeping a corresponding list of generation
# counts we can calculate the period.  We also keep lists of population
# counts and bounding boxes; they are used to reduce the chance of spurious
# oscillator detection due to hash collisions.  The bounding box info also
# allows us to detect moving oscillators (spaceships/knightships).

import golly as g
from glife import rect, pattern
from time import time

# --------------------------------------------------------------------

# initialize lists
hashlist = []        # for pattern hash values
genlist = []         # corresponding generation counts
poplist = []         # corresponding population counts
boxlist = []         # corresponding bounding boxes

# --------------------------------------------------------------------

def show_spaceship_speed(period, deltax, deltay):
    # we found a moving oscillator
    if (deltax == deltay) or (deltax == 0) or (deltay == 0):
        speed = ""
        if (deltax == 0) or (deltay == 0):
            # orthogonal spaceship
            if (deltax > 1) or (deltay > 1):
                speed += str(deltax + deltay)
        else:
            # diagonal spaceship (deltax == deltay)
            if deltax > 1:
                speed += str(deltax)
        if period == 1:
            g.show("Spaceship detected (speed = " + speed + "c)")
        else:
            g.show("Spaceship detected (speed = " + speed + "c/" +str(period) + ")")
    else:
        # deltax != deltay and both > 0
        speed = str(deltay) + "," + str(deltax)
        if period == 1:
            g.show("Knightship detected (speed = " + speed + "c)")
        else:
            g.show("Knightship detected (speed = " + speed + "c/" + str(period) + ")")

# --------------------------------------------------------------------

def oscillating():
    # return True if the pattern is empty, stable or oscillating

    # first get current pattern's bounding box
    prect = g.getrect()
    pbox = rect(prect)
    if pbox.empty:
        g.show("The pattern is empty.")
        return True

    # get current pattern and create hash of "normalized" version -- ie. shift
    # its top left corner to 0,0 -- so we can detect spaceships and knightships
    ## currpatt = pattern( g.getcells(prect) )
    ## h = hash( tuple( currpatt(-pbox.left, -pbox.top) ) )

    # use Golly's hash command (3 times faster than above code)
    h = g.hash(prect)

    # check if outer-totalistic rule has B0 but not S8
    rule = g.getrule().split(":")[0]
    hasB0notS8 = rule.startswith("B0") and (rule.find("/") > 1) and not rule.endswith("8")

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
            if (int(g.getpop()) == poplist[pos]) and \
               (pbox.wd == boxlist[pos].wd) and \
               (pbox.ht == boxlist[pos].ht):
                period = int(g.getgen()) - genlist[pos]

                if hasB0notS8 and (period % 2 > 0) and (pbox == boxlist[pos]):
                    # ignore this hash value because B0-and-not-S8 rules are
                    # emulated by using different rules for odd and even gens,
                    # so it's possible to have identical patterns at gen G and
                    # gen G+p if p is odd
                    return False

                if pbox == boxlist[pos]:
                    # pattern hasn't moved
                    if period == 1:
                        g.show("The pattern is stable.")
                    else:
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
    genlist.insert(pos, int(g.getgen()))
    poplist.insert(pos, int(g.getpop()))
    boxlist.insert(pos, pbox)

    return False

# --------------------------------------------------------------------

def fit_if_not_visible():
    # fit pattern in viewport if not empty and not completely visible
    r = rect(g.getrect())
    if (not r.empty) and (not r.visible()): g.fit()

# --------------------------------------------------------------------

g.show("Checking for oscillation... (hit escape to abort)")

oldsecs = time()
while not oscillating():
    g.run(1)
    newsecs = time()
    if newsecs - oldsecs >= 1.0:     # show pattern every second
        oldsecs = newsecs
        fit_if_not_visible()
        g.update()

fit_if_not_visible()
