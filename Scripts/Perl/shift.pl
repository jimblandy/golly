# Shift current selection by given x y amounts using optional mode.
# Author: Andrew Trevorrow (andrew@trevorrow.com), June 2007.

use strict;

my @selrect = g_getselrect();
g_exit("There is no selection.") if @selrect == 0;

my $s = g_getstring("Enter x y shift amounts and an optional mode\n".
                    "(valid modes are copy/or/xor, default is or):",
                    "0 0 or",
                    "Shift selection");
my ($x, $y, $mode) = split(' ', $s, 3);

# check x and y
g_exit() if $x eq "";
g_exit("Enter x and y amounts separated by a space.") if $y eq "";
g_exit("Bad x value: $x") unless $x =~ /^[+-]?\d+$/;
g_exit("Bad y value: $y") unless $y =~ /^[+-]?\d+$/;

# check optional mode
if ($mode eq "") {
   $mode = "or";
} else {
   $mode = lc($mode);
   $mode = "copy" if $mode eq "c";
   $mode = "or"   if $mode eq "o";
   $mode = "xor"  if $mode eq "x";
   if (not ($mode eq "copy" or $mode eq "or" or $mode eq "xor")) {
      g_exit("Unknown mode: $mode (must be copy/or/xor)");
   }
}

# this method cuts the current selection and pastes it into the
# new position (without changing the current clipboard pattern)
my $selcells = g_getcells(@selrect);
g_clear(0);
$selrect[0] += $x;
$selrect[1] += $y;
g_select(@selrect);
g_clear(0) if $mode eq "copy";
g_putcells($selcells, $x, $y, 1, 0, 0, 1, $mode);

g_fitsel() if !g_visrect(@selrect);
