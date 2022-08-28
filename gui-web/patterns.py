# Enumerate the Patterns folder in a format suitable to paste into shell.html.

import os
from os.path import join, isfile

def walkdir(dir):
    result = ""
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
                result += "addfile(\"" + fullpath[2:] + "\");\n"
        else:
            temp = walkdir(fullpath)
            # temp will be empty if dir has no files
            if len(temp) > 0:
                result += "dirStart(\"" + item + "\");\n"
                result += temp
                result += "dirEnd();\n"
    return result

print(walkdir("../Patterns"))
