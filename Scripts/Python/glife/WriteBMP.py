# Just to save the user having to install PIL.
# BMP code from: http://pseentertainmentcorp.com/smf/index.php?topic=2034.0
# Distributed with kind permission from James Main <jdmain@comcast.net>

import struct

def WriteBMP(pixels,filename):
    '''
    Write a BMP to filename from the (r,g,b) triples in pixels[row][column].

    Usage example:

    WriteBMP( [[(255,0,0),(0,255,0),(255,255,0)],[(0,0,255),(0,0,0),(0,255,255)]], "test.bmp" )
    '''
    # Here is a minimal dictionary with header values.
    d = {
        'mn1':66,
        'mn2':77,
        'filesize':0,
        'undef1':0,
        'undef2':0,
        'offset':54,
        'headerlength':40,
        'width':len(pixels[0]),
        'height':len(pixels),
        'colorplanes':1,
        'colordepth':24,
        'compression':0,
        'imagesize':0,
        'res_hor':0,
        'res_vert':0,
        'palette':0,
        'importantcolors':0
        }
    # Build the byte array.  This code takes the height
    # and width values from the dictionary above and
    # generates the pixels row by row.  The row_mod and padding
    # stuff is necessary to ensure that the byte count for each
    # row is divisible by 4.  This is part of the specification.
    bytes = ''
    for row in range(d['height']-1,-1,-1): # (BMPs are encoded left-to-right from the bottom-left)
        for column in range(d['width']):
            r,g,b = pixels[row][column]
            pixel = struct.pack('<BBB',b,g,r)
            bytes += pixel
        row_mod = (d['width']*d['colordepth']//8) % 4
        if row_mod == 0:
            padding = 0
        else:
            padding = (4 - row_mod)
        padbytes = ''
        for i in range(padding):
            x = struct.pack('<B',0)
            padbytes = padbytes + x
        bytes = bytes + padbytes
    # These header values are described in the bmp format spec.
    # You can find it on the internet. This is for a Windows
    # Version 3 DIB header.
    mn1 = struct.pack('<B',d['mn1'])
    mn2 = struct.pack('<B',d['mn2'])
    filesize = struct.pack('<L',d['filesize'])
    undef1 = struct.pack('<H',d['undef1'])
    undef2 = struct.pack('<H',d['undef2'])
    offset = struct.pack('<L',d['offset'])
    headerlength = struct.pack('<L',d['headerlength'])
    width = struct.pack('<L',d['width'])
    height = struct.pack('<L',d['height'])
    colorplanes = struct.pack('<H',d['colorplanes'])
    colordepth = struct.pack('<H',d['colordepth'])
    compression = struct.pack('<L',d['compression'])
    imagesize = struct.pack('<L',d['imagesize'])
    res_hor = struct.pack('<L',d['res_hor'])
    res_vert = struct.pack('<L',d['res_vert'])
    palette = struct.pack('<L',d['palette'])
    importantcolors = struct.pack('<L',d['importantcolors'])
    # create the outfile
    outfile = open(filename,'wb')
    # write the header + the bytes
    outfile.write(mn1+mn2+filesize+undef1+undef2+offset+headerlength+width+height+\
                  colorplanes+colordepth+compression+imagesize+res_hor+res_vert+\
                  palette+importantcolors+bytes)
    outfile.flush()
    outfile.close()
