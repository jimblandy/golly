# Use multiple layers to create a history of the current pattern.
# The "envelope" layer remembers all live cells.
# Author: Andrew Trevorrow (andrew@trevorrow.com), June 2007.

use strict;

g_exit("There is no pattern.") if g_empty();

my $currname = "current";
my $envname = "envelope";
my $currindex = g_getlayer();
my $envindex;

if ($currindex > 1
         and g_getname($currindex) eq $currname
         and g_getname($currindex - 1) eq $envname) {
   # continue from where we left off
   $envindex = $currindex - 1;
}
elsif ($currindex + 1 < g_numlayers()
         and g_getname($currindex) eq $envname
         and g_getname($currindex + 1) eq $currname) {
   # switch from envelope layer to currname layer and continue
   g_setlayer($currindex + 1);
   $envindex = $currindex;
   $currindex += 1;
}
elsif ($currindex + 2 < g_numlayers()
         and g_getname($currindex + 1) eq $envname
         and g_getname($currindex + 2) eq $currname) {
   # switch from starting layer to currname layer and continue
   g_setlayer($currindex + 2);
   $envindex = $currindex + 1;
   $currindex += 2;
}
else {
   # start a new envelope using pattern in current layer
   if (g_numlayers() + 1 > g_maxlayers()) {
      g_exit("You need to delete a couple of layers.");
   }
   if (g_numlayers() + 2 > g_maxlayers()) {
      g_exit("You need to delete a layer.");
   }
   
   # get starting pattern from current layer
   my $startpatt = g_getcells(g_getrect());
   my $startstep = g_getstep();
   
   $envindex = g_addlayer();  # create layer for remembering all live cells
   g_putcells($startpatt);    # copy starting pattern into this layer
   
   $currindex = g_addlayer(); # create layer for generating pattern
   g_putcells($startpatt);    # copy starting pattern into this layer
   g_setstep($startstep);

   # name the current and envelope layers so user can run script
   # again and continue from where it was stopped
   g_setname($currname);
   g_setname($envname, $envindex);
}

# draw stacked layers using same location and scale
g_setoption("stacklayers", 1);

g_show("Hit escape key to stop script...");
while (1) {
   g_dokey( g_getkey() );
   g_run(1);
   if (g_empty()) {
      g_show("Pattern died out.");
      last;
   }
   
   # copy current pattern to envelope layer;
   # we temporarily disable event checking so thumb scrolling
   # and other mouse events won't cause confusing changes
   my $currpatt = g_getcells(g_getrect());
   g_check(0);
   g_setlayer($envindex);
   g_putcells($currpatt);
   g_setlayer($currindex);
   g_check(1);
   
   my $step = 1;
   my $expo = g_getstep();
   if ($expo > 0) {
      $step = g_getbase()**$expo;
   }
   if (g_getgen() % $step == 0) {
      # display all 3 layers (start, envelope, current)
      g_update();
   }
}
