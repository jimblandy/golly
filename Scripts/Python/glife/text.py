from glife import *

# each symbol is a pattern with an additional field .width

# Eric Angelini integer font

__eafont = dict ()
__eafont['0'] = pattern ("3o$obo$obo$obo$3o!", 0, -5)
__eafont['0'].width = 4
__eafont['1'] = pattern ("o$o$o$o$o!", 0, -5)
__eafont['1'].width = 2
__eafont['2'] = pattern ("3o$2bo$3o$o$3o!", 0, -5)
__eafont['2'].width = 4
__eafont['3'] = pattern ("3o$2bo$3o$2bo$3o!", 0, -5)
__eafont['3'].width = 4
__eafont['4'] = pattern ("obo$obo$3o$2bo$2bo!", 0, -5)
__eafont['4'].width = 4
__eafont['5'] = pattern ("3o$o$3o$2bo$3o!", 0, -5)
__eafont['5'].width = 4
__eafont['6'] = pattern ("3o$o$3o$obo$3o!", 0, -5)
__eafont['6'].width = 4
__eafont['7'] = pattern ("3o$2bo$2bo$2bo$2bo!", 0, -5)
__eafont['7'].width = 4
__eafont['8'] = pattern ("3o$obo$3o$obo$3o!", 0, -5)
__eafont['8'].width = 4
__eafont['9'] = pattern ("3o$obo$3o$2bo$3o!", 0, -5)
__eafont['9'].width = 4
__eafont[' '] = pattern ("", 0, 0)
__eafont[' '].width=2  # allow spaces to mark unknown characters
__eafont['-'] = pattern ("", 0, 0)
__eafont['-'].width=0  #

# Mono-spaced ASCII font

