Welcome to Golly, a Game of Life simulator.  For the latest information
please visit the Golly web site:

   http://golly.sourceforge.net/


How to install wxWidgets
------------------------

If you want to build Golly from source code then you'll have to install
wxWidgets first.  Visit http://www.wxwidgets.org/downloads/ and grab
the appropriate source archive for your platform:

   wxMSW   - for Windows (get the installer)
   wxMac   - for Mac OS X
   wxGTK   - for Linux/Unix with GTK+
   wxX11   - for Linux/Unix with X11

Golly should compile with wxWidgets 2.7.0 or later, but it's best to
use the latest stable version.


Building wxWidgets on Windows
-----------------------------

If you have an older version of wxWidgets already installed then make
sure you uninstall that version before installing a newer version.
Run the wxWidgets installer and follow the instructions.  We'll assume
this creates a C:\wxWidgets directory.  Open up a command window and
type these commands:

   cd \wxWidgets\build\msw
   nmake -f makefile.vc BUILD=release RUNTIME_LIBS=static UNICODE=1

This assumes you have the MS Visual C++ compiler and linker installed.
It's also a good idea to edit \wxWidgets\build\msw\config.vc and set

   BUILD=release
   RUNTIME_LIBS=static
   UNICODE=1

so you won't have to specify those settings each time you run nmake
to build Golly.


Building wxWidgets on Mac OS X
------------------------------

Unpack the wxMac source archive wherever you like, start up Terminal
and type these commands (using the correct version number):

   cd /path/to/wxMac-2.8.0
   mkdir build-osx
   cd build-osx
   ../configure --with-mac --disable-shared --enable-unicode \
                --enable-universal_binary
   make


Building wxWidgets on Linux/Unix with GTK+
------------------------------------------

Unpack the wxGTK source archive wherever you like, start up a terminal
session and type these commands (using the correct version number):

   cd /path/to/wxGTK-2.8.0
   mkdir build-gtk
   cd build-gtk
   ../configure --with-gtk --disable-shared --enable-unicode
   make
   su
   make install
   ldconfig

This installs the wx libraries in a suitable directory.  It also
installs the wx-config program which will be called by makefile-gtk
to set the appropriate compile and link options for building Golly.


Building wxWidgets on Linux/Unix with X11
-----------------------------------------

Unpack the wxX11 source archive wherever you like, start up a terminal
session and type these commands (using the correct version number):

   cd /path/to/wxX11-2.8.0
   ./configure --with-x11 --disable-shared --enable-unicode
   make

Note that configure is run in the top-level wxWidgets directory and
there is no "make install" step as in the above GTK+ commands.
This allows you to have both GTK+ and X11 builds of wxWidgets on
the one system.  (There are other ways to achieve this goal, but
the above arrangement is the one currently required by Golly's
makefile-gtk and makefile-x11.)


How to install Perl and Python
------------------------------

Golly uses Perl and Python for scripting, so you'll need to make
sure both are installed.  Mac OS X users don't have to do anything
because Perl and Python are already installed.

If you are running Linux, you probably have Perl installed.
Type "perl -v" at the command line to print out the version.
Golly's code should compile happily with Perl 5.8.x or later.
Windows users are advised to download the ActivePerl installer
from http://www.activestate.com/Products/ActivePerl/.

Windows and Linux users can download a Python installer from
http://www.python.org/download/.


How to build Golly
------------------

Once wxWidgets, Perl and Python are installed, building Golly should
be easy.  Just use the appropriate makefile for your platform:

   nmake -f makefile-win   - on Windows 98 or later
   make -f makefile-mac    - on Mac OS X 10.3.9 or later
   make -f makefile-gtk    - on Linux/Unix with GTK+
   make -f makefile-x11    - on Linux/Unix with X11

NOTES:

- You need to edit makefile-win, makefile-mac and makefile-x11 to
  specify where wxWidgets is installed.  Change the WX_DIR path
  near the start of the file.  Also make sure WX_RELEASE specifies
  the first two digits of your wxWidgets version.

- In makefile-win you need to include the headers for Perl and Python
  so change the paths in PERL_INCLUDE and PYTHON_INCLUDE if necessary.


How to build bgolly (the batch mode version)
--------------------------------------------

Golly can also be run as a command line program without any GUI.
To build this "batch mode" version, just specify bgolly as the target
of the make command.  Note that you don't need to install wxWidgets,
Perl or Python to build bgolly.


Source code road map
--------------------

If you'd like to modify Golly then the following notes should help you
get started.  Each module is described, along with some key routines.

The GUI code is implemented in these wx* modules:

wxgolly.*      - The GUI application.
                 OnInit() is where it all starts.

wxmain.*       - Implements the main window.
                 OnMenu() handles all menu commands.
                 UpdateEverything() updates all parts of the GUI.

wxfile.cpp     - File menu functions.
                 NewPattern() creates a new, empty universe.
                 LoadPattern() reads in a pattern file.

wxedit.cpp     - Edit menu functions.
                 CopySelection() copies selection to clipboard.
                 PasteClipboard() pastes in clipboard pattern.
                 RotateSelection() rotates current selection.

wxcontrol.cpp  - Control menu functions.
                 GeneratePattern() runs the current pattern.
                 ToggleHashing() toggles the hashing algorithm.

wxview.*       - Implements the viewport window.
                 ProcessKey() processes keyboard shortcuts.
                 ProcessClick() processes mouse clicks.

wxrender.*     - Rendering routines for updating the viewport.
                 DrawView() draws the pattern, grid lines, etc.

wxlayer.*      - Layer menu functions.
                 AddLayer() adds a new, empty layer.
                 DeleteLayer() deletes the current layer.

wxundo.*       - Implements unlimited undo/redo.
                 RememberCellChanges() saves cell state changes.
                 UndoChange() undoes a recent change.
                 RedoChange() redoes an undone change.

wxstatus.*     - Implements status bar at top of main window.
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

wxscript.*     - High-level scripting interface.
                 RunScript() executes a given script file.

wxperl.*       - Implements Perl script support.
                 RunPerlScript() executes a given .pl file.

wxpython.*     - Implements Python script support.
                 RunPythonScript() executes a given .py file.

wxutils.*      - Various utility routines.
                 Warning() displays message in modal dialog.
                 Fatal() displays message and exits app.

The following non-wx modules are sufficient to build bgolly.
They also define abstract viewport and rendering interfaces used
by the above GUI code:

bgolly.cpp     - The batch mode program.
                 main() does all the option parsing.

platform.h     - Platform specific defines (eg. 64-bit changes).

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

Andrew Trevorrow <andrew@trevorrow.com>
Tomas Rokicki <rokicki@gmail.com>
