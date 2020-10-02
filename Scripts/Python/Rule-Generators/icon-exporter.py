# Extract icon images in current layer (created by icon-importer.py)
# and either create or update the appropriate .rule file.
# Author: Andrew Trevorrow (andrew@trevorrow.com), Feb 2013.

import golly as g
from glife import getminbox, pattern
from glife.BuiltinIcons import circles, diamonds, hexagons, triangles
import os
from tempfile import mkstemp
from shutil import move

iconcolors = []     # list of (r,g,b) colors used in all icons

allcolors = g.getcolors()
deadrgb = (allcolors[1], allcolors[2], allcolors[3])

# this flag becomes True only if an icon uses a non-gray color (ignoring deadrgb)
multi_color_icons = False   # grayscale

# ------------------------------------------------------------------------------

def hex2(i):
    # convert number from 0..255 into 2 hex digits
    hexdigit = "0123456789ABCDEF"
    result = hexdigit[i // 16]
    result += hexdigit[i % 16]
    return result

# --------------------------------------------------------------------

def extract_icons(iconsize):
    global iconcolors, allcolors, deadrgb, multi_color_icons
    
    # do 1st pass to determine the colors used and the number of icons
    numicons = 0
    while True:
        blank = True
        x = numicons*32
        y = 0
        if iconsize == 15: y = 32
        if iconsize == 7: y = 48
        for row in range(iconsize):
            for col in range(iconsize):
                state = g.getcell(x+col, y+row)
                rgb = (allcolors[state*4+1], allcolors[state*4+2], allcolors[state*4+3])
                if not rgb in iconcolors:
                    iconcolors.append(rgb)
                    if rgb != deadrgb:
                        R,G,B = rgb
                        if R != G or G != B: multi_color_icons = True
                if state > 0:
                    blank = False
        if blank: break
        numicons += 1
    
    if numicons == 0: return ""
    
    # build string of lines suitable for @ICONS section
    charsperpixel = 1
    numcolors = len(iconcolors)
    if numcolors > 26: charsperpixel = 2

    icondata = "\nXPM\n"
    icondata += "/* width height num_colors chars_per_pixel */\n"
    icondata += "\"" + str(iconsize) + " " + str(iconsize*numicons) + " " + \
                  str(numcolors) + " " + str(charsperpixel) + "\"\n"
    
    icondata += "/* colors */\n"
    cindex = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    n = 0
    for RGB in iconcolors:
        R,G,B = RGB
        hexcolor = "#" + hex2(R) + hex2(G) + hex2(B)
        if RGB == deadrgb:
            # use . or .. for background color
            icondata += "\"."
            if charsperpixel == 2: icondata += "."
            if not multi_color_icons:
                # grayscale icons so specify black as the background color
                hexcolor = "#000000"
        else:
            icondata += "\""
            if charsperpixel == 1:
                icondata += cindex[n]
            else:
                icondata += cindex[n % 16] + cindex[n // 16]
        icondata += " c " + hexcolor + "\"\n"
        n += 1
    
    for i in range(numicons):
        icondata += "/* icon for state " + str(i+1) + " */\n"
        x = i*32
        y = 0
        if iconsize == 15: y = 32
        if iconsize == 7: y = 48
        for row in range(iconsize):
            icondata += "\""
            for col in range(iconsize):
                state = g.getcell(x+col, y+row)
                if state == 0:
                    # show . or .. for background color
                    icondata += "."
                    if charsperpixel == 2: icondata += "."
                else:
                    rgb = (allcolors[state*4+1], allcolors[state*4+2], allcolors[state*4+3])
                    n = 0
                    for RGB in iconcolors:
                        if rgb == RGB: break
                        n += 1
                    if charsperpixel == 1:
                        icondata += cindex[n]
                    else:
                        icondata += cindex[n % 16] + cindex[n // 16]
            icondata += "\"\n"
    
    return icondata

# --------------------------------------------------------------------

def parse_hex(hexstr):
    # parse a string like "FF00EE" or "FFFF0000EEEE"
    R, G, B = (0, 0, 0)
    if len(hexstr) == 6:
        R = int(hexstr[0:2],16)
        G = int(hexstr[2:4],16)
        B = int(hexstr[4:6],16)
    elif len(hexstr) == 12:
        # only use upper 2 hex digits
        R = int(hexstr[0:2],16)
        G = int(hexstr[4:6],16)
        B = int(hexstr[8:10],16)
    else:
        g.warn("Unexpected hex string: " + hexstr)
    return (R,G,B)

# --------------------------------------------------------------------

def create_average_colors(iconsection):
    global deadrgb
    colordata = "@COLORS\n\n"
    
    R, G, B = deadrgb
    colordata += "0 " + str(R) + " " + str(G) + " " + str(B) + "\n"

    width = 0
    height = 0
    num_colors = 0
    chars_per_pixel = 0
    xpmcount = 0
    colordict = {}
    row = 0
    state = 0
    nbcount = 0
    totalR = 0
    totalG = 0
    totalB = 0
    
    for line in iconsection.splitlines():
        if len(line) > 0 and line[0] == "\"":
            # extract the stuff inside double quotes
            line = line.lstrip("\"").rstrip("\"")
            xpmcount += 1
            if xpmcount == 1:
                # parse "width height num_colors chars_per_pixel"
                header = line.split()
                width = int(header[0])
                height = int(header[1])
                num_colors = int(header[2])
                chars_per_pixel = int(header[3])
            
            elif xpmcount > 1 and xpmcount <= 1 + num_colors:
                # parse color index line like "A c #FFFFFF" or "AB c #FF009900BB00"
                key, c, hexrgb = line.split()
                rgb = parse_hex(hexrgb.lstrip("#"))
                colordict[key] = rgb
            
            elif xpmcount <= 1 + num_colors + height:
                # parse pixel data in line like "......AAA......"
                for col in range(width):
                    offset = col*chars_per_pixel
                    key = line[offset : offset + chars_per_pixel]
                    if not key in colordict:
                        g.warn("Unexpected key in icon data: " + key)
                        return colordata
                    if key[0] != ".":
                        R, G, B = colordict[key]
                        nbcount += 1
                        totalR += R
                        totalG += G
                        totalB += B
                row += 1
                if row == width:
                    # we've done this icon
                    state += 1
                    if nbcount > 0:
                        colordata += str(state) + " " + str(totalR // nbcount) + " " \
                                                      + str(totalG // nbcount) + " " \
                                                      + str(totalB // nbcount) + "\n"
                    else:
                        # avoid div by zero
                        colordata += str(state) + " 0 0 0\n"
                    # reset counts for next icon
                    row = 0
                    nbcount = 0
                    totalR = 0
                    totalG = 0
                    totalB = 0
                if xpmcount == 1 + num_colors + height:
                    break
    
    colordata += "\n"
    return colordata

# --------------------------------------------------------------------

def get_color_section(rulepath):
    colordata = ""
    rulefile = open(rulepath,"rU")
    copylines = False
    for line in rulefile:
        if line.startswith("@COLORS"):
            copylines = True
        elif copylines and line.startswith("@"):
            break
        if copylines:
            colordata += line
    rulefile.close()
    return colordata

# --------------------------------------------------------------------

def export_icons(iconsection, rulename):
    global multi_color_icons
    
    if multi_color_icons:
        # prepend a new @COLORS section with the average colors in each icon
        iconsection = create_average_colors(iconsection) + iconsection
        
    # replace any illegal filename chars with underscores
    filename = rulename.replace("/","_").replace("\\","_")

    # we will only create/update a .rule file in the user's rules folder
    # (ie. we don't modify the supplied Rules folder)
    rulepath = g.getdir("rules") + filename + ".rule"
    fileexists = os.path.isfile(rulepath)
    if fileexists:
        # .rule file already exists so replace or add @ICONS section
        rulefile = open(rulepath,"rU")
    
        # create a temporary file for writing new rule info
        temphdl, temppath = mkstemp()
        tempfile = open(temppath,"w")
        
        wroteicons = False
        skiplines = False
        for line in rulefile:
            if line.startswith("@ICONS"):
                # replace the existing @ICONS section
                tempfile.write(iconsection)
                wroteicons = True
                skiplines = True
            elif line.startswith("@COLORS") and multi_color_icons:
                # skip the existing @COLORS section
                # (iconsection contains a new @COLORS section)
                skiplines = True
            elif skiplines and line.startswith("@"):
                if wroteicons: tempfile.write("\n")
                skiplines = False
            if not skiplines:
                tempfile.write(line)
        
        if not wroteicons:
            # .rule file had no @ICONS section
            tempfile.write("\n")
            tempfile.write(iconsection)
        
        # close files
        rulefile.close()
        tempfile.flush()
        tempfile.close()
        os.close(temphdl)
        
        # remove original .rule file and rename temporary file
        os.remove(rulepath)
        move(temppath, rulepath)
        
    else:
        # .rule file doesn't exist so create it
        rulefile = open(rulepath,"w")
        rulefile.write("@RULE " + filename + "\n\n")
        
        if not multi_color_icons:
            # grayscale icons, so check if Rules/filename.rule exists
            # and if so copy any existing @COLORS section
            suppliedrule = g.getdir("app") + "Rules/" + filename + ".rule"
            if os.path.isfile(suppliedrule):
                colordata = get_color_section(suppliedrule)
                if len(colordata) > 0:
                    rulefile.write(colordata)
        
        rulefile.write(iconsection)
        rulefile.flush()
        rulefile.close()
    
    # create another layer for displaying the new icons
    if g.numlayers() < g.maxlayers():
        g.addlayer()
        g.new("icon test")
        g.setrule(rulename)
        for i in range(g.numstates()-1):
            g.setcell(i, 0, i+1)
        g.fit()
        g.setoption("showicons",True)
        g.update()
    
    if fileexists:
        g.note("Updated the icon data in " + rulepath)
    else:
        g.note("Created " + rulepath)

# --------------------------------------------------------------------

# WARNING: changing this prefix will require same change in icon-importer.py
layerprefix = "imported icons for "

# check that the current layer was created by icon-importer.py
layername = g.getname()
if not layername.startswith(layerprefix):
    g.exit("The current layer wasn't created by icon-importer.py.")

# get the original rule
rulename = layername.split(" for ")[1]

icondata = extract_icons(31)
icondata += extract_icons(15)
icondata += extract_icons(7)

if len(icondata) == 0:
    g.exit("There are no icon images to export.")

if not multi_color_icons:
    # check if the icon data matches Golly's built-in grayscale icons
    if icondata == circles:
        icondata = "\ncircles\n"
    elif icondata == diamonds:
        icondata = "\ndiamonds\n"
    elif icondata == hexagons:
        icondata = "\nhexagons\n"
    elif icondata == triangles:
        icondata = "\ntriangles\n"

export_icons("@ICONS\n" + icondata, rulename)
