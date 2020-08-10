# Go to a requested generation.  The given generation can be:
#
#  * an absolute number like 1,000,000 (commas are optional)
#  * a number relative to the current generation like +9 or -6
#  * an expression like "17*(2^(3*143)+2^(2*3*27))"
#  * an expression starting with + or -, indicating a relative move
#  * any of the above preceded by "f" (fast). This wil set the base
#    step to 2 and provide less visual feedback of progress
#
# If the target generation is less than the current generation then
# we go back to the starting generation (normally 0) and advance to
# the target.
#
# If the input is preceded by F "fast" or q "quick" then we use the
# algorithm of goto-fast.py, which sets the base to 2 and jumps by
# powers of 2.
#
# Authors:
# Original goto.py by Andrew Trevorrow and Dave Greene, April 2006.
#   Updated Sept-Oct 2006 -- XRLE support and reusable default value.
#   Updated April 2010 -- much faster, thanks to PM 2Ring.
# goto-expression.py by Robert Munafo, using regexp expression
# evaluation like Hypercalc (mrob.com/pub/perl/hypercalc.html)
#  20111103 First stand-alone version of "expr.py" written as an
#           exercise to learn me some Python
#  20111105 Add expr_2 using re.sub (a cleaner implementation)
#  20120624 Remove whitespace before evaluating
#  20130213 Move expr_2 code to "goto-expression.py" for Golly.
#  20130214 Fix precedednce bugs, add expr_3
#  20130215 Remove redundant g.getstep and g.setstep calls. Full handling
#           of scientific notation and leading signs (like "300+-3")
#
# TODO:
#   Allow decimal point in EE notation, so "6.02e23" would work (right
#     now you have to do "602e21")
#   Make - and / associate left-to-right, so 100-10+1 gives 91 instead of 89
#   Remove use of deprecated string.find and string.replace functions
#   Why is it much faster when you start from gen 0? For example, start
#     with puffer-train.rle, use this script to goto 10^100 then goto 2*10^100
#     it's much faster if you start from gen 0 and go directly to 2*10^100
#   Given that gofast saves and restores the base, should we use it always?
#     Note that patterns with a lot of period-P oscillators run more
#     efficiently when the base is set to P or a factor of P, so this
#     is not a simple decision.

from glife import validint
from time import time
import golly as g
import re

# --------------------------------------------------------------------
# Regexp-based expression evaluator. The main loop contains match
# cases for each type of operation, and performs one operation per
# loop until there are none left to match. We use re.sub and pass
# functions for the repl parameter. See:
#   http://docs.python.org/2/library/re.html#re.sub
#   http://docs.python.org/2/library/re.html#re.MatchObject.group

p_whitespace = re.compile('[ \t]+')
p_paren = re.compile('\((\d+)\)')
p_parexp = re.compile('\((\d+[*/+-^][^()]*\d)\)')
p_exp = re.compile('(\d+)\^(\d+)')
p_mul = re.compile('(\d+)([*/])(n?\d+)')
p_add = re.compile('(\d+)([-+])(n?\d+)')

p_ee = re.compile('([.0-9]+)e([+-]?\d+)')
p_mantissa = re.compile('^n?\d*\.?\d+$')
p_leadn = re.compile('^n')
p_digits = re.compile('^[+-]?\d+$')
p_dot = re.compile('\.')

p_plusminus = re.compile('([-+])([-+])')

# erf = expression replace function
def erf_paren(match):
  "Remove parentheses: (123) -> 123"
  a = match.group(1)
  return a

# ee_approx - Python-like integer approximation of a number in scientific
#             notation. The mantissa and exponent are given separately and
#             may be either numeric or string. (the caller is expected to
#             generate them separately, e.g. by using a regexp to match
#             parts of a string). The exponent must be an integer or an
#             equivalent string; more flexibility is available for the
#             mantissa (see examples)
# All of the following examples evaluate to True:
#   ee_approx(2.0, 20) == 2*10**20
#   ee_approx('2.', 20) == 2*10**20
#   ee_approx('12000', -3) == 12
#   ee_approx(12e+03, '-3') == 12
# The following return 0 because of an error:
#   ee_approx(2.3, 4.0)      # Exponent may not be a float
#   ee_approx(6.02, '23.')   # Exponent may not contain '.'
# The following evaluate to False because of roundoff error:
#   ee_approx('.02', 22) == 2*10**20
def ee_approx(m, e):
  "Integer approximation of m*10^e given m and e"
  m = str(m)
  # Make sure mantissa matches its pattern
  if p_dot.search(m):
    m = m + '0'
  else:
    m = m + '.0'
  if not p_mantissa.search(m):
    return 0
  m = float(m)
  m = int(m * float(2**64) * (1.0+2.0**-52.0))
  e = str(e)
  if not p_digits.search(e):
    return 0
  e = int(e)
  if e<0:
    e = 10**(-e)
    return m//(e*2**64)
  else:
    e = 10**e
    return (m*e)//(2**64)

