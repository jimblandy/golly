--[[
This module makes it easy to create a new cellular automaton
that is not supported natively by Golly.  It should be possible
to implement almost any CA that uses square cells with up to
256 states on a finite grid.

To implement a new CA you need to create a .lua file that looks
something like this:

    local g = golly()
    require "gplus.NewCA"
    
    SCRIPT_NAME = "MyCA"    -- must match the name of your .lua file
    DEFAULT_RULE = "XXX"    -- must be a valid rule in your CA
    RULE_HELP = "HTML code describing the rules allowed by your CA."
    
    function ParseRule(newrule)
        -- Parse the given rule string.
        -- If valid then return nil, the canonical rule string,
        -- the width and height of the grid, and the number of states.
        -- If not valid then just return an appropriate error message.
        
        ... parsing code for your rule syntax ...
        
        return nil, canonrule, wd, ht, numstates
        -- Note that wd and ht must be from 4 to 4000,
        -- and numstates must be from 2 to 256.
    end
    
    function NextPattern(currcells, minx, miny, maxx, maxy)
        -- Create the next pattern using the given parameters:
        -- currcells is a non-empty cell array containing the current pattern.
        -- minx, miny, maxx, maxy are the cell coordinates of the grid edges.
        -- This function must return the new pattern as a cell array.
        
        local newcells = {}

        -- Note that currcells and newcells are one-state cell arrays if
        -- g.numstates() == 2, otherwise they are both multi-state.
        
        ... code to create the next pattern ...

        -- delete the old pattern and add the new pattern
        g.putcells(currcells, 0, 0, 1, 0, 0, 1, "xor")
        g.putcells(newcells)
        
        return newcells
    end
    
    StartNewCA()

Author: Andrew Trevorrow (andrew@trevorrow.com), October 2019.
--]]

local g = golly()
-- require "gplus.strict"
local gp = require "gplus"
local op = require "oplus"
local split = gp.split
local int = gp.int
local ov = g.overlay
local ovt = g.ovtable
local rand = math.random

-- scripts must set these globals:

SCRIPT_NAME = nil       -- must match the name of the new .lua file
DEFAULT_RULE = nil      -- default rule string (must be valid)
RULE_HELP = nil         -- HTML data for ShowHelp
ParseRule = nil         -- function to validate given rule string
NextPattern = nil       -- function to calculate the next pattern

-- scripts might also want to access the following globals (eg. if redefining
-- functions like FlipPaste, RotatePaste, RandomFill, etc):

current_rule = ""               -- the current rule (set by CheckRule)
real_rule = ""                  -- the real rule used by the new layer's algo (set by CheckRule)
minx, miny, maxx, maxy = 0,0,0,0    -- cell coordinates of grid edges (set by CheckRule)
gridwd, gridht = 0, 0           -- current dimensions of grid (set by CheckRule)
pattname = "untitled"           -- initial pattern name
currtitle = ""                  -- current window title (set by UpdateWindowTitle)
currpattern = nil               -- cell array with current pattern
generating = false              -- generate pattern?
gencount = 0                    -- current generation count
stopgen = 0                     -- when to stop generating (if > 0)
stepsize = 1                    -- current step size
perc = 50                       -- initial percentage density for RandomPattern
drawstate = 1                   -- for drawing/erasing cells
viewwd, viewht = 0, 0           -- current size of viewport
ovwd, ovht = 0, 0               -- current size of overlay
minwd, minht = 920, 500         -- minimum size of overlay
mbar = nil                      -- the menu bar
mbarwd = 165                    -- width of menu bar
mbarht = 32                     -- height of menu bar (and tool bar)
buttonht = 20                   -- height of buttons in tool bar
hgap = 10                       -- horizontal space between buttons
vgap = 6                        -- vertical space above buttons
currbarht = mbarht              -- 0 if menu/tool bar is not shown (eg. fullscreen mode)
arrow_cursor = false            -- true if cursor is in menu/tool bar
pastecells = {}                 -- cell array with paste pattern
pastewd, pasteht = 0, 0         -- dimensions of paste pattern (in cells)
updatepaste = false             -- paste pattern needs to be updated?
pastemenu = nil                 -- pop-up menu for choosing a paste action
pastemode = "or"                -- initial paste mode
pastepos = "topleft"            -- initial position of mouse in paste pattern
pasteclip = "pasteclip"         -- clip with translucent paste pattern
modeclip = "modeclip"           -- clip with translucent paste mode
modeht = 0                      -- height of modeclip (in pixels)
alpha1 = "rgba 0 0 0 1"         -- area below tool bar will be mostly transparent

-- tool bar controls (set by CreateToolBar)
ssbutton = nil                  -- Start/Stop
s1button = nil                  -- +1
stepbutton = nil                -- Step
resetbutton = nil               -- Reset
undobutton = nil                -- Undo
redobutton = nil                -- Redo
rulebutton = nil                -- Rule...
helpbutton = nil                -- ?
exitbutton = nil                -- X
stepslider = nil                -- slider for adjusting stepsize

-- for undo/redo
undostack = {}                  -- stack of states that can be undone
redostack = {}                  -- stack of states that can be redone
startcount = 0                  -- starting gencount (can be > 0)
startstate = {}                 -- starting state (used by Reset)
dirty = false                   -- pattern has been modified?

pattdir = g.getdir("data")      -- initial directory for OpenPattern/SavePattern
scriptdir = g.getdir("app")     -- initial directory for RunScript
scriptlevel = 0                 -- greater than 0 if a user script is running

pathsep = g.getdir("app"):sub(-1)   -- "/" on Mac and Linux, "\" on Windows

-- the following paths depend on SCRIPT_NAME and so are initialized in SanityChecks
startup = ""                    -- path to the startup script
settingsfile = ""               -- path to the settings file

math.randomseed(os.time())      -- init seed for math.random

--------------------------------------------------------------------------------

function SetColors()
    g.setcolors( {0,40,40,80} )             -- dark blue background
    g.setcolors( {255,255,0, 255,0,0} )     -- live states vary from yellow to red
end

--------------------------------------------------------------------------------

