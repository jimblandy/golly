# Tile current selection with pattern inside selection.
# Author: Andrew Trevorrow (andrew@trevorrow.com), June 2007.

use strict;

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

sub clip_left {
   my ($cells, $left) = @_;
   my $x = 0;
   while ($x < @{$cells}) {
      if ($cells->[$x] >= $left) {
         g_setcell($cells->[$x], $cells->[$x+1], 1);
      }
      $x += 2;
   }
}

# ------------------------------------------------------------------------------

sub clip_right {
   my ($cells, $right) = @_;
   my $x = 0;
   while ($x < @{$cells}) {
      if ($cells->[$x] <= $right) {
         g_setcell($cells->[$x], $cells->[$x+1], 1);
      }
      $x += 2;
   }
}

# ------------------------------------------------------------------------------

sub clip_top {
   my ($cells, $top) = @_;
   my $y = 1;
   while ($y < @{$cells}) {
      if ($cells->[$y] >= $top) {
         g_setcell($cells->[$y-1], $cells->[$y], 1);
      }
      $y += 2;
   }
}

# ------------------------------------------------------------------------------

sub clip_bottom {
   my ($cells, $bottom) = @_;
   my $y = 1;
   while ($y < @{$cells}) {
      if ($cells->[$y] <= $bottom) {
         g_setcell($cells->[$y-1], $cells->[$y], 1);
      }
      $y += 2;
   }
}

# ------------------------------------------------------------------------------

my @selrect = g_getselrect();
g_exit("There is no selection.") if @selrect == 0;

my $selpatt = g_getcells(@selrect);
g_exit("No pattern in selection.") if !$selpatt;

# set selection edges
my $selleft = $selrect[0];
my $seltop = $selrect[1];
my $selright = $selleft + $selrect[2] - 1;
my $selbottom = $seltop + $selrect[3] - 1;

# find selpatt's minimal bounding box
my @bbox = getminbox($selpatt);
my $i;

# first tile selpatt horizontally, clipping where necessary
my $left = $bbox[0];
my $right = $left + $bbox[2] - 1;
$i = 0;
while ($left > $selleft) {
   $left -= $bbox[2];
   $i += 1;
   if ($left >= $selleft) {
      g_putcells($selpatt, -$bbox[2] * $i, 0);
   } else {
      my $tempcells = g_transform($selpatt, -$bbox[2] * $i, 0, 1, 0, 0, 1);
      clip_left($tempcells, $selleft);
   }
}
$i = 0;
while ($right < $selright) {
   $right += $bbox[2];
   $i += 1;
   if ($right <= $selright) {
      g_putcells($selpatt, $bbox[2] * $i, 0);
   } else {
      my $tempcells = g_transform($selpatt, $bbox[2] * $i, 0, 1, 0, 0, 1);
      clip_right($tempcells, $selright);
   }
}

# get new selection pattern and tile vertically, clipping where necessary
$selpatt = g_getcells(@selrect);
@bbox = getminbox($selpatt);
my $top = $bbox[1];
my $bottom = $top + $bbox[3] - 1;
$i = 0;
while ($top > $seltop) {
   $top -= $bbox[3];
   $i += 1;
   if ($top >= $seltop) {
      g_putcells($selpatt, 0, -$bbox[3] * $i);
   } else {
      my $tempcells = g_transform($selpatt, 0, -$bbox[3] * $i, 1, 0, 0, 1);
      clip_top($tempcells, $seltop);
   }
}
$i = 0;
while ($bottom < $selbottom) {
   $bottom += $bbox[3];
   $i += 1;
   if ($bottom <= $selbottom) {
      g_putcells($selpatt, 0, $bbox[3] * $i);
   } else {
      my $tempcells = g_transform($selpatt, 0, $bbox[3] * $i, 1, 0, 0, 1);
      clip_bottom($tempcells, $selbottom);
   }
}

g_fitsel() if !g_visrect(@selrect);