def erf_ee(match):
  "Scientific notation: 1.2e5 -> 120000"
  a = match.group(1)
  b = match.group(2)
  return str(ee_approx(a,b))

def erf_exp(match):
  "Exponentiation: 2^24 -> 16777216"
  a = int(match.group(1))
  b = int(match.group(2))
  return str(a**b)

def erf_mul(match):
  "Multiplication and (integer) division"
  a = int(match.group(1))
  # match.group(2) is operator
  b = match.group(3)
  # Check for leading 'n'
  if p_leadn.search(b):
    b = p_leadn.sub('-', b)
  b = int(b)
  if(match.group(2) == '*'):
    return str(a*b)
  else:
    return str(a//b)

def erf_add(match):
  "Addition and subtraction"
  a = int(match.group(1))
  # match.group(2) is operator
  b = match.group(3)
  # Check for leading 'n'
  if p_leadn.search(b):
    b = p_leadn.sub('-', b)
  b = int(b)
  if(match.group(2) == '+'):
    return str(a+b)
  else:
    return str(a-b)

""" Evaluate an expression without parentheses, like 2+3*4^5 = 3074
    The precedence is:
      If we match something like "6.02e23", expand it
      Else if we match something like "4^5", expand it
      Else if we match something like "3*17", expand it
      Else if we match something like "2+456", expand it
      Else return
    It loops in all cases but the last
"""
def expr_2(p):
  going = 1
  # print 'e2:', p
  while going:
    if p_ee.search(p):
      p = p_ee.sub(erf_ee, p, count=1)
      # print 'e2 ee, got:', p
    elif p_exp.search(p):
      p = p_exp.sub(erf_exp, p, count=1)
      # print 'e2 exp, got:', p
    elif p_mul.search(p):
      p = p_mul.sub(erf_mul, p, count=1)
      # print 'e2 mul, got:', p
    elif p_add.search(p):
      p = p_add.sub(erf_add, p, count=1)
      # print 'e2 add, got:', p
    else:
      # print 'e2 done'
      going = 0
  # print 'e2 return:', p
  return p

def erf_e2(match):
  "Parenthesized bare expression"
  a = expr_2(match.group(1))
  return str(a)

def erf_plusminus(match):
  "--, -+, +-, or ++"
  if match.group(2) == '-':
    return match.group(1)+'n'
  return match.group(1)

"""
    Evaluate an expression possibly including parenthesized sub-expressions
    and numbers in scientific notation,
    like 17*4^((7^2-3)*6^2+2e6)+602e21
    The precedence is:
      If we match something like "6.02e23", expand it
      Else if we match something like "(3+4*5)", expand it using expr_2
      Else if we match something like "(23456)", remove the parens
      Else expand the whole string using expr_2 and return
    It loops in all cases but the last
"""
def expr_3(p):
  p = p_whitespace.sub('', p)
  if p_plusminus.search(p):
    p = p_plusminus.sub(erf_plusminus, p)
  going = 1
  while going:
    if p_ee.search(p):
      p = p_ee.sub(erf_ee, p, count=1)
    elif p_parexp.search(p):
      p = p_parexp.sub(erf_e2, p, count=1)
    elif p_paren.search(p):
      p = p_paren.sub(erf_paren, p, count=1)
    else:
      p = expr_2(p)
      going = 0
  return p

# --------------------------------------------------------------------

"""
    gt_setup computes how many generations to move forwards. If we need to
    move backwards, it rewinds as far as possible, then returns the number
    of generations we need to move forwards.
"""
def gt_setup(gen):
   currgen = int(g.getgen())
   # Remove leading '+' or '-' if any, and convert rest to int or long
   if gen[0] == '+':
      n = int(gen[1:])
      newgen = currgen + n
   elif gen[0] == '-':
      n = int(gen[1:])
      if currgen > n:
         newgen = currgen - n
      else:
         newgen = 0
   else:
      newgen = int(gen)
   
   if newgen < currgen:
      # try to go back to starting gen (not necessarily 0) and
      # then forwards to newgen; note that reset() also restores
      # algorithm and/or rule, so too bad if user changed those
      # after the starting info was saved;
      # first save current location and scale
      midx, midy = g.getpos()
      mag = g.getmag()
      g.reset()
      # restore location and scale
      g.setpos(midx, midy)
      g.setmag(mag)
      # current gen might be > 0 if user loaded a pattern file
      # that set the gen count
      currgen = int(g.getgen())
      if newgen < currgen:
         g.error("Can't go back any further; pattern was saved " +
                 "at generation " + str(currgen) + ".")
         return 0
      return newgen - currgen
   elif newgen > currgen:
      return newgen - currgen
   else:
     return 0

# --------------------------------------------------------------------

def intbase(n, b):
   # convert integer n >= 0 to a base b digit list (thanks to PM 2Ring)
   digits = []
   while n > 0:
      digits += [n % b]
      n //= b
   return digits or [0]

def goto(newgen, delay):
   g.show("goto running, hit escape to abort...")
   oldsecs = time()
   
   # before stepping we advance by 1 generation, for two reasons:
   # 1. if we're at the starting gen then the *current* step size
   #    will be saved (and restored upon Reset/Undo) rather than a
   #    possibly very large step size
   # 2. it increases the chances the user will see updates and so
   #    get some idea of how long the script will take to finish
   #    (otherwise if the base is 10 and a gen like 1,000,000,000
   #    is given then only a single step() of 10^9 would be done)
   if delay <= 1.0:
     g.run(1)
     newgen -= 1
   
   # use fast stepping (thanks to PM 2Ring)
   for i, d in enumerate(intbase(newgen, g.getbase())):
      if d > 0:
         g.setstep(i)
         for j in range(d):
            if g.empty():
               g.show("Pattern is empty.")
               return
            g.step()
            newsecs = time()
            if newsecs - oldsecs >= delay:  # time to do an update?
               oldsecs = newsecs
               g.update()
   g.show("")

# --------------------------------------------------------------------
# This is the "fast goto" algorithm from goto-fast.py that uses
# binary stepsizes and does not do that initial step by 1 generation.

def intbits(n):
   ''' Convert integer n to a bit list '''
   bits = []
   while n:
      bits += [n & 1]
      n >>= 1
   return bits

def gofast(newgen, delay):
   ''' Fast goto '''
   #Save current settings
   oldbase = g.getbase()
   # oldhash = g.setoption("hashing", True)
   
   g.show('gofast running, hit escape to abort')
   oldsecs = time()
   
   #Advance by binary powers, to maximize advantage of hashing
   g.setbase(2)
   for i, b in enumerate(intbits(newgen)):
      if b:
         g.setstep(i)
         g.step()
         g.dokey(g.getkey())
         newsecs = time()
         if newsecs - oldsecs >= delay:  # do an update every sec
            oldsecs = newsecs
            g.update()
         if   g.empty():
            break
   
   g.show('')
   
   #Restore settings
   # g.setoption("hashing", oldhash)
   g.setbase(oldbase)

# --------------------------------------------------------------------

def savegen(filename, gen):
   try:
      f = open(filename, 'w')
      f.write(gen)
      f.close()
   except:
      g.warn("Unable to save given gen in file:\n" + filename)

# --------------------------------------------------------------------

# use same file name as in goto.lua
GotoINIFileName = g.getdir("data") + "goto.ini"
previousgen = ""
try:
   f = open(GotoINIFileName, 'r')
   previousgen = f.readline()
   f.close()
   if not validint(previousgen):
      previousgen = ""
except:
   # should only happen 1st time (GotoINIFileName doesn't exist)
   pass

gen = g.getstring("Enter the desired generation number as an\n" +
                  "expression, prepend -/+ for relative\n" +
                  "move back/forwards, prepend f to use faster\n"
                  "powers-of-2 steps:",
                  previousgen, "Go to generation")
# Evaluate the expression. This leaves leading "f" and/or "+/-"
# intact.
gen = expr_3(gen)

# Find out if they want to get there quickly
# %%% TODO: Use re instead of string.find and string.replace (which are
#           deprecated in later versions of Python)
fastmode = 0
if(gen.find("f") == 0):
  gen = gen.replace("f","")
  fastmode = 1

if len(gen) == 0:
   g.exit()
elif gen == "+" or gen == "-":
   # clear the default
   savegen(GotoINIFileName, "")
elif not validint(gen):
   g.exit('Sorry, but "' + gen + '" is not a valid integer.')
else:
   # best to save given gen now in case user aborts script
   savegen(GotoINIFileName, gen)
   oldstep = g.getstep()
   # %%% TODO: Use re instead of string.replace to remove the commas
   newgen = gt_setup(gen.replace(",",""))
   if newgen > 0:
     if fastmode:
       gofast(newgen, 10.0)
     else:
       goto(newgen, 1.0)
   g.setstep(oldstep)
