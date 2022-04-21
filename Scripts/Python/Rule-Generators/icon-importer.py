# Import any icons for the current rule so the user can edit them
# and when finished run icon-exporter.py.
# Author: Andrew Trevorrow (andrew@trevorrow.com), Feb 2013.

import golly as g
from glife import getminbox, pattern
from glife.text import make_text
from glife.BuiltinIcons import circles, diamonds, hexagons, triangles
from colorsys import hsv_to_rgb
import os

iconinfo31 = []     # info for 31x31 icons
iconinfo15 = []     # info for 15x15 icons
iconinfo7 = []      # info for 7x7 icons
iconcolors = []     # list of (r,g,b) colors used in ALL icon sizes
colorstate = {}     # dictionary for mapping (r,g,b) colors to cell states

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

def use_builtin_icons(shape):
    global iconinfo31, iconinfo15, iconinfo7, iconcolors
    
    if shape == "circles": lines = circles.split('\n')
    if shape == "diamonds": lines = diamonds.split('\n')
    if shape == "hexagons": lines = hexagons.split('\n')
    if shape == "triangles": lines = triangles.split('\n')
    
    xpmcount = 0
    width = 0
    height = 0
    num_colors = 0
    chars_per_pixel = 0
    iconinfo = []
    colordict = {}
    iconcolors = []
    
    for line in lines:
        if line == "XPM":
            xpmcount = 1
            iconinfo = []
            colordict = {}
        elif xpmcount > 0 and line[0] == "\"":
            # extract the stuff inside double quotes, ignoring anything after 2nd one
            line = line.lstrip("\"").split("\"")[0]
            if xpmcount == 1:
                # parse "width height num_colors chars_per_pixel"
                header = line.split()
                width = int(header[0])
                height = int(header[1])
                num_colors = int(header[2])
                chars_per_pixel = int(header[3])
                iconinfo.append(width)
                iconinfo.append(height)
                iconinfo.append(num_colors)
                iconinfo.append(chars_per_pixel)
            
            elif xpmcount > 1 and xpmcount <= 1 + num_colors:
                # parse color index line like "A c #FFFFFF" or "AB c #FF009900BB00"
                key, c, hexrgb = line.split()
                rgb = parse_hex(hexrgb.lstrip("#"))
                if not rgb in iconcolors:
                    iconcolors.append(rgb)
                colordict[key] = rgb
                if xpmcount == 1 + num_colors:
                    iconinfo.append(colordict)
            
            elif xpmcount <= 1 + num_colors + height:
                # simply append pixel data in line like "......AAA......"
                iconinfo.append(line)
            
            if xpmcount == 1 + num_colors + height:
                if width == 31: iconinfo31 = iconinfo
                if width == 15: iconinfo15 = iconinfo
                if width == 7: iconinfo7 = iconinfo
                xpmcount = 0    # skip any extra lines
            else:
                xpmcount += 1

# --------------------------------------------------------------------

def import_icons(rulename):
    global iconinfo31, iconinfo15, iconinfo7, iconcolors
    
    # replace any illegal filename chars with underscores
    rulename = rulename.replace("/","_").replace("\\","_")
    
    rulepath = g.getdir("rules") + rulename + ".rule"
    if not os.path.isfile(rulepath):
        rulepath = g.getdir("app") + "Rules/" + rulename + ".rule"
        if not os.path.isfile(rulepath):
            # there is no .rule file
            return
    
    try:
        # open in universal newline mode to handle LF, CR, or CR+LF
        rulefile = open(rulepath,"rU")
    except:
        g.exit("Failed to open .rule file: " + rulepath)
    
    foundicons = False
    xpmcount = 0
    width = 0
    height = 0
    num_colors = 0
    chars_per_pixel = 0
    iconinfo = []
    colordict = {}
    iconcolors = []
    
    # WARNING: The code below must agree with how Golly looks for icon info
    # (see LoadRuleInfo and ParseIcons in wxlayer.cpp).  In particular, if
    # there are multiple @ICONS sections then only the 1st one is used.
    
    for line in rulefile:
        if foundicons and line.startswith("@"):
            # start of another section (possibly another @ICONS section)
            break
        line = line.rstrip("\n")
        if line == "@ICONS":
            foundicons = True
        elif foundicons and line == "XPM":
            xpmcount = 1
            iconinfo = []
            colordict = {}
        elif foundicons and line in ("circles","diamonds","hexagons","triangles"):
            use_builtin_icons(line)
            # don't break (agrees with Golly)
        elif xpmcount > 0 and line[0] == "\"":
            # extract the stuff inside double quotes, ignoring anything after 2nd one
            line = line.lstrip("\"").split("\"")[0]
            if xpmcount == 1:
                # parse "width height num_colors chars_per_pixel"
                header = line.split()
                width = int(header[0])
                height = int(header[1])
                num_colors = int(header[2])
                chars_per_pixel = int(header[3])
                iconinfo.append(width)
                iconinfo.append(height)
                iconinfo.append(num_colors)
                iconinfo.append(chars_per_pixel)
            
            elif xpmcount > 1 and xpmcount <= 1 + num_colors:
                # parse color index line like "A c #FFFFFF" or "AB c #FF009900BB00"
                key, c, hexrgb = line.split()
                rgb = parse_hex(hexrgb.lstrip("#"))
                if not rgb in iconcolors:
                    iconcolors.append(rgb)
                colordict[key] = rgb
                if xpmcount == 1 + num_colors:
                    iconinfo.append(colordict)
            
            elif xpmcount <= 1 + num_colors + height:
                # simply append pixel data in line like "......AAA......"
                iconinfo.append(line)
            
            if xpmcount == 1 + num_colors + height:
                if width == 31: iconinfo31 = iconinfo
                if width == 15: iconinfo15 = iconinfo
                if width == 7: iconinfo7 = iconinfo
                xpmcount = 0    # skip any extra lines
            else:
                xpmcount += 1
    
    rulefile.close()

