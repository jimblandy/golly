# Script to produce Moore CA rule tables that emulate Margolus neighbourhood
# CA by using extra states to indicate the current state of the partitioning.
#
# Use import.py and export.py to bring two-state patterns into and out of the
# special partition colouring you need to get these rules to work properly.
#
# Only for two-state Margolus CAs at the moment, becoming 5-state in the
# emulation. The method could also be extended to larger partitions...
#
# We owe a lot to MCell's implementation of the Margolus neighbourhood. Thanks Mirek!
#
# Tim Hutton <tim.hutton@gmail.com>

import golly as g

# ask the user for the MCell string to convert (comma-separated works too)
s = g.getstring("Enter a specification string.\n\nThis can either be an MCell format Margolus\nstring or just a comma-separated list of the 16 case indices.\n\nSee: http://psoup.math.wisc.edu/mcell/rullex_marg.html\n","MS,D0;8;4;3;2;5;9;7;1;6;10;11;12;13;14;15","Enter Margolus specification") # defaults to BBM

# pull out the 16 numeric tokens that tell us what each partition becomes
becomes = map(int,s.replace('M',' ').replace('S',' ').replace(',',' ').replace('D',' ').replace(';',' ').split())

# we should be able to write straight into the Rules folder, so we can call setrule immediately
# (we assume we're in Scripts/Python/Margolus)
folder = '../../../Rules/'

# construct a rule_name from next case indices
rule_name = 'Margolus-' + '-'.join(map(str,becomes))
# ask the user to confirm this name or suggest a more readable one (e.g. "BBM")
rule_name = g.getstring("Enter a name for the rule:",rule_name,"Enter rule name")

# todo: detect slashes and tildes, tell user that can't change dir like that

# open the rule table file for writing
f = open(folder + rule_name + '.table','w')

# write the initial descriptors and some variables
f.write('# Emulation of Margolus neighbourhood for MCell string:\n# %s\n\n'%s)
f.write('# (see: http://psoup.math.wisc.edu/mcell/rullex_marg.html )\n')
f.write('#\n')
f.write('# Rule table produced by Scripts/Python/Margolus/make-Margolus.py, which\n')
f.write('# can convert any MCell Margolus specification into a rule table.\n')
f.write('#\n')
f.write('# At the time of writing, Golly does not support the Margolus neighborhood,\n')
f.write('# so we are emulating it by using some extra states. Configurations must\n')
f.write('# have the correct pattern of states to work correctly - you can use the\n')
f.write('# scripts import.py and export.py in Scripts/Python/Margolus for easier\n')
f.write('# drawing.\n')
f.write('#\n')
f.write('# cell states:\n')
f.write('# 0 : background (never changes)\n')
f.write('# 1 :  o  (a cell is present, this isn\'t a top-left)\n')
f.write('# 2 :  .  (a cell isn\'t present, this isn\'t a top-left)\n')
f.write('# 3 : [o] (a cell is present, this is a top-left)\n')
f.write('# 4 : [.] (a cell isn\'t present, this is a top-left)\n')
f.write('#\n')
f.write('n_states:5\nneighborhood:Moore\nsymmetries:none\n\n')
f.write('# variables standing for any state that is currently in the top-left of a partition:\n')
f.write('var a={0,3,4}\n')
f.write('var b={0,3,4}\n')
f.write('var c={0,3,4}\n')
f.write('# variables standing for any state that is currently not in the top-left of a partition:\n')
f.write('var d={0,1,2}\n')
f.write('var e={0,1,2}\n')
f.write('var f={0,1,2}\n')
f.write('var g={0,1,2}\n')
f.write('var h={0,1,2}\n')
f.write('# variables standing for an empty cell or off-grid:\n')
f.write('var i={0,2}\n')
f.write('var j={0,2}\n')
f.write('var k={0,2}\n')
f.write('var l={0,4}\n')
f.write('var m={0,4}\n')
f.write('var n={0,4}\n\n')

