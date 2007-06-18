# Run the current pattern for a given number of steps (using current
# step size) and create a plot of population vs time in separate layer.
# Author: Andrew Trevorrow (andrew@trevorrow.com), June 2007.

use strict;
use Time::HiRes qw (time);
use List::Util qw (min max);

# ------------------------------------------------------------------------------

# size of plot
my $xlen = 500;        # length of x axis
my $ylen = 500;        # length of y axis

# ------------------------------------------------------------------------------

# create a mono-spaced ASCII font
my %mfont = ();
$mfont{' '} = g_parse('',0,0,1,0,0,1);
$mfont{'!'} = g_parse('2bo$2bo$2bo$2bo$2bo2$2bo!',0,0,1,0,0,1);
$mfont{'"'} = g_parse('bobo$bobo$bobo!',0,0,1,0,0,1);
$mfont{'#'} = g_parse('bobo$bobo$5o$bobo$5o$bobo$bobo!',0,0,1,0,0,1);
$mfont{'$'} = g_parse('b3o$obobo$obo$b3o$2bobo$obobo$b3o!',0,0,1,0,0,1);
$mfont{'%'} = g_parse('2o2bo$2o2bo$3bo$2bo$bo$o2b2o$o2b2o!',0,0,1,0,0,1);
$mfont{'&'} = g_parse('b2o$o2bo$o2bo$b2o$o2bo$o2bo$b2obo!',0,0,1,0,0,1);
$mfont{'\''} = g_parse('2bo$2bo$2bo!',0,0,1,0,0,1);
$mfont{'('} = g_parse('3bo$2bo$2bo$2bo$2bo$2bo$3bo!',0,0,1,0,0,1);
$mfont{')'} = g_parse('bo$2bo$2bo$2bo$2bo$2bo$bo!',0,0,1,0,0,1);
$mfont{'*'} = g_parse('$obobo$b3o$5o$b3o$obobo!',0,0,1,0,0,1);
$mfont{'+'} = g_parse('$2bo$2bo$5o$2bo$2bo!',0,0,1,0,0,1);
$mfont{','} = g_parse('6$2bo$2bo$bo!',0,0,1,0,0,1);
$mfont{'-'} = g_parse('3$5o!',0,0,1,0,0,1);
$mfont{'.'} = g_parse('6$2bo!',0,0,1,0,0,1);
$mfont{'/'} = g_parse('3bo$3bo$2bo$2bo$2bo$bo$bo!',0,0,1,0,0,1);
$mfont{'0'} = g_parse('b3o$o3bo$o2b2o$obobo$2o2bo$o3bo$b3o!',0,0,1,0,0,1);
$mfont{'1'} = g_parse('2bo$b2o$2bo$2bo$2bo$2bo$b3o!',0,0,1,0,0,1);
$mfont{'2'} = g_parse('b3o$o3bo$4bo$3bo$2bo$bo$5o!',0,0,1,0,0,1);
$mfont{'3'} = g_parse('b3o$o3bo$4bo$2b2o$4bo$o3bo$b3o!',0,0,1,0,0,1);
$mfont{'4'} = g_parse('3bo$2b2o$bobo$o2bo$5o$3bo$3bo!',0,0,1,0,0,1);
$mfont{'5'} = g_parse('5o$o$o$b3o$4bo$o3bo$b3o!',0,0,1,0,0,1);
$mfont{'6'} = g_parse('b3o$o$o$4o$o3bo$o3bo$b3o!',0,0,1,0,0,1);
$mfont{'7'} = g_parse('5o$4bo$3bo$2bo$bo$o$o!',0,0,1,0,0,1);
$mfont{'8'} = g_parse('b3o$o3bo$o3bo$b3o$o3bo$o3bo$b3o!',0,0,1,0,0,1);
$mfont{'9'} = g_parse('b3o$o3bo$o3bo$b4o$4bo$4bo$b3o!',0,0,1,0,0,1);
$mfont{':'} = g_parse('2$2bo4$2bo!',0,0,1,0,0,1);
$mfont{';'} = g_parse('2$2bo4$2bo$2bo$bo!',0,0,1,0,0,1);
$mfont{'<'} = g_parse('$3bo$2bo$bo$2bo$3bo!',0,0,1,0,0,1);
$mfont{'='} = g_parse('2$5o2$5o!',0,0,1,0,0,1);
$mfont{'>'} = g_parse('$bo$2bo$3bo$2bo$bo!',0,0,1,0,0,1);
$mfont{'?'} = g_parse('b3o$o3bo$4bo$2b2o$2bo2$2bo!',0,0,1,0,0,1);
$mfont{'@'} = g_parse('b3o$o3bo$ob3o$obobo$ob2o$o$b3o!',0,0,1,0,0,1);
$mfont{'A'} = g_parse('b3o$o3bo$o3bo$5o$o3bo$o3bo$o3bo!',0,0,1,0,0,1);
$mfont{'B'} = g_parse('4o$o3bo$o3bo$4o$o3bo$o3bo$4o!',0,0,1,0,0,1);
$mfont{'C'} = g_parse('b3o$o3bo$o$o$o$o3bo$b3o!',0,0,1,0,0,1);
$mfont{'D'} = g_parse('4o$o3bo$o3bo$o3bo$o3bo$o3bo$4o!',0,0,1,0,0,1);
$mfont{'E'} = g_parse('5o$o$o$3o$o$o$5o!',0,0,1,0,0,1);
$mfont{'F'} = g_parse('5o$o$o$3o$o$o$o!',0,0,1,0,0,1);
$mfont{'G'} = g_parse('b3o$o3bo$o$o2b2o$o3bo$o3bo$b3o!',0,0,1,0,0,1);
$mfont{'H'} = g_parse('o3bo$o3bo$o3bo$5o$o3bo$o3bo$o3bo!',0,0,1,0,0,1);
$mfont{'I'} = g_parse('b3o$2bo$2bo$2bo$2bo$2bo$b3o!',0,0,1,0,0,1);
$mfont{'J'} = g_parse('2b3o$3bo$3bo$3bo$3bo$o2bo$b2o!',0,0,1,0,0,1);
$mfont{'K'} = g_parse('o3bo$o2bo$obo$2o$obo$o2bo$o3bo!',0,0,1,0,0,1);
$mfont{'L'} = g_parse('o$o$o$o$o$o$5o!',0,0,1,0,0,1);
$mfont{'M'} = g_parse('o3bo$2ob2o$obobo$obobo$o3bo$o3bo$o3bo!',0,0,1,0,0,1);
$mfont{'N'} = g_parse('o3bo$2o2bo$obobo$o2b2o$o3bo$o3bo$o3bo!',0,0,1,0,0,1);
$mfont{'O'} = g_parse('b3o$o3bo$o3bo$o3bo$o3bo$o3bo$b3o!',0,0,1,0,0,1);
$mfont{'P'} = g_parse('4o$o3bo$o3bo$4o$o$o$o!',0,0,1,0,0,1);
$mfont{'Q'} = g_parse('b3o$o3bo$o3bo$o3bo$obobo$o2bo$b2obo!',0,0,1,0,0,1);
$mfont{'R'} = g_parse('4o$o3bo$o3bo$4o$o2bo$o3bo$o3bo!',0,0,1,0,0,1);
$mfont{'S'} = g_parse('b3o$o3bo$o$b3o$4bo$o3bo$b3o!',0,0,1,0,0,1);
$mfont{'T'} = g_parse('5o$2bo$2bo$2bo$2bo$2bo$2bo!',0,0,1,0,0,1);
$mfont{'U'} = g_parse('o3bo$o3bo$o3bo$o3bo$o3bo$o3bo$b3o!',0,0,1,0,0,1);
$mfont{'V'} = g_parse('o3bo$o3bo$o3bo$o3bo$o3bo$bobo$2bo!',0,0,1,0,0,1);
$mfont{'W'} = g_parse('o3bo$o3bo$o3bo$obobo$obobo$2ob2o$o3bo!',0,0,1,0,0,1);
$mfont{'X'} = g_parse('o3bo$o3bo$bobo$2bo$bobo$o3bo$o3bo!',0,0,1,0,0,1);
$mfont{'Y'} = g_parse('o3bo$o3bo$bobo$2bo$2bo$2bo$2bo!',0,0,1,0,0,1);
$mfont{'Z'} = g_parse('5o$4bo$3bo$2bo$bo$o$5o!',0,0,1,0,0,1);
$mfont{'['} = g_parse('2b2o$2bo$2bo$2bo$2bo$2bo$2b2o!',0,0,1,0,0,1);
$mfont{'\\'} = g_parse('bo$bo$2bo$2bo$2bo$3bo$3bo!',0,0,1,0,0,1);
$mfont{']'} = g_parse('b2o$2bo$2bo$2bo$2bo$2bo$b2o!',0,0,1,0,0,1);
$mfont{'^'} = g_parse('2bo$bobo$o3bo!',0,0,1,0,0,1);
$mfont{'_'} = g_parse('6$5o!',0,0,1,0,0,1);
$mfont{'`'} = g_parse('o$bo!',0,0,1,0,0,1);
$mfont{'a'} = g_parse('2$b4o$o3bo$o3bo$o3bo$b4o!',0,0,1,0,0,1);
$mfont{'b'} = g_parse('o$o$4o$o3bo$o3bo$o3bo$4o!',0,0,1,0,0,1);
$mfont{'c'} = g_parse('2$b4o$o$o$o$b4o!',0,0,1,0,0,1);
$mfont{'d'} = g_parse('4bo$4bo$b4o$o3bo$o3bo$o3bo$b4o!',0,0,1,0,0,1);
$mfont{'e'} = g_parse('2$b3o$o3bo$5o$o$b4o!',0,0,1,0,0,1);
$mfont{'f'} = g_parse('2b2o$bo2bo$bo$3o$bo$bo$bo!',0,0,1,0,0,1);
$mfont{'g'} = g_parse('2$b3o$o3bo$o3bo$o3bo$b4o$4bo$b3o!',0,0,1,0,0,1);
$mfont{'h'} = g_parse('o$o$ob2o$2o2bo$o3bo$o3bo$o3bo!',0,0,1,0,0,1);
$mfont{'i'} = g_parse('$2bo2$2bo$2bo$2bo$2b2o!',0,0,1,0,0,1);
$mfont{'j'} = g_parse('$3bo2$3bo$3bo$3bo$3bo$o2bo$b2o!',0,0,1,0,0,1);
$mfont{'k'} = g_parse('o$o$o2bo$obo$3o$o2bo$o3bo!',0,0,1,0,0,1);
$mfont{'l'} = g_parse('b2o$2bo$2bo$2bo$2bo$2bo$2b2o!',0,0,1,0,0,1);
$mfont{'m'} = g_parse('2$bobo$obobo$obobo$o3bo$o3bo!',0,0,1,0,0,1);
$mfont{'n'} = g_parse('2$4o$o3bo$o3bo$o3bo$o3bo!',0,0,1,0,0,1);
$mfont{'o'} = g_parse('2$b3o$o3bo$o3bo$o3bo$b3o!',0,0,1,0,0,1);
$mfont{'p'} = g_parse('2$4o$o3bo$o3bo$o3bo$4o$o$o!',0,0,1,0,0,1);
$mfont{'q'} = g_parse('2$b4o$o3bo$o3bo$o3bo$b4o$4bo$4bo!',0,0,1,0,0,1);
$mfont{'r'} = g_parse('2$ob2o$2o2bo$o$o$o!',0,0,1,0,0,1);
$mfont{'s'} = g_parse('2$b4o$o$b3o$4bo$4o!',0,0,1,0,0,1);
$mfont{'t'} = g_parse('$2bo$5o$2bo$2bo$2bo$3b2o!',0,0,1,0,0,1);
$mfont{'u'} = g_parse('2$o3bo$o3bo$o3bo$o3bo$b4o!',0,0,1,0,0,1);
$mfont{'v'} = g_parse('2$o3bo$o3bo$o3bo$bobo$2bo!',0,0,1,0,0,1);
$mfont{'w'} = g_parse('2$o3bo$o3bo$obobo$2ob2o$o3bo!',0,0,1,0,0,1);
$mfont{'x'} = g_parse('2$o3bo$bobo$2bo$bobo$o3bo!',0,0,1,0,0,1);
$mfont{'y'} = g_parse('2$o3bo$o3bo$o3bo$o3bo$b4o$4bo$b3o!',0,0,1,0,0,1);
$mfont{'z'} = g_parse('2$5o$3bo$2bo$bo$5o!',0,0,1,0,0,1);
$mfont{'{'} = g_parse('3bo$2bo$2bo$bo$2bo$2bo$3bo!',0,0,1,0,0,1);
$mfont{'|'} = g_parse('2bo$2bo$2bo$2bo$2bo$2bo$2bo!',0,0,1,0,0,1);
$mfont{'}'} = g_parse('bo$2bo$2bo$3bo$2bo$2bo$bo!',0,0,1,0,0,1);
$mfont{'~'} = g_parse('2$bo$obobo$3bo!',0,0,1,0,0,1);

