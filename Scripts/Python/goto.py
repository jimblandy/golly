# Go to a requested generation.  The given generation can be an
# absolute number like 1,000,000 (commas are optional) or a number
# relative to the current generation like +9 or -6.  If the target
# generation is less than the current generation then we go back
# to the starting generation (normally 0) and advance to the target.
# Authors: Andrew Trevorrow and Dave Greene, April 2006.
# Updated Sept-Oct 2006 -- XRLE support and reusable default value.
# Updated April 2010 -- much faster, thanks to PM 2Ring.

from glife import validint
from time import time
import golly as g

# --------------------------------------------------------------------

def intbase(n, b):
    # convert integer n >= 0 to a base b digit list (thanks to PM 2Ring)
    digits = []
    while n > 0:
        digits += [n % b]
        n //= b
    return digits or [0]

# --------------------------------------------------------------------

def goto(gen):
    currgen = int(g.getgen())
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
        # algorithm and/or rule, so too bad if user changed those
        # after the starting info was saved;
        # first save current location and scale
        midx, midy = g.getpos()
        mag = g.getmag()
        g.reset()
        # restore location and scale
        g.setpos(midx, midy)
        g.setmag(mag)
        # current gen might be > 0 if user loaded a pattern file
        # that set the gen count
        currgen = int(g.getgen())
        if newgen < currgen:
            g.error("Can't go back any further; pattern was saved " +
                    "at generation " + str(currgen) + ".")
            return
    if newgen == currgen: return

    g.show("Hit escape to abort...")
    oldsecs = time()

    # before stepping we advance by 1 generation, for two reasons:
    # 1. if we're at the starting gen then the *current* step size
    #    will be saved (and restored upon Reset/Undo) rather than a
    #    possibly very large step size
    # 2. it increases the chances the user will see updates and so
    #    get some idea of how long the script will take to finish
    #    (otherwise if the base is 10 and a gen like 1,000,000,000
    #    is given then only a single step() of 10^9 would be done)
    g.run(1)
    currgen += 1

    # use fast stepping (thanks to PM 2Ring)
    for i, d in enumerate(intbase(newgen - currgen, g.getbase())):
        if d > 0:
            g.setstep(i)
            for j in range(d):
                if g.empty():
                    g.show("Pattern is empty.")
                    return
                g.step()
                newsecs = time()
                if newsecs - oldsecs >= 1.0:  # do an update every sec
                    oldsecs = newsecs
                    g.update()
    g.show("")

# --------------------------------------------------------------------

def savegen(filename, gen):
    try:
        f = open(filename, 'w')
        f.write(gen)
        f.close()
    except:
        g.warn("Unable to save given gen in file:\n" + filename)

# --------------------------------------------------------------------

# use same file name as in goto.lua
GotoINIFileName = g.getdir("data") + "goto.ini"
previousgen = ""
try:
    f = open(GotoINIFileName, 'r')
    previousgen = f.readline()
    f.close()
    if not validint(previousgen):
        previousgen = ""
except:
    # should only happen 1st time (GotoINIFileName doesn't exist)
    pass

gen = g.getstring("Enter the desired generation number,\n" +
                  "or -n/+n to go back/forwards by n:",
                  previousgen, "Go to generation")
if len(gen) == 0:
    g.exit()
elif gen == "+" or gen == "-":
    # clear the default
    savegen(GotoINIFileName, "")
elif not validint(gen):
    g.exit('Sorry, but "' + gen + '" is not a valid integer.')
else:
    # best to save given gen now in case user aborts script
    savegen(GotoINIFileName, gen)
    oldstep = g.getstep()
    goto(gen.replace(",",""))
    g.setstep(oldstep)