__mfont = dict()
__mfont[' '] = pattern("")
__mfont['!'] = pattern("2bo$2bo$2bo$2bo$2bo2$2bo!")
__mfont['"'] = pattern("bobo$bobo$bobo!")
__mfont['#'] = pattern("bobo$bobo$5o$bobo$5o$bobo$bobo!")
__mfont['$'] = pattern("b3o$obobo$obo$b3o$2bobo$obobo$b3o!")
__mfont['%'] = pattern("2o2bo$2o2bo$3bo$2bo$bo$o2b2o$o2b2o!")
__mfont['&'] = pattern("b2o$o2bo$o2bo$b2o$o2bo$o2bo$b2obo!")
__mfont['\''] = pattern("2bo$2bo$2bo!")
__mfont['('] = pattern("3bo$2bo$2bo$2bo$2bo$2bo$3bo!")
__mfont[')'] = pattern("bo$2bo$2bo$2bo$2bo$2bo$bo!")
__mfont['*'] = pattern("$obobo$b3o$5o$b3o$obobo!")
__mfont['+'] = pattern("$2bo$2bo$5o$2bo$2bo!")
__mfont[','] = pattern("6$2bo$2bo$bo!")
__mfont['-'] = pattern("3$5o!")
__mfont['.'] = pattern("6$2bo!")
__mfont['/'] = pattern("3bo$3bo$2bo$2bo$2bo$bo$bo!")
__mfont['0'] = pattern("b3o$o3bo$o2b2o$obobo$2o2bo$o3bo$b3o!")
__mfont['1'] = pattern("2bo$b2o$2bo$2bo$2bo$2bo$b3o!")
__mfont['2'] = pattern("b3o$o3bo$4bo$3bo$2bo$bo$5o!")
__mfont['3'] = pattern("b3o$o3bo$4bo$2b2o$4bo$o3bo$b3o!")
__mfont['4'] = pattern("3bo$2b2o$bobo$o2bo$5o$3bo$3bo!")
__mfont['5'] = pattern("5o$o$o$b3o$4bo$o3bo$b3o!")
__mfont['6'] = pattern("b3o$o$o$4o$o3bo$o3bo$b3o!")
__mfont['7'] = pattern("5o$4bo$3bo$2bo$bo$o$o!")
__mfont['8'] = pattern("b3o$o3bo$o3bo$b3o$o3bo$o3bo$b3o!")
__mfont['9'] = pattern("b3o$o3bo$o3bo$b4o$4bo$4bo$b3o!")
__mfont[':'] = pattern("2$2bo4$2bo!")
__mfont[';'] = pattern("2$2bo4$2bo$2bo$bo!")
__mfont['<'] = pattern("$3bo$2bo$bo$2bo$3bo!")
__mfont['='] = pattern("2$5o2$5o!")
__mfont['>'] = pattern("$bo$2bo$3bo$2bo$bo!")
__mfont['?'] = pattern("b3o$o3bo$4bo$2b2o$2bo2$2bo!")
__mfont['@'] = pattern("b3o$o3bo$ob3o$obobo$ob2o$o$b3o!")
__mfont['A'] = pattern("b3o$o3bo$o3bo$5o$o3bo$o3bo$o3bo!")
__mfont['B'] = pattern("4o$o3bo$o3bo$4o$o3bo$o3bo$4o!")
__mfont['C'] = pattern("b3o$o3bo$o$o$o$o3bo$b3o!")
__mfont['D'] = pattern("4o$o3bo$o3bo$o3bo$o3bo$o3bo$4o!")
__mfont['E'] = pattern("5o$o$o$3o$o$o$5o!")
__mfont['F'] = pattern("5o$o$o$3o$o$o$o!")
__mfont['G'] = pattern("b3o$o3bo$o$o2b2o$o3bo$o3bo$b3o!")
__mfont['H'] = pattern("o3bo$o3bo$o3bo$5o$o3bo$o3bo$o3bo!")
__mfont['I'] = pattern("b3o$2bo$2bo$2bo$2bo$2bo$b3o!")
__mfont['J'] = pattern("2b3o$3bo$3bo$3bo$3bo$o2bo$b2o!")
__mfont['K'] = pattern("o3bo$o2bo$obo$2o$obo$o2bo$o3bo!")
__mfont['L'] = pattern("o$o$o$o$o$o$5o!")
__mfont['M'] = pattern("o3bo$2ob2o$obobo$obobo$o3bo$o3bo$o3bo!")
__mfont['N'] = pattern("o3bo$2o2bo$obobo$o2b2o$o3bo$o3bo$o3bo!")
__mfont['O'] = pattern("b3o$o3bo$o3bo$o3bo$o3bo$o3bo$b3o!")
__mfont['P'] = pattern("4o$o3bo$o3bo$4o$o$o$o!")
__mfont['Q'] = pattern("b3o$o3bo$o3bo$o3bo$obobo$o2bo$b2obo!")
__mfont['R'] = pattern("4o$o3bo$o3bo$4o$o2bo$o3bo$o3bo!")
__mfont['S'] = pattern("b3o$o3bo$o$b3o$4bo$o3bo$b3o!")
__mfont['T'] = pattern("5o$2bo$2bo$2bo$2bo$2bo$2bo!")
__mfont['U'] = pattern("o3bo$o3bo$o3bo$o3bo$o3bo$o3bo$b3o!")
__mfont['V'] = pattern("o3bo$o3bo$o3bo$o3bo$o3bo$bobo$2bo!")
__mfont['W'] = pattern("o3bo$o3bo$o3bo$obobo$obobo$2ob2o$o3bo!")
__mfont['X'] = pattern("o3bo$o3bo$bobo$2bo$bobo$o3bo$o3bo!")
__mfont['Y'] = pattern("o3bo$o3bo$bobo$2bo$2bo$2bo$2bo!")
__mfont['Z'] = pattern("5o$4bo$3bo$2bo$bo$o$5o!")
__mfont['['] = pattern("2b2o$2bo$2bo$2bo$2bo$2bo$2b2o!")
__mfont['\\'] = pattern("bo$bo$2bo$2bo$2bo$3bo$3bo!")
__mfont[']'] = pattern("b2o$2bo$2bo$2bo$2bo$2bo$b2o!")
__mfont['^'] = pattern("2bo$bobo$o3bo!")
__mfont['_'] = pattern("6$5o!")
__mfont['`'] = pattern("o$bo!")
__mfont['a'] = pattern("2$b4o$o3bo$o3bo$o3bo$b4o!")
__mfont['b'] = pattern("o$o$4o$o3bo$o3bo$o3bo$4o!")
__mfont['c'] = pattern("2$b4o$o$o$o$b4o!")
__mfont['d'] = pattern("4bo$4bo$b4o$o3bo$o3bo$o3bo$b4o!")
__mfont['e'] = pattern("2$b3o$o3bo$5o$o$b4o!")
__mfont['f'] = pattern("2b2o$bo2bo$bo$3o$bo$bo$bo!")
__mfont['g'] = pattern("2$b3o$o3bo$o3bo$o3bo$b4o$4bo$b3o!")
__mfont['h'] = pattern("o$o$ob2o$2o2bo$o3bo$o3bo$o3bo!")
__mfont['i'] = pattern("$2bo2$2bo$2bo$2bo$2b2o!")
__mfont['j'] = pattern("$3bo2$3bo$3bo$3bo$3bo$o2bo$b2o!")
__mfont['k'] = pattern("o$o$o2bo$obo$3o$o2bo$o3bo!")
__mfont['l'] = pattern("b2o$2bo$2bo$2bo$2bo$2bo$2b2o!")
__mfont['m'] = pattern("2$bobo$obobo$obobo$o3bo$o3bo!")
__mfont['n'] = pattern("2$4o$o3bo$o3bo$o3bo$o3bo!")
__mfont['o'] = pattern("2$b3o$o3bo$o3bo$o3bo$b3o!")
__mfont['p'] = pattern("2$4o$o3bo$o3bo$o3bo$4o$o$o!")
__mfont['q'] = pattern("2$b4o$o3bo$o3bo$o3bo$b4o$4bo$4bo!")
__mfont['r'] = pattern("2$ob2o$2o2bo$o$o$o!")
__mfont['s'] = pattern("2$b4o$o$b3o$4bo$4o!")
__mfont['t'] = pattern("$2bo$5o$2bo$2bo$2bo$3b2o!")
__mfont['u'] = pattern("2$o3bo$o3bo$o3bo$o3bo$b4o!")
__mfont['v'] = pattern("2$o3bo$o3bo$o3bo$bobo$2bo!")
__mfont['w'] = pattern("2$o3bo$o3bo$obobo$2ob2o$o3bo!")
__mfont['x'] = pattern("2$o3bo$bobo$2bo$bobo$o3bo!")
__mfont['y'] = pattern("2$o3bo$o3bo$o3bo$o3bo$b4o$4bo$b3o!")
__mfont['z'] = pattern("2$5o$3bo$2bo$bo$5o!")
__mfont['{'] = pattern("3bo$2bo$2bo$bo$2bo$2bo$3bo!")
__mfont['|'] = pattern("2bo$2bo$2bo$2bo$2bo$2bo$2bo!")
__mfont['}'] = pattern("bo$2bo$2bo$3bo$2bo$2bo$bo!")
__mfont['~'] = pattern("2$bo$obobo$3bo!")
for key in __mfont:
    __mfont[key].width = 6

