# Use the current selection to create a toroidal universe.
# Author: Andrew Trevorrow (andrew@trevorrow.com), Oct 2010.

use strict;

my @selrect = g_getselrect();
g_exit("There is no selection.") if @selrect == 0;
my $x =  $selrect[0];
my $y =  $selrect[1];
my $wd = $selrect[2];
my $ht = $selrect[3];

my $selcells = g_getcells(@selrect);
if (!g_empty()) {
    g_clear(0);
    g_clear(1);
}

# get current rule, remove any existing suffix, then add new suffix
my ($rule, $suffix) = split(":",g_getrule(),2);
g_setrule("$rule:T$wd,$ht");

my $newx = -int($wd/2);
my $newy = -int($ht/2);
$selrect[0] = $newx;
$selrect[1] = $newy;
g_select(@selrect);
g_putcells($selcells, $newx - $x, $newy - $y) if length($selcells) > 0;
g_fitsel();
