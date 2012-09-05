# Oscar is an OSCillation AnalyzeR for use with Golly.
# Author: Andrew Trevorrow (andrew@trevorrow.com), June 2007.
# Modified to use Math::BigInt, December 2008.
# Modified to handle B0-and-not-S8 rules, August 2009.

# This script uses Gabriel Nivasch's "keep minima" algorithm.
# For each generation, calculate a hash value for the pattern.  Keep all of
# the record-breaking minimal hashes in a list, with the oldest first.
# For example, after 5 generations the saved hash values might be:
# 
#   8 12 16 24 25,
# 
# If the next hash goes down to 13 then the list can be shortened:
# 
#   8 12 13.
# 
# If the current hash matches one of the saved hashes, it is highly likely
# the pattern is oscillating.  By keeping a corresponding list of generation
# counts we can calculate the period.  We also keep lists of population
# counts and bounding boxes to reduce the chance of spurious oscillator
# detection due to hash collisions.  The bounding box info also allows us
# to detect moving oscillators (spaceships/knightships).

use strict;
use Time::HiRes qw ( time );

use Math::BigInt;          # use arbitrarily big integers

# --------------------------------------------------------------------

# initialize lists
my @hashlist = ();         # for pattern hash values
my @genlist = ();          # corresponding generation counts
my @poplist = ();          # corresponding population counts
my @boxlist = ();          # corresponding bounding boxes

# --------------------------------------------------------------------

sub show_spaceship_speed {
    # we found a moving oscillator
    my ($period, $deltax, $deltay) = @_;
    if ($period == 1) {
        g_show("Spaceship detected (speed = c)");
    }
    elsif ($deltax == $deltay or $deltax == 0 or $deltay == 0) {
        my $speed = "";
        if ($deltax == 0 or $deltay == 0) {
            # orthogonal spaceship
            if ($deltax > 1 or $deltay > 1) {
                $speed .= ($deltax + $deltay);
            }
        } else {
            # diagonal spaceship (deltax == deltay)
            if ($deltax > 1) {
                $speed .= $deltax;
            }
        }
        g_show("Spaceship detected (speed = $speed"."c/$period)");
    }
    else {
        # deltax != deltay and both > 0
        my $speed = "$deltay,$deltax";
        g_show("Knightship detected (speed = $speed"."c/$period)");
    }
}

# --------------------------------------------------------------------

sub oscillating {
    # return 1 if the pattern is empty, stable or oscillating
    
    # first get current pattern's bounding box
    my @pbox = g_getrect();
    if (@pbox == 0) {
        g_show("The pattern is empty.");
        return 1;
    }
    
    # get current pattern and create "normalized" version -- ie. shift its
    # top left corner to 0,0 -- so we can detect spaceships and knightships
    ## my $shifted = g_transform(g_getcells(@pbox), -$pbox[0], -$pbox[1]);
    # create hash value of shifted pattern
    ## my $h = 0;
    ## foreach (@{$shifted}) {
    ##     $h = ($h % 10000000) * 33 + $_;
    ## }
    
    # use g_hash command (5 times faster than above code)
    my $h = g_hash(@pbox);
    
    # check if outer-totalistic rule has B0 but not S8
    my $rule = g_getrule();
    my ($prefix, $suffix) = split(':', $rule, 2);
    my $hasB0notS8 = ($prefix =~ m/^B0/ and $prefix =~ m|/| and $prefix !~ m/8$/);
    
    # determine where to insert h into hashlist
    my $pos = 0;
    while ($pos < @hashlist) {
        if ($h > $hashlist[$pos]) {
            $pos += 1;
        }
        elsif ($h < $hashlist[$pos]) {
            # shorten lists and append info below
            splice(@hashlist, $pos);
            splice(@genlist, $pos);
            splice(@poplist, $pos);
            splice(@boxlist, $pos);
            last;
        }
        else {
            # h == hashlist[pos] so pattern is probably oscillating, but just in
            # case this is a hash collision we also compare pop count and box size
            my @rect = @{$boxlist[$pos]};
            my $currpop = Math::BigInt->new(g_getpop());
            if ( $currpop->bcmp($poplist[$pos]) == 0 and
                $pbox[2] == $rect[2] and
                $pbox[3] == $rect[3] ) {
                    my $currgen = Math::BigInt->new(g_getgen());
                    my $bigp = $currgen->bsub($genlist[$pos]);
                    my $period = $bigp->numify();
                    
                    if ($hasB0notS8 and ($period % 2) and
                        $pbox[0] == $rect[0] and $pbox[1] == $rect[1] and
                        $pbox[2] == $rect[2] and $pbox[3] == $rect[3]) {
                            # ignore this hash value because B0-and-not-S8 rules are
                            # emulated by using different rules for odd and even gens,
                            # so it's possible to have identical patterns at gen G and
                            # gen G+p if p is odd
                            return 0;
                        }
                    
                    if ($period == 1) {
                        if ($pbox[0] == $rect[0] and $pbox[1] == $rect[1] and
                            $pbox[2] == $rect[2] and $pbox[3] == $rect[3]) {
                                g_show("The pattern is stable.");
                            } else {
                                show_spaceship_speed(1, 0, 0);
                            }
                    } elsif ($pbox[0] == $rect[0] and $pbox[1] == $rect[1] and
                    $pbox[2] == $rect[2] and $pbox[3] == $rect[3]) {
                        g_show("Oscillator detected (period = $period)");
                    } else {
                        my $deltax = abs($rect[0] - $pbox[0]);
                        my $deltay = abs($rect[1] - $pbox[1]);
                        show_spaceship_speed($period, $deltax, $deltay);
                    }
                    return 1;
                } else {
                    # look at next matching hash value or insert if no more
                    $pos += 1;
                }
        }
    }
    
    # store hash/gen/pop/box info at same position in various lists
    splice(@hashlist, $pos, 0, $h);
    splice(@genlist, $pos, 0, Math::BigInt->new(g_getgen()));
    splice(@poplist, $pos, 0, Math::BigInt->new(g_getpop()));
    splice(@boxlist, $pos, 0, \@pbox);
    
    return 0;
}

# --------------------------------------------------------------------

sub fit_if_not_visible {
    # fit pattern in viewport if not empty and not completely visible
    my @r = g_getrect();
    if (@r > 0 and !g_visrect(@r)) { g_fit() }
}

# --------------------------------------------------------------------

g_show("Checking for oscillation... (hit escape to abort)");

my $oldsecs = time;
while ( not oscillating() ) {
    g_run(1);
    my $newsecs = time;
    if ($newsecs - $oldsecs >= 1.0) {   # show pattern every second
        $oldsecs = $newsecs;
        fit_if_not_visible();
        g_update();
    }
}

fit_if_not_visible();
