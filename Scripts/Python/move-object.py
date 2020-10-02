# Allow user to move a connected group of live cells.
# Author: Andrew Trevorrow (andrew@trevorrow.com), Jan 2011.

import golly as g
from glife import rect, getminbox

ncells = []    # list of neighboring live cells

# set edges of bounded grid for later use
if g.getwidth() > 0:
    gridl = -int(g.getwidth()/2)
    gridr = gridl + g.getwidth() - 1
if g.getheight() > 0:
    gridt = -int(g.getheight()/2)
    gridb = gridt + g.getheight() - 1

# --------------------------------------------------------------------

def showhelp():
    g.note(
"""Alt-clicking on an object allows you to COPY
it to another location (the original object
is not deleted).
 
While dragging the object the following keys
can be used:

x  - flip object left-right
y  - flip object top-bottom
>  - rotate object clockwise
<  - rotate object anticlockwise
escape  - abort and restore the object""")

# ------------------------------------------------------------------------------

def getstate(x, y):
    # first check if x,y is outside bounded grid
    if g.getwidth() > 0 and (x < gridl or x > gridr): return 0
    if g.getheight() > 0 and (y < gridt or y > gridb): return 0
    return g.getcell(x, y)

# ------------------------------------------------------------------------------

def findlivecell(x, y):
    if g.getcell(x, y) > 0: return [x, y]

    # spiral outwards from x,y looking for a nearby live cell;
    # the smaller the scale the smaller the area searched
    maxd = 10
    mag = g.getmag()
    if mag > 0:
        # mag can be 1..5 (ie. scales 1:2 to 1:32)
        maxd = 2 * (6 - mag)    # 10, 8, 6, 4, 2
    d = 1
    while d <= maxd:
        x -= 1
        y -= 1
        for i in range(2*d):
            x += 1   # move east
            if getstate(x, y) > 0: return [x, y]
        for i in range(2*d):
            y += 1   # move south
            if getstate(x, y) > 0: return [x, y]
        for i in range(2*d):
            x -= 1   # move west
            if getstate(x, y) > 0: return [x, y]
        for i in range(2*d):
            y -= 1   # move north
            if getstate(x, y) > 0: return [x, y]
        d += 1

    return []   # failed to find a live cell

# ------------------------------------------------------------------------------

def checkneighbor(x, y):
    # first check if x,y is outside bounded grid
    if g.getwidth() > 0 and (x < gridl or x > gridr): return
    if g.getheight() > 0 and (y < gridt or y > gridb): return
    if g.getcell(x, y) == 0: return
    # no need for next test because we kill cell after adding it to ncells
    # if (x, y) in ncells: return False
    ncells.append( (x, y, g.getcell(x,y)) )
    g.setcell(x, y, 0)

# ------------------------------------------------------------------------------

def getobject(x, y):
    object = []
    ncells.append( (x, y, g.getcell(x,y)) )
    g.setcell(x, y, 0)
    while len(ncells) > 0:
        # remove cell from end of ncells and append to object
        x, y, s = ncells.pop()
        object.append(x)
        object.append(y)
        object.append(s)
        # add any live neighbors to ncells
        checkneighbor(x  , y+1)
        checkneighbor(x  , y-1)
        checkneighbor(x+1, y  )
        checkneighbor(x-1, y  )
        checkneighbor(x+1, y+1)
        checkneighbor(x+1, y-1)
        checkneighbor(x-1, y+1)
        checkneighbor(x-1, y-1)
    # append padding int if necessary
    if len(object) > 0 and (len(object) & 1) == 0: object.append(0)
    g.putcells(object)
    return object

# ------------------------------------------------------------------------------

def underneath(object):
    # return list of live cells underneath given object (a multi-state list)
    cells = []
    objlen = len(object)
    if objlen % 3 == 1: objlen -= 1    # ignore padding int
    i = 0
    while i < objlen:
        x = object[i]
        y = object[i+1]
        s = g.getcell(x, y)
        if s > 0:
            cells.append(x)
            cells.append(y)
            cells.append(s)
        i += 3
    # append padding int if necessary
    if len(cells) > 0 and (len(cells) & 1) == 0: cells.append(0)
    return cells

# ------------------------------------------------------------------------------

def rectingrid(r):
    # return True if all of given rectangle is inside grid
    if g.getwidth() > 0 and (r[0] < gridl or r[0] + r[2] - 1 > gridr): return False
    if g.getheight() > 0 and (r[1] < gridt or r[1] + r[3] - 1 > gridb): return False
    return True

# ------------------------------------------------------------------------------

