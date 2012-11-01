# Create .rule files from corresponding .table/tree/colors/icons files in
# the user's rules folder.  It won't overwrite any existing .rule files.
# Author: Andrew Trevorrow (andrew@trevorrow.com), November 2012.

import golly as g
import os
from os.path import join

# check if the Python Imaging Library is installed
hasPIL = True
try:
    import Image
except:
    hasPIL = False
    g.warn("The Python Imaging Library needs to be installed to convert .icons files.  " +
           "If you don't want to create any .rule files, hit the escape key after " +
           "closing this dialog.")

# ------------------------------------------------------------------------------

def analyze_files(dir, allfiles):
    candidates = []
    message = "Analyzing the files in your rules folder: "
    for filename in allfiles:
        if filename.endswith(".colors") or filename.endswith(".icons") or \
           filename.endswith(".table") or filename.endswith(".tree"):
            g.show(message + filename)
            # add .rule file to candidates if it hasn't been added yet
            rulefile = filename.rpartition(".")[0] + ".rule"
            if not rulefile in candidates:
                candidates.append(rulefile)
    
    # look for .rule files of the form foo-*.rule and ignore any foo.rule
    # entries in candidates if foo.table and foo.tree don't exist
    # (ie. foo.colors and/or foo.icons is shared by foo-*.table/tree)
    ignore = []
    for rulefile in candidates:
        prefix, sep, suffix = rulefile.rpartition("-")
        if len(sep) > 0:
            tablefile = prefix + ".table"
            treefile = prefix + ".tree"
            if (not tablefile in allfiles) and (not treefile in allfiles):
                sharedfile = prefix + ".rule"
                if sharedfile in candidates:
                    ignore.append(sharedfile)
        # also ignore any existing .rule files
        if rulefile in allfiles:
            ignore.append(rulefile)
    
    # copy non-ignored candidates to result
    result = []
    for rulefile in candidates:
        if not rulefile in ignore:
            result.append(rulefile)
    del candidates
    del ignore
    return result

# ------------------------------------------------------------------------------

def create_TABLE(path):
    contents = "\n@TABLE\n\n"
    # append contents of .table file (use Universal mode to convert \r\n to \n)
    f = open(path, "rU")
    contents += f.read()
    f.close()
    if len(contents) > 0 and contents[len(contents)-1] != "\n":
        contents += "\n"
    return contents

# ------------------------------------------------------------------------------

def create_TREE(path):
    contents = "\n@TREE\n\n"
    # append contents of .tree file (use Universal mode to convert \r\n to \n)
    f = open(path, "rU")
    contents += f.read()
    f.close()
    if len(contents) > 0 and contents[len(contents)-1] != "\n":
        contents += "\n"
    return contents

# ------------------------------------------------------------------------------

def create_COLORS(path):
    contents = "\n@COLORS\n\n"
    f = open(path, "rU")    # use Universal mode to convert \r\n to \n
    while True:
        line = f.readline()
        if len(line) == 0: break
        if line.startswith("color") or line.startswith("gradient"):
            # strip off everything before 1st digit
            line = line.lstrip("colorgadient= \t")
        contents += line
    f.close()
    if len(contents) > 0 and contents[len(contents)-1] != "\n":
        contents += "\n"
    return contents

# ------------------------------------------------------------------------------

def create_state_colors(im, numicons):
    contents = "\n@COLORS\n\n"
    
    # if the last icon has only 1 color then assume it is the extra 15x15 icon
    # supplied to set the color of state 0
    if numicons > 1:
        icon = im.crop(((numicons-1)*15, 0, (numicons-1)*15+15, 15))
        colors = icon.getcolors()
        if len(colors) == 1:
            count, RGB = colors[0]
            R,G,B = RGB
            contents += "0 " + str(R) + " " + str(G) + " " + str(B) + "\n"
            numicons -= 1
    
    # set non-icon colors for each live state to the average of the non-black pixels
    # in each 15x15 icon (note we've skipped the extra icon detected above)
    for i in xrange(numicons):
        icon = im.crop((i*15, 0, i*15+15, 15))
        colors = icon.getcolors()
        nbcount = 0   # of non-black pixels
        totalR = 0
        totalG = 0
        totalB = 0
        for count, RGB in colors:
            R,G,B = RGB
            if R > 0 or G > 0 or B > 0:
                nbcount += count
                totalR += R * count
                totalG += G * count
                totalB += B * count
        if nbcount > 0:
            contents += str(i+1) + " " + str(totalR / nbcount) + " " \
                                       + str(totalG / nbcount) + " " \
                                       + str(totalB / nbcount) + "\n"
        else:
            # unlikely, but avoid div by zero
            contents += str(i+1) + " 0 0 0\n"
    
    return contents

# ------------------------------------------------------------------------------

def hex2(i):
    # convert number from 0..255 into 2 hex digits
    hexdigit = "0123456789ABCDEF"
    result = hexdigit[i / 16]
    result += hexdigit[i % 16]
    return result

# ------------------------------------------------------------------------------

