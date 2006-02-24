# Display all patterns in Golly's Patterns folder.

from glife import *
import golly as g

import os
from os.path import join

# ------------------------------------------------------------------------------

def slideshow ():
   for root, dirs, files in os.walk(g.appdir() + "Patterns"):
      for name in files:
         # ignore hidden files (like .DS_Store on Mac)
         if not name.startswith('.'):
            fullname = join(root, name)
            g.open(fullname)
            g.update()
            g.show("Hit space to continue or any other key to stop the slide show...")
            if g.getkey() != " ": return
      
      if "CVS" in dirs:
         dirs.remove("CVS")  # don't visit CVS directories

# ------------------------------------------------------------------------------

# save hashing state and turn it off!!!
# switch to full screen!!!
# show status bar so user sees message!!!

slideshow()
g.show("")

# restore original hashing state
# turn off full screen mode!!!