def lookforkeys(event):
    global oldcells, object

    # look for keys used to flip/rotate object
    if event == "key x none" or event == "key y none":
        # flip floating object left-right or top-bottom
        g.putcells(object, 0, 0, 1, 0, 0, 1, "xor")  # erase object
        if len(oldcells) > 0: g.putcells(oldcells)
        obox = getminbox(object)
        if event == "key x none":
            # translate object so that bounding box doesn't change
            xshift = 2 * (obox.left + int(obox.wd//2))
            if obox.wd % 2 == 0: xshift -= 1
            object = g.transform(object, xshift, 0, -1, 0, 0, 1)
        else:
            # translate object so that bounding box doesn't change
            yshift = 2 * (obox.top + int(obox.ht//2))
            if obox.ht % 2 == 0: yshift -= 1
            object = g.transform(object, 0, yshift, 1, 0, 0, -1)
        oldcells = underneath(object)
        g.putcells(object)
        g.update()
        return

    if event == "key > none" or event == "key < none":
        # rotate floating object clockwise or anticlockwise
        # about the center of the object's bounding box
        obox = getminbox(object)
        midx = obox.left + int(obox.wd//2)
        midy = obox.top + int(obox.ht//2)
        newleft = midx + obox.top - midy
        newtop = midy + obox.left - midx
        rotrect = [ newleft, newtop, obox.ht, obox.wd ]
        if not rectingrid(rotrect):
            g.show("Rotation is not allowed if object would be outside grid.")
            return
        g.putcells(object, 0, 0, 1, 0, 0, 1, "xor")  # erase object
        if len(oldcells) > 0: g.putcells(oldcells)
        if event == "key > none":
            # rotate clockwise
            object = g.transform(object, 0, 0, 0, -1, 1, 0)
        else:
            # rotate anticlockwise
            object = g.transform(object, 0, 0, 0, 1, -1, 0)
        # shift rotated object to same position as rotrect
        obox = getminbox(object)
        object = g.transform(object, newleft - obox.left, newtop - obox.top)
        oldcells = underneath(object)
        g.putcells(object)
        g.update()
        return

    if event == "key h none":
        # best not to show Help window while dragging object!
        return

    g.doevent(event)

# ------------------------------------------------------------------------------

def moveobject():
    global oldcells, object, object1

    # wait for click in or near a live cell
    while True:
        event = g.getevent()
        if event.startswith("click"):
            # event is a string like "click 10 20 left none"
            evt, xstr, ystr, butt, mods = event.split()
            result = findlivecell(int(xstr), int(ystr))
            if len(result) > 0:
                prevx = int(xstr)
                prevy = int(ystr)
                oldmouse = xstr + ' ' + ystr
                g.show("Extracting object...")
                x, y = result
                object = getobject(x, y)
                object1 = list(object)     # save in case user aborts script
                if mods == "alt":
                    # don't delete object
                    oldcells = list(object)
                break
            else:
                g.warn("Click on or near a live cell belonging to the desired object.")
        elif event == "key h none":
            showhelp()
        else:
            g.doevent(event)

    # wait for mouse-up while moving object
    g.show("Move mouse and release button...")
    mousedown = True
    while mousedown:
        event = g.getevent()
        if event.startswith("mup"):
            mousedown = False
        elif len(event) > 0:
            lookforkeys(event)
        mousepos = g.getxy()
        if len(mousepos) > 0 and mousepos != oldmouse:
            # mouse has moved, so move object
            g.putcells(object, 0, 0, 1, 0, 0, 1, "xor")  # erase object
            if len(oldcells) > 0: g.putcells(oldcells)
            xstr, ystr = mousepos.split()
            x = int(xstr)
            y = int(ystr)

            if g.getwidth() > 0:
                # ensure object doesn't move beyond left/right edge of grid
                obox = getminbox( g.transform(object, x - prevx, y - prevy) )
                if obox.left < gridl:
                    x += gridl - obox.left
                elif obox.right > gridr:
                    x -= obox.right - gridr
            if g.getheight() > 0:
                # ensure object doesn't move beyond top/bottom edge of grid
                obox = getminbox( g.transform(object, x - prevx, y - prevy) )
                if obox.top < gridt:
                    y += gridt - obox.top
                elif obox.bottom > gridb:
                    y -= obox.bottom - gridb

            object = g.transform(object, x - prevx, y - prevy)
            oldcells = underneath(object)
            g.putcells(object)
            prevx = x
            prevy = y
            oldmouse = mousepos
            g.update()

# ------------------------------------------------------------------------------

if len(g.getrect()) == 0: g.exit("There are no objects.")

g.show("Click on or near live cell, move mouse and release button... (hit 'h' for help)")
oldcursor = g.getcursor()
g.setcursor("Move")
oldcells = []        # cells under moved object
object = []          # cells in moving object
object1 = []         # cells in initial object

try:
    aborted = True
    moveobject()
    aborted = False
finally:
    g.setcursor(oldcursor)
    if aborted:
        # erase object if it moved
        if len(object) > 0: g.putcells(object, 0, 0, 1, 0, 0, 1, "xor")
        if len(oldcells) > 0: g.putcells(oldcells)
        if len(object1) > 0: g.putcells(object1)
    else:
        g.show(" ")
