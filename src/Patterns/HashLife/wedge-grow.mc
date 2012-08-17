[M2] (golly 2.0)
#C 26-cell quadratic growth pattern: a forward-glider-producing switch
#C engine repeatedly overtakes a crystal formed by collision with
#C sideways gliders produced by a c/12 rake assembly.  When the
#C switch engine reaches the crystal, a reaction produces an
#C orthogonal block-laying switch engine and restarts the crystal
#C production at the c/12 rake boundary.
#C Nick Gotts, 17 March 2006 -- closely based on Bill Gosper's
#C 41-cell O(t ln t)-growth pattern from 11 March.
$$$$$$$.**$
4 0 0 1 0
5 0 0 2 0
6 0 0 3 0
7 0 0 4 0
8 0 0 5 0
9 0 0 6 0
10 0 0 7 0
11 0 0 8 0
12 0 0 9 0
13 0 0 10 0
14 0 0 11 0
**$.*$
4 13 0 0 0
$$$$$$$.......*$
4 0 0 0 15
$$$$$$*$.*$
4 0 0 17 0
5 14 0 16 18
.......*$......*$
4 0 20 0 0
5 21 0 0 0
6 19 0 22 0
7 23 0 0 0
8 24 0 0 0
9 25 0 0 0
10 26 0 0 0
11 27 0 0 0
12 28 0 0 0
13 29 0 0 0
$...*...*$....*.*$.....*$
4 0 31 0 0
5 0 0 0 32
$$.......*$$$$.......*$
4 0 0 0 34
5 0 35 0 0
$$$*$.*$*$$.***$
4 0 0 37 0
5 38 0 0 0
6 0 33 36 39
$$$*$*$*$
4 41 0 0 0
5 0 0 42 0
6 43 0 0 0
7 0 0 40 44
8 0 0 45 0
9 0 46 0 0
10 0 0 0 47
11 0 48 0 0
12 0 0 0 49
13 0 0 0 50
14 30 0 0 51
15 0 12 0 52
