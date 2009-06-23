# Display all patterns in Golly's Patterns folder.
# Author: Andrew Trevorrow (andrew@trevorrow.com), March 2006.

from glife import *
import golly as g

import os
from os.path import join
from time import sleep

# ------------------------------------------------------------------------------

def slideshow ():
   oldalgo = g.getalgo()
   oldrule = g.getrule()
   
   g.show("Hit space to continue or escape to exit the slide show...")
   for root, dirs, files in os.walk(g.getdir("app") + "Patterns"):
      for name in files:
         if name.startswith("."):
            # ignore hidden files (like .DS_Store on Mac)
            pass
         elif name.endswith(".pl") or name.endswith(".py"):
            # ignore Perl/Python scripts (Golly's RunScript is not re-entrant)
            pass
         else:
            fullname = join(root, name)
            g.open(fullname, False)       # don't add file to Open Recent submenu
            g.update()
            
            while True:
               ch = g.getkey()
               if ch == " ": break
               g.dokey(ch)                # allow keyboard interaction
               sleep(0.01)                # avoid hogging cpu
      
      if "CVS" in dirs:
         dirs.remove("CVS")  # don't visit CVS directories
   
   # if all patterns have been displayed then restore original algo and rule
   # (don't do this if user hits escape in case they want to explore pattern)
   g.new("untitled")
   g.setalgo(oldalgo)
   g.setrule(oldrule)

# ------------------------------------------------------------------------------

# show status bar but hide other info to maximize viewport
oldstatus = g.setoption("showstatusbar", True)
oldtoolbar = g.setoption("showtoolbar", False)
oldlayerbar = g.setoption("showlayerbar", False)
oldeditbar = g.setoption("showeditbar", False)
oldscripts = g.setoption("showscripts", False)
oldpatterns = g.setoption("showpatterns", False)

try:
   slideshow()
finally:
   # this code is always executed, even after escape/error;
   # clear message line in case there was no escape/error
   g.show("")
   # restore original state
   g.setoption("showstatusbar", oldstatus)
   g.setoption("showtoolbar", oldtoolbar)
   g.setoption("showlayerbar", oldlayerbar)
   g.setoption("showeditbar", oldeditbar)
   g.setoption("showscripts", oldscripts)
   g.setoption("showpatterns", oldpatterns)
