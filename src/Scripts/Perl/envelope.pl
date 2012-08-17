# Use multiple layers to create a history of the current pattern.
# The "envelope" layer remembers all live cells.
# Author: Andrew Trevorrow (andrew@trevorrow.com), June 2007.
# Updated to use new setcolors command, September 2008.

use strict;

g_exit("There is no pattern.") if g_empty();

my $currindex = g_getlayer();
my $startindex;
my $envindex;
my $startname = "starting pattern";
my $envname = "envelope";

if ($currindex > 1 and g_getname($currindex - 1) eq $startname
                   and g_getname($currindex - 2) eq $envname) {
   # continue from where we left off
   $startindex = $currindex - 1;
   $envindex = $currindex - 2;
}
elsif ($currindex + 2 < g_numlayers()
                   and g_getname($currindex + 1) eq $startname
                   and g_getname($currindex)     eq $envname) {
   # switch from envelope layer to current layer and continue
   $currindex += 2;
   g_setlayer($currindex);
   $startindex = $currindex - 1;
   $envindex = $currindex - 2;
}
elsif ($currindex + 1 < g_numlayers()
                   and g_getname($currindex)     eq $startname
                   and g_getname($currindex - 1) eq $envname) {
   # switch from starting layer to current layer and continue
   $currindex += 1;
   g_setlayer($currindex);
   $startindex = $currindex - 1;
   $envindex = $currindex - 2;
}
else {
   # start a new envelope using pattern in current layer
   if (g_numlayers() + 1 > g_maxlayers()) {
      g_exit("You need to delete a couple of layers.");
   }
   if (g_numlayers() + 2 > g_maxlayers()) {
      g_exit("You need to delete a layer.");
   }
   
   # get current layer's starting pattern
   my $startpatt = g_getcells(g_getrect());
   
   $envindex = g_addlayer();        # create layer for remembering all live cells
   g_setcolors([-1,100,100,100]);   # set all states to darkish gray
   g_putcells($startpatt);          # copy starting pattern into this layer
   
   $startindex = g_addlayer();      # create layer for starting pattern
   g_setcolors([-1,0,255,0]);       # set all states to green
   g_putcells($startpatt);          # copy starting pattern into this layer

   # move currindex to above the envelope and starting pattern
   g_movelayer($currindex, $envindex);
   g_movelayer($envindex, $startindex);
   $currindex = $startindex;
   $startindex = $currindex - 1;
   $envindex = $currindex - 2;

   # name the starting and envelope layers so user can run script
   # again and continue from where it was stopped
   g_setname($startname, $startindex);
   g_setname($envname, $envindex);
}

# ------------------------------------------------------------------------------

sub envelope {
   # draw stacked layers using same location and scale
   g_setoption("stacklayers", 1);
   
   g_show("Hit escape key to stop script...");
   while (1) {
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
         # display all 3 layers (envelope, start, current)
         g_update();
      }
   }
}

# ------------------------------------------------------------------------------

# show status bar but hide layer & edit bars (faster, and avoids flashing)
my $oldstatus = g_setoption("showstatusbar", 1);
my $oldlayerbar = g_setoption("showlayerbar", 0);
my $oldeditbar = g_setoption("showeditbar", 0);

envelope();

END {
   # this code is always executed, even after escape/error;
   # restore original state of status/layer/edit bars
   g_setoption("showstatusbar", $oldstatus);
   g_setoption("showlayerbar", $oldlayerbar);
   g_setoption("showeditbar", $oldeditbar);
}