def create_XPM(path, im, size, numicons):
    # create XPM data for given set of icons
    contents = "\nXPM\n"
    colors = im.getcolors()
    numcolors = len(colors)
    charsperpixel = 1
    cindex = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    if numcolors > 256:
        g.warn("Image in " + path + "has more than 256 colors.")
        g.exit()
    if numcolors > 26:
        charsperpixel = 2   # AABA..PA, ABBB..PB, ... , APBP..PP
    
    contents += "/* width height num_colors chars_per_pixel */\n"
    contents += "\"" + str(size) + " " + str(size*numicons) + " " + \
                  str(numcolors) + " " + str(charsperpixel) + "\"\n"
    
    contents += "/* colors */\n"
    n = 0
    for count, RGB in colors:
        R,G,B = RGB
        if R == 0 and G == 0 and B == 0:
            # nicer to show . or .. for black pixels
            contents += "\"."
            if charsperpixel == 2: contents += "."
            contents += " c #000000\"\n"
        else:
            hexcolor = "#" + hex2(R) + hex2(G) + hex2(B)
            contents += "\""
            if charsperpixel == 1:
                contents += cindex[n]
            else:
                contents += cindex[n % 16] + cindex[n / 16]
            contents += " c " + hexcolor + "\"\n"
        n += 1
    
    for i in xrange(numicons):
        contents += "/* icon for state " + str(i+1) + " */\n"
        icon = im.crop((i*15, 0, i*15+size, size))
        pxl = icon.load()
        for y in xrange(size):
            contents += "\""
            for x in xrange(size):
                R,G,B = pxl[x,y]
                if R == 0 and G == 0 and B == 0:
                    # nicer to show . or .. for black pixels
                    contents += "."
                    if charsperpixel == 2: contents += "."
                else:
                    n = 0
                    thisRGB = (R,G,B)
                    for count, RGB in colors:
                        if thisRGB == RGB: break
                        n += 1
                    if charsperpixel == 1:
                        contents += cindex[n]
                    else:
                        contents += cindex[n % 16] + cindex[n / 16]
            contents += "\"\n"
    
    return contents

# ------------------------------------------------------------------------------

def create_ICONS(path, colordata):
    global hasPIL
    if hasPIL:
        contents = "\n@ICONS\n"
        im = Image.open(path)
        if im.mode != "RGB": im = im.convert("RGB")
        wd, ht = im.size
        if not (ht == 15 or ht == 22):
            g.warn("Image in " + path + "has incorrect height (should be 15 or 22).")
            g.exit()
        if wd % 15 > 0:
            g.warn("Image in " + path + "has incorrect width (should be multiple of 15).")
            g.exit()
        numicons = wd / 15
        
        if len(colordata) == 0 and len(im.getcolors()) > 2:
            # there was no .colors file and .icons file is multi-color,
            # so prepend a @COLORS section that sets non-icon colors
            contents = create_state_colors(im.crop((0,0,wd,15)), numicons) + contents
        
        if ht == 15:
            contents += create_XPM(path, im, 15, numicons)
        else:
            contents += create_XPM(path, im.crop((0,0,wd,15)), 15, numicons)
            contents += create_XPM(path, im.crop((0,15,wd,22)), 7, numicons)
    else:
        contents = "\n# Python Imaging Library is not installed so could not convert .icons\n"
    return contents

# ------------------------------------------------------------------------------

def create_rules(dir, allfiles, rulefiles):
    numcreated = 0
    message = "Creating .rule files: "
    for rulefile in rulefiles:
        g.show(message + rulefile)
        rulename = rulefile.rpartition(".")[0]
        prefix, sep, suffix = rulename.rpartition("-")
        tablefile = rulename + ".table"
        treefile = rulename + ".tree"
        colorsfile = rulename + ".colors"
        iconsfile = rulename + ".icons"
        tabledata = ""
        treedata = ""
        colordata = ""
        icondata = ""
        
        if tablefile in allfiles:
            tabledata = create_TABLE(join(dir, tablefile))
        
        if treefile in allfiles:
            treedata = create_TREE(join(dir, treefile))
        
        if colorsfile in allfiles:
            colordata = create_COLORS(join(dir, colorsfile))
        elif len(sep) > 0:
            # check for shared .colors file
            sharedcolors = prefix + ".colors"
            if sharedcolors in allfiles:
                colordata = create_COLORS(join(dir, sharedcolors))
        
        if iconsfile in allfiles:
            icondata = create_ICONS(join(dir, iconsfile), colordata)
        elif len(sep) > 0:
            # check for shared .icons file
            sharedicons = prefix + ".icons"
            if sharedicons in allfiles:
                icondata = create_ICONS(join(dir, sharedicons), colordata)

        contents = "@RULE " + rulename + "\n"
        contents += tabledata
        contents += treedata
        contents += colordata
        contents += icondata
        
        # write contents to .rule file
        fullpath = join(dir, rulefile)
        f = open(fullpath, "w")
        f.write(contents)
        f.close()
        
        numcreated += 1
    return numcreated

# ------------------------------------------------------------------------------

numcreated = 0
try:
    dir = g.getdir("rules")
    # enable the next line to convert .table/tree/etc files in Golly's Rules folder
    # dir = g.getdir("app") + "Rules"
    allfiles = os.listdir(dir)
    rulefiles = analyze_files(dir, allfiles)
    numcreated = create_rules(dir, allfiles, rulefiles)
finally:
    # this code is always executed, even after escape/error
    g.show("Finished.")
    g.note("Number of .rule files created: " + str(numcreated))
