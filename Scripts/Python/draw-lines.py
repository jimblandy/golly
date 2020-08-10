# Allow user to draw one or more straight lines by clicking end points.
# Author: Andrew Trevorrow (andrew@trevorrow.com), Jan 2011.

import golly as g

# ------------------------------------------------------------------------------

def drawline(x1, y1, x2, y2):
    # draw a line of cells from x1,y1 to x2,y2 using Bresenham's algorithm;
    # we also return the old cells in the line so we can erase line later
    oldcells = []
    # note that x1,y1 has already been drawn
    # oldcells.append( (x1, y1, g.getcell(x1, y1)) )
    # g.setcell(x1, y1, drawstate)
    if x1 == x2 and y1 == y2:
        g.update()
        return oldcells

    dx = x2 - x1
    ax = abs(dx) * 2
    sx = 1
    if dx < 0: sx = -1
    dy = y2 - y1
    ay = abs(dy) * 2
    sy = 1
    if dy < 0: sy = -1

    if ax > ay:
        d = ay - (ax // 2)
        while x1 != x2:
            oldcells.append( (x1, y1, g.getcell(x1, y1)) )
            g.setcell(x1, y1, drawstate)
            if d >= 0:
                y1 += sy
                d -= ax
            x1 += sx
            d += ay
    else:
        d = ax - (ay // 2)
        while y1 != y2:
            oldcells.append( (x1, y1, g.getcell(x1, y1)) )
            g.setcell(x1, y1, drawstate)
            if d >= 0:
                x1 += sx
                d -= ay
            y1 += sy
            d += ax

    oldcells.append( (x2, y2, g.getcell(x2, y2)) )
    g.setcell(x2, y2, drawstate)
    g.update()
    return oldcells

# ------------------------------------------------------------------------------

def eraseline(oldcells):
    for t in oldcells:
        x, y, s = t
        g.setcell(x, y, s)

# ------------------------------------------------------------------------------

def drawlines():
    global oldline, firstcell
    started = False
    oldmouse = ""
    while True:
        event = g.getevent()
        if event.startswith("click"):
            # event is a string like "click 10 20 left altctrlshift"
            evt, x, y, butt, mods = event.split()
            oldmouse = x + ' ' + y
            if started:
                # draw permanent line from start pos to end pos
                endx = int(x)
                endy = int(y)
                drawline(startx, starty, endx, endy)
                # this is also the start of another line
                startx = endx
                starty = endy
                oldline = []
                firstcell = []
            else:
                # start first line
                startx = int(x)
                starty = int(y)
                firstcell = [ startx, starty, g.getcell(startx, starty) ]
                g.setcell(startx, starty, drawstate)
                g.update()
                started = True
                g.show("Click where to end this line (and start another line)...")
        else:
            # event might be "" or "key m none"
            if len(event) > 0: g.doevent(event)
            mousepos = g.getxy()
            if started and len(mousepos) == 0:
                # erase old line if mouse is not over grid
                if len(oldline) > 0:
                    eraseline(oldline)
                    oldline = []
                    g.update()
            elif started and len(mousepos) > 0 and mousepos != oldmouse:
                # mouse has moved, so erase old line (if any) and draw new line
                if len(oldline) > 0: eraseline(oldline)
                x, y = mousepos.split()
                oldline = drawline(startx, starty, int(x), int(y))
                oldmouse = mousepos

# ------------------------------------------------------------------------------

g.show("Click where to start line...")
oldcursor = g.getcursor()
g.setcursor("Draw")
drawstate = g.getoption("drawingstate")
oldline = []
firstcell = []    # pos and state of the 1st cell clicked

try:
    drawlines()
finally:
    g.setcursor(oldcursor)
    if len(oldline) > 0: eraseline(oldline)
    if len(firstcell) > 0:
        x, y, s = firstcell
        g.setcell(x, y, s)
