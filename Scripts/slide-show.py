# Display all patterns in Golly's Patterns folder.
# Author: Andrew Trevorrow (andrew@trevorrow.com), March 2006.

from glife import *
import golly as g

import os
from os.path import join

# remember initial hashing state so we can restore it if changed by a pattern file
inithash = g.getoption("hashing")

# ------------------------------------------------------------------------------

def readkey ():
   while True:
      ch = g.getkey()
      if len(ch) > 0: return ch

# ------------------------------------------------------------------------------

def slideshow ():
   for root, dirs, files in os.walk(g.appdir() + "Patterns"):
      for name in files:
         # ignore hidden files (like .DS_Store on Mac)
         if not name.startswith("."):
            fullname = join(root, name)
            g.open(fullname, False)       # don't add file to Open Recent submenu
            g.update()
            g.show("Hit space to continue or any other key to stop the slide show...")
            if readkey() != " ": return
            g.new("")
            if inithash != g.getoption("hashing"):
               if inithash:
                  # turn on hashing (B0-not-S8 rule turned it off)
                  g.setrule("b3/s23")
                  g.setoption("hashing", True)
               else:
                  # turn off hashing (.mc file turned it on)
                  g.setoption("hashing", False)
      
      if "CVS" in dirs:
         dirs.remove("CVS")  # don't visit CVS directories

# ------------------------------------------------------------------------------

# show status bar so user sees messages
initstatus = g.setoption("showstatusbar", True)

# hide other stuff to maximize the viewport
# (don't switch to fullscreen because user won't see pattern name in title)
inittoolbar = g.setoption("showtoolbar", False)
initscripts = g.setoption("showscripts", False)
initpatterns = g.setoption("showpatterns", False)

slideshow()
g.show("")

# restore original state
g.setoption("showstatusbar", initstatus)
g.setoption("showtoolbar", inittoolbar)
g.setoption("showscripts", initscripts)
g.setoption("showpatterns", initpatterns)
