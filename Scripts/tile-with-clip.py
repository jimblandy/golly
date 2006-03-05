# Tile current selection with clipboard pattern.

from glife import *
import golly as g

# ------------------------------------------------------------------------------

def clip_rb (patt, right, bottom):
   # remove any cells outside given right and bottom edges
   clist = list(patt)
   x = 0
   y = 1
   while x < len(clist):
      if (clist[x] > right) or (clist[y] > bottom):
         # remove cell from list
         clist[x : x+2] = []
      else:
         x += 2
         y += 2
   return pattern(clist)

# ------------------------------------------------------------------------------

def main ():
   selrect = rect( g.getselrect() )
   
   if selrect.empty:
      g.error("There is no selection.")
      return

   cliplist = g.getclip()                    # 1st 2 items are wd,ht
   pbox = rect( [0, 0] + cliplist[0 : 2] )
   cliplist[0 : 2] = []                      # remove wd,ht
   p = pattern( cliplist )
   
   g.clear(inside)
   if len(p) > 0:
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
   return

# ------------------------------------------------------------------------------

main()