# ------------------------------------------------------------------------------

# convert given string to a cell array using above mono-spaced font
sub make_text {
   my $string = shift;
   my @p = ();
   my $x = 0;
   
   for my $ch (split(//,$string)) {
      $ch = '?' if not exists $mfont{$ch};
      my $symbol = g_transform($mfont{$ch}, $x, 0, 1, 0, 0, 1);
      push(@p, @{$symbol});
      $x += 6;
   }
   
   return \@p;
}

# ------------------------------------------------------------------------------

# draw a line of cells from x1,y1 to x2,y2 using Bresenham's algorithm
sub draw_line {
   my ($x1, $y1, $x2, $y2) = @_;
   g_setcell($x1, $y1, 1);
   return if $x1 == $x2 and $y1 == $y2;
   
   my $dx = $x2 - $x1;
   my $ax = abs($dx) * 2;
   my $sx = 1;
   $sx = -1 if $dx < 0;
   
   my $dy = $y2 - $y1;
   my $ay = abs($dy) * 2;
   my $sy = 1;
   $sy = -1 if $dy < 0;
   
   if ($ax > $ay) {
      my $d = $ay - ($ax / 2);
      while ($x1 != $x2) {
         g_setcell($x1, $y1, 1);
         if ($d >= 0) {
            $y1 += $sy;
            $d -= $ax;
         }
         $x1 += $sx;
         $d += $ay;
      }
   } else {
      my $d = $ax - ($ay / 2);
      while ($y1 != $y2) {
         g_setcell($x1, $y1, 1);
         if ($d >= 0) {
            $x1 += $sx;
            $d -= $ay;
         }
         $y1 += $sy;
         $d += $ax;
      }
   }
   g_setcell($x2, $y2, 1);
}

# ------------------------------------------------------------------------------

# fit pattern in viewport if not empty and not completely visible
sub fit_if_not_visible {
   my @r = g_getrect();
   if (@r > 0 and !g_visrect(@r)) { g_fit() }
}

# ------------------------------------------------------------------------------

sub getminbox {
   # return a rect which is the minimal bounding box of given pattern
   my $cells = shift;
   my $minx =  10000000000;    #!!!???  maxint;
   my $maxx = -10000000000;    #!!!??? -maxint;
   my $miny =  10000000000;    #!!!???  maxint;
   my $maxy = -10000000000;    #!!!??? -maxint;
   my $clen = @{$cells};
   for (my $x = 0; $x < $clen; $x += 2) {
      if ($cells->[$x] < $minx) { $minx = $cells->[$x] }
      if ($cells->[$x] > $maxx) { $maxx = $cells->[$x] }
   }
   for (my $y = 1; $y < $clen; $y += 2) {
      if ($cells->[$y] < $miny) { $miny = $cells->[$y] }
      if ($cells->[$y] > $maxy) { $maxy = $cells->[$y] }
   }
   return ($minx, $miny, $maxx - $minx + 1, $maxy - $miny + 1);
}

# ------------------------------------------------------------------------------

g_exit("There is no pattern.") if g_empty();

# check that a layer is available for population plot
my $layername = "population plot";
my $poplayer = -1;
for my $i (0..g_numlayers()-1) {
   if (g_getname($i) eq $layername) {
      $poplayer = $i;
      last;
   }
}
if ($poplayer == -1 and g_numlayers() == g_maxlayers()) {
   g_exit("You need to delete a layer.");
}

# prompt user for number of steps
my $s = g_getstring("Enter the number of steps:", $xlen,
                    "Population plotter");
g_exit() if length($s) == 0;
g_exit("Number of steps is not a +ve integer: $s") if $s !~ /^\d+$/ or $s == 0;
my $numsteps = $s;

# generate pattern for given number of steps
my @poplist = ( int(g_getpop()) );
my @genlist = ( int(g_getgen()) );
my $oldsecs = time;
for my $i (0..$numsteps-1) {
   g_step();
   push(@poplist, int(g_getpop()));
   push(@genlist, int(g_getgen()));
   
   g_dokey( g_getkey() );
   my $newsecs = time;
   if ($newsecs - $oldsecs >= 1.0) {     # show pattern every second
      $oldsecs = $newsecs;
      fit_if_not_visible();
      g_update();
      g_show(sprintf("Step %d of %d", $i+1, $numsteps));
   }
}

fit_if_not_visible();

# save some info before we switch layers
my $stepsize = sprintf("%d^%d", g_getbase(), g_getstep());
my $pattname = g_getname();

# create population plot in separate layer
g_setoption("stacklayers", 0);
g_setoption("tilelayers", 0);
if ($poplayer == -1) {
   $poplayer = g_addlayer();
} else {
   g_setlayer($poplayer);
}
g_movelayer($poplayer, 0);   # layer 0 has nicer color scheme
g_new($layername);

my $minpop = min(@poplist);
my $maxpop = max(@poplist);
if ($minpop == $maxpop) {
   # avoid division by zero
   $minpop -= 1;
}
my $popscale = ($maxpop - $minpop) / $ylen;

my $mingen = min(@genlist);
my $maxgen = max(@genlist);
my $genscale = ($maxgen - $mingen) / $xlen;

# draw axes with origin at 0,0
draw_line(0, 0, $xlen, 0);
draw_line(0, 0, 0, -$ylen);

# add annotation using mono-spaced ASCII font
my $t = make_text(uc($pattname));
my @tbox = getminbox($t);
g_putcells($t, ($xlen - $tbox[2]) / 2, -$ylen - 10 - $tbox[3]);

$t = make_text("POPULATION");
@tbox = getminbox($t);
g_putcells($t, -10 - $tbox[3], -($ylen - $tbox[2]) / 2, 0, 1, -1, 0);

$t = make_text("$minpop");
@tbox = getminbox($t);
g_putcells($t, -$tbox[2] - 10, -$tbox[3] / 2);

$t = make_text("$maxpop");
@tbox = getminbox($t);
g_putcells($t, -$tbox[2] - 10, -$ylen - $tbox[3] / 2);

$t = make_text("GENERATION (step=$stepsize)");
@tbox = getminbox($t);
g_putcells($t, ($xlen - $tbox[2]) / 2, 10);

$t = make_text("$mingen");
@tbox = getminbox($t);
g_putcells($t, -$tbox[2] / 2, 10);

$t = make_text("$maxgen");
@tbox = getminbox($t);
g_putcells($t, $xlen - $tbox[2] / 2, 10);

# display result at scale 1:1
g_fit();
g_setmag(0);
g_show("");

# plot the data (do last because it could take a while if numsteps is huge)
my $x = int(($genlist[0] - $mingen) / $genscale);
my $y = int(($poplist[0] - $minpop) / $popscale);
$oldsecs = time;
for my $i (0..$numsteps-1) {
   my $newx = int(($genlist[$i+1] - $mingen) / $genscale);
   my $newy = int(($poplist[$i+1] - $minpop) / $popscale);
   draw_line($x, -$y, $newx, -$newy);
   $x = $newx;
   $y = $newy;
   g_dokey( g_getkey() );
   my $newsecs = time;
   if ($newsecs - $oldsecs >= 1.0) {     # show pattern every second
      $oldsecs = $newsecs;
      g_update();
   }
}
