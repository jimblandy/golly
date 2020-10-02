# Initialization script executed by using "import glife".
# Based on python/life/__init__.py in Eugene Langvagen's PLife.

import golly
import sys
import time

__doc__ = """High-level scripting aids for Golly.""";

# --------------------------------------------------------------------

class rect(list):
    """A simple class to make it easier to manipulate rectangles."""

    def visible(self):
        """Return true if rect is completely visible in viewport."""
        return golly.visrect( [self.x, self.y, self.wd, self.ht] )

    def __init__(self, R = []):
        if len(R) == 0:
            self.empty = True
        elif len(R) == 4:
            self.empty = False
            self.x  = self.left   = R[0]
            self.y  = self.top    = R[1]
            self.wd = self.width  = R[2]
            self.ht = self.height = R[3]
            if self.wd <= 0: raise ValueError("rect width must be > 0")
            if self.ht <= 0: raise ValueError("rect height must be > 0")
            self.right  = self.left + self.wd - 1
            self.bottom = self.top  + self.ht - 1
        else:
            raise TypeError("rect arg must be [] or [x,y,wd,ht]")
        list.__init__(self, R)

# --------------------------------------------------------------------

# Define some useful synonyms:

# for golly.clear and golly.advance
inside = 0
outside = 1

# for golly.flip
left_right = 0
top_bottom = 1
up_down = 1

# for golly.rotate
clockwise = 0
anticlockwise = 1

# for golly.setcursor (must match strings in Cursor Mode submenu)
draw =    "Draw"
pick =    "Pick"
select =  "Select"
move =    "Move"
zoomin =  "Zoom In"
zoomout = "Zoom Out"

# --------------------------------------------------------------------

# Define some transformation matrices:

identity = ( 1,  0,  0,  1)

flip     = (-1,  0,  0, -1)
flip_x   = (-1,  0,  0,  1)
flip_y   = ( 1,  0,  0, -1)

swap_xy      = ( 0,  1,  1,  0)
swap_xy_flip = ( 0, -1, -1,  0)

# Rotation:

rcw  = ( 0, -1,  1,  0)
rccw = ( 0,  1, -1,  0)

# --------------------------------------------------------------------

def rule(s = "B3/S23"):
    """\
 Set the rule for the Game of Life.
 Although it affects subsequent calls to pattern.evolve(),
 only the last call to this function matters for the viewer."""
    golly.setrule(s)

# --------------------------------------------------------------------

def description(s):
    """Supply a textual description to the whole pattern."""
    for line in s.split("\n"):
        print(("#D", line))

# --------------------------------------------------------------------

def compose(S, T):
    """\
 Return the composition of two transformations S and T.
 A transformation is a tuple of the form (x, y, A), which denotes
 multiplying by matrix A and then translating by vector (x, y).
 These tuples can be passed to pattern.__call__()."""
    x = S[0]; y = S[1]; A = S[2]
    s = T[0]; t = T[1]; B = T[2]
    return (x * B[0] + y * B[1] + s, x * B[2] + y * B[3] + t,
       (A[0] * B[0] + A[2] * B[1], A[1] * B[0] + A[3] * B[1],
        A[0] * B[2] + A[2] * B[3], A[1] * B[2] + A[3] * B[3]))

# --------------------------------------------------------------------

