# Tile current selection with clipboard pattern.
# Author: Andrew Trevorrow (andrew@trevorrow.com), June 2007.
# Updated to handle multi-state patterns, Aug 2008.

use strict;

# assume one-state cell array (may change below)
my $multistate = 0;

# ------------------------------------------------------------------------------

sub clip_rb {
    # set given cells except those outside given right and bottom edges
    my ($cells, $right, $bottom) = @_;
    my $len = @{$cells};
    my $x = 0;
    my $y = 1;
    if ($multistate) {
        #  ignore padding int if present
        $len -= 1 if $len % 3 == 1;
        while ($x < $len) {
            if (($cells->[$x] <= $right) and ($cells->[$y] <= $bottom)) {
                g_setcell($cells->[$x], $cells->[$y], $cells->[$x+2]);
            }
            $x += 3;
            $y += 3;
        }
    } else {   
        while ($x < $len) {
            if (($cells->[$x] <= $right) and ($cells->[$y] <= $bottom)) {
                g_setcell($cells->[$x], $cells->[$y], 1);
            }
            $x += 2;
            $y += 2;
        }
    }
}

# ------------------------------------------------------------------------------

my @selrect = g_getselrect();
g_exit("There is no selection.") if @selrect == 0;

# set selection edges
my $selleft = $selrect[0];
my $seltop = $selrect[1];
my $selright = $selleft + $selrect[2] - 1;
my $selbottom = $seltop + $selrect[3] - 1;

my $p = g_getclip();          # 1st 2 items are wd,ht
my $pwidth = shift(@{$p});
my $pheight = shift(@{$p});

if (@{$p} & 1 == 1) { $multistate = 1 }

g_clear(0);
if (@{$p} > 0) {
    # tile selrect with p, clipping right & bottom edges if necessary
    my $y = $seltop;
    while ($y <= $selbottom) {
        my $bottom = $y + $pheight - 1;
        my $x = $selleft;
        while ($x <= $selright) {
            my $right = $x + $pwidth - 1;
            if (($right <= $selright) and ($bottom <= $selbottom)) {
                g_putcells($p, $x, $y);
            } else {
                my $tempcells = g_transform($p, $x, $y);
                clip_rb($tempcells, $selright, $selbottom);
            }
            $x += $pwidth;
        }
        $y += $pheight;
    }
}
g_fitsel() if !g_visrect(@selrect);
