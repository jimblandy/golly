# We owe a lot to MCell's implementation of the Margolus neighbourhood. Thanks Mirek!
#
# Tim Hutton <tim.hutton@gmail.com>

import golly as g

# ask the user for the MCell string to convert (comma-separated works too)
s = g.getstring("Enter a specification string.\n\nThis can either be an MCell format Margolus\nstring or just a comma-separated list of the 16 case indices.\n\nSee: http://psoup.math.wisc.edu/mcell/rullex_marg.html\n","MS,D0;8;4;3;2;5;9;7;1;6;10;11;12;13;14;15","Enter Margolus specification") # defaults to BBM

# pull out the 16 numeric tokens that tell us what each partition becomes
becomes = map(int,s.replace('M',' ').replace('S',' ').replace(',',' ').replace('D',' ').replace(';',' ').split())

# we should be able to write straight into the user's rules folder,
# so we can call setrule immediately
folder = g.getdir('rules')

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
f.write('# Rule table produced by convert-MCell-string.py, which can\n')
f.write('# convert any MCell Margolus specification into a rule table.\n')
f.write('#\n')
f.write('n_states:2\nneighborhood:Margolus\nsymmetries:none\n\n')

# the 16 cases of the (two-state) Margolus partition are:
dot = (0,0,0,0),(1,0,0,0),(0,1,0,0),(1,1,0,0),(0,0,1,0),(1,0,1,0),(0,1,1,0),(1,1,1,0),\
      (0,0,0,1),(1,0,0,1),(0,1,0,1),(1,1,0,1),(0,0,1,1),(1,0,1,1),(0,1,1,1),(1,1,1,1)
# cell order: 1  2
#             3  4
# (see: http://psoup.math.wisc.edu/mcell/rullex_marg.html )

for i in range(16):
    if not i==becomes[i]: 
        # (we can skip no-change transitions)
        f.write(','.join(map(str,dot[i]))+' : '+\
            ','.join(map(str,dot[becomes[i]]))+\
            '   # '+str(i)+' -> '+str(becomes[i])+'\n')
f.close()

# tell the user what we wrote, and what to do next
g.note('Wrote file: '+folder+rule_name+'.table\n\n\
You can now use RuleTableToTree.py to emulate the rule table.')

