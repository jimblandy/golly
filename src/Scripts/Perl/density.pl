# Calculates the density of live cells in the current pattern.
# Author: Andrew Trevorrow (andrew@trevorrow.com), June 2007.

use strict;

my @rect = g_getrect();
g_exit("The pattern is empty.") if @rect == 0;

my $d = g_getpop() / ($rect[2] * $rect[3]);
if ($d < 0.000001) {
   g_show(sprintf("Density = %.1e",$d));
} else {
   g_show(sprintf("Density = %.6f",$d));
}
