# Use multiple layers to create a history of the current pattern.
# The "envelope" layer remembers all live cells.
# Author: Andrew Trevorrow (andrew@trevorrow.com), December 2006.
# Updated for better compatibility with envelope.pl, June 2007.
# Updated to use new setcolors command, September 2008.

import golly as g

if g.empty(): g.exit("There is no pattern.")

currindex = g.getlayer()
startindex = 0
envindex = 0
startname = "starting pattern"
envname = "envelope"

if currindex > 1 and g.getname(currindex - 1) == startname \
                 and g.getname(currindex - 2) == envname :
    # continue from where we left off
    startindex = currindex - 1
    envindex = currindex - 2

elif currindex + 2 < g.numlayers() \
                 and g.getname(currindex + 1) == startname \
                 and g.getname(currindex)     == envname :
    # switch from envelope layer to current layer and continue
    currindex += 2
    g.setlayer(currindex)
    startindex = currindex - 1
    envindex = currindex - 2

elif currindex + 1 < g.numlayers() \
                 and g.getname(currindex)     == startname \
                 and g.getname(currindex - 1) == envname :
    # switch from starting layer to current layer and continue
    currindex += 1
    g.setlayer(currindex)
    startindex = currindex - 1
    envindex = currindex - 2

else:
    # start a new envelope using pattern in current layer
    if g.numlayers() + 1 > g.maxlayers():
        g.exit("You need to delete a couple of layers.")
    if g.numlayers() + 2 > g.maxlayers():
        g.exit("You need to delete a layer.")

    # get current layer's starting pattern
    startpatt = g.getcells(g.getrect())

    envindex = g.addlayer()       # create layer for remembering all live cells
    g.setcolors([-1,100,100,100]) # set all states to darkish gray
    g.putcells(startpatt)         # copy starting pattern into this layer

    startindex = g.addlayer()     # create layer for starting pattern
    g.setcolors([-1,0,255,0])     # set all states to green
    g.putcells(startpatt)         # copy starting pattern into this layer

    # move currindex to above the envelope and starting pattern
    g.movelayer(currindex, envindex)
    g.movelayer(envindex, startindex)
    currindex = startindex
    startindex = currindex - 1
    envindex = currindex - 2

    # name the starting and envelope layers so user can run script
    # again and continue from where it was stopped
    g.setname(startname, startindex)
    g.setname(envname, envindex)

# ------------------------------------------------------------------------------

def envelope ():
    # draw stacked layers using same location and scale
    g.setoption("stacklayers", 1)

    g.show("Hit escape key to stop script...")
    while True:
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
        g.putcells(currpatt)
        g.setlayer(currindex)
        g.check(1)

        step = 1
        exp = g.getstep()
        if exp > 0:
            step = g.getbase()**exp
        if int(g.getgen()) % step == 0:
            # display all 3 layers (envelope, start, current)
            g.update()

# ------------------------------------------------------------------------------

# show status bar but hide layer & edit bars (faster, and avoids flashing)
oldstatus = g.setoption("showstatusbar", True)
oldlayerbar = g.setoption("showlayerbar", False)
oldeditbar = g.setoption("showeditbar", False)

try:
    envelope()
finally:
    # this code is always executed, even after escape/error;
    # restore original state of status/layer/edit bars
    g.setoption("showstatusbar", oldstatus)
    g.setoption("showlayerbar", oldlayerbar)
    g.setoption("showeditbar", oldeditbar)
