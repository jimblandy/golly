# Go to a requested generation.  The given generation can be an
# absolute number like 1,000,000 (commas are optional) or a number
# relative to the current generation like +9 or -6.  If the target
# generation is less than the current generation then we go back
# to the starting generation (normally 0) and advance to the target.
# Authors: Andrew Trevorrow and Dave Greene, June 2007.

use strict;
use Time::HiRes qw ( time );

# --------------------------------------------------------------------

sub go_to {
   my $gen = shift;
   my $currgen = g_getgen();
   my $newgen = $gen;
   if (substr($gen,0,1) eq '+') {
      $newgen = $currgen + $gen;
   } elsif (substr($gen,0,1) eq '-') {
      my $n = -$gen;
      if ($currgen > $n) {
         $newgen = $currgen - $n;
      } else {
         $newgen = 0;
      }
   }
   
   if ($newgen < $currgen) {
      # try to go back to starting gen (not necessarily 0) and
      # then forwards to newgen; note that reset() also restores
      # algorithm and/or rule, so too bad if user changed those
      # after the starting info was saved;
      # first save current location and scale
      my ($midx, $midy) = g_getpos();
      my $mag = g_getmag();
      g_reset();
      # restore location and scale
      g_setpos($midx, $midy);
      g_setmag($mag);
      # current gen might be > 0 if user loaded a pattern file
      # that set the gen count
      $currgen = g_getgen();
      if ($newgen < $currgen) {
         g_error("Can't go back any further; pattern was saved ".
                 "at generation $currgen.");
         return;
      }
   }
   
   g_show("Hit escape to abort...");
   my $oldsecs = time;
   while ($currgen < $newgen) {
      if (g_empty()) {
         g_show("Pattern is empty.");
         return;
      }
      # using g_step() as a faster method for getting to newgen
      # is left as an exercise for the reader :)
      g_run(1);
      $currgen += 1;
      g_dokey( g_getkey() );              # allow keyboard interaction
      my $newsecs = time;
      if ($newsecs - $oldsecs >= 1.0) {   # do an update every sec
         $oldsecs = $newsecs;
         g_update();
      }
   }
   g_show("");
}

# --------------------------------------------------------------------

# use same file name as in goto.py
my $GotoINIFileName = g_datadir()."goto.ini";
my $previousgen = "";

if (open(INFILE, $GotoINIFileName)) {
   $previousgen = <INFILE>;
   close INFILE;
}

my $gen = g_getstring("Enter the desired generation number,\n".
                      "or -n/+n to go back/forwards by n:",
                      $previousgen, "Go to generation");
if ($gen eq "") {
   g_exit();
} elsif ($gen eq '+' or $gen eq '-') {
   # clear the default
   $previousgen = "";
} elsif ($gen !~ /^[+-]?\d[\d,]*$/) {
   g_exit("Sorry, but \"$gen\" is not a valid integer.");
} else {
   $previousgen = $gen;
   $gen =~ s/,//g;
   go_to($gen);
}

if (not open(OUTFILE, ">".$GotoINIFileName)) {
   g_warn("Can't save gen in $GotoINIFileName:\n$!");
} else {
   print OUTFILE $previousgen;
   close OUTFILE;
}
