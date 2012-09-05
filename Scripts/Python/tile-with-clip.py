# Tile current selection with clipboard pattern.
# Author: Andrew Trevorrow (andrew@trevorrow.com), March 2006.
# Updated to use exit command, Nov 2006.
# Updated to handle multi-state patterns, Aug 2008.

from glife import *
import golly as g

# assume one-state cell list (may change below)
inc = 2

# ------------------------------------------------------------------------------

def clip_rb (patt, right, bottom):
    # remove any cells outside given right and bottom edges
    clist = list(patt)

    #  remove padding int if present
    if (inc == 3) and (len(clist) % 3 == 1): clist.pop()

    x = 0
    y = 1
    while x < len(clist):
        if (clist[x] > right) or (clist[y] > bottom):
            # remove cell from list
            clist[x : x+inc] = []
        else:
            x += inc
            y += inc

    # append padding int if necessary
    if (inc == 3) and (len(clist) & 1 == 0): clist.append(0)

    return pattern(clist)

# ------------------------------------------------------------------------------

selrect = rect( g.getselrect() )
if selrect.empty: g.exit("There is no selection.")

cliplist = g.getclip()                    # 1st 2 items are wd,ht
pbox = rect( [0, 0] + cliplist[0 : 2] )
cliplist[0 : 2] = []                      # remove wd,ht
p = pattern( cliplist )

if len(cliplist) & 1 == 1: inc = 3        # multi-state?

g.clear(inside)
if len(cliplist) > 0:
    # tile selrect with p, clipping right & bottom edges if necessary
    y = selrect.top
    while y <= selrect.bottom:
        bottom = y + pbox.height - 1
        x = selrect.left
        while x <= selrect.right:
            right = x + pbox.width - 1
            if (right <= selrect.right) and (bottom <= selrect.bottom):
                p.put(x, y)
            else:
                clip_rb( p(x, y), selrect.right, selrect.bottom ).put()
            x += pbox.width
        y += pbox.height

if not selrect.visible(): g.fitsel()