function CheckRule(newrule)
    -- call ParseRule and if valid set current_rule, real_rule,
    -- gridwd, gridht, minx, miny, maxx, maxy,
    -- then call SetColors() and return nil;
    -- if not valid then return the error message from ParseRule
    
    local err, canonrule, wd, ht, numstates = ParseRule(newrule)
    if err then
        return err
    end
    
    -- wd, ht, numstates must be integers for setting real_rule
    wd = math.floor(wd)
    ht = math.floor(ht)
    numstates = math.floor(numstates)
    
    -- if any of these sanity checks fail we must exit because caller code needs to be fixed
    if #canonrule == 0 then
        g.exit("Canonical rule must not be empty!")
    elseif canonrule:find(" ") then
        g.exit("Canonical rule must not contain any spaces!")
    end
    if wd < 4 or wd > 4000 or
       ht < 4 or ht > 4000 then
        g.exit("Grid size must be from 4 to 4000!")
    end
    if numstates < 2 or numstates > 256 then
        g.exit("Number of states must be from 2 to 256!")
    end

    -- given rule is valid so set current_rule to its canonical form
    current_rule = canonrule

    -- set the real LtL rule for the desired number of states
    -- (note that it must be canonical for FixClipboardRule to work)
    if numstates > 2 then
        real_rule = "R1,C"..numstates..",M0,S1..1,B1..1,NM:T"..wd..","..ht
    else
        -- can't use C2 (not canonical)
        real_rule = "R1,C0,M0,S1..1,B1..1,NM:T"..wd..","..ht
    end
    g.setrule(real_rule)
    
    -- set grid dimensions and edge coordinates
    gridwd = wd
    gridht = ht
    minx = -(gridwd // 2)
    miny = -(gridht // 2)
    maxx = minx + gridwd - 1
    maxy = miny + gridht - 1
    
    SetColors()     -- color scheme might depend on g.numstates()
    
    return nil      -- success
end

--------------------------------------------------------------------------------

function NextGeneration()
    if g.empty() then
        StopGenerating()
        ShowError("All cells are dead.")
        Refresh()
        return
    end
    
    if gencount == startcount then
        -- remember starting state for later use in Reset()
        if scriptlevel > 0 then
            -- can't use undostack if user script is running
            startstate = SaveState()
        else
            -- starting state is on top of undostack
            startstate = undostack[#undostack]
        end
    end
    
    if currpattern == nil then
        -- currpattern needs to be updated
        currpattern = g.getcells( g.getrect() )
    end
    
    currpattern = NextPattern(currpattern, minx, miny, maxx, maxy)
    
    gencount = gencount + 1
    g.setgen(tostring(gencount))    -- for status bar
    
    if scriptlevel == 0 then
        if g.empty() or (stopgen > 0 and gencount == stopgen) then
            StopGenerating()
            Refresh()
        elseif stopgen == 0 and gencount % stepsize == 0 then
            Refresh()
        end
    end
end

--------------------------------------------------------------------------------

function ShowHelp()
    local htmldata = [[
<html><title>Golly Help: SCRIPT_NAME.lua</title>
<body bgcolor="#FFFFCE">

<p>
<dd><a href="#rules"><b>Supported rules</b></a></dd>
<dd><a href="#keyboard"><b>Keyboard shortcuts</b></a></dd>
<dd><a href="#menus"><b>Menus</b></a></dd>
<dd>&nbsp;&nbsp;&nbsp;&nbsp; <a href="#file"><b>File menu</b></a></dd>
<dd>&nbsp;&nbsp;&nbsp;&nbsp; <a href="#edit"><b>Edit menu</b></a></dd>
<dd>&nbsp;&nbsp;&nbsp;&nbsp; <a href="#view"><b>View menu</b></a></dd>
<dd><a href="#scripts"><b>Running scripts</b></a></dd>
<dd>&nbsp;&nbsp;&nbsp;&nbsp; <a href="#shortcuts"><b>Creating your own keyboard shortcuts</b></a></dd>
<dd>&nbsp;&nbsp;&nbsp;&nbsp; <a href="#functions"><b>Script functions</b></a></dd>
<dd><a href="#extras"><b>Extra features</b></a></dd>
</p>

<p><a name="rules"></a><br>
<font size=+1><b>Supported rules</b></font>

<p>
RULE_HELP

<p><a name="keyboard"></a><br>
<font size=+1><b>Keyboard shortcuts</b></font>

<p>
The following keyboard shortcuts are provided (but see <a href="#shortcuts">below</a>
for how you can write a script to create new shortcuts or override any of the supplied
shortcuts):

<p>
<center>
<table cellspacing=1 border=2 cols=2 width="90%">
<tr><td align=right> Keys &nbsp;</td><td>&nbsp; Actions </td></tr>
<tr><td align=right> ENTER &nbsp;</td><td>&nbsp; start/stop generating pattern </td></tr>
<tr><td align=right> space &nbsp;</td><td>&nbsp; advance pattern by one generation </td></tr>
<tr><td align=right> tab &nbsp;</td><td>&nbsp; advance pattern by the step size </td></tr>
<tr><td align=right> - &nbsp;</td><td>&nbsp; decrease step size </td></tr>
<tr><td align=right> = &nbsp;</td><td>&nbsp; increase step size </td></tr>
<tr><td align=right> CTRL-N &nbsp;</td><td>&nbsp; create a new, empty pattern </td></tr>
<tr><td align=right> CTRL-P &nbsp;</td><td>&nbsp; create a new, random pattern </td></tr>
<tr><td align=right> CTRL-5 &nbsp;</td><td>&nbsp; randomly fill the selection </td></tr>
<tr><td align=right> D &nbsp;</td><td>&nbsp; set the density for Random Fill/Pattern </td></tr>
<tr><td align=right> CTRL-O &nbsp;</td><td>&nbsp; open a selected pattern file </td></tr>
<tr><td align=right> CTRL-S &nbsp;</td><td>&nbsp; save the current pattern in a file </td></tr>
<tr><td align=right> shift-O &nbsp;</td><td>&nbsp; open pattern from clipboard </td></tr>
<tr><td align=right> shift-R &nbsp;</td><td>&nbsp; run script from clipboard </td></tr>
<tr><td align=right> CTRL-R &nbsp;</td><td>&nbsp; reset to the starting pattern </td></tr>
<tr><td align=right> Z &nbsp;</td><td>&nbsp; undo </td></tr>
<tr><td align=right> shift-Z &nbsp;</td><td>&nbsp; redo </td></tr>
<tr><td align=right> CTRL-X &nbsp;</td><td>&nbsp; cut the selection </td></tr>
<tr><td align=right> CTRL-C &nbsp;</td><td>&nbsp; copy the selection </td></tr>
<tr><td align=right> CTRL-V &nbsp;</td><td>&nbsp; paste pattern from clipboard </td></tr>
<tr><td align=right> delete &nbsp;</td><td>&nbsp; delete live cells inside selection </td></tr>
<tr><td align=right> shift-delete &nbsp;</td><td>&nbsp; delete live cells outside selection</td></tr>
<tr><td align=right> A &nbsp;</td><td>&nbsp; select all </td></tr>
<tr><td align=right> K &nbsp;</td><td>&nbsp; remove selection </td></tr>
<tr><td align=right> X &nbsp;</td><td>&nbsp; flip selection left-right</td></tr>
<tr><td align=right> Y &nbsp;</td><td>&nbsp; flip selection top-bottom</td></tr>
<tr><td align=right> &gt; &nbsp;</td><td>&nbsp; rotate selection clockwise</td></tr>
<tr><td align=right> &lt; &nbsp;</td><td>&nbsp; rotate selection anticlockwise</td></tr>
<tr><td align=right> F &nbsp;</td><td>&nbsp; fit pattern within view </td></tr>
<tr><td align=right> shift-F &nbsp;</td><td>&nbsp; fit selection within view </td></tr>
<tr><td align=right> T &nbsp;</td><td>&nbsp; toggle the tool bar </td></tr>
<tr><td align=right> R &nbsp;</td><td>&nbsp; change the rule </td></tr>
<tr><td align=right> M &nbsp;</td><td>&nbsp; move cell at 0,0 to middle of view </td></tr>
<tr><td align=right> H &nbsp;</td><td>&nbsp; show this help </td></tr>
<tr><td align=right> Q &nbsp;</td><td>&nbsp; quit SCRIPT_NAME.lua </td></tr>
</table>
</center>

<p><a name="menus"></a><br>
<font size=+1><b>Menus</b></font>

<p>
SCRIPT_NAME.lua has its own menu bar.  It contains menus with items that are
somewhat similar to those in Golly's menu bar.

<p><a name="file"></a><br>
<font size=+1><b>File menu</b></font>

<a name="new"></a><p><dt><b>New Pattern</b></dt>
<dd>
Create a new, empty pattern.
All undo/redo history is deleted and the step size is reset to 1.
</dd>

<a name="rand"></a><p><dt><b>Random Pattern</b></dt>
<dd>
Create a new pattern randomly filled with live cells using the density
set by the most recent "Set Density..." command (in the Edit menu).
All undo/redo history is deleted and the step size is reset to 1.
</dd>

<a name="open"></a><p><dt><b>Open Pattern...</b></dt>
<dd>
Open a selected pattern file.
All undo/redo history is deleted and the step size is reset to 1.
</dd>

<a name="openclip"></a><p><dt><b>Open Clipboard</b></dt>
<dd>
Open the pattern stored in the clipboard.
All undo/redo history is deleted and the step size is reset to 1.
</dd>

<a name="save"></a><p><dt><b>Save Pattern...</b></dt>
<dd>
Save the current pattern in a file.
</dd>

<a name="run"></a><p><dt><b>Run Script...</b></dt>
<dd>
Run a selected Lua script.
</dd>

<a name="runclip"></a><p><dt><b>Run Clipboard</b></dt>
<dd>
Run the Lua code stored in the clipboard.
</dd>

<a name="startup"></a><p><dt><b>Set Startup Script...</b></dt>
<dd>
Select a Lua script that will be run automatically every time SCRIPT_NAME.lua starts up.
</dd>

<a name="exit"></a><p><dt><b>Exit SCRIPT_NAME.lua</b></dt>
<dd>
Terminate SCRIPT_NAME.lua.  If there are any unsaved changes (indicated by an asterisk at
the start of the pattern name) then you'll be asked if you want to save them.
</dd>

<p><a name="edit"></a><br>
<font size=+1><b>Edit menu</b></font>

<a name="undo"></a><p><dt><b>Undo</b></dt>
<dd>
Undo the most recent change.  This could be an editing change or a generating change.
</dd>

<a name="redo"></a><p><dt><b>Redo</b></dt>
<dd>
Redo the most recently undone change.
</dd>

<a name="cut"></a><p><dt><b>Cut</b></dt>
<dd>
Copy the selection to the clipboard in RLE format,
then delete the cells inside the selection.
</dd>

<a name="copy"></a><p><dt><b>Copy</b></dt>
<dd>
Copy the selection to the clipboard in RLE format.
</dd>

<a name="paste"></a><p><dt><b>Paste</b></dt>
<dd>
If the clipboard contains a valid RLE pattern then a translucent paste pattern will appear.
Just like Golly, you can use various keys:
"X" to flip the paste pattern left-to-right, "Y" to flip top-to-bottom,
"&gt;" to rotate clockwise, "&lt;" to rotate anticlockwise,
"shift-M" to cycle the paste mode,
"shift-L" to cycle the location of the mouse within the paste pattern.
Click when you want to do the paste, or hit escape to cancel it.
Unlike Golly, you can also control-click or right-click to see a pop-up menu with
various paste actions.
</dd>

<a name="clear"></a><p><dt><b>Clear</b></dt>
<dd>
Delete all the live cells that are inside the selection.
</dd>

<a name="outside"></a><p><dt><b>Clear Outside</b></dt>
<dd>
Delete all the live cells that are outside the selection.
</dd>

<a name="selall"></a><p><dt><b>Select All</b></dt>
<dd>
Select the entire pattern, assuming there are one or more live cells.
If there are no live cells then any existing selection is removed.
</dd>

<a name="cancelsel"></a><p><dt><b>Remove Selection</b></dt>
<dd>
Remove the selection.
</dd>

<a name="shrink"></a><p><dt><b>Shrink Selection</b></dt>
<dd>
Reduce the size of the current selection to the smallest rectangle
containing all of the selection's live cells.
</dd>

<a name="flipy"></a><p><dt><b>Flip Top-Bottom</b></dt>
<dd>
The cells in the current selection are reflected about its
central horizontal axis.
</dd>

<a name="flipx"></a><p><dt><b>Flip Left-Right</b></dt>
<dd>
The cells in the current selection are reflected about its
central vertical axis.
</dd>

<a name="rotate"></a><p><dt><b>Rotate Clockwise</b></dt>
<dd>
The current selection is rotated 90 degrees clockwise about its center.
</dd>

<a name="rotatea"></a><p><dt><b>Rotate Anticlockwise</b></dt>
<dd>
As above, but the rotation is anticlockwise.
</dd>

<a name="randfill"></a><p><dt><b>Random Fill</b></dt>
<dd>
Randomly fill the selection using the density
set by the most recent "Set Density..." command.
</dd>

<a name="density"></a><p><dt><b>Set Density...</b></dt>
<dd>
Set the percentage density for Random Fill and Random Pattern.
</dd>

<p><a name="view"></a><br>
<font size=+1><b>View menu</b></font>

<a name="fit"></a><p><dt><b>Fit Grid</b></dt>
<dd>
Fit the entire grid so it just fits within the middle of the viewport.
</dd>

<a name="fit"></a><p><dt><b>Fit Pattern</b></dt>
<dd>
Fit the pattern so it just fits within the middle of the viewport.
</dd>

<a name="fitsel"></a><p><dt><b>Fit Selection</b></dt>
<dd>
Fit the selection so it just fits within the middle of the viewport.
</dd>

<a name="middle"></a><p><dt><b>Middle</b></dt>
<dd>
Change the location so the cell at 0,0 is in the middle of the viewport.
</dd>

<a name="help"></a><p><dt><b>Help</b></dt>
<dd>
Show this help.
</dd>

<p><a name="scripts"></a><br>
<font size=+1><b>Running scripts</b></font>

<p>
SCRIPT_NAME.lua can run other Lua scripts, either by selecting File > Run Script
and choosing a .lua file, or by copying Lua code to the clipboard and
selecting File > Run Clipboard.  Try the latter method with this example
which creates a small random pattern:

<dd><table border=0><pre>
-- for SCRIPT_NAME.lua (make sure you copy this line)
local g = golly()
NewPattern("random pattern")
for y = 0, 19 do
    for x = 0, 19 do
        if math.random(0,99) &lt; 50 then
            g.setcell(x, y, 1)
        end
    end
end
FitPattern()
g.setcursor("Move") -- sets the hand cursor</pre></table></dd>

<p>
Note that SCRIPT_NAME.lua will only run a script if the clipboard or the file
contains "SCRIPT_NAME.lua" or "NewCA.lua" somewhere in the first line.
This avoids nasty problems that can occur if you run a script not written
for SCRIPT_NAME.lua or NewCA.lua.

<p>
Any syntax or runtime errors in a script won't abort SCRIPT_NAME.lua.
The script will terminate and you'll get a warning message, hopefully
with enough information that lets you fix the error.

<p><a name="shortcuts"></a><br>
<font size=+1><b>Creating your own keyboard shortcuts</b></font>

<p>
SCRIPT_NAME.lua relies on many global functions defined in NewCA.lua.
It's possible to override any of these functions in your own scripts.
The following example shows how to override the HandleKey function
to create a keyboard shortcut for running a particular script.
You can get SCRIPT_NAME.lua to run this script automatically
when it starts up by going to File > Set Startup Script and selecting
a .lua file containing this code:

<dd><table border=0><pre>
-- a startup script for SCRIPT_NAME.lua
local g = golly()
local gp = require "gplus"
local savedHandler = HandleKey
function HandleKey(event)
    local _, key, mods = gp.split(event)
    if key == "o" and mods == "alt" then
        RunScript(g.getdir("app").."My-scripts/SCRIPT_NAME/oscar.lua")
    else
        -- pass the event to the original HandleKey function
        savedHandler(event)
    end
end</pre></table></dd>

<p>
Note that a startup script called SCRIPT_NAME-start.lua and stored in
an application sub-folder called My-scripts will be run automatically
(ie. no need to select it via File > Set Startup Script).

<p><a name="functions"></a><br>
<font size=+1><b>Script functions</b></font>

<p>
Here is an alphabetical list of some of the global functions in NewCA.lua
you might want to call from your own scripts:

<p>
<dd>
<table cellspacing=0 cellpadding=0>
<tr>
<td valign=top>
<a href="#CheckCursor"><b>CheckCursor</b></a><br>
<a href="#CheckWindowSize"><b>CheckWindowSize</b></a><br>
<a href="#Exit"><b>Exit</b></a><br>
<a href="#FitGrid"><b>FitGrid</b></a><br>
<a href="#FitPattern"><b>FitPattern</b></a><br>
<a href="#FitSelection"><b>FitSelection</b></a><br>
<a href="#GetBarHeight"><b>GetBarHeight</b></a>
</td>
<td valign=top width=30> </td>
<td valign=top>
<a href="#GetDensity"><b>GetDensity</b></a><br>
<a href="#GetGenCount"><b>GetGenCount</b></a><br>
<a href="#GetRule"><b>GetRule</b></a><br>
<a href="#GetStepSize"><b>GetStepSize</b></a><br>
<a href="#HandleKey"><b>HandleKey</b></a><br>
<a href="#HandlePasteKey"><b>HandlePasteKey</b></a><br>
<a href="#NewPattern"><b>NewPattern</b></a>
</td>
<td valign=top width=30> </td>
<td valign=top>
<a href="#OpenPattern"><b>OpenPattern</b></a><br>
<a href="#RandomPattern"><b>RandomPattern</b></a><br>
<a href="#Reset"><b>Reset</b></a><br>
<a href="#RestoreState"><b>RestoreState</b></a><br>
<a href="#RunScript"><b>RunScript</b></a><br>
<a href="#SavePattern"><b>SavePattern</b></a><br>
<a href="#SaveState"><b>SaveState</b></a>
</td>
<td valign=top width=30> </td>
<td valign=top>
<a href="#SetColors"><b>SetColors</b></a><br>
<a href="#SetDensity"><b>SetDensity</b></a><br>
<a href="#SetRule"><b>SetRule</b></a><br>
<a href="#SetStepSize"><b>SetStepSize</b></a><br>
<a href="#Step"><b>Step</b></a><br>
<a href="#Update"><b>Update</b></a>
</td>
</tr>
</table>
</dd>
</p>

<a name="CheckCursor"></a><p><dt><b>CheckCursor()</b></dt>
<dd>
Changes the cursor to an arrow if the mouse moves over the tool bar.
Useful in scripts that allow user interaction.
</dd>

<a name="CheckWindowSize"></a><p><dt><b>CheckWindowSize()</b></dt>
<dd>
If the Golly window size has changed then this function resizes the overlay.
Useful in scripts that allow user interaction.
</dd>

<a name="Exit"></a><p><dt><b>Exit(<i>message</i>)</b></dt>
<dd>
Display the given message in the status bar and exit the script.
(Calling g.exit will also exit the script but its message will be overwritten
by "Script aborted".)
</dd>

<a name="FitGrid"></a><p><dt><b>FitGrid()</b></dt>
<dd>
Fit the entire grid so it just fits within the middle of the viewport.
</dd>

<a name="FitPattern"></a><p><dt><b>FitPattern()</b></dt>
<dd>
Fit the pattern so it just fits within the middle of the viewport.
</dd>

<a name="FitSelection"></a><p><dt><b>FitSelection()</b></dt>
<dd>
Fit the selection so it just fits within the middle of the viewport.
</dd>

<a name="GetBarHeight"></a><p><dt><b>GetBarHeight()</b></dt>
<dd>
Return the height of the tool bar.
The value will be 0 if the user has turned them off (by hitting the "T" key)
or switched to full screen mode.
</dd>

<a name="GetDensity"></a><p><dt><b>GetDensity()</b></dt>
<dd>
Return the current density used by Random Pattern and Random Fill
as a percentage from 1 to 100.
</dd>

<a name="GetGenCount"></a><p><dt><b>GetGenCount()</b></dt>
<dd>
Return the current generation count.
</dd>

<a name="GetRule"></a><p><dt><b>GetRule()</b></dt>
<dd>
Return the current rule.
WARNING: Do not use g.getrule()!  It returns the rule used by the underlying layer.
</dd>

<a name="GetStepSize"></a><p><dt><b>GetStepSize()</b></dt>
<dd>
Return the tool bar's current step size (a number from 1 to 100).
</dd>

<a name="HandleKey"></a><p><dt><b>HandleKey(<i>event</i>)</b></dt>
<dd>
Process the given keyboard event.
A startup script might want to modify how such events are handled.
</dd>

<a name="HandlePasteKey"></a><p><dt><b>HandlePasteKey(<i>event</i>)</b></dt>
<dd>
Process the given keyboard event while waiting for a paste click.
A startup script might want to modify these events.
</dd>

<a name="NewPattern"></a><p><dt><b>NewPattern(<i>title</i>)</b></dt>
<dd>
Create a new, empty pattern.
All undo/redo history is deleted and the step size is reset to 1.
The given title string will appear in the title bar of the Golly window.
If not supplied it is set to "untitled".
</dd>

<a name="OpenPattern"></a><p><dt><b>OpenPattern(<i>filepath</i>)</b></dt>
<dd>
Open the specified pattern file.  If the <i>filepath</i> is not supplied then
the user will be prompted to select a file.
All undo/redo history is deleted and the step size is reset to 1.
</dd>

<a name="RandomPattern"></a><p><dt><b>RandomPattern(<i>percentage, wd, ht</i>)</b></dt>
<dd>
Create a new, random pattern of the given width and height (100 if not supplied)
and with the given percentage density (1 to 100) of live cells.
All undo/redo history is deleted and the step size is reset to 1.
</dd>

<a name="Reset"></a><p><dt><b>Reset()</b></dt>
<dd>
Restore the starting generation and step size.
</dd>

<a name="RestoreState"></a><p><dt><b>RestoreState(<i>state</i>)</b></dt>
<dd>
Restore the state saved earlier by <a href="#SaveState">SaveState</a>.
</dd>

<a name="RunScript"></a><p><dt><b>RunScript(<i>filepath</i>)</b></dt>
<dd>
Run the specified .lua file, but only if "SCRIPT_NAME.lua" or "NewCA.lua"
occurs somewhere in a comment on the first line of the file.
If the <i>filepath</i> is not supplied then the user will be prompted
to select a .lua file.
</dd>

<a name="SavePattern"></a><p><dt><b>SavePattern(<i>filepath</i>)</b></dt>
<dd>
Save the current pattern in a specified RLE file.  If the <i>filepath</i>
is not supplied then the user will be prompted for its name and location.
</dd>

<a name="SaveState"></a><p><dt><b>SaveState()</b></dt>
<dd>
Return an object representing the current state.  The object can be given
later to <a href="#RestoreState">RestoreState</a> to restore the saved state.
The saved information includes the cursor mode, the rule, the pattern,
the generation count, the step size, and the selection.
</dd>

<a name="SetColors"></a><p><dt><b>SetColors()</b></dt>
<dd>
Called in various places to set the colors for each state.
A startup script can override this function to change the colors.
</dd>

<a name="SetDensity"></a><p><dt><b>SetDensity(<i>n</i>)</b></dt>
<dd>
Set the density used by Random Pattern and Random Fill.
The given number is a percentage from 1 to 100.
If not supplied then the user will be prompted to enter a value.
</dd>

<a name="SetRule"></a><p><dt><b>SetRule(<i>rule</i>)</b></dt>
<dd>
Switch to the given rule.  If <i>rule</i> is not supplied then the default rule is used.
WARNING: Do not use g.setrule()!  That will set the rule for the underlying layer.
</dd>

<a name="SetStepSize"></a><p><dt><b>SetStepSize(<i>n</i>)</b></dt>
<dd>
Set the tool bar's step size to a number from 1 to 100.
</dd>

<a name="Step"></a><p><dt><b>Step(<i>n</i>)</b></dt>
<dd>
While the population is &gt; 0 calculate the next <i>n</i> generations.
If <i>n</i> is not supplied it defaults to 1.
</dd>

<a name="Update"></a><p><dt><b>Update()</b></dt>
<dd>
Update everything, including the viewport, the status bar, the edit bar,
the tool bar, and the window's title bar.
Note that g.update() only updates the viewport and the status bar.
Update() is automatically called when a script finishes,
so there's no need to call it at the end of a script.
</dd>

<p>
Note that most of the functions and variables in NewCA.lua are global,
so it's possible to redefine these functions or use these variables
in various ways.  This allows a lot of flexibility but requires caution
and a good understanding of how they are used.

<p><a name="extras"></a><br>
<font size=+1><b>Extra features</b></font>

<p>
NewCA.lua provides a couple of additional features not available in Golly:

<p>
<ul>
<li>
Drawing is allowed at scales up to 2^3:1.
<li>
When doing a paste you can control-click or right-click to get a pop-up menu
with various paste actions.
</ul>

</body></html>
]]

    htmldata = htmldata:gsub("SCRIPT_NAME", SCRIPT_NAME)
    htmldata = htmldata:gsub("RULE_HELP", RULE_HELP, 1)
    
    if g.os() == "Mac" then
        htmldata = htmldata:gsub("ENTER", "return")
        htmldata = htmldata:gsub("ALT", "option")
        htmldata = htmldata:gsub("CTRL", "cmd")
    else
        htmldata = htmldata:gsub("ENTER", "enter")
        htmldata = htmldata:gsub("ALT", "alt")
        htmldata = htmldata:gsub("CTRL", "ctrl")
    end

    local htmlfile = g.getdir("temp")..SCRIPT_NAME..".html"
    local f = io.open(htmlfile,"w")
    if not f then
        g.warn("Failed to create html file!", false)
        return
    end
    f:write(htmldata)
    f:close()

    g.open(htmlfile)
end

--------------------------------------------------------------------------------

function SanityChecks()
    -- ensure caller has defined the necessary strings and functions
    if type(SCRIPT_NAME) ~= "string" then
        g.warn("You need to set SCRIPT_NAME to the name of your script!")
        g.exit()
    end
    if type(DEFAULT_RULE) ~= "string" then
        g.warn("You need to set DEFAULT_RULE to a valid rule string!")
        g.exit()
    end
    if type(RULE_HELP) ~= "string" then
        g.warn("You need to set RULE_HELP to HTML data describing the supported rules!")
        g.exit()
    end
    if type(ParseRule) ~= "function" then
        g.warn("You need to write a ParseRule function!")
        g.exit()
    end
    if type(NextPattern) ~= "function" then
        g.warn("You need to write a NextPattern function!")
        g.exit()
    end

    -- now set the paths that depend on SCRIPT_NAME
    startup = g.getdir("app").."My-scripts"..pathsep..SCRIPT_NAME.."-start.lua"
    settingsfile = g.getdir("data")..SCRIPT_NAME..".ini"
    
    -- initialize current_rule for very 1st call (validated later in Main)
    current_rule = DEFAULT_RULE
end

--------------------------------------------------------------------------------

function AddNewLayer()
    -- create a new layer and set the algo to Larger than Life because it
    -- has faster g.getcell/setcell calls
    if g.numlayers() < g.maxlayers() then
        g.addlayer()
        g.setalgo("Larger than Life")
        -- note that this must be done BEFORE ReadSettings calls CheckRule
    else
        g.exit("Could not create a new layer!")
    end
end

--------------------------------------------------------------------------------

function ReadSettings()
    local f = io.open(settingsfile, "r")
    if f then
        while true do
            -- no need to worry about CRs here because file was created by WriteSettings
            local line = f:read("*l")
            if not line then break end
            local keyword, value = split(line,"=")
            -- look for a keyword used in WriteSettings
            if not value then
                -- ignore keyword
            elseif keyword == "rule" then current_rule = value  -- validate later in Main
            elseif keyword == "perc" then perc = tonumber(value)
            elseif keyword == "startup" then startup = value
            elseif keyword == "pattdir" then pattdir = value
            elseif keyword == "scriptdir" then scriptdir = value
            elseif keyword == "pastemode" then pastemode = value
            elseif keyword == "pastepos" then pastepos = value
            end
        end
        f:close()
    end
end

--------------------------------------------------------------------------------

function WriteSettings()
    local f = io.open(settingsfile, "w")
    if f then
        -- keywords must match those in ReadSettings (but order doesn't matter)
        f:write("rule=", current_rule, "\n")
        f:write("perc=", tostring(perc), "\n")
        f:write("startup=", startup, "\n")
        f:write("pattdir=", pattdir, "\n")
        f:write("scriptdir=", scriptdir, "\n")
        f:write("pastemode=", pastemode, "\n")
        f:write("pastepos=", pastepos, "\n")
        f:close()
    end
end

--------------------------------------------------------------------------------

function SaveGollyState()
    local oldstate = {}
    oldstate.status = g.setoption("showstatusbar", 1)   -- show status bar
    oldstate.edit = g.setoption("showeditbar", 1)       -- show edit bar
    oldstate.time = g.setoption("showtimeline", 0)      -- hide timeline bar
    oldstate.tool = g.setoption("showtoolbar", 0)       -- hide tool bar
    oldstate.layer = g.setoption("showlayerbar", 0)     -- hide layer bar
    oldstate.buttons = g.setoption("showbuttons", 0)    -- hide translucent buttons
    oldstate.tile = g.setoption("tilelayers", 0)        -- don't tile layers
    oldstate.stack = g.setoption("stacklayers", 0)      -- don't stack layers
    oldstate.files = g.setoption("showfiles", 0)        -- hide file panel
    oldstate.filesdir = g.getdir("files")               -- save file directory
    -- save colors of grid border and status bar
    oldstate.br, oldstate.bg, oldstate.bb = g.getcolor("border")
    oldstate.sr, oldstate.sg, oldstate.sb = g.getcolor(g.getalgo())
    return oldstate
end

--------------------------------------------------------------------------------

function RestoreGollyState(oldstate)
    -- restore settings saved by SaveGollyState
    g.setoption("showstatusbar", oldstate.status)
    g.setoption("showeditbar", oldstate.edit)
    g.setoption("showtimeline", oldstate.time)
    g.setoption("showtoolbar", oldstate.tool)
    g.setoption("showlayerbar", oldstate.layer)
    g.setoption("showbuttons", oldstate.buttons)
    g.setoption("tilelayers", oldstate.tile)
    g.setoption("stacklayers", oldstate.stack)
    g.setoption("showfiles", oldstate.files)
    g.setdir("files", oldstate.filesdir)
    g.setcolor("border", oldstate.br, oldstate.bg, oldstate.bb)
    g.setcolor(g.getalgo(), oldstate.sr, oldstate.sg, oldstate.sb)
    
    -- delete the overlay and the layer added by AddNewLayer
    ov("delete")
    g.dellayer()
end

--------------------------------------------------------------------------------

function EnableControls(bool)
    -- disable/enable unsafe menu items so user scripts can call op.process
    -- File menu:
    mbar.enableitem(1, 1, bool)     -- New Pattern
    mbar.enableitem(1, 2, bool)     -- Random Pattern
    mbar.enableitem(1, 3, bool)     -- Open Pattern
    mbar.enableitem(1, 4, bool)     -- Open Clipboard
    mbar.enableitem(1, 5, bool)     -- Save Pattern
    mbar.enableitem(1, 7, bool)     -- Run Script
    mbar.enableitem(1, 8, bool)     -- Run Clipboard
    mbar.enableitem(1, 9, bool)     -- Set Startup Script
    if bool then
        -- g.exit will abort SCRIPT_NAME.lua
        mbar.setitem(1, 11, "Exit "..SCRIPT_NAME..".lua")
    else
        -- g.exit will abort the user script
        mbar.setitem(1, 11, "Exit Script")
    end
    -- Edit menu:
    mbar.enableitem(2, 1, bool)     -- Undo
    mbar.enableitem(2, 2, bool)     -- Redo
    mbar.enableitem(2, 4, bool)     -- Cut
    mbar.enableitem(2, 5, bool)     -- Copy
    mbar.enableitem(2, 6, bool)     -- Paste
    mbar.enableitem(2, 7, bool)     -- Clear
    mbar.enableitem(2, 8, bool)     -- Clear Outside
    mbar.enableitem(2, 10, bool)    -- Select All
    mbar.enableitem(2, 11, bool)    -- Remove Selection
    mbar.enableitem(2, 12, bool)    -- Shrink Selection
    mbar.enableitem(2, 13, bool)    -- Flip Top-Bottom
    mbar.enableitem(2, 14, bool)    -- Flip Left-Right
    mbar.enableitem(2, 15, bool)    -- Rotate Clockwise
    mbar.enableitem(2, 16, bool)    -- Rotate Anticlockwise
    mbar.enableitem(2, 17, bool)    -- Random Fill
    mbar.enableitem(2, 18, bool)    -- Set Density...
    -- View menu:
    mbar.enableitem(3, 1, bool)     -- Fit Grid
    mbar.enableitem(3, 2, bool)     -- Fit Pattern
    mbar.enableitem(3, 3, bool)     -- Fit Selection
    mbar.enableitem(3, 4, bool)     -- Middle

    -- disable/enable unsafe buttons
    ssbutton.enable(bool)
    s1button.enable(bool)
    stepbutton.enable(bool)
    resetbutton.enable(bool)
    undobutton.enable(bool)
    redobutton.enable(bool)
    rulebutton.enable(bool)

    if currbarht > 0 and not bool then
        -- user script is about to be run so avoid enabling menu items and buttons
        DrawMenuBar(false)
        DrawToolBar(false)
        g.update()
    end
end

--------------------------------------------------------------------------------

function DrawMenuBar(update)
    if update then
        -- enable/disable some menu items
        local selexists = #g.getselrect() > 0
        -- Edit menu:
        mbar.enableitem(2, 1, #undostack > 0)   -- Undo
        mbar.enableitem(2, 2, #redostack > 0)   -- Redo
        mbar.enableitem(2, 4, selexists)        -- Cut
        mbar.enableitem(2, 5, selexists)        -- Copy
        mbar.enableitem(2, 7, selexists)        -- Clear
        mbar.enableitem(2, 8, selexists)        -- Clear Outside
        mbar.enableitem(2, 10, not g.empty())   -- Select All
        mbar.enableitem(2, 11, selexists)       -- Remove Selection
        mbar.enableitem(2, 12, selexists)       -- Shrink Selection
        mbar.enableitem(2, 13, selexists)       -- Flip Top-Bottom
        mbar.enableitem(2, 14, selexists)       -- Flip Left-Right
        mbar.enableitem(2, 15, selexists)       -- Rotate Clockwise
        mbar.enableitem(2, 16, selexists)       -- Rotate Anticlockwise
        mbar.enableitem(2, 17, selexists)       -- Random Fill
        -- View menu:
        mbar.enableitem(3, 3, selexists)        -- Fit Selection
    end
    
    -- draw menu bar in top left corner of layer
    mbar.show(0, 0, mbarwd, mbarht)
end

--------------------------------------------------------------------------------

function DrawToolBar(update)
    -- draw tool bar to right of menu bar
    ov(op.menubg)
    ovt{"fill", mbarwd, 0, ovwd - mbarwd, mbarht}

    if update then
        -- enable/disable some buttons
        ssbutton.enable(not g.empty())
        s1button.enable(not g.empty())
        stepbutton.enable(not g.empty())
        resetbutton.enable(gencount > startcount)
        undobutton.enable(#undostack > 0)
        redobutton.enable(#redostack > 0)
    end

    local x = mbarwd + hgap
    local y = vgap
    local biggap = hgap * 3

    ssbutton.show(x, y)
    x = x + ssbutton.wd + hgap
    s1button.show(x, y)
    x = x + s1button.wd + hgap
    stepbutton.show(x, y)
    x = x + stepbutton.wd + hgap
    resetbutton.show(x, y)
    x = x + resetbutton.wd + biggap
    undobutton.show(x, y)
    x = x + undobutton.wd + hgap
    redobutton.show(x, y)
    x = x + redobutton.wd + biggap
    rulebutton.show(x, y)

    -- show slider to right of rule button
    stepslider.show(x + rulebutton.wd + biggap, y, stepsize)
    -- show stepsize at right end of slider
    op.pastetext(stepslider.x + stepslider.wd + 2, y + 1, op.identity, "stepclip")

    -- last 2 buttons are at right end of tool bar
    x = ovwd - hgap - exitbutton.wd
    exitbutton.show(x, y)
    x = x - hgap - helpbutton.wd
    helpbutton.show(x, y)
end

--------------------------------------------------------------------------------

function UpdateWindowTitle()
    -- set window title if it has changed
    local newtitle = string.format("%s [%s]", pattname, current_rule)
    if dirty then newtitle = "*"..newtitle end
    if g.os() ~= "Mac" then newtitle = newtitle.." - Golly" end
    if newtitle ~= currtitle then
        g.settitle(newtitle)
        currtitle = newtitle
    end
end

--------------------------------------------------------------------------------

function UpdateEditBar()
    if g.getoption("showeditbar") > 0 then
        -- force edit bar to update
        g.setcursor( g.getcursor() )
    end
end

--------------------------------------------------------------------------------

function Refresh(update)
    if scriptlevel > 0 and not update then
        -- user scripts need to call Update() when they want to refresh everything
        -- (calling g.update() will only refresh the pattern and status bar)
        return
    end

    if currbarht > 0 then
        DrawMenuBar(true)
        DrawToolBar(true)
    end
    
    UpdateEditBar()    
    UpdateWindowTitle()
    g.update()
end

----------------------------------------------------------------------

-- for user scripts
function Update()
    Refresh(true)
end

--------------------------------------------------------------------------------

function UpdateStartButton()
    -- change label in ssbutton without changing the button's width
    if generating then
        ssbutton.customcolor = "rgba 210 0 0 255"
        ssbutton.darkcustomcolor = "rgba 150 0 0 255"
        ssbutton.setlabel("Stop", false)
    else
        ssbutton.customcolor = "rgba 0 150 0 255"
        ssbutton.darkcustomcolor = "rgba 0 90 0 255"
        ssbutton.setlabel("Start", false)
    end
end

--------------------------------------------------------------------------------

function StopGenerating()
    if generating then
        generating = false
        UpdateStartButton()
    end
end

--------------------------------------------------------------------------------

function SaveState()
    -- return a table containing current state
    local state = {}

    -- save current rule
    state.saverule = current_rule

    -- save current pattern
    state.savecells = g.getcells( g.getrect() )
    state.savedirty = dirty
    state.savename = pattname
    state.savegencount = gencount

    -- save current selection
    state.savesel = g.getselrect()

    -- save current cursor
    state.savecursor = g.getcursor()

    -- save current step size
    state.savestep = stepsize
    
    -- save current position and scale
    state.savex, state.savey = g.getpos()
    state.savemag = g.getmag()

    return state
end

--------------------------------------------------------------------------------

function RestoreState(state)
    -- restore state from given info (created earlier by SaveState)

    -- restore rule if necessary
    if current_rule ~= state.saverule then
        CheckRule(state.saverule)
    end

    -- restore pattern
    if not g.empty() then
        g.new("")
    end
    g.putcells(state.savecells)
    dirty = state.savedirty
    pattname = state.savename
    gencount = state.savegencount
    g.setgen(tostring(gencount))

    -- call SetColors AFTER restoring gencount in case caller has overridden it
    -- to use different colors depending on gencount
    SetColors()

    -- restore selection
    g.select(state.savesel)

    -- restore cursor
    g.setcursor(state.savecursor)

    -- restore step size
    SetStepSize(state.savestep)
    
    -- restore position and scale
    g.setpos(state.savex, state.savey)
    g.setmag(state.savemag)
end

--------------------------------------------------------------------------------

function SameState(state)
    -- return true if given state matches the current state
    if g.getcursor() ~= state.savecursor then return false end
    if current_rule ~= state.saverule then return false end
    if dirty ~= state.savedirty then return false end
    if pattname ~= state.savename then return false end
    if gencount ~= state.savegencount then return false end

    if not gp.equal(g.getcells(g.getrect()), state.savecells) then return false end
    if not gp.equal(g.getselrect(), state.savesel) then return false end

    -- note that we don't check state.savestep, state.savex, state.savey or state.savemag
    -- (we don't call RememberCurrentState when the user changes the step size, position or scale)

    return true
end

--------------------------------------------------------------------------------

function ClearUndoRedo()
    -- this might be called if a user script is running (eg. if it calls NewPattern)
    undostack = {}
    redostack = {}
    dirty = false
end

--------------------------------------------------------------------------------

function Undo()
    -- ignore if user script is running
    -- (scripts can call SaveState and RestoreState if they need to undo stuff)
    if scriptlevel > 0 then return end

    if #undostack > 0 then
        StopGenerating()
        -- push current state onto redostack
        redostack[#redostack+1] = SaveState()
        -- pop state off undostack and restore it
        RestoreState( table.remove(undostack) )
        Refresh()
    end
end

--------------------------------------------------------------------------------

function Redo()
    -- ignore if user script is running
    if scriptlevel > 0 then return end

    if #redostack > 0 then
        StopGenerating()
        -- push current state onto undostack
        undostack[#undostack+1] = SaveState()
        -- pop state off redostack and restore it
        RestoreState( table.remove(redostack) )
        Refresh()
    end
end

--------------------------------------------------------------------------------

function RememberCurrentState()
    -- ignore if user script is running
    if scriptlevel > 0 then return end

    redostack = {}
    undostack[#undostack+1] = SaveState()
    
    -- pattern might be about to change so tell NextGeneration that currpattern needs to be updated
    currpattern = nil
end

--------------------------------------------------------------------------------

function ShowMessage(msg)
    -- don't show msg if user script is running or status bar is hidden
    if scriptlevel > 0 or g.getoption("showstatusbar") == 0 then return end
    
    g.show(msg)
end

--------------------------------------------------------------------------------

function ShowError(msg)
    -- don't show msg if user script is running
    if scriptlevel > 0 then return end
    
    -- best to show status bar so user sees error
    g.setoption("showstatusbar",1)
    
    g.error(msg)    -- same as g.show but with a beep
end

--------------------------------------------------------------------------------

function CheckIfGenerating()
    if generating and not g.empty() and gencount > startcount then
        -- NextGeneration will be called soon
        RememberCurrentState()
    end
end

--------------------------------------------------------------------------------

function AdjustPosition()
    if currbarht > 0 then
        -- adjust vertical position to allow for tool bar
        local barcells
        if g.getmag() >= 0 then
            barcells = currbarht // int(2^g.getmag())
        else
            barcells = currbarht * int(2^-g.getmag())
        end
        local x, y = g.getpos()
        y = tonumber(y) - (barcells+1)//2
        if y < miny then y = miny end
        if y > maxy then y = maxy end
        g.setpos(x, tostring(y))
    end
end

--------------------------------------------------------------------------------

function FitGrid()
    local oldsel = g.getselrect()
    g.select( {minx, miny, gridwd, gridht} )
    g.fitsel()
    g.select(oldsel)
    AdjustPosition()
    Refresh()
end

--------------------------------------------------------------------------------

function FitPattern()
    g.fit()
    AdjustPosition()
    Refresh()
end

--------------------------------------------------------------------------------

function FitSelection()
    if #g.getselrect() > 0 then
        g.fitsel()
        AdjustPosition()
        Refresh()
    end
end

--------------------------------------------------------------------------------

function MiddleView()
    g.setpos("0","0")
    AdjustPosition()
    Refresh()
end

--------------------------------------------------------------------------------

function NewPattern(title)
    pattname = title or "untitled"
    g.new(pattname)
    g.setcursor("Draw")
    gencount = 0
    startcount = 0
    SetStepSize(1)
    SetColors()
    StopGenerating()
    ClearUndoRedo()
    FitPattern()    -- calls Refresh
end

--------------------------------------------------------------------------------

function ReadPattern(filepath, source)
    local f = io.open(filepath,"r")
    if not f then
        return "Failed to open file:\n"..filepath
    end
    local contents = f:read("*a")
    f:close()
    
    -- check that the file contains a valid RLE header line
    local wd, ht, rule = contents:match("x[= ]+(%d+)[, ]+y[= ]+(%d+)[, ]+rule[= ]+([^\n\r]+)")
    if wd and ht and rule then
        wd = tonumber(wd)
        ht = tonumber(ht)
    else
        return source.." does not contain a valid RLE header."
    end

    -- save current rule, pattern, etc
    local oldstate = SaveState()
    
    -- check that rule is valid
    if CheckRule(rule) ~= nil then
        return source.." contains an unsupported rule:\n"..rule
    end

    -- check for a comment line like "#CXRLE Pos=-6,-7 Gen=2"
    local xp, yp = contents:match("#CXRLE.+Pos=(%-?%d+),(%-?%d+)")
    if xp and yp then
        xp = tonumber(xp)
        yp = tonumber(yp)
    end
    local gens = contents:match("#CXRLE.+Gen=(%d+)")
    if gens then
        gens = tonumber(gens)
    else
        gens = 0
    end
    
    -- create temporary file with same contents but replace rule with real_rule
    if rule:find("%-") then
        rule = rule:gsub("%-","%%-")    -- need to escape hyphens
    end
    contents = contents:gsub("rule[= ]+"..rule, "rule = "..real_rule, 1)
    local temppath = g.getdir("temp").."temp.rle"
    f = io.open(temppath,"w")
    if not f then
        RestoreState(oldstate)
        return "Failed to create temporary file!"
    end
    f:write(contents)
    f:close()

    -- save current state, clear universe and use g.open to load temporary file
    local cellarray = {}
    g.new("")
    g.open(temppath)
    SetColors()
    cellarray = g.getcells(g.getrect())

    -- success
    local newpattern = {
        rule = current_rule,        -- set by above CheckRule
        width = wd,
        height = ht,
        xpos = xp,                  -- nil if no Pos info in #CXRLE line
        ypos = yp,                  -- ditto
        gen = gens,
        cells = cellarray
    }

    RestoreState(oldstate)          -- restores rule, pattern, stepsize, etc

    return nil, newpattern
end

--------------------------------------------------------------------------------

function CreatePattern(newpattern)
    -- called by OpenPattern/OpenClipboard
    g.new(pattname)
    SetStepSize(1)
    g.setcursor("Move")
    CheckRule(newpattern.rule)      -- sets current_rule and calls SetColors
    g.putcells(newpattern.cells)
    gencount = newpattern.gen
    g.setgen(tostring(gencount))    -- for status bar
    startcount = gencount           -- for Reset
    StopGenerating()
    ClearUndoRedo()                 -- dirty = false
    FitPattern()                    -- calls Refresh
end

--------------------------------------------------------------------------------

function OpenPattern(filepath)
    if filepath then
        local err, newpattern = ReadPattern(filepath, "File")
        if err then
            g.warn(err, false)
        else
            -- pattern ok so use info in newpattern to create the new pattern;
            -- set pattname to file name at end of filepath
            pattname = filepath:match("^.+"..pathsep.."(.+)$")
            CreatePattern(newpattern)
        end
    else
        -- prompt user for a .rle file to open
        local filetype = "RLE file (*.rle)|*.rle"
        local path = g.opendialog("Open a pattern", filetype, pattdir, "")
        if #path > 0 then
            -- update pattdir by stripping off the file name
            pattdir = path:gsub("[^"..pathsep.."]+$","")
            -- open the chosen pattern
            OpenPattern(path)
        end
    end
end

--------------------------------------------------------------------------------

function CopyClipboardToFile()
    -- create a temporary file containing clipboard text
    local filepath = g.getdir("temp").."clipboard.rle"
    local f = io.open(filepath,"w")
    if not f then
        g.warn("Failed to create temporary clipboard file!", false)
        return nil
    end
    -- NOTE: we can't do f:write(string.gsub(g.getclipstr(),"\r","\n"))
    -- because gsub returns 2 results and we'd get count appended to file!
    local clip = string.gsub(g.getclipstr(),"\r","\n")
    f:write(clip)
    f:close()
    return filepath
end

--------------------------------------------------------------------------------

function OpenClipboard()
    local filepath = CopyClipboardToFile()
    if filepath then
        local err, newpattern = ReadPattern(filepath, "Clipboard")
        if err then
            g.warn(err, false)
        else
            -- pattern ok so use info in newpattern to create the new pattern
            pattname = "clipboard"
            CreatePattern(newpattern)
        end
    end
end

--------------------------------------------------------------------------------

function WritePattern(filepath)
    -- save current pattern as RLE file
    g.save(filepath,"rle")
    
    -- now modify the file, replacing real_rule with current_rule
    local f = io.open(filepath,"r")
    if not f then
        return "Failed to read RLE file:\n"..filepath
    end
    local newcontents = f:read("*a"):gsub("rule = "..real_rule, "rule = "..current_rule, 1)
    f:close()
    
    f = io.open(filepath,"w")
    if not f then
        return "Failed to fix rule in RLE file:\n"..filepath
    end
    f:write(newcontents)
    f:close()

    return nil  -- success
end

--------------------------------------------------------------------------------

function SavePattern(filepath)
    if filepath then
        local err = WritePattern(filepath)
        if err then
            g.warn(err, false)
        else
            -- set pattname to file name at end of filepath
            pattname = filepath:match("^.+"..pathsep.."(.+)$")
            dirty = false
            Refresh()
        end
    else
        -- prompt user for file name and location
        local filetype = "RLE file (*.rle)|*.rle"
        local path = g.savedialog("Save pattern", filetype, pattdir, pattname)
        if #path > 0 then
            -- update pattdir by stripping off the file name
            pattdir = path:gsub("[^"..pathsep.."]+$","")
            -- ensure file name ends with ".rle"
            if not path:find("%.rle$") then path = path..".rle" end
            -- save the current pattern
            SavePattern(path)
        end
    end
end

--------------------------------------------------------------------------------

local Exit_called = false

function Exit(message)
    -- user scripts should call this function rather than g.exit
    Exit_called = true
    g.exit(message)
end

--------------------------------------------------------------------------------

function CallScript(func, fromclip)
    -- avoid infinite recursion
    if scriptlevel == 100 then
        g.warn("Script is too recursive!", false)
        return
    end

    if scriptlevel == 0 then
        RememberCurrentState()  -- #undostack is > 0
        EnableControls(false)   -- disable most menu items and buttons
        Exit_called = false
    end

    scriptlevel = scriptlevel + 1
    local status, err = pcall(func)
    scriptlevel = scriptlevel - 1

    if err then
        g.continue("")
        if err == "GOLLY: ABORT SCRIPT" then
            -- user hit escape or script called Exit(message);
            -- if the latter then don't clobber the message in the status bar
            if not Exit_called then
                ShowMessage("Script aborted.")
            end
        else
            if fromclip then
                g.warn("Runtime error in clipboard script:\n\n"..err, false)
            else
                g.warn("Runtime error in script:\n\n"..err, false)
            end
        end
    end

    if scriptlevel == 0 then
        -- note that if the script called NewPattern/RandomPattern/OpenPattern
        -- or any other function that called ClearUndoRedo then the undostack
        -- is empty and dirty should be false
        if #undostack == 0 then
            -- some call might have set dirty to true, so reset it
            dirty = false
            if gencount > startcount then
                -- script called Step after NewPattern/RandomPattern/OpenPattern
                -- so push startstate onto undostack so user can Reset/Undo
                undostack[1] = startstate
                startstate.savedirty = false
            end
        elseif SameState(undostack[#undostack]) then
            -- script didn't change the current state so pop undostack
            table.remove(undostack)
        end
        EnableControls(true)    -- enable menu items and buttons that were disabled above
        CheckIfGenerating()
        Refresh()               -- calls DrawMenuBar and DrawToolBar
    end
end

--------------------------------------------------------------------------------

function RunScript(filepath)
    if generating then
        StopGenerating()
        Refresh()
    end
    if filepath then
        local f = io.open(filepath, "r")
        if f then
            -- Lua's f:read("*l") doesn't detect CR as EOL so we do this ugly stuff:
            -- read entire file and convert any CRs to LFs
            local all = f:read("*a"):gsub("\r", "\n").."\n"
            local nextline = all:gmatch("(.-)\n")
            local line1 = nextline()
            f:close()
            if not (line1 and (line1:find(SCRIPT_NAME..".lua") or
                               line1:find("NewCA.lua"))) then
                g.warn("First line of script must contain "..SCRIPT_NAME..".lua or NewCA.lua.", false)
                return
            end
        else
            g.warn("Script file could not be opened:\n"..filepath, false)
            return
        end
        local func, msg = loadfile(filepath)
        if func then
            CallScript(func, false)
        else
            g.warn("Syntax error in script:\n\n"..msg, false)
        end
    else
        -- prompt user for a .lua file to run
        local filetype = "Lua file (*.lua)|*.lua"
        local path = g.opendialog("Choose a Lua script", filetype, scriptdir, "")
        if #path > 0 then
            -- update scriptdir by stripping off the file name
            scriptdir = path:gsub("[^"..pathsep.."]+$","")
            -- run the chosen script
            RunScript(path)
        end
    end
end

--------------------------------------------------------------------------------

function RunClipboard()
    if generating then
        StopGenerating()
        Refresh()
    end
    local cliptext = g.getclipstr()
    local eol = cliptext:find("[\n\r]")
    if not (eol and (cliptext:sub(1,eol):find(SCRIPT_NAME..".lua") or
                     cliptext:sub(1,eol):find("NewCA.lua"))) then
        g.warn("First line of clipboard must contain "..SCRIPT_NAME..".lua or NewCA.lua.", false)
        return
    end
    local func, msg = load(cliptext)
    if func then
        CallScript(func, true)
    else
        g.warn("Syntax error in clipboard script:\n\n"..msg, false)
    end
end

--------------------------------------------------------------------------------

function SetStartupScript()
    -- prompt user for a .lua file to run automatically when SCRIPT_NAME.lua starts up
    local filetype = "Lua file (*.lua)|*.lua"
    local path = g.opendialog("Select your startup script", filetype, scriptdir, "")
    if #path > 0 then
        -- update scriptdir by stripping off the file name
        scriptdir = path:gsub("[^"..pathsep.."]+$","")
        startup = path
        -- the above path will be saved by WriteSettings
    end
end

--------------------------------------------------------------------------------

function SetDensity(percentage)
    local function getperc()
        local initstring = tostring(perc)
        ::try_again::
        local s = g.getstring("Enter density as a percentage from 1 to 100\n"..
                              "(used by Random Pattern and Random Fill):\n",
                              initstring, "Density")
        initstring = s
        if gp.validint(s) and (tonumber(s) >= 1) and (tonumber(s) <= 100) then
            perc = tonumber(s)
        else
            g.warn("Percentage must be an integer from 1 to 100.\nTry again.")
            goto try_again
        end
    end

    if percentage then
        perc = percentage
        if perc < 1 then perc = 1 end
        if perc > 100 then perc = 100 end
    else
        -- prompt user for the percentage;
        -- if user hits Cancel button we want to avoid aborting script
        local status, err = pcall(getperc)
        if err then
            g.continue("")  -- don't show error when script finishes
        end
    end
end

--------------------------------------------------------------------------------

function RandomPattern(percentage, wd, ht)
    percentage = percentage or perc
    if percentage < 1 then percentage = 1 end
    if percentage > 100 then percentage = 100 end
    wd = wd or 100
    ht = ht or 100
    if wd > gridwd then wd = gridwd end
    if ht > gridht then ht = gridht end
    
    pattname = "random"
    g.new(pattname)
    g.setcursor("Move")
    gencount = 0
    startcount = 0
    SetStepSize(1)
    SetColors()
    StopGenerating()
    ClearUndoRedo()

    local set = g.setcell
    local maxstate = g.numstates()-1
    for y = -(ht//2), (ht+1)//2-1 do
        for x = -(wd//2), (wd+1)//2-1 do
            if rand(0,99) < percentage then
                set(x, y, rand(1,maxstate))
            end
        end
    end

    FitGrid()   -- calls Refresh
end

--------------------------------------------------------------------------------

function ChangeRule()
    local function getrule()
        local newrule = current_rule
        ::try_again::
        newrule = g.getstring("Enter the new rule (with an optional suffix that\n"..
                              "specifies the grid width and height):\n",
                              newrule, "Set rule")
        local err = CheckRule(newrule)
        if err then
            g.warn(err)
            goto try_again
        end
    end

    RememberCurrentState()
    local oldrule = current_rule

    -- if user hits Cancel button we want to avoid aborting script
    local status, err = pcall(getrule)
    if err then
        g.continue("")  -- don't show error when script finishes
    end
    
    if oldrule == current_rule then
        -- error or rule wasn't changed so pop undostack
        table.remove(undostack)
    else
        CheckIfGenerating()
        Refresh()
    end
end

--------------------------------------------------------------------------------

function CellToPixel(x, y)
    -- convert cell position to pixel position in overlay
    -- (assumes overlay is in top left corner of viewport)
    local viewmag = g.getmag()
    local negmagpow2 = 2^(-viewmag)
    local magpow2 = 2^viewmag
    
    -- get position of central cell in viewport
    local xc, yc = g.getpos()
    xc = tonumber(xc)
    yc = tonumber(yc)
    
    -- use method in viewport::reposition to calculate top left cell
    local x0 = xc - int(viewwd * negmagpow2) // 2
    local y0 = yc - int(viewht * negmagpow2) // 2

    -- use method in viewport::screenPosOf
    if viewmag < 0 then
        local xx0 = x0
        local yy0 = y0
        -- from ltlalgo::lowerRightPixel
        xx0 = int(xx0 // negmagpow2)
        xx0 = int(xx0 * negmagpow2)
        yy0 = yy0 - 1
        yy0 = int(yy0 // negmagpow2)
        yy0 = int(yy0 * negmagpow2)
        yy0 = yy0 + 1
        x = x - xx0
        y = y - yy0
    else
        x = x - x0
        y = y - y0
    end
    x = int(x * magpow2)
    y = int(y * magpow2)
    return x, y
end

--------------------------------------------------------------------------------

function PixelToCell(x, y)
    -- convert pixel position in overlay to cell position
    -- (which might be outside bounded grid)
    local viewmag = g.getmag()
    local negmagpow2 = 2^(-viewmag)
    
    -- get position of central cell in viewport
    local xc, yc = g.getpos()
    xc = tonumber(xc)
    yc = tonumber(yc)
    
    -- use method in viewport::reposition to calculate top left cell
    local x0 = xc - int(viewwd * negmagpow2) // 2
    local y0 = yc - int(viewht * negmagpow2) // 2

    -- use method in viewport::at to calculate cell position of x,y
    xc = int(x * negmagpow2) + x0
    yc = int(y * negmagpow2) + y0
    return xc, yc
end

--------------------------------------------------------------------------------

local function DrawPasteCell(xoff, yoff, viewmag)
    if viewmag == 0 then
        ovt{"set", xoff, yoff}
    elseif viewmag > 0 then
        local cellsize = int(2^viewmag)
        local x = xoff * cellsize
        local y = yoff * cellsize
        if cellsize > 2 then cellsize = cellsize - 1 end
        ovt{"fill", x, y, cellsize, cellsize}
    else -- viewmag < 0
        ovt{"set", xoff // 2^(-viewmag), yoff // 2^(-viewmag)}
    end
end

--------------------------------------------------------------------------------

function CreatePastePattern()
    local viewmag = g.getmag()
    local wd, ht = pastewd, pasteht
    if viewmag > 0 then
        wd = wd * int(2^viewmag)
        ht = ht * int(2^viewmag)
    elseif viewmag < 0 then
        wd = wd // int(2^(-viewmag)) + 1
        ht = ht // int(2^(-viewmag)) + 1
    end
    
    -- create a translucent red rectangle in pasteclip
    ov("create "..wd.." "..ht.." "..pasteclip)
    ov("target "..pasteclip)
    ovt{"rgba", 255, 0, 0, 128}
    ovt{"fill", 0, 0, wd, ht}
    -- blend the live cells into the translucent rectangle
    ov("blend 1")
    local len = #pastecells
    if len % 2 == 0 then
        -- pastecells is a one-state cell array
        local S,R,G,B = table.unpack(g.getcolors(1))
        ovt{"rgba", R, G, B, 128}
        for i = 1, len, 2 do
            DrawPasteCell(pastecells[i], pastecells[i+1], viewmag)
        end
    else
        -- pastecells is a multi-state cell array
        if len % 3 > 0 then len = len - 1 end
        local colors = g.getcolors()
        for i = 1, len, 3 do
            local Rpos = pastecells[i+2] * 4 + 2
            ovt{"rgba", colors[Rpos], colors[Rpos+1], colors[Rpos+2], 128}
            DrawPasteCell(pastecells[i], pastecells[i+1], viewmag)
        end
    end
    ov("blend 0")
    ov("target")
    
    -- create black-on-white pastemode text in modeclip
    ov("rgba 0 0 0 255")
    wd, modeht = op.maketext(pastemode:upper())
    wd = wd + 8
    ov("create "..wd.." "..modeht.." "..modeclip)
    ov("target "..modeclip)
    ovt{"rgba", 255, 255, 255, 255}
    ovt{"fill", 0, 0, wd, modeht}
    ov("blend 1")
    op.pastetext(4, 0)
    ov("blend 0")
    ov("target")
end

--------------------------------------------------------------------------------

function ClearPastePattern()
    ov(alpha1)
    ovt{"fill", 0, currbarht, ovwd, ovht-currbarht}
end

--------------------------------------------------------------------------------

function DrawPastePattern(xc, yc)
    -- draw the translucent paste pattern with mouse at the given cell position
    
    -- adjust xc,yc if pastepos is not "topleft"
    if pastepos == "topright" then
        xc = xc - pastewd + 1
    elseif pastepos == "bottomright" then
        xc = xc - pastewd + 1
        yc = yc - pasteht + 1
    elseif pastepos == "bottomleft" then
        yc = yc - pasteht + 1
    elseif pastepos == "middle" then
        xc = xc - pastewd//2
        yc = yc - pasteht//2
    end
    local xp, yp = CellToPixel(xc, yc)

    ov("blend 1")
    ov("paste "..xp.." "..yp.." "..pasteclip)
    ov("paste "..xp.." "..(yp-modeht).." "..modeclip)
    ov("blend 0")
    
    if currbarht > 0 then
        -- may need to redraw menu/tool bar
        if yp-modeht <= currbarht then
            DrawMenuBar(false)
            DrawToolBar(false)
        end
    end
end

--------------------------------------------------------------------------------

function FlipPaste(leftright)
    if #pastecells > 0 then
        if leftright then
            pastecells = g.transform(pastecells, pastewd-1, 0, -1, 0, 0, 1)
        else
            pastecells = g.transform(pastecells, 0, pasteht-1, 1, 0, 0, -1)
        end
        updatepaste = true
    end
end

--------------------------------------------------------------------------------

function RotatePaste(clockwise)
    if #pastecells > 0 or pasteht ~= pastewd then
        if #pastecells > 0 then
            if clockwise then
                pastecells = g.transform(pastecells, pasteht-1, 0, 0, -1, 1, 0)
            else
                pastecells = g.transform(pastecells, 0, pastewd-1, 0, 1, -1, 0)
            end
        end
        pastewd, pasteht = pasteht, pastewd
        updatepaste = true
    end
end

--------------------------------------------------------------------------------

function SetPasteMode(mode)
    pastemode = mode
    updatepaste = true
end

--------------------------------------------------------------------------------

function CyclePasteMode()
    if pastemode == "and" then
        pastemode = "copy"
    elseif pastemode == "copy" then
        pastemode = "or"
    elseif pastemode == "or" then
        pastemode = "xor"
    elseif pastemode == "xor" then
        pastemode = "and"
    else
        -- should never happen but play safe
        pastemode = "or"
    end
    updatepaste = true
end

--------------------------------------------------------------------------------

function SetPasteLocation(location)
    pastepos = location
    updatepaste = true
end

--------------------------------------------------------------------------------

function CyclePasteLocation()
    if pastepos == "topleft" then
        pastepos = "topright"
    elseif pastepos == "topright" then
        pastepos = "bottomright"
    elseif pastepos == "bottomright" then
        pastepos = "bottomleft"
    elseif pastepos == "bottomleft" then
        pastepos = "middle"
    elseif pastepos == "middle" then
        pastepos = "topleft"
    else
        -- should never happen but play safe
        pastepos = "topleft"
    end
    updatepaste = true
end

--------------------------------------------------------------------------------

function HandlePasteKey(event)
    local _, key, mods = split(event)
    if     key == "x" and mods == "none" then FlipPaste(true)
    elseif key == "y" and mods == "none" then FlipPaste(false)
    elseif key == ">" and mods == "none" then RotatePaste(true)
    elseif key == "<" and mods == "none" then RotatePaste(false)
    elseif key == "m" and mods == "shift" then CyclePasteMode()
    elseif key == "l" and mods == "shift" then CyclePasteLocation()
    elseif key == "t" and mods == "none" then ToggleToolBar()
    elseif key == "g" and mods == "none" then FitGrid()
    elseif key == "f" and mods == "none" then FitPattern()
    elseif key == "f" and mods == "shift" then FitSelection()
    elseif key == "m" and mods == "none" then MiddleView()
    elseif key == "h" and mods == "none" then ShowHelp()
    else
        -- allow keyboard shortcut to change scale etc
        g.doevent(event)
    end
end

--------------------------------------------------------------------------------

function DoPaste(x, y)
    -- adjust x,y if pastepos isn't "topleft"
    if pastepos == "topright" then
        x = x - pastewd + 1
    elseif pastepos == "bottomright" then
        x = x - pastewd + 1
        y = y - pasteht + 1
    elseif pastepos == "bottomleft" then
        y = y - pasteht + 1
    elseif pastepos == "middle" then
        x = x - pastewd//2
        y = y - pasteht//2
    end

    if pastemode == "copy" and not g.empty() then
        -- erase any live cells under paste pattern
        if x > maxx or x + pastewd - 1 < minx or
           y > maxy or y + pasteht - 1 < miny then
            -- paste pattern is completely outside bounded grid
        else
            local left = math.max(x, minx)
            local top = math.max(y, miny)
            local right = math.min(x + pastewd - 1, maxx)
            local bottom = math.min(y + pasteht - 1, maxy)
            local livecells = g.getcells( {left, top, right-left+1, bottom-top+1} )
            g.putcells(livecells, 0, 0, 1, 0, 0, 1, "xor")
        end
    end
    
    -- silently clip any cells outside bounded grid (like Golly)
    -- by temporarily switching to an unbounded universe
    g.setrule( real_rule:sub(1,real_rule:find(":")-1) )
    g.putcells(pastecells, x, y, 1, 0, 0, 1, pastemode)
    local oldsel = g.getselrect()
    g.select( {minx, miny, gridwd, gridht} )
    g.clear(1)
    g.select(oldsel)
    g.setrule(real_rule)
    SetColors()
end

--------------------------------------------------------------------------------

function CreatePopUpMenu()
    -- create a pop-up menu with various paste actions
    pastemenu = op.popupmenu()
    pastemenu.additem("Flip Top-Bottom", FlipPaste, {false})
    pastemenu.additem("Flip Left-Right", FlipPaste, {true})
    pastemenu.additem("Rotate Clockwise", RotatePaste, {true})
    pastemenu.additem("Rotate Anticlockwise", RotatePaste, {false})
    pastemenu.additem("---", nil)
    pastemenu.additem("AND Mode", SetPasteMode, {"and"})
    pastemenu.additem("COPY Mode", SetPasteMode, {"copy"})
    pastemenu.additem("OR Mode", SetPasteMode, {"or"})
    pastemenu.additem("XOR Mode", SetPasteMode, {"xor"})
    pastemenu.additem("---", nil)
    pastemenu.additem("Top Left Location", SetPasteLocation, {"topleft"})
    pastemenu.additem("Top Right Location", SetPasteLocation, {"topright"})
    pastemenu.additem("Bottom Right Location", SetPasteLocation, {"bottomright"})
    pastemenu.additem("Bottom Left Location", SetPasteLocation, {"bottomleft"})
    pastemenu.additem("Middle Location", SetPasteLocation, {"middle"})
    pastemenu.additem("---", nil)
    pastemenu.additem("Cancel Paste", g.exit)
end

--------------------------------------------------------------------------------

function ChoosePasteAction(x, y)
    -- show dark red pop-up menu and let user choose a paste action
    pastemenu.setbgcolor("rgba 128 0 0 255")
    ov("cursor arrow")
    pastemenu.show(x, y, viewwd, viewht)
    ov("cursor current")
end

--------------------------------------------------------------------------------

function WaitForPaste()
    -- create a translucent paste pattern in pasteclip
    CreatePastePattern()
    updatepaste = false
    
    local currwd, currht = viewwd, viewht
    local currmag = g.getmag()
    local prevx, prevy
    
    while true do
        local event = g.getevent()
        if #event == 0 then
            g.sleep(2)          -- don't hog the CPU
            CheckWindowSize()
            -- scale or window size might have changed
            if currmag ~= g.getmag() or currwd ~= viewwd or currht ~= viewht or updatepaste then
                currwd, currht = viewwd, viewht
                currmag = g.getmag()
                -- need to recreate the translucent paste pattern
                CreatePastePattern()
                updatepaste = false
                prevx, prevy = nil, nil
            end
            local pixelpos = ov("xy")
            if #pixelpos == 0 then
                ClearPastePattern()
                g.update()
            else
                local x, y = split(pixelpos)
                x, y = tonumber(x), tonumber(y)
                local xc, yc = PixelToCell(x, y)
                if xc ~= prevx or yc ~= prevy then
                    -- mouse has moved to a new cell (possibly outside bounded grid)
                    ClearPastePattern()
                    if y >= currbarht then
                        DrawPastePattern(xc, yc)
                    end
                    g.update()
                    prevx = xc
                    prevy = yc
                end
            end            
        elseif event:find("^key") then
            HandlePasteKey(event)
            -- updatepaste might be true
        elseif event:find("^oclick") then
            local _, x, y, button, mods = split(event)
            x, y = tonumber(x), tonumber(y)
            if y < currbarht then
                -- cancel paste if click in menu/tool bar
                g.exit()
            elseif (button == "right" and (mods == "none" or mods == "ctrl")) or
                   (button == "left" and mods == "ctrl") then
                -- right-click or ctrl-click so display pop-up menu
                ChoosePasteAction(x, y)
                -- updatepaste might be true
            elseif button == "left" and mods == "none" then
                DoPaste( PixelToCell(x, y) )
                break
            end
        elseif event:find("^ozoom") then
            -- remove the "o" and do the zoom
            g.doevent(event:sub(2))
        end
    end

    return nil  -- cells were successfully pasted
end

--------------------------------------------------------------------------------

function Paste()
    -- if the clipboard contains a valid pattern then create a paste pattern
    local filepath = CopyClipboardToFile()
    if not filepath then return end

    local err, newpattern = ReadPattern(filepath, "Clipboard")
    if err then
        g.warn(err, false)
        Refresh()
        return
    end

    if #newpattern.cells == 0 then
        -- clipboard pattern is empty
        pastewd = newpattern.width
        pasteht = newpattern.height
        pastecells = {}
    else
        -- find the pattern's minimal bounding box
        local bbox = gp.getminbox(newpattern.cells)
        pastewd = math.max(bbox.wd, newpattern.width)
        pasteht = math.max(bbox.ht, newpattern.height)
        if newpattern.xpos and newpattern.ypos then
            pastecells = g.transform(newpattern.cells, -newpattern.xpos, -newpattern.ypos)
        else
            -- g.open centered pattern
            pastecells = g.transform(newpattern.cells, newpattern.width//2, newpattern.height//2)
        end
    end

    RememberCurrentState()
    
    g.show("Click where you want to paste...")
    local oldcursor = g.setcursor("Select")
    
    -- call WaitForPaste via pcall so user can hit escape to cancel paste
    local status, err = xpcall(WaitForPaste, gp.trace)
    if err then
        g.continue("")
        if not err:find("^GOLLY: ABORT SCRIPT") then
            -- runtime error occurred somewhere inside WaitForPaste
            g.warn(err, false)
        end
        g.show("Paste aborted.")
        table.remove(undostack)
    else
        -- paste succeeded
        g.show("")
        dirty = true
    end
    
    g.setcursor(oldcursor)
    
    -- delete the clips and clear the paste pattern
    ov("delete "..pasteclip)
    ov("delete "..modeclip)
    ClearPastePattern()
    pastecells = {}
    
    CheckIfGenerating()
    Refresh()
end

--------------------------------------------------------------------------------

function RemoveSelection()
    if #g.getselrect() > 0 then
        -- remove the current selection
        RememberCurrentState()
        g.select({})
        ShowMessage("")
        Refresh()
    end
end

--------------------------------------------------------------------------------

function SelectAll()
    local sel = g.getselrect()
    local r = g.getrect()
    if #r == 0 then
        if #sel > 0 then
            RemoveSelection()   -- calls RememberCurrentState
        end
        ShowError("All cells are dead.")
    else
        -- pattern exists
        if #sel > 0 and r[1] == sel[1] and r[2] == sel[2] and
                        r[3] == sel[3] and r[4] == sel[4] then
            -- selection rect won't change
        else
            RememberCurrentState()
            g.select(r)
            ShowMessage("Selection wd x ht = "..r[3].." x "..r[4])
            Refresh()
        end
    end
end

--------------------------------------------------------------------------------

function ClearSelection()
    local sel = g.getselrect()
    if #sel > 0 then
        -- check if there are any live cells inside the selection
        local cells = g.getcells(sel)
        if #cells > 0 then
            -- there are live cells inside the selection
            RememberCurrentState()
            dirty = true
            g.clear(0)
            Refresh()
        end
    end
end

--------------------------------------------------------------------------------

function ClearOutside()
    local sel = g.getselrect()
    if #sel > 0 then
        -- check if there are any live cells outside the selection
        local r = g.getrect()
        if #r == 0 or ( r[1] >= sel[1] and r[2] >= sel[2] and
                        r[1]+r[3]-1 <= sel[1]+sel[3]-1 and
                        r[2]+r[4]-1 <= sel[2]+sel[4]-1 ) then
            -- there are no live cells outside the selection
        else
            RememberCurrentState()
            dirty = true
            g.clear(1)
            Refresh()
        end
    end
end

--------------------------------------------------------------------------------

function FixClipboardRule()
    -- get clipboard string and replace real_rule with current_rule
    local s = string.gsub(g.getclipstr(), "rule = "..real_rule, "rule = "..current_rule, 1)
    g.setclipstr(s)
end

--------------------------------------------------------------------------------

function CopySelection()
    if #g.getselrect() > 0 then
        -- copy selection to clipboard
        g.copy()
        FixClipboardRule()
    end
end

--------------------------------------------------------------------------------

function CutSelection()
    local sel = g.getselrect()
    if #sel > 0 then
        -- check if there are any live cells inside the selection
        local cells = g.getcells(sel)
        if #cells > 0 then
            -- there are live cells inside the selection
            RememberCurrentState()
            dirty = true
        end
        g.cut() -- saves selection in clipboard even if empty
        FixClipboardRule()
        Refresh()
    end
end

--------------------------------------------------------------------------------

function ShrinkSelection()
    local sel = g.getselrect()
    if #sel > 0 then
        -- check if there are any live cells inside the selection
        local cells = g.getcells(sel)
        if #cells == 0 then
            ShowError("There are no live cells in the selection.")
        else
            RememberCurrentState()
            g.shrink()
            Refresh()
        end
    end
end

--------------------------------------------------------------------------------

function FlipTopBottom()
    if #g.getselrect() > 0 then
        RememberCurrentState()
        dirty = true
        g.flip(1)
        Refresh()
    end
end

--------------------------------------------------------------------------------

function FlipLeftRight()
    if #g.getselrect() > 0 then
        RememberCurrentState()
        dirty = true
        g.flip(0)
        Refresh()
    end
end

--------------------------------------------------------------------------------

function InsideGrid(left, top, right, bottom)
    -- return true if all edges of given rectangle are inside grid
    if left < minx or right > maxx then return false end
    if top < miny or bottom > maxy then return false end
    return true
end

--------------------------------------------------------------------------------

function RotateClockwise()
    local sel = g.getselrect()
    if #sel > 0 then
        -- rotate selection clockwise about its center (the following method ensures
        -- the original selection will be restored after 4 such rotations)
        local selwd = sel[3]
        local selht = sel[4]
        local midx = sel[1] + (selwd - 1) // 2
        local midy = sel[2] + (selht - 1) // 2
        local newx = midx + sel[2] - midy
        local newy = midy + sel[1] - midx
        if not InsideGrid(newx, newy, newx+selht-1, newy+selwd-1) then
            ShowError("Rotation is not allowed if selection would be outside grid.")
            return
        end
        local selcells = g.getcells(sel)
        if #selcells > 0 or selht ~= selwd then
            RememberCurrentState()
            dirty = true
            g.rotate(0)
            Refresh()
        end
    end
end

--------------------------------------------------------------------------------

function RotateAnticlockwise()
    local sel = g.getselrect()
    if #sel > 0 then
        -- rotate selection anticlockwise about its center (the following method ensures
        -- the original selection will be restored after 4 such rotations)
        local selwd = sel[3]
        local selht = sel[4]
        local midx = sel[1] + (selwd - 1) // 2
        local midy = sel[2] + (selht - 1) // 2
        local newx = midx + sel[2] - midy
        local newy = midy + sel[1] - midx
        if not InsideGrid(newx, newy, newx+selht-1, newy+selwd-1) then
            ShowError("Rotation is not allowed if selection would be outside grid.")
            return
        end
        local selcells = g.getcells(sel)
        if #selcells > 0 or selht ~= selwd then
            RememberCurrentState()
            dirty = true
            g.rotate(1)
            Refresh()
        end
    end
end

--------------------------------------------------------------------------------

function RandomFill()
    local sel = g.getselrect()
    if #sel > 0 then
        RememberCurrentState()
        dirty = true
        local set = g.setcell
        local maxstate = g.numstates()-1
        -- note that LtL's g.randfill only fills with state 1
        g.clear(0)
        for y = sel[2], sel[2]+sel[4]-1 do
            for x = sel[1], sel[1]+sel[3]-1 do
                if rand(0,99) < perc then
                    set(x, y, rand(1,maxstate))
                end
            end
        end
        Refresh()
    end
end

--------------------------------------------------------------------------------

function OpenFile(filepath)
    if filepath:find("%.rle$") then
        OpenPattern(filepath)
    elseif filepath:find("%.lua$") then
        RunScript(filepath)
    else
        g.warn("Unexpected file:\n"..filepath.."\n\n"..
               "File extension must be .rle or .lua.", false)
    end
end

--------------------------------------------------------------------------------

function StartStop()
    generating = not generating
    UpdateStartButton()
    Refresh()
    if generating and not g.empty() then
        RememberCurrentState()
        stopgen = 0
        -- EventLoop will call NextGeneration repeatedly
    end
end

--------------------------------------------------------------------------------

function Step1()
    if generating then
        StopGenerating()
        Refresh()
    else
        if not g.empty() then
            RememberCurrentState()
        end
        stopgen = gencount + 1
        generating = true
        -- EventLoop will call NextGeneration once
    end
end

--------------------------------------------------------------------------------

function NextStep()
    if generating then
        StopGenerating()
        Refresh()
    else
        if not g.empty() then
            RememberCurrentState()
        end
        stopgen = gencount + stepsize
        generating = true
        -- EventLoop will call NextGeneration until gencount >= stopgen
    end
end

--------------------------------------------------------------------------------

MAX_STEP_SIZE = 100     -- startup script might want to change this

function SetStepSize(n)
    if n > MAX_STEP_SIZE then n = MAX_STEP_SIZE end
    if n < 1 then n = 1 end
    stepsize = n
    
    -- show same step size in status bar
    if n == 1 then
        g.setbase(2)
        g.setstep(0)
    else
        g.setbase(n)
        g.setstep(1)
    end
    
    -- create clip with "Step=..." text for use in DrawToolBar
    local oldfont = ov(op.textfont)
    local oldbg = ov("textoption background "..op.menubg:sub(6))
    op.maketext("Step="..stepsize, "stepclip", op.white, 2, 2)
    ov("textoption background "..oldbg)
    ov("font "..oldfont)
end

--------------------------------------------------------------------------------

function Faster()
    SetStepSize(stepsize + 1)
    Refresh()
end

--------------------------------------------------------------------------------

function Slower()
    SetStepSize(stepsize - 1)
    Refresh()
end

--------------------------------------------------------------------------------

function StepChange(newval)
    -- called if stepslider position has changed
    SetStepSize(newval)
    Refresh()
end

--------------------------------------------------------------------------------

function Reset()
    if gencount > startcount then
        -- restore the starting state
        if scriptlevel > 0 then
            -- Reset was called by user script so don't modify undo/redo stacks
            RestoreState(startstate)
        else
            -- push current state onto redostack
            redostack[#redostack+1] = SaveState()
            while true do
                -- unwind undostack until gencount == startcount
                local state = table.remove(undostack)
                if state.savegencount == startcount then
                    -- restore starting state
                    RestoreState(state)
                    break
                else
                    -- push state onto redostack
                    redostack[#redostack+1] = state
                end
            end
            StopGenerating()
            Refresh()
        end
    end
end

--------------------------------------------------------------------------------

-- getters for user scripts

function GetRule() return current_rule end
function GetGenCount() return gencount end
function GetStepSize() return stepsize end
function GetBarHeight() return currbarht end
function GetDensity() return perc end

--------------------------------------------------------------------------------

-- for user scripts
function Step(n)
    n = n or 1
    while not g.empty() and n > 0 do
        NextGeneration()
        n = n - 1
    end
end

--------------------------------------------------------------------------------

-- for user scripts
function SetRule(newrule)
    newrule = newrule or DEFAULT_RULE
    local err = CheckRule(newrule)
    if err then
        error("Bad rule in SetRule!\n"..err, 2)
    end
end

--------------------------------------------------------------------------------

function ToggleToolBar()
    if currbarht > 0 then
        -- hide all the controls
        mbar.hide()
        ssbutton.hide()
        s1button.hide()
        stepbutton.hide()
        resetbutton.hide()
        undobutton.hide()
        redobutton.hide()
        rulebutton.hide()
        exitbutton.hide()
        helpbutton.hide()
        stepslider.hide()
        -- hide the menu bar and tool bar
        ov(alpha1)
        ovt{"fill", 0, 0, ovwd, currbarht}
        currbarht = 0
    else
        currbarht = mbarht
    end
    Refresh()
end

--------------------------------------------------------------------------------

local startpop  -- for saving population at start of drawing

function StartDrawing(x, y)
    RememberCurrentState()
    dirty = true
    startpop = g.getpop()
    drawstate = g.getoption("drawingstate")
    if drawstate == g.getcell(x, y) then
        drawstate = 0
    end
    g.setcell(x, y, drawstate)
    g.update()
end

--------------------------------------------------------------------------------

local anchorx, anchory      -- initial cell clicked in StartSelecting
local initsel = {}          -- initial selection rectangle
local modifysel = false     -- modify current selection?
local forceh = false        -- only modify horizontal size?
local forcev = false        -- only modify vertical size?

function StartSelecting(x, y, modify)
    RememberCurrentState()
    anchorx = x
    anchory = y
    initsel = g.getselrect()
    if #initsel > 0 and not modify then
        -- remove current selection
        g.select({})
        Refresh()
    end
    modifysel = modify
    if modify then
        -- use same logic as Golly to modify existing selection
        forceh = false
        forcev = false
        local selleft = initsel[1]
        local seltop = initsel[2]
        local selwd = initsel[3]
        local selht = initsel[4]
        local selbottom = seltop + selht - 1
        local selright = selleft + selwd - 1

        if y <= seltop and x <= selleft then
            -- click is in or outside top left corner
            seltop = y
            selleft = x
            anchory = selbottom
            anchorx = selright
        elseif y <= seltop and x >= selright then
            -- click is in or outside top right corner
            seltop = y
            selright = x
            anchory = selbottom
            anchorx = selleft
        elseif y >= selbottom and x >= selright then
            -- click is in or outside bottom right corner
            selbottom = y
            selright = x
            anchory = seltop
            anchorx = selleft
        elseif y >= selbottom and x <= selleft then
            -- click is in or outside bottom left corner
            selbottom = y
            selleft = x
            anchory = seltop
            anchorx = selright
        elseif y <= seltop then
            -- click is in or above top edge
            forcev = true
            seltop = y
            anchory = selbottom
        elseif y >= selbottom then
            -- click is in or below bottom edge
            forcev = true
            selbottom = y
            anchory = seltop
        elseif x <= selleft then
            -- click is in or left of left edge
            forceh = true
            selleft = x
            anchorx = selright
        elseif x >= selright then
            -- click is in or right of right edge
            forceh = true
            selright = x
            anchorx = selleft
        else
            -- click is somewhere inside selection
            local onethirdx = selleft + selwd / 3.0
            local twothirdx = selleft + selwd * 2.0 / 3.0
            local onethirdy = seltop + selht / 3.0
            local twothirdy = seltop + selht * 2.0 / 3.0
            local midy = seltop + selht / 2.0
            if y < onethirdy and x < onethirdx then
                -- click is near top left corner
                seltop = y
                selleft = x
                anchory = selbottom
                anchorx = selright
            elseif y < onethirdy and x > twothirdx then
                -- click is near top right corner
                seltop = y
                selright = x
                anchory = selbottom
                anchorx = selleft
            elseif y > twothirdy and x > twothirdx then
                -- click is near bottom right corner
                selbottom = y
                selright = x
                anchory = seltop
                anchorx = selleft
            elseif y > twothirdy and x < onethirdx then
                -- click is near bottom left corner
                selbottom = y
                selleft = x
                anchory = seltop
                anchorx = selright
            elseif x < onethirdx then
                -- click is near middle of left edge
                forceh = true
                selleft = x
                anchorx = selright
            elseif x > twothirdx then
                -- click is near middle of right edge
                forceh = true
                selright = x
                anchorx = selleft
            elseif y < midy then
                -- click is below middle section of top edge
                forcev = true
                seltop = y
                anchory = selbottom
            else
                -- click is above middle section of bottom edge
                forcev = true
                selbottom = y
                anchory = seltop
            end
        end

        -- crop selection if outside bounded grid
        if selleft < minx then selleft = minx end
        if seltop < miny then seltop = miny end
        if selright > maxx then selright = maxx end
        if selbottom > maxy then selbottom = maxy end

        selwd = selright-selleft+1
        selht = selbottom-seltop+1

        g.select( {selleft, seltop, selwd, selht} )
        ShowMessage("Selection wd x ht = "..selwd.." x "..selht)
        g.update()
    end
end

--------------------------------------------------------------------------------

function UpdateSelection(x, y)
    local selx = math.min(anchorx, x)
    local sely = math.min(anchory, y)
    local selwd = math.abs(anchorx - x) + 1
    local selht = math.abs(anchory - y) + 1
    local selright = selx+selwd-1
    local selbottom = sely+selht-1

    if modifysel then
        -- use same logic as Golly to modify existing selection
        local selrect = g.getselrect()
        if forcev then
            -- only change vertical size
            selx = selrect[1]
            selright = selx + selrect[3] - 1
        end
        if forceh then
            -- only change horizontal size
            sely = selrect[2]
            selbottom = sely + selrect[4] - 1
        end
        selwd = selright-selx+1
        selht = selbottom-sely+1
        selright = selx+selwd-1
        selbottom = sely+selht-1
    end

    -- check if selection is completely outside grid
    if selx > maxx or selright < minx or
       sely > maxy or selbottom < miny then
        g.select({})
        ShowMessage("")
        g.update()
        return
    end
    
    -- crop selection if necessary
    if selx < minx then selx = minx end
    if sely < miny then sely = miny end
    if selright > maxx then selright = maxx end
    if selbottom > maxy then selbottom = maxy end
    selwd = selright-selx+1
    selht = selbottom-sely+1
    
    g.select( {selx, sely, selwd, selht} )
    ShowMessage("Selection wd x ht = "..selwd.." x "..selht)
    Refresh()
end

--------------------------------------------------------------------------------

function CreateOverlay()
    -- overlay covers entire viewport (more if viewport is too small) but is mostly
    -- transparent except for opaque menu bar and tool bar at top of viewport
    viewwd, viewht = g.getview(g.getlayer())
    ovwd, ovht = viewwd, viewht
    if ovwd < minwd then ovwd = minwd end
    if ovht < minht then ovht = minht end
    ov("create "..ovwd.." "..ovht)
    ov("cursor current")
    
    -- we use a nearly transparent overlay so we can detect clicks outside the bounded grid
    ov(alpha1)
    ov("fill")

    -- set parameters for menu bar and tool bar buttons
    op.buttonht = buttonht
    op.textgap = 8                          -- gap between edge of button and its label
    op.textfont = "font 10 default-bold"    -- font for button labels
    op.menufont = "font 11 default-bold"    -- font for menu and item labels
    op.textshadowx = 2
    op.textshadowy = 2
    if g.os() == "Linux" then
        op.textfont = "font 10 default"
        op.menufont = "font 11 default"
    end
    op.menubg = "rgba 160 160 160 255"      -- gray background for menu bar and items
    op.selcolor = "rgba 110 110 110 255"    -- darker gray for selected menu/item
    op.discolor = "rgba 210 210 210 255"    -- lighter gray for disabled items and separator lines
end

--------------------------------------------------------------------------------

function CreateMenuBar()
    -- create the menu bar and add some menus;
    -- WARNING: changes to the order of menus or their items will require
    -- changes to DrawMenuBar and EnableControls
    mbar = op.menubar()
    mbar.addmenu("File")
    mbar.addmenu("Edit")
    mbar.addmenu("View")

    -- add items to File menu
    mbar.additem(1, "New Pattern", NewPattern)
    mbar.additem(1, "Random Pattern", RandomPattern)
    mbar.additem(1, "Open Pattern...", OpenPattern)
    mbar.additem(1, "Open Clipboard", OpenClipboard)
    mbar.additem(1, "Save Pattern...", SavePattern)
    mbar.additem(1, "---", nil)
    mbar.additem(1, "Run Script...", RunScript)
    mbar.additem(1, "Run Clipboard", RunClipboard)
    mbar.additem(1, "Set Startup Script...", SetStartupScript)
    mbar.additem(1, "---", nil)
    mbar.additem(1, "Exit "..SCRIPT_NAME..".lua", g.exit)

    -- add items to Edit menu
    mbar.additem(2, "Undo", Undo)
    mbar.additem(2, "Redo", Redo)
    mbar.additem(2, "---", nil)
    mbar.additem(2, "Cut", CutSelection)
    mbar.additem(2, "Copy", CopySelection)
    mbar.additem(2, "Paste", Paste)
    mbar.additem(2, "Clear", ClearSelection)
    mbar.additem(2, "Clear Outside", ClearOutside)
    mbar.additem(2, "---", nil)
    mbar.additem(2, "Select All", SelectAll)
    mbar.additem(2, "Remove Selection", RemoveSelection)
    mbar.additem(2, "Shrink Selection", ShrinkSelection)
    mbar.additem(2, "Flip Top-Bottom", FlipTopBottom)
    mbar.additem(2, "Flip Left-Right", FlipLeftRight)
    mbar.additem(2, "Rotate Clockwise", RotateClockwise)
    mbar.additem(2, "Rotate Anticlockwise", RotateAnticlockwise)
    mbar.additem(2, "Random Fill", RandomFill)
    mbar.additem(2, "Set Density...", SetDensity)

    -- add items to View menu
    mbar.additem(3, "Fit Grid", FitGrid)
    mbar.additem(3, "Fit Pattern", FitPattern)
    mbar.additem(3, "Fit Selection", FitSelection)
    mbar.additem(3, "Middle", MiddleView)
    mbar.additem(3, "---", nil)
    mbar.additem(3, "Help", ShowHelp)
end

--------------------------------------------------------------------------------

function CreateToolBar()
    -- create tool bar buttons
    ssbutton = op.button("Start", StartStop)
    s1button = op.button("+1", Step1)
    stepbutton = op.button("Step", NextStep)
    resetbutton = op.button("Reset", Reset)
    undobutton = op.button("Undo", Undo)
    redobutton = op.button("Redo", Redo)
    rulebutton = op.button("Rule...", ChangeRule)
    helpbutton = op.button("?", ShowHelp)
    exitbutton = op.button("X", g.exit)

    -- create the slider for adjusting stepsize
    stepslider = op.slider("", op.white, 100, 1, MAX_STEP_SIZE, StepChange)

    UpdateStartButton()
end

--------------------------------------------------------------------------------

local showbars = false   -- restore menu bar and tool bar?

function CheckWindowSize()
    -- if viewport size has changed then resize the overlay
    local newwd, newht = g.getview(g.getlayer())
    if newwd ~= viewwd or newht ~= viewht then
        viewwd, viewht = newwd, newht
        ovwd, ovht = viewwd, viewht
        if ovwd < minwd then ovwd = minwd end
        if ovht < minht then ovht = minht end
        local fullscreen = g.getoption("fullscreen")
        if fullscreen == 1 and currbarht > 0 then
            -- hide menu bar and tool bar but restore them when we exit full screen mode
            currbarht = 0
            showbars = true
        elseif fullscreen == 0 and showbars then
            if currbarht == 0 then
                -- restore menu bar and tool bar
                currbarht = mbarht
            end
            showbars = false
        end
        
        ov("resize "..ovwd.." "..ovht)
        ov(alpha1)
        ovt{"fill", 0, currbarht, ovwd, ovht-currbarht}

        if scriptlevel > 0 and currbarht > 0 then
            -- avoid enabling menu items and buttons
            DrawMenuBar(false)
            DrawToolBar(false)
            g.update()
        else
            Refresh()
        end
    end
end

--------------------------------------------------------------------------------

function CheckCursor()
    local pixelpos = ov("xy")
    if #pixelpos > 0 then
        -- update cursor if mouse moves in/out of menu/tool bar
        local x, y = split(pixelpos)
        x = tonumber(x)
        y = tonumber(y)
        if y < currbarht then
            if not arrow_cursor then
                -- mouse moved inside menu/tool bar
                ov("cursor arrow")
                arrow_cursor = true
            end
        else
            if arrow_cursor then
                -- mouse moved outside menu/tool bar
                ov("cursor current")
                arrow_cursor = false
            end
        end
    end
end

--------------------------------------------------------------------------------

function MouseDown(event, mouseinfo)
    -- mouse button has been pressed
    local _, x, y, button, mods = split(event)
    x, y = tonumber(x), tonumber(y)
    if y < currbarht then return end
    if button ~= "left" then return end

    local xc, yc = PixelToCell(x, y)
    mouseinfo.prevx = xc
    mouseinfo.prevy = yc
    mouseinfo.mousedown = true

    local curs = g.getcursor()
    if curs == "Draw" then
        if mods == "none" or mods == "shift" then
            if not InsideGrid(xc, yc, xc, yc) then
                ShowError("Drawing is not allowed outside bounded grid.")
            elseif g.getmag() < -3 then
                ShowError("Drawing is not allowed at scales beyond 2^3:1.")
            else
                mouseinfo.drawing = true
                StartDrawing(xc, yc)
            end
        end
    elseif curs == "Pick" then
        if not InsideGrid(xc, yc, xc, yc) then
            ShowError("Picking is not allowed outside bounded grid.")
        else
            g.setoption("drawingstate", g.getcell(xc, yc))
        end
        g.update()
    elseif curs == "Select" then
        if mods == "none" then
            mouseinfo.selecting = true
            StartSelecting(xc, yc, false)
        elseif mods == "shift" then
            mouseinfo.selecting = true
            StartSelecting(xc, yc, #g.getselrect() > 0)
        end
    elseif curs == "Move" then
        g.doevent("click "..xc.." "..yc.." "..button.." "..mods)
    elseif curs == "Zoom In" or curs == "Zoom Out" then
        g.doevent("click "..xc.." "..yc.." "..button.." "..mods)
    end
end

--------------------------------------------------------------------------------

function MouseUp(mouseinfo)
    -- mouse button has been released
    mouseinfo.mousedown = false
    if mouseinfo.drawing then
        mouseinfo.drawing = false
        if drawstate == 0 and g.getpop() == startpop then
            -- no cells were deleted so pop undostack
            table.remove(undostack)
        end
        CheckIfGenerating()
        Refresh()
    elseif mouseinfo.selecting then
        mouseinfo.selecting = false
        if gp.equal(initsel, g.getselrect()) then
            -- selection wasn't changed so pop undostack
            table.remove(undostack)
        end
        CheckIfGenerating()
        Refresh()
    end
end

--------------------------------------------------------------------------------

function CheckMousePosition(mouseinfo)
    local pixelpos = ov("xy")
    if #pixelpos > 0 then
        local x, y = split(pixelpos)
        if tonumber(y) >= currbarht then
            local xc, yc = PixelToCell(tonumber(x), tonumber(y))
            if xc ~= mouseinfo.prevx or yc ~= mouseinfo.prevy then
                -- mouse has moved
                if mouseinfo.drawing then
                    -- check if xc,yc is outside bounded grid
                    if xc < minx then xc = minx end
                    if xc > maxx then xc = maxx end
                    if yc < miny then yc = miny end
                    if yc > maxy then yc = maxy end
                    gp.drawline(mouseinfo.prevx, mouseinfo.prevy, xc, yc, drawstate)
                    g.update()
                elseif mouseinfo.selecting then
                    UpdateSelection(xc, yc)
                end
                mouseinfo.prevx = xc
                mouseinfo.prevy = yc
            end
        end
    end
end

--------------------------------------------------------------------------------

function HandleKey(event)
    local CMDCTRL = "cmd"
    if g.os() ~= "Mac" then CMDCTRL = "ctrl" end
    local _, key, mods = split(event)
    if     key == "return" and mods == "none" then StartStop()
    elseif key == "space"  and mods == "none" then Step1()
    elseif key == "tab"    and mods == "none" then NextStep()
    elseif key == "delete" and mods == "none" then ClearSelection()
    elseif key == "delete" and mods == "shift" then ClearOutside()
    elseif key == "=" and mods == "none" then Faster()
    elseif key == "-" and mods == "none" then Slower()
    elseif key == "n" and mods == CMDCTRL then NewPattern()
    elseif key == "o" and mods == CMDCTRL then OpenPattern()
    elseif key == "s" and mods == CMDCTRL then SavePattern()
    elseif key == "o" and mods == "shift" then OpenClipboard()
    elseif key == "r" and mods == "shift" then RunClipboard()
    elseif key == "r" and mods == CMDCTRL then Reset()
    elseif key == "r" and mods == "none" then ChangeRule()
    elseif key == "a" and (mods == "none" or mods == CMDCTRL) then SelectAll()
    elseif key == "k" and (mods == "none" or mods == CMDCTRL) then RemoveSelection()
    elseif key == "z" and (mods == "none" or mods == CMDCTRL) then Undo()
    elseif key == "z" and (mods == "shift" or mods == CMDCTRL.."shift") then Redo()
    elseif key == "x" and mods == CMDCTRL then CutSelection()
    elseif key == "c" and mods == CMDCTRL then CopySelection()
    elseif key == "v" and (mods == "none" or mods == CMDCTRL) then Paste()
    elseif key == "x" and mods == "none" then FlipLeftRight()
    elseif key == "y" and mods == "none" then FlipTopBottom()
    elseif key == ">" and mods == "none" then RotateClockwise()
    elseif key == "<" and mods == "none" then RotateAnticlockwise()
    elseif key == "5" and mods == CMDCTRL then RandomFill()
    elseif key == "p" and mods == CMDCTRL then RandomPattern()
    elseif key == "d" and mods == "none" then SetDensity()
    elseif key == "t" and mods == "none" then ToggleToolBar()
    elseif key == "g" and mods == "none" then FitGrid()
    elseif key == "f" and mods == "none" then FitPattern()
    elseif key == "f" and mods == "shift" then FitSelection()
    elseif key == "m" and mods == "none" then MiddleView()
    elseif key == "h" and mods == "none" then ShowHelp()
    elseif key == "q" then g.exit()
    else
        -- could be a keyboard shortcut (eg. to toggle full screen mode)
        g.doevent(event)
    end
end

--------------------------------------------------------------------------------

function EventLoop()
    local mouseinfo = {
        mousedown = false,          -- mouse button is down?
        drawing = false,            -- draw/erase cells with pencil cursor?
        selecting = false,          -- select/deselect cells with cross-hairs cursor?
        prevx = nil, prevy = nil    -- previous mouse position
    }

    while true do
        local event = g.getevent()
        if #event == 0 then
            if not mouseinfo.mousedown then
                if not generating then
                    g.sleep(5)      -- don't hog the CPU when idle
                end
                CheckWindowSize()   -- may need to resize the overlay
            end
        else
            if event:find("^key") or event:find("^oclick") then
                ShowMessage("")     -- remove any recent message
            end
            event = op.process(event)
            if #event == 0 then
                -- op.process handled the given event (click in menu or button)
            elseif event:find("^key") then
                if mouseinfo.mousedown then
                    -- allow arrow keys and other keyboard shortcuts while mouse pressed
                    g.doevent(event)
                else
                    HandleKey(event)
                end
            elseif event:find("^oclick") then
                MouseDown(event, mouseinfo)
            elseif event:find("^mup") then
                MouseUp(mouseinfo)
            elseif event:find("^ozoom") then
                -- remove the "o" and do the zoom
                g.doevent(event:sub(2))
            elseif event:find("^file") then
                OpenFile(event:sub(6))
            end
        end

        if mouseinfo.mousedown then
            CheckMousePosition(mouseinfo)
        else
            CheckCursor()
            if generating then NextGeneration() end
        end
    end
end

--------------------------------------------------------------------------------

local Main_called = false

function Main()
    if Main_called then
        -- skip the initialization code
        EventLoop()
        return
    end
    
    Main_called = true
    CreateOverlay()
    CreateMenuBar()
    CreateToolBar()
    CreatePopUpMenu()

    -- validate current_rule
    local err = CheckRule(current_rule)
    if err then
        -- switch to DEFAULT_RULE and ensure it is valid
        err = CheckRule(DEFAULT_RULE)
        if err then
            g.warn("DEFAULT_RULE is not valid!\n"..err)
            g.exit()
        end
    end
        
    -- call NewPattern but without doing a Refresh
    local saveRefresh = Refresh
    Refresh = function() end
    NewPattern("untitled")
    Refresh = saveRefresh

    -- run the user's startup script if it exists
    local f = io.open(startup, "r")
    if f then
        f:close()
        RunScript(startup)
        ClearUndoRedo()     -- don't want to undo startup script
        SetColors()         -- startup script might override this
    end
    
    Refresh()
    EventLoop()
end

--------------------------------------------------------------------------------

function OkayToExit()
    if dirty then
        local answer = g.savechanges("Save your changes?",
                                     "If you don't save, the changes will be lost.")
        if answer == "yes" then
            SavePattern()
            if dirty then
                -- error occurred or user hit Cancel in g.savedialog
                return false
            end
        elseif answer == "no" then
            return true
        else -- answer == "cancel"
            return false
        end
    end
    return true
end

--------------------------------------------------------------------------------

function StartNewCA()
    SanityChecks()
    AddNewLayer()
    ReadSettings()
    local oldstate = SaveGollyState()
    
    ::call_Main_again::
    g.check(true)
    
    local status, err = xpcall(Main, gp.trace)
    if err then g.continue(err) end
    -- the following code is always executed
    
    g.check(false)  -- ensure the following code can't be interrupted
    
    -- err starts with "GOLLY: ABORT SCRIPT" if user hit escape or g.exit was called
    if err:find("^GOLLY: ABORT SCRIPT") then
        if not OkayToExit() then
            goto call_Main_again
        end
    end
    
    RestoreGollyState(oldstate)
    WriteSettings()
end