# cell states:
# 0 : background (never changes)
# 1 :  o  (a cell is present, this isn't a top-left)
# 2 :  .  (a cell isn't present, this isn't a top-left)
# 3 : [o] (a cell is present, this is a top-left)
# 4 : [.] (a cell isn't present, this is a top-left)
# (this is a slightly odd choice but the reasoning was that mod 2 these become the natural cell values)

# the 16 cases of the (two-state) Margolus partition are:
dot = (0,0,0,0),(1,0,0,0),(0,1,0,0),(1,1,0,0),(0,0,1,0),(1,0,1,0),(0,1,1,0),(1,1,1,0),\
      (0,0,0,1),(1,0,0,1),(0,1,0,1),(1,1,0,1),(0,0,1,1),(1,0,1,1),(0,1,1,1),(1,1,1,1)
#
# case 0: [.] .     case 1:  [o] .    case 2:  [.] o    case 3:  [o] o        case 4:  [.] .     etc.
#          .  .               .  .              .  .              .  .                  o  .
#
# (see: http://psoup.math.wisc.edu/mcell/rullex_marg.html )

# for the interface between emulated area and off-grid to work, we need to provide for either case by using variables
# (all off-grid cells count as 'off' cells)
remap = ('0','1','i','3','l'), ('0','1','j','3','m'), ('0','1','k','3','n')
# (we need to use successive variables i,j,k and l,m,n because variables in .table files are bound within each transition)

# work through the 16 cases
for iCase in range(0,16):

   iNewCase = becomes[iCase]
   f.write('# transition from case %d to %d:\n'%(iCase,iNewCase))

   # this cell is currently in the top-left of the partition
   input = 4-dot[iCase][0]
   output = 2-dot[iNewCase][0]
   if input != output:
      f.write('%d,d,e,%s,%s,%s,f,g,h,%d\n'%(input,
		remap[0][2-dot[iCase][1]],remap[1][2-dot[iCase][3]],remap[2][2-dot[iCase][2]],output))

   # this cell is currently in the top-right of the partition
   input = 2-dot[iCase][1]
   output = 2-dot[iNewCase][1]
   if input != output:
      f.write('%d,d,e,a,f,%s,%s,%s,g,%d\n'%(input,
		remap[0][2-dot[iCase][3]],remap[1][2-dot[iCase][2]],remap[2][4-dot[iCase][0]],output))

   # this cell is currently in the bottom-left of the partition
   input = 2-dot[iCase][2]
   output = 2-dot[iNewCase][2]
   if input != output:
      f.write('%d,%s,%s,%s,d,a,e,f,g,%d\n'%(input,
		remap[0][4-dot[iCase][0]],remap[1][2-dot[iCase][1]],remap[2][2-dot[iCase][3]],output))

   # this cell is currently in the bottom-right of the partition (becomes top-left on the next generation)
   input = 2-dot[iCase][3]
   output = 4-dot[iNewCase][3]
   if input != output:
      f.write('%d,%s,a,d,b,e,c,%s,%s,%d\n'%(input,
		remap[0][2-dot[iCase][1]],remap[1][2-dot[iCase][2]],remap[2][4-dot[iCase][0]],output))

# we're done writing the rule table
f.close()

# let's also write out a default colour scheme
cf = open(folder+rule_name + '.colors','w')
cf.write('color=1 192 192 192\ncolor=2   0  64 128\ncolor=3 255 255 255\ncolor=4   0 100 150')
cf.close()

# now we can directly set the rule
g.setrule(rule_name)

# tell the user what we wrote, and what to do next
g.note('Wrote file: '+folder+rule_name+'.table\nWrote file: '+folder+rule_name+'.colors\nSet rule: '+rule_name+'\n\nYou can use the scripts import.py and export.py to initialise an area for use with the Margolus rule just created.')

