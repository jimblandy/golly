# Tile current selection with pattern inside selection.
# Author: Andrew Trevorrow (andrew@trevorrow.com), June 2007.
# Updated to handle multi-state patterns, Aug 2008.

use strict;

my @selrect = g_getselrect();
g_exit("There is no selection.") if @selrect == 0;

my $selpatt = g_getcells(@selrect);
g_exit("No pattern in selection.") if @{$selpatt} == 0;

# determine if selpatt is one-state or multi-state
my $inc = 2;
if (@{$selpatt} & 1 == 1) { $inc = 3 }

# ------------------------------------------------------------------------------

# return a rect which is the minimal bounding box of given pattern
sub getminbox {
    my $cells = shift;
    my $len = @{$cells};
    return () if $len < 2;
    
    my $minx = $cells->[0];
    my $miny = $cells->[1];
    my $maxx = $minx;
    my $maxy = $miny;
    
    # ignore padding int if present
    $len -= 1 if ($inc == 3) and ($len % 3 == 1);
    
    for (my $x = 0; $x < $len; $x += $inc) {
        if ($cells->[$x] < $minx) { $minx = $cells->[$x] }
        if ($cells->[$x] > $maxx) { $maxx = $cells->[$x] }
    }
    for (my $y = 1; $y < $len; $y += $inc) {
        if ($cells->[$y] < $miny) { $miny = $cells->[$y] }
        if ($cells->[$y] > $maxy) { $maxy = $cells->[$y] }
    }
    
    return ($minx, $miny, $maxx - $minx + 1, $maxy - $miny + 1);
}

# ------------------------------------------------------------------------------

sub clip_left {
    my ($cells, $left) = @_;
    my $len = @{$cells};
    my $x = 0;
    if ($inc == 3) {
        #  ignore padding int if present
        $len -= 1 if $len % 3 == 1;
        while ($x < $len) {
            if ($cells->[$x] >= $left) {
                g_setcell($cells->[$x], $cells->[$x+1], $cells->[$x+2]);
            }
            $x += 3;
        }
    } else {
        while ($x < $len) {
            if ($cells->[$x] >= $left) {
                g_setcell($cells->[$x], $cells->[$x+1], 1);
            }
            $x += 2;
        }
    }
}

# ------------------------------------------------------------------------------

sub clip_right {
    my ($cells, $right) = @_;
    my $len = @{$cells};
    my $x = 0;
    if ($inc == 3) {
        #  ignore padding int if present
        $len -= 1 if $len % 3 == 1;
        while ($x < $len) {
            if ($cells->[$x] <= $right) {
                g_setcell($cells->[$x], $cells->[$x+1], $cells->[$x+2]);
            }
            $x += 3;
        }
    } else {   
        while ($x < $len) {
            if ($cells->[$x] <= $right) {
                g_setcell($cells->[$x], $cells->[$x+1], 1);
            }
            $x += 2;
        }
    }
}

# ------------------------------------------------------------------------------

sub clip_top {
    my ($cells, $top) = @_;
    my $len = @{$cells};
    my $y = 1;
    if ($inc == 3) {
        #  ignore padding int if present
        $len -= 1 if $len % 3 == 1;
        while ($y < $len) {
            if ($cells->[$y] >= $top) {
                g_setcell($cells->[$y-1], $cells->[$y], $cells->[$y+1]);
            }
            $y += 3;
        }
    } else {   
        while ($y < $len) {
            if ($cells->[$y] >= $top) {
                g_setcell($cells->[$y-1], $cells->[$y], 1);
            }
            $y += 2;
        }
    }
}

# ------------------------------------------------------------------------------

sub clip_bottom {
    my ($cells, $bottom) = @_;
    my $len = @{$cells};
    my $y = 1;
    if ($inc == 3) {
        #  ignore padding int if present
        $len -= 1 if $len % 3 == 1;
        while ($y < $len) {
            if ($cells->[$y] <= $bottom) {
                g_setcell($cells->[$y-1], $cells->[$y], $cells->[$y+1]);
            }
            $y += 3;
        }
    } else {   
        while ($y < $len) {
            if ($cells->[$y] <= $bottom) {
                g_setcell($cells->[$y-1], $cells->[$y], 1);
            }
            $y += 2;
        }
    }
}

# ------------------------------------------------------------------------------

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
        my $tempcells = g_transform($selpatt, -$bbox[2] * $i, 0);
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
        my $tempcells = g_transform($selpatt, $bbox[2] * $i, 0);
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
        my $tempcells = g_transform($selpatt, 0, -$bbox[3] * $i);
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
        my $tempcells = g_transform($selpatt, 0, $bbox[3] * $i);
        clip_bottom($tempcells, $selbottom);
    }
}

g_fitsel() if !g_visrect(@selrect);
