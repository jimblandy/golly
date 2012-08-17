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
   return if $newgen == $currgen;

   g_show("Hit escape to abort...");
   my $oldsecs = time;

   # do 1 step first, to ensure a reasonable value is saved for the step size:
   g_run(1);
   ++$currgen;

   my $base = g_getbase();
   my $todo = $newgen - $currgen;
   my $exp = 0;
   while ($todo > 0) {
      my $num_steps = $todo % $base;
      g_setstep($exp++);
      $todo = ($todo - $num_steps)/$base;
      while ($num_steps-- > 0) {
         if (g_empty()) {
            g_show("Pattern is empty.");
            return;
         }
         g_step();
         my $newsecs = time;
         if ($newsecs - $oldsecs >= 1.0) {   # do an update every sec
            $oldsecs = $newsecs;
            g_update();
         }
      }
   }
   g_show("");
}

# --------------------------------------------------------------------

sub savegen {
   my ($filename, $gen) = @_;
   if (not open(OUTFILE, ">".$filename)) {
      g_warn("Can't save gen in $filename:\n$!");
   } else {
      print OUTFILE $gen;
      close OUTFILE;
   }
}

# --------------------------------------------------------------------

# use same file name as in goto.py
my $GotoINIFileName = g_getdir("data")."goto.ini";
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
   savegen($GotoINIFileName, "");
} elsif ($gen !~ /^[+-]?\d[\d,]*$/) {
   g_exit("Sorry, but \"$gen\" is not a valid integer.");
} else {
   # best to save given gen now in case user aborts script
   savegen($GotoINIFileName, $gen);
   $gen =~ s/,//g;
   my $oldstep = g_getstep();
   go_to($gen);
   g_setstep($oldstep);
}
