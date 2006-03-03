# Display all patterns in Golly's Patterns folder.

from glife import *
import golly as g

import os
from os.path import join

hashstate = g.getoption("hashing")

# ------------------------------------------------------------------------------

def slideshow ():
   for root, dirs, files in os.walk(g.appdir() + "Patterns"):
      for name in files:
         # ignore hidden files (like .DS_Store on Mac)
         if not name.startswith('.'):
            fullname = join(root, name)
            g.open(fullname, 0)           # don't add file to Open Recent submenu
            g.update()
            g.show("Hit space to continue or any other key to stop the slide show...")
            if g.getkey() != " ": return
            if hashstate != g.getoption("hashing"):
               # turn off hashing in case a .mc file turned it on
               g.setoption("hashing", False)
      
      if "CVS" in dirs:
         dirs.remove("CVS")  # don't visit CVS directories

# ------------------------------------------------------------------------------

# show status bar so user sees messages
hidestatus = not g.getoption("showstatusbar")
g.setoption("showstatusbar", True)

# hide other stuff to maximize the viewport
showtoolbar = g.getoption("showtoolbar")
showscripts = g.getoption("showscripts")
showpatterns = g.getoption("showpatterns")
g.setoption("showtoolbar", False)
g.setoption("showscripts", False)
g.setoption("showpatterns", False)

slideshow()
g.show("")

# restore original state
if hidestatus: g.setoption("showstatusbar", False)
if showtoolbar: g.setoption("showtoolbar", True)
if showscripts: g.setoption("showscripts", True)
if showpatterns: g.setoption("showpatterns", True)
