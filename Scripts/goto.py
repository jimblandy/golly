# Go to a requested generation.  The given generation can be an
# absolute number like 1,000,000 or a number relative to the
# current generation like +99 or -66.  If the target generation
# is less than the current generation then we go back to the
# starting generation (0) and advance to the target.
# Authors: Andrew Trevorrow and Dave Greene, April 2006.

import golly as g
from time import clock

# ----------------------------------------------------------------

def ask(prompt):
   # prompt user and return entered string
   cursor1 = "_"
   cursor2 = ""

   g.show(prompt + " " + cursor1)
   inp = ""
   oldsecs = clock()
   
   while True:
      newsecs = clock()
      if newsecs - oldsecs >= 0.5:   # blink cursor each sec
         oldsecs = newsecs
         cursor1, cursor2 = cursor2, cursor1
         g.show(prompt + " " + inp + cursor1)

      ch = g.getkey()
      if len(ch) > 0:
         if ord(ch) == 13:    # return
         	g.show("")
         	return inp
         if ord(ch) == 8:     # backspace
            inp = inp[:-1]
            ch = ""
         inp += ch
         g.show(prompt + " " + inp + cursor1)

# ----------------------------------------------------------------

def validint(istring):
   # return True if given string is a valid integer
   n = istring.replace(",","")
   if n[0] == '+' or n[0] == '-': n = n[1:]
   return n.isdigit()

# ----------------------------------------------------------------

def stripzeros(istring):
   # remove any leading zeros
   result = istring.lstrip('0')
   if len(result) == 0: result = '0'
   return result

# ----------------------------------------------------------------

def add(a, b):
   # return a + b using symbolic arithmetic (assume a,b >= 0)
   result = ""
   carry = 0
   ap = len(a) - 1
   bp = len(b) - 1
   
   while ap >= 0 or bp >= 0:
      if ap >= 0:
         ad = int(a[ap])
         ap -= 1
      else:
         ad = 0
      if bp >= 0:
         bd = int(b[bp])
         bp -= 1
      else:
         bd = 0
      sum = ad + bd + carry
      if sum >= 10:
         sum = sum - 10
         carry = 1
      else:
         carry = 0
      result = str(sum) + result
   
   if carry == 1:
      result = '1' + result
   else:
      result = stripzeros(result)
   
   return result

# ----------------------------------------------------------------

def sub(a, b):
   # return a - b using symbolic arithmetic (assume a >= b and a,b >= 0)
   result = ""
   carry = 0
   ap = len(a) - 1
   bp = len(b) - 1
   
   while ap >= 0:
      ad = int(a[ap])
      ap -= 1
      if bp >= 0:
         bd = int(b[bp])
         bp -= 1
      else:
         bd = 0
      if (bd + carry) > ad:
         result = str(ad + 10 - (bd + carry)) + result
         carry = 1
      else:
         result = str(ad - (bd + carry)) + result
         carry = 0
   
   return stripzeros(result)

# ----------------------------------------------------------------

def greater_or_equal(a, b):
   # return True if a >= b (assume a,b >= 0)
   if len(a) > len(b): return True
   if len(a) < len(b): return False
   # same length so compare digits from left to right
   for i in range( len(a) ):
      if a[i] > b[i]: return True
      if a[i] < b[i]: return False
   # a == b
   return True

# ----------------------------------------------------------------

def goto(gen):
   currgen = g.getgen().replace(",","")
   if gen[0] == '+':
      n = stripzeros( gen[1:].replace(",","") )
      newgen = add(currgen, n)
   elif gen[0] == '-':
      n = stripzeros( gen[1:].replace(",","") )
      if greater_or_equal(currgen, n):
         newgen = sub(currgen, n)
      else:
         newgen = "0"
   else:
      newgen = stripzeros( gen.replace(",","") )
   
   if not greater_or_equal(newgen, currgen):
      # go back to gen 0 and then forwards to newgen; note that reset()
      # also restores rule and hashing option, so too bad if user
      # changed those options after gen 0 info was saved;
      # first save current location and scale
      midx, midy = g.getpos()
      mag = g.getmag()
      g.reset()
      # restore location and scale
      g.setpos(midx, midy)
      g.setmag(mag)
   
   g.show("Hit escape to abort...")
   oldsecs = clock()
   while not (newgen == g.getgen().replace(",","")):
      if g.empty():
         g.show("Pattern is empty.")
         return
      # using g.step() as a faster method for getting to newgen
      # is left as an exercise for the reader :)
      g.run(1)
      newsecs = clock()
      if newsecs - oldsecs >= 1.0:     # do an update every sec
         oldsecs = newsecs
         g.update()
   g.show("")

# ----------------------------------------------------------------

gen = ask("Go to this generation:")
if len(gen) == 0:
   g.show("")
elif not validint(gen):
   g.error('Sorry, but "' + gen + '" is not a valid integer.')
else:
   goto(gen)
