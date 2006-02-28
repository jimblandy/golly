from glife import rect
import golly as g

r = rect( g.getselrect() )

if r.empty:
   g.error("There is no selection.")

else:
   for row in range (r.top, r.top + r.height):
      for col in range (r.left, r.left + r.width):
         g.setcell(col, row, 1 - g.getcell(col, row))

   if not r.visible():
      g.fitsel()
