from glife import *
from string import *

# each symbol is a pattern with an additional field .width

__binaryfont = dict ()
__binaryfont['0'] = pattern ("3o$obo$obo$obo$3o!", 0, -5)
__binaryfont['0'].width = 4
__binaryfont['0'] = pattern ("bo$2o$bo$bo$3o!", 0, -5)
__binaryfont['0'].width = 4

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

# Snakial font

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

__sfont['-'] = pattern ("""
**.*
*.**
""", 0, -8)
__sfont['-'].width = 6

def make_text (string, font='Snakial'):
   p = pattern ()
   x = 0
   for c in string:
      if lower(font[:2])=="ea":
         if not __eafont.has_key (c):
            c = '-'
         symbol = __eafont[c]      
      else:
         if not __sfont.has_key (c):
            c = '-'
         symbol = __sfont[c]
      p += symbol (x, 0)
      x += symbol.width
   return p
