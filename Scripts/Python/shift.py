# Shift current selection by given x y amounts using optional mode.
# Author: Andrew Trevorrow (andrew@trevorrow.com), June 2006.
# Updated to use exit command, Nov 2006.
# Updated to check for bounded grid, Oct 2010.

from glife import validint, inside
import golly as g

selrect = g.getselrect()
if len(selrect) == 0: g.exit("There is no selection.")

# use same file name as in shift.lua
INIFileName = g.getdir("data") + "shift.ini"
oldparams = "0 0 or"
try:
    f = open(INIFileName, 'r')
    oldparams = f.readline()
    f.close()
except:
    # should only happen 1st time (INIFileName doesn't exist)
    pass

answer = g.getstring("Enter x y shift amounts and an optional mode\n" +
                     "(valid modes are copy/or/xor, default is or):",
                     oldparams, "Shift selection")
xym = answer.split()

# extract x and y amounts
if len(xym) == 0: g.exit()
if len(xym) == 1: g.exit("Supply x and y amounts separated by a space.")
if not validint(xym[0]): g.exit("Bad x value: " + xym[0])
if not validint(xym[1]): g.exit("Bad y value: " + xym[1])
x = int(xym[0])
y = int(xym[1])

# extract optional mode
if len(xym) > 2:
    mode = xym[2].lower()
    if mode=="c": mode="copy"
    if mode=="o": mode="or"
    if mode=="x": mode="xor"
    if not (mode == "copy" or mode == "or" or mode == "xor"):
        g.exit("Unknown mode: " + xym[2] + " (must be copy/or/xor)")
else:
    mode = "or"

# given parameters are valid so save them for next run
try:
    f = open(INIFileName, 'w')
    f.write(answer)
    f.close()
except:
    g.warn("Unable to save given parameters in file:\n" + INIFileName)

# abort shift if the new selection would be outside a bounded grid
if g.getwidth() > 0:
    gridl = -int(g.getwidth()/2)
    gridr = gridl + g.getwidth() - 1
    newl = selrect[0] + x
    newr = newl + selrect[2] - 1
    if newl < gridl or newr > gridr:
        g.exit("New selection would be outside grid.")
if g.getheight() > 0:
    gridt = -int(g.getheight()/2)
    gridb = gridt + g.getheight() - 1
    newt = selrect[1] + y
    newb = newt + selrect[3] - 1
    if newt < gridt or newb > gridb:
        g.exit("New selection would be outside grid.")

# do the shift by cutting the current selection and pasting it into
# the new position without changing the current clipboard pattern
selcells = g.getcells(selrect)
g.clear(inside)
selrect[0] += x
selrect[1] += y
g.select(selrect)
if mode == "copy":
    g.clear(inside)
g.putcells(selcells, x, y, 1, 0, 0, 1, mode)

if not g.visrect(selrect): g.fitsel()
