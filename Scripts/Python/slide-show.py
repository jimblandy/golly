# Display all patterns in Golly's Patterns folder.
# Author: Andrew Trevorrow (andrew@trevorrow.com), March 2006.

from glife import *
import golly as g

import os
from os.path import join

from time import sleep

# remember initial hashing state so we can restore it if changed by a pattern file
inithash = g.getoption("hashing")

# ------------------------------------------------------------------------------

def slideshow ():
   for root, dirs, files in os.walk(g.appdir() + "Patterns"):
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
            
            g.show("Hit space to continue or escape to exit the slide show...")
            while True:
               sleep(0.01)                # avoid hogging cpu
               ch = g.getkey()
               if ch == " ": break
               g.dokey(ch)                # allow keyboard interaction
            
            g.new("")
            if inithash != g.getoption("hashing"):
               if inithash:
                  # turn on hashing (B0-not-S8 rule turned it off)
                  g.setrule("B3/S23")
                  g.setoption("hashing", True)
               else:
                  # turn off hashing (.mc file turned it on)
                  g.setoption("hashing", False)
      
      if "CVS" in dirs:
         dirs.remove("CVS")  # don't visit CVS directories

# ------------------------------------------------------------------------------

# show status bar but hide other info to maximize viewport
oldstatus = g.setoption("showstatusbar", True)
oldtoolbar = g.setoption("showtoolbar", False)
oldlayerbar = g.setoption("showlayerbar", False)
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
   g.setoption("showscripts", oldscripts)
   g.setoption("showpatterns", oldpatterns)