# Snakial font (all chars are stable Life patterns)

__sfont = dict ()
__sfont['0'] = pattern ("2b2obo$2bob2o$2o4b2o$o5bo$bo5bo$2o4b2o$o5bo$bo5bo$2o4b2o$o5bo$bo5bo$2o4b2o$2b2obo$2bob2o!", 0, -14)
__sfont['0'].width = 10
__sfont['1'] = pattern ("2o$bo$o$2o2$2o$bo$o$2o2$2o$bo$o$2o!", 1, -14)
__sfont['1'].width = 6
__sfont['2'] = pattern ("2b2obo$2bob2o$6b2o$6bo$7bo$6b2o$2b2obo$2bob2o$2o$o$bo$2o$2b2obo$2bob2o!", 0, -14)
__sfont['2'].width = 10
__sfont['3'] = pattern ("2obo$ob2o$4b2o$4bo$5bo$4b2o$2obo$ob2o$4b2o$4bo$5bo$4b2o$2obo$ob2o!", 0, -14)
__sfont['3'].width = 8
__sfont['4'] = pattern ("2o3b2o$2o3b2o2$2o3b2o$obobobo$2bobo$b2obo$5b2o$6bo$5bo$5b2o$6bo$5bo$5b2o!", 0, -14)
__sfont['4'].width = 9
__sfont['5'] = pattern ("2b2obo$2bob2o$2o$o$bo$2o$2b2obo$2bob2o$6b2o$6bo$7bo$6b2o$2b2obo$2bob2o!", 0, -14)
__sfont['5'].width = 10
__sfont['6'] = pattern ("2b2obo$2bob2o$2o$o$bo$2o$2b2obo$2bob2o$2o4b2o$o5bo$bo5bo$2o4b2o$2b2obo$2bob2o!", 0, -14)
__sfont['6'].width = 10
__sfont['7'] = pattern ("ob2o$2obo$4b2o$5bo$4bo$4b2o$2b2o$3bo$2bo$2b2o$2o$bo$o$2o!", 0, -14)
__sfont['7'].width = 8
__sfont['8'] = pattern ("2b2obo$2bob2o$2o4b2o$o5bo$bo5bo$2o4b2o$2b2obo$2bob2o$2o4b2o$o5bo$bo5bo$2o4b2o$2b2obo$2bob2o!", 0, -14)
__sfont['8'].width = 10
__sfont['9'] = pattern ("2b2obo$2bob2o$2o4b2o$o5bo$bo5bo$2o4b2o$2b2obo$2bob2o$6b2o$6bo$7bo$6b2o$2b2obo$2bob2o!", 0, -14)
__sfont['9'].width = 10
__sfont['-'] = pattern ("2obo$ob2o!", 0, -8)
__sfont['-'].width = 6

def make_text (string, font='Snakial'):
    p = pattern ()
    x = 0

    if font[:2].lower() == "ea":
        f = __eafont
        unknown = '-'
    elif font.lower() == "mono":
        f = __mfont
        unknown = '?'
    else:
        f = __sfont
        unknown = '-'

    for c in string:
        if c not in f: c = unknown
        symbol = f[c]
        p += symbol (x, 0)
        x += symbol.width

    return p