class pattern(list):
    """This class represents a cell list."""

    def __add__(self, q):
        """Join patterns."""
        return pattern(golly.join(self, q))

    def __getitem__(self, N):
        """\
  The __getitem__() function is an alias to evolve().
  It allows to access the pattern's phases as elements of an array."""
        return self.evolve(N)

    def __call__(self, x, y, A = identity):
        """The same as 'apply(A).translate(x, y)'."""
        return pattern(golly.transform(self, x, y, *A))

    def translate(self, x, y):
        """Translate the pattern."""
        return self(x, y)

    def apply(self, A):
        """\
  Apply a matrix transformation to the pattern.
  Predefined matrices are:
  identity, flip, flip_x, flip_y, swap_xy, swap_xy_flip,
  rcw (rotate clockwise) and rccw (rotate counter-clockwise)."""
        return self(0, 0, A)

    def put(self, x = 0, y = 0, A = identity):
        """Paste pattern into current universe."""
        golly.putcells(self, x, y, *A)

    def display(self, title = "untitled", x = 0, y = 0, A = identity):
        """Paste pattern into new universe and display it all."""
        golly.new(title)
        golly.putcells(self, x, y, *A)
        golly.fit()
        golly.setcursor(zoomin)

    def save(self, fn, desc = None):
        """\
  Save the pattern to file 'fn' in RLE format.
  An optional description 'desc' may be given."""
        golly.store(self, fn, desc)

    def evolve(self, N):
        """\
  Return N-th generation of the pattern.
  Once computed, the N-th generation is remembered and quickly accessible.
  It is also the base for computing generations subsequent to N-th."""
        if N < 0:
            raise ValueError("backward evolving requested")
        if N in self.__phases:
            return self.__phases[N]
        M = 0
        for k in list(self.__phases.keys()):
            if M < k < N: M = k
        p = self.__phases[N] = pattern(golly.evolve(self.__phases[M], N - M))
        return p

    def __init__(self, P = [], x0 = 0, y0 = 0, A = identity):
        """\
  Initialize a pattern from argument P.
  P may be another pattern, a cell list, or a multi-line string.
  A cell list should look like [x1, y1, x2, y2, ...];
  a string may be in one of the two autodetected formats:
  'visual' or 'RLE'.
  o  'visual' format means that the pattern is represented
     in a visual way using symbols '*' (on cell), '.' (off cell)
     and '\\n' (newline), just like in Life 1.05 format.
     (Note that an empty line should contain at least one dot).
  o  'RLE' format means that a string is Run-Length Encoded.
     The format uses 'o' for on-cells, 'b' for off-cells and
     '$' for newlines.
     Moreover, any of these symbols may be prefixed by a number,
     to denote that symbol repeated that number of times.

  When P is a string, an optional transformation
  (x0, y0, A) may be specified.
  """
        self.__phases = dict()

        if type(P) == list:
            list.__init__(self, P)
        elif type(P) == pattern:
            list.__init__(self, list(P))
        elif type(P) == str:
            list.__init__(self, golly.parse(P, x0, y0, *A))
        else:
            raise TypeError("list or string is required here")
        self.__phases[0] = self

# --------------------------------------------------------------------

def load(fn):
    # note that top left cell of bounding box will be at 0,0
    return pattern(golly.load(fn))

# --------------------------------------------------------------------

def getminbox(patt):
    # return a rect which is the minimal bounding box of given pattern;
    # note that if the pattern is a multi-state list then any dead cells
    # are included in the bounding box
    minx =  sys.maxsize
    maxx = -sys.maxsize
    miny =  sys.maxsize
    maxy = -sys.maxsize
    clist = list(patt)
    clen = len(clist)
    inc = 2
    if clen & 1 == 1:
        # multi-state list (3 ints per cell)
        inc = 3
        # ignore padding int if it is present
        if clen % 3 == 1: clen -= 1

    for x in range(0, clen, inc):
        if clist[x] < minx: minx = clist[x]
        if clist[x] > maxx: maxx = clist[x]

    for y in range(1, clen, inc):
        if clist[y] < miny: miny = clist[y]
        if clist[y] > maxy: maxy = clist[y]

    return rect( [ minx, miny, maxx - minx + 1, maxy - miny + 1 ] )

# --------------------------------------------------------------------

def validint(s):
    # return True if given string represents a valid integer
    if len(s) == 0: return False
    s = s.replace(",","")
    if s[0] == '+' or s[0] == '-': s = s[1:]
    return s.isdigit()

# --------------------------------------------------------------------

def getposint():
    # return current viewport position as integer coords
    x, y = golly.getpos()
    return int(x), int(y)

# --------------------------------------------------------------------

def setposint(x,y):
    # convert integer coords to strings and set viewport position
    golly.setpos(str(x), str(y))

# --------------------------------------------------------------------

def getstring(prompt):
    # this routine is deprecated
    golly.warn("Change the script to use the getstring() command\n"+
               "from golly rather than from glife.")
    golly.exit("")