# --------------------------------------------------------------------

def check_for_shared_rule(rulename):
    # rulename has at least one hyphen so get all chars before the last hyphen
    prefix = rulename.rsplit('-',1)[0]
    
    # replace any illegal filename chars with underscores
    filename = prefix.replace("/","_").replace("\\","_") + "-shared.rule"
    
    rulepath = g.getdir("rules") + filename
    if not os.path.isfile(rulepath):
        rulepath = g.getdir("app") + "Rules/" + filename
        if not os.path.isfile(rulepath):
            # there is no prefix-shared.rule file
            return ""
    
    # ask user if they would prefer to edit icons in shared file
    sharedname = prefix + "-shared"
    try:
        answer = g.getstring("There are no icons in " + rulename + ".rule.\n" +
                             "Would you prefer to edit the icons in " + sharedname + ".rule?",
                             "Yes", "Edit icons in shared rule?")
    except:
        # user hit Cancel (which would normally abort script)
        return ""
    if len(answer) == 0 or (answer[0] != "Y" and answer[0] != "y"):
        return ""
    
    return sharedname   # user said Yes

# --------------------------------------------------------------------

def draw_line(x1, y1, x2, y2, state = 1):
    # draw a line of cells in given state from x1,y1 to x2,y2
    g.setcell(x1, y1, state)
    if x1 == x2 and y1 == y2: return
    
    dx = x2 - x1
    ax = abs(dx) * 2
    sx = 1
    if dx < 0: sx = -1
    dy = y2 - y1
    ay = abs(dy) * 2
    sy = 1
    if dy < 0: sy = -1
    
    if ax > ay:
        d = ay - (ax // 2)
        while x1 != x2:
            g.setcell(x1, y1, state)
            if d >= 0:
                y1 += sy
                d -= ax
            x1 += sx
            d += ay
    else:
        d = ax - (ay // 2)
        while y1 != y2:
            g.setcell(x1, y1, state)
            if d >= 0:
                x1 += sx
                d -= ay
            y1 += sy
            d += ax
    
    g.setcell(x2, y2, state)

# --------------------------------------------------------------------

def color_text(string, extrastate):
    t = make_text(string, "mono")
    bbox = getminbox(t)
    # convert two-state pattern to multi-state and set state to extrastate
    mlist = []
    tlist = list(t)
    for i in range(0, len(tlist), 2):
        mlist.append(tlist[i])
        mlist.append(tlist[i+1])
        mlist.append(extrastate)
    if len(mlist) % 2 == 0: mlist.append(0)
    p = pattern(mlist)
    return p, bbox.wd, bbox.ht

# --------------------------------------------------------------------

def init_colors():
    global iconcolors, colorstate
    if len(iconcolors) > 240: g.exit("The imported icons use too many colors!")
    
    # start with gradient from white to black (this makes it easier to
    # copy grayscale icons from one rule to another rule)
    s = 1
    g.setcolors([s,255,255,255])
    colorstate[(255,255,255)] = s
    s += 1
    graylevel = 256
    while graylevel > 0:
        graylevel -= 32
        g.setcolors([s, graylevel, graylevel, graylevel])
        colorstate[(graylevel, graylevel, graylevel)] = s
        s += 1

    # now add all the colors used in the imported icons (unless added above)
    for rgb in iconcolors:
        if not rgb in colorstate:
            R,G,B = rgb
            g.setcolors([s,R,G,B])
            colorstate[rgb] = s
            s += 1
    
    # finally add rainbow colors in various shades (bright, pale, dark)
    for hue in range(12):
        if s > 255: break
        R,G,B = hsv_to_rgb(hue/12.0, 1.0, 1.0)
        g.setcolors([s, int(255*R), int(255*G), int(255*B)])
        s += 1
    for hue in range(12):
        if s > 255: break
        R,G,B = hsv_to_rgb(hue/12.0, 0.5, 1.0)
        g.setcolors([s, int(255*R), int(255*G), int(255*B)])
        s += 1
    for hue in range(12):
        if s > 255: break
        R,G,B = hsv_to_rgb(hue/12.0, 1.0, 0.5)
        g.setcolors([s, int(255*R), int(255*G), int(255*B)])
        s += 1
    
    # return the 50% gray state (used for drawing boxes and text)
    return colorstate[(128,128,128)]

# --------------------------------------------------------------------

def draw_icon_boxes(numicons, linestate):
    for i in range(numicons):
        x = -1 + i*32
        y = -1
        
        # draw boxes for 31x31 icons
        draw_line(x, y, x, y+32, linestate)
        draw_line(x, y, x+32, y, linestate)
        draw_line(x+32, y, x+32, y+32, linestate)
        draw_line(x, y+32, x+32, y+32, linestate)
        
        # draw boxes for 15x15 icons
        draw_line(x, y+32, x, y+32+16, linestate)
        draw_line(x, y+32+16, x+16, y+32+16, linestate)
        draw_line(x+16, y+32, x+16, y+32+16, linestate)
        
        # draw boxes for 7x7 icons
        draw_line(x, y+32+16, x, y+32+16+8, linestate)
        draw_line(x, y+32+16+8, x+8, y+32+16+8, linestate)
        draw_line(x+8, y+32+16, x+8, y+32+16+8, linestate)
        
        # show state number above top row of icons
        t, twd, tht = color_text(str(i+1), linestate)
        t.put(x + 32//2 - twd//2, y - 2 - tht)

# --------------------------------------------------------------------

def draw_icons(iconinfo, transparent):
    global colorstate
    if len(iconinfo) == 0: return
    width = iconinfo[0]
    height = iconinfo[1]
    num_colors = iconinfo[2]
    chars_per_pixel = iconinfo[3]
    colordict = iconinfo[4]
    pos = 5
    numicons = height//width
    for i in range(numicons):
        x = i*32
        y = 0
        if width == 15: y = 32
        if width == 7: y = 48
        for row in range(width):
            pxls = iconinfo[pos]
            pos += 1
            for col in range(width):
                offset = col*chars_per_pixel
                key = pxls[offset : offset + chars_per_pixel]
                if not key in colordict:
                    g.exit("Unexpected key in icon data: " + key)
                rgb = colordict[key]
                if rgb != transparent:
                    g.setcell(x+col, y+row, colorstate[rgb])

# --------------------------------------------------------------------

def create31x31icons():
    # scale up the 15x15 bitmaps into 31x31 bitmaps using a simple
    # algorithm that conserves any vertical or horizontal symmetry
    global iconinfo15
    width = 15
    middle = 7                  # middle row or column in 15x15 icon
    height = iconinfo15[1]
    numicons = height//width
    for i in range(numicons):
        x = i*32
        y = 32
        for row in range(width):
            for col in range(width):
                state = g.getcell(x+col, y+row)
                if state > 0:
                    if row == middle and col == middle:
                        # expand middle cell into 9 cells
                        xx = i*32+15
                        yy = 15
                        g.setcell(xx, yy, state)
                        g.setcell(xx, yy+1, state)
                        g.setcell(xx, yy-1, state)
                        g.setcell(xx+1, yy, state)
                        g.setcell(xx-1, yy, state)
                        g.setcell(xx+1, yy+1, state)
                        g.setcell(xx+1, yy-1, state)
                        g.setcell(xx-1, yy+1, state)
                        g.setcell(xx-1, yy-1, state)
                    elif row == middle:
                        # expand cell in middle row into 6 cells
                        xx = i*32+col*2
                        yy = row*2
                        if col > middle: xx += 1
                        g.setcell(xx, yy, state)
                        g.setcell(xx, yy+1, state)
                        g.setcell(xx+1, yy, state)
                        g.setcell(xx+1, yy+1, state)
                        g.setcell(xx, yy+2, state)
                        g.setcell(xx+1, yy+2, state)
                    elif col == middle:
                        # expand cell in middle column into 6 cells
                        xx = i*32+col*2
                        yy = row*2
                        if row > middle: yy += 1
                        g.setcell(xx, yy, state)
                        g.setcell(xx, yy+1, state)
                        g.setcell(xx+1, yy, state)
                        g.setcell(xx+1, yy+1, state)
                        g.setcell(xx+2, yy, state)
                        g.setcell(xx+2, yy+1, state)
                    else:
                        # expand all other cells into 4 cells
                        xx = i*32+col*2
                        yy = row*2
                        if col > middle: xx += 1
                        if row > middle: yy += 1
                        g.setcell(xx, yy, state)
                        g.setcell(xx, yy+1, state)
                        g.setcell(xx+1, yy, state)
                        g.setcell(xx+1, yy+1, state)

# --------------------------------------------------------------------

def create_smaller_icons(big, small):
    # scale down the big x big bitmaps into small x small bitmaps
    # using a simple sampling algorithm
    global iconinfo15, iconinfo31
    if big == 15:
        numicons = iconinfo15[1] // 15
        ybig = 32
    else:
        # big = 31
        numicons = iconinfo31[1] // 31
        ybig = 0
    if small == 7:
        y = 48
    else:
        # small = 15
        y = 32
    sample = big // small
    offset = sample // 2
    for i in range(numicons):
        x = i*32
        for row in range(small):
            for col in range(small):
                state = g.getcell(x + offset + col*sample, ybig + offset + row*sample)
                if state > 0:
                    g.setcell(x+col, y+row, state)

# --------------------------------------------------------------------

def multi_color_icons(iconcolors):
    # return True if at least one icon color isn't a shade of gray
    for R,G,B in iconcolors:
        if R != G or G != B: return True
    # grayscale
    return False

# --------------------------------------------------------------------

# check that a layer is available
if g.numlayers() == g.maxlayers():
    g.exit("You need to delete a layer.")

# WARNING: changing this prefix will require same change in icon-exporter.py
layerprefix = "imported icons for "
if g.getname().startswith(layerprefix):
    g.exit("You probably meant to run icon-exporter.py.")

g.addlayer()
rulename = g.getrule().split(":")[0]

# search for rulename.rule and import any icon data (also builds iconcolors)
import_icons(rulename)

if len(iconcolors) == 0 and ("-" in rulename) and not (rulename.endswith("-shared")):
    # rulename.rule has no icons and rulename contains a hyphen, so
    # check if prefix-shared.rule exists and ask user if they want to edit
    # the icons in that file
    sharedname = check_for_shared_rule(rulename)
    if len(sharedname) > 0:
        rulename = sharedname
        import_icons(rulename)

iconnote = ""
if len(iconcolors) == 0:
    iconnote = "There are currently no icons for this rule.\n\n"

# check if icons are grayscale
grayscale_icons = not multi_color_icons(iconcolors)

g.new(layerprefix + rulename)
livestates = g.numstates() - 1
deadcolor = g.getcolors(0)
deadrgb = (deadcolor[1], deadcolor[2], deadcolor[3])

# switch to a Generations rule so we can have lots of colors
g.setrule("//256")
if grayscale_icons and deadrgb == (255,255,255):
    # if icons are grayscale and state 0 color was white then switch
    # to black background to avoid confusion (ie. we want black pixels
    # to be transparent)
    g.setcolors([0,0,0,0])
else:
    g.setcolors(deadcolor)
graystate = init_colors()

# if icons are grayscale then change deadrgb to black so that draw_icons
# will treat black pixels as transparent
if grayscale_icons: deadrgb = (0,0,0)

draw_icon_boxes(livestates, graystate)
draw_icons(iconinfo31, deadrgb)
draw_icons(iconinfo15, deadrgb)
draw_icons(iconinfo7, deadrgb)

# create missing icons by scaling up/down the existing icons
if len(iconinfo31) == 0 and len(iconinfo15) > 0:
    create31x31icons()
    iconnote += "The 31x31 icons were created by scaling up the 15x15 icons.\n\n"

if len(iconinfo15) == 0 and len(iconinfo31) > 0:
    create_smaller_icons(31, 15)
    iconnote += "The 15x15 icons were created by scaling down the 31x31 icons.\n\n"

if len(iconinfo7) == 0:
    if len(iconinfo15) > 0:
        create_smaller_icons(15, 7)
        iconnote += "The 7x7 icons were created by scaling down the 15x15 icons.\n\n"
    elif len(iconinfo31) > 0:
        create_smaller_icons(31, 7)
        iconnote += "The 7x7 icons were created by scaling down the 31x31 icons.\n\n"

g.setoption("showlayerbar",True)
g.setoption("showallstates",True)
g.setoption("showicons",False)
g.fit()
g.update()
g.note(iconnote + "Edit the icons and then run icon-exporter.py.")
