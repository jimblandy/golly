# Runs the current selection for a given number of steps and
# creates a black and white animated GIF file.
# Based on code by Tony Smith.

use strict;

g_exit("There is no pattern.") if g_empty();
my @rect = g_getselrect();
g_exit("There is no selection.") if @rect == 0;
my $x = $rect[0];
my $y = $rect[1];
my $width = $rect[2];
my $height = $rect[3];

my $s = g_getstring("Enter the number of frames, the pause time between\n".
                    "each frame (in centisecs) and the output file:",
                    "100 1 out.gif",
                    "Create animated GIF");
my ($frames, $pause, $filename) = split(' ', $s, 3);

$frames = 100 if $frames eq "";
$pause = 1 if $pause eq "";
$filename = "out.gif" if $filename eq "";

g_exit("Number of frames is not an integer: $frames") if $frames !~ /^\d+$/;
g_exit("Pause time is not an integer: $pause") if $pause !~ /^\d+$/;

# ------------------------------------------------------------------------------

{
    my $header = "GIF89a";
    my $global = pack('v2B8c2', $width, $height, '10000000', 0, 0);
    my $colortable = pack('H*', 'FFFFFF000000');
    my $applic = chr(11) . 'NETSCAPE2.0' . pack('c2vc', 3, 1, 0, 0);
    my $descriptor = pack('v4B8', 0, 0, $width, $height, '00000000');
    
    open GIF, '>', $filename;
    print GIF $header, $global, $colortable;
    print GIF '!', chr(0xFF), $applic;
    for (my $f = 0; $f < $frames; $f++) {
        print GIF '!', chr(0xF9), pack('cB8vc2', 4, '00000000', $pause, 0, 0);
        # get data for this frame
        print GIF ',', $descriptor, chr(2), &compress( &getdata() );
        my $finc = $f + 1;
        g_show "frame: $finc/$frames";
        if ($finc < $frames) {
            g_step();
            g_update();
        }
    }
    print GIF ';';
    close(GIF);
    g_show "GIF animation saved in $filename";
}

# ------------------------------------------------------------------------------

sub getdata {
    my @lines = ();
    # each array element is a line of 0 and 1 characters
    for (my $row = $y; $row < $y + $height; $row++) {
        my $line = "";
        for (my $col = $x; $col < $x + $width; $col++) {
            if (g_getcell($col, $row)) {
                $line .= "1";
            } else {
                $line .= "0";
            }
        }
        push(@lines, $line);
    }
    return \@lines;
}

# ------------------------------------------------------------------------------

sub compress { # black and white special
    my @lines = @{$_[0]}; # array reference is parameter
    my %table = ('0' => 0, '1' => 1);
    my $curr = my $cc = 4;
    my $used = my $eoi = 5;
    my $bits = my $size = 3;
    my $mask = 7;
    my $output = my $code = '';
    foreach my $input (@lines) {
        while (length($input)) {
            my $next = substr($input, 0, 1, '');
            if (exists $table{"$code$next"}) {$code .= $next}
            else {
                $used++;
                $table{"$code$next"} = $used;
                $curr += $table{$code} << $bits;
                $bits += $size;
                while ($bits >= 8) {
                    $output .= chr($curr & 255);
                    $curr = $curr >> 8;
                    $bits -= 8;
                }
                if ($used > $mask) {
                    if ($size < 12) {
                        $size ++;
                        $mask = $mask * 2 + 1;
                    }
                    else {
                        $curr += $cc << $bits; # output cc in current width
                        $bits += $size;
                        while ($bits >= 8) {
                            $output .= chr($curr & 255);
                            $curr = $curr >> 8;
                            $bits -= 8;
                        }
                        %table = ('0' => 0, '1' => 1); # reset table
                        $used = 5;
                        $size = 3;
                        $mask = 7;
                    }
                }
                $code = $next;
            }
        }
    }
    $curr += $table{$code} << $bits;
    $bits += $size;
    while ($bits >= 8) {
        $output .= chr($curr & 255);
        $curr = $curr >> 8;
        $bits -= 8;
    }
    $output .= chr($curr);
    my $subbed = '';
    while (length($output) > 255) {$subbed .= chr(255) . substr($output, 0, 255, '')}
    return $subbed . chr(length($output)) . $output . chr(0);
}
