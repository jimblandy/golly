Welcome to Golly, a Game of Life simulator.  For the latest information
please visit the Golly web site:

   http://golly.sourceforge.net/


How to build Golly
------------------

Ready-to-run executables are available for all the major platforms,
but if you want to build Golly from the source code then you'll have to
install wxWidgets first (visit http://www.wxwidgets.org/).  After you
do that, building Golly is easy.  Just use the appropriate makefile:

   nmake -f makefile-win   (on Windows 98 or later)
   make -f makefile-mac    (on Mac OS X)
   make -f makefile-gtk    (on Linux or other Unix platforms)
   make -f makefile-x11    (ditto)

NOTES:

- You need to edit makefile-win, makefile-mac and makefile-x11 to
  specify where wxWidgets is installed.

- You need to edit makefile-win to specify where Python is installed.

- Your compiler must use 32-bit integers and 32-bit pointers.

- Golly requires wxWidgets 2.6.1 or later.


How to build bgolly (the batch mode version)
--------------------------------------------

Golly can also be run as a command line program without any GUI.
To build this "batch mode" version, just specify bgolly as the target
of the make command.  Note that you don't need to install wxWidgets
to build bgolly.


Source code road map
--------------------

If you'd like to modify Golly then the following notes should help you
get started.  Each module is described, along with some key routines.

The GUI code is implemented in these wx* modules:

wxgolly.*      - The GUI application.
                 OnInit() is where it all starts.

wxmain.*       - Implements the main window.
                 OnMenu() handles all menu commands.
                 NewPattern() creates a new, empty universe.
                 OpenPattern() loads a pattern file.
                 GeneratePattern() generates the current pattern.

wxview.*       - Viewport for displaying and editing patterns.
                 ProcessKey() processes keyboard shortcuts.
                 ProcessClick() processes mouse clicks.

wxrender.*     - Rendering routines for viewport.
                 DrawView() draws the pattern, grid lines, etc.

wxstatus.*     - Status bar at top of main window.
                 DrawStatusBar() shows gen count, pop count, etc.
                 DisplayMessage() shows message in bottom line.

wxhelp.*       - Implements a modeless help window.
                 ShowHelp() displays given Help/*.html file.

wxinfo.*       - Implements a modeless info window.
                 ShowInfo() displays comments in given pattern file.

wxprefs.*      - For loading, saving and changing user preferences.
                 GetPrefs() loads data from GollyPrefs file.
                 SavePrefs() writes data to GollyPrefs file.
                 ChangePrefs() opens the Preferences dialog.

wxrule.*       - Lets user change the current rule.
                 ChangeRule() opens the Rule dialog.

wxscript.*     - Implements Python script support.
                 RunScript() executes a given .py file.

wxutils.*      - Utility dialogs for showing errors and progress.
                 Warning() displays message in modal dialog.
                 Fatal() displays message and exits app.

The following non-wx modules are sufficient to build bgolly.
They also define abstract viewport and rendering interfaces used
by the above GUI code:

bgolly.cpp     - The batch mode program.
                 main() does all the option parsing.

lifealgo.*     - Abstract Life algorithm operations:
                 setcell() sets given cell to given state.
                 getcell() gets state of given cell.
                 nextcell() finds next live cell in current row.
                 step() advances pattern by current increment.
                 fit() fits pattern within given viewport.
                 draw() renders pattern in given viewport.

qlifealgo.*    - Implements a quick, conventional algorithm.

hlifealgo.*    - Implements a super fast hashing algorithm.

viewport.*     - Abstract viewport operations:
                 zoom() zooms into a given location.
                 unzoom() zooms out from a given location.
                 setmag() sets the magnification.
                 move() scrolls view by given number of pixels.

liferender.*   - Abstract routines for rendering a pattern:
                 killrect() fills rectangle with dead cell color.
                 blit() draws bitmap with at least one live cell.

qlifedraw.cpp  - Rendering routines for qlifealgo.

hlifedraw.cpp  - Rendering routines for hlifealgo.

liferules.*    - Implements rules used by lifealgo.
                 setrule() parses and validates a given rule.

lifepoll.*     - Allows lifealgo routines to do event processing.
                 checkevents() processes events.

readpattern.*  - Reads pattern files in a variety of formats.
                 readpattern() loads pattern into given universe.
                 readcomments() extracts comments from given file.

writepattern.* - Saves current pattern to a file.
                 writepattern() saves pattern in requested format.

bigint.*       - Operations on arbitrarily large integers.

util.*         - Utilities for displaying errors and progress info.
                 warning() displays error message.
                 fatal() displays error message and exits.


Have fun, and please let us know if you make any changes!

Tomas Rokicki <rokicki@gmail.com>
Andrew Trevorrow <andrew@trevorrow.com>
