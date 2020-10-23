# Enumerate the Patterns folder in a format suitable to paste into shell.html.

import os
from os.path import join, isfile

def walkdir(dir):
    for item in sorted(os.listdir(dir)):
        fullpath = join(dir, item)
        if isfile(fullpath):
            if item.startswith("."):
                # ignore hidden files (like .DS_Store on Mac)
                pass
            elif item.endswith(".lua") or item.endswith(".zip"):
                # ignore Lua scripts and zip files
                pass
            else:
                print "addfile(\"" + fullpath[2:] + "\");"
        else:
            print "dirStart(\"" + item + "\");"
            walkdir(fullpath)
            print "dirEnd();"

walkdir("../Patterns")
