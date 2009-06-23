# emulate-Margolus-table.py
# reads a Margolus rule table, and produces a Moore-nhood rule table that
# emulates it.

import golly
import os

# ask user to select .table file
fn = golly.opendialog('Open a Margolus table to emulate',
                      'Rule tables (*.table)|*.table',
                      golly.getdir('rules'))

if len(fn) == 0: golly.exit()    # user hit Cancel

# state s becomes 1+2s and 2+2s, the first for when the cell is currently
# the top-left square of the Margolus partition, the other for when it isn't.

def as_top_left(s):
    return 1+2*s
def as_not_top_left(s):
    return 2+2*s

# for the various symmetries we need to remap the inputs and outputs:
symm = {
'none':[[0,1,2,3,4,5,6,7]],
'reflect_horizontal':[[0,1,2,3,4,5,6,7],[1,0,3,2,5,4,7,6]],
'reflect_vertical':[[0,1,2,3,4,5,6,7],[2,3,0,1,6,7,4,5]],
'rotate4':
    [[0,1,2,3,4,5,6,7],[2,0,3,1,6,4,7,5],[3,2,1,0,7,6,5,4],[1,3,0,2,5,7,4,6]],
'rotate4reflect':[
    [0,1,2,3,4,5,6,7],[2,0,3,1,6,4,7,5],[3,2,1,0,7,6,5,4],[1,3,0,2,5,7,4,6],
    [1,0,3,2,5,4,7,6],[0,2,1,3,4,6,5,7],[2,3,0,1,6,7,4,5],[3,1,2,0,7,5,6,4]]
}

rule_name = os.path.splitext(os.path.split(fn)[1])[0]
f = open(fn,'r')
g = open(golly.getdir('rules')+rule_name+'-emulated.table','w')
g.write('# Automatically produced by emulate-Margolus-table.py\n')
g.write('# from '+rule_name+'.table\n\n')
g.write('neighborhood:Moore\nsymmetries:none\n')
n_states=0
vars={}
symmetry = []
for line in f:
    if line[0]=='#' or line.strip()=='':
        # write this comment line into the destination file
        g.write(line)
    elif line[0:4]=='var ':
        # read each variable into a dictionary mapping string to list of ints
        entries = line[4:].replace('=',' ').replace('{',' ').replace('{',' ').\
            replace(',',' ').replace('}',' ').replace('\n',' ').split()
        vars[entries[0]] = [int(s) for s in entries[1:]]
    elif line[0:9]=='n_states:':
        n_states = int(line[9:])
        # add each state to the variables dictionary 
        # (simplifies code in next section)
        for i in range(n_states):
            vars[str(i)] = [i]
        g.write('n_states:'+str(1+n_states*2)+'\n')
        g.write('# variables standing for any state that is currently \
in the top-left of a partition (or off-grid):\n')
        g.write('var _a1={0,'+
            ','.join(str(as_top_left(s)) for s in range(n_states))+'}\n')
        g.write('var _a2={0,'+
            ','.join(str(as_top_left(s)) for s in range(n_states))+'}\n')
        g.write('var _a3={0,'+
            ','.join(str(as_top_left(s)) for s in range(n_states))+'}\n')
        g.write('var _a4={0,'+
            ','.join(str(as_top_left(s)) for s in range(n_states))+'}\n')
        g.write('# variables standing for any state that is currently \
not in the top-left of a partition:\n')
        g.write('var _b1={0,'+
            ','.join(str(as_not_top_left(s)) for s in range(n_states))+'}\n')
        g.write('var _b2={0,'+
            ','.join(str(as_not_top_left(s)) for s in range(n_states))+'}\n')
        g.write('var _b3={0,'+
            ','.join(str(as_not_top_left(s)) for s in range(n_states))+'}\n')
        g.write('var _b4={0,'+
            ','.join(str(as_not_top_left(s)) for s in range(n_states))+'}\n')
        g.write('var _b5={0,'+
            ','.join(str(as_not_top_left(s)) for s in range(n_states))+'}\n')
        g.write('var _b6={0,'+
            ','.join(str(as_not_top_left(s)) for s in range(n_states))+'}\n')
        g.write('var _b7={0,'+
            ','.join(str(as_not_top_left(s)) for s in range(n_states))+'}\n')
        g.write('var _b8={0,'+
            ','.join(str(as_not_top_left(s)) for s in range(n_states))+'}\n')
    elif line[0:13]=='neighborhood:':
        if not line[13:].strip()=='Margolus':
            golly.exit('Unknown neighborhood: '+line[13:-1])
    elif line[0:11]=='symmetries:':
        if not line[11:].strip() in symm:
            golly.exit('Unknown symmetry: '+line[11:])
        symmetry = symm[line[11:].strip()]
    else:
        # assume line is a Margolus transition
        entries = [s for s in line.replace('\n',' ').replace(',',' ').\
            replace(':',' ').split()][:8]
        # we need to expand variables (because of change-of-state with
        # partitioning trick)
        vars_used = list(set(entries))
        var_val_indices = dict(zip(vars_used,[0]*len(vars_used)))
        g.write('# '+line)
        # iterate through all the possible values of each variable
        while True:
            # output different versions for each Margolus symmetry 
            # (we can't use the Moore symmetries)
            for iSymm in range(len(symmetry)):
                # output 4 version of this transition: one for each
                # cell of the partition
                # case 0: central cell is the top-left: (but not next time)
                g.write('%s,_b1,_b2,%s,%s,%s,_b3,_b4,_b5,%s    # TL -> BR\n'\
                    %(as_top_left(vars[entries[symmetry[iSymm][0]]]\
                    [var_val_indices[entries[symmetry[iSymm][0]]]]),\
                    as_not_top_left(vars[entries[symmetry[iSymm][1]]]
                    [var_val_indices[entries[symmetry[iSymm][1]]]]),\
                    as_not_top_left(vars[entries[symmetry[iSymm][3]]]\
                    [var_val_indices[entries[symmetry[iSymm][3]]]]),\
                    as_not_top_left(vars[entries[symmetry[iSymm][2]]]\
                    [var_val_indices[entries[symmetry[iSymm][2]]]]),\
                    as_not_top_left(vars[entries[symmetry[iSymm][4]]]\
                    [var_val_indices[entries[symmetry[iSymm][4]]]])))
                # case 1: central cell is the top-right:
                g.write('%s,_b1,_b2,_a1,_b3,%s,%s,%s,_b4,%s    # TR -> BL\n'\
                    %(as_not_top_left(vars[entries[symmetry[iSymm][1]]]\
                    [var_val_indices[entries[symmetry[iSymm][1]]]]),\
                    as_not_top_left(vars[entries[symmetry[iSymm][3]]]\
                    [var_val_indices[entries[symmetry[iSymm][3]]]]),\
                    as_not_top_left(vars[entries[symmetry[iSymm][2]]]\
                    [var_val_indices[entries[symmetry[iSymm][2]]]]),\
                    as_top_left(vars[entries[symmetry[iSymm][0]]]\
                    [var_val_indices[entries[symmetry[iSymm][0]]]]),\
                    as_not_top_left(vars[entries[symmetry[iSymm][5]]]\
                    [var_val_indices[entries[symmetry[iSymm][5]]]])))
                # case 2: central cell is the bottom-left:
                g.write('%s,%s,%s,%s,_b1,_a1,_b2,_b3,_b4,%s    # BL -> TR\n'\
                    %(as_not_top_left(vars[entries[symmetry[iSymm][2]]]
                    [var_val_indices[entries[symmetry[iSymm][2]]]]),\
                    as_top_left(vars[entries[symmetry[iSymm][0]]]\
                    [var_val_indices[entries[symmetry[iSymm][0]]]]),\
                    as_not_top_left(vars[entries[symmetry[iSymm][1]]]
                    [var_val_indices[entries[symmetry[iSymm][1]]]]),\
                    as_not_top_left(vars[entries[symmetry[iSymm][3]]]\
                    [var_val_indices[entries[symmetry[iSymm][3]]]]),\
                    as_not_top_left(vars[entries[symmetry[iSymm][6]]]
                    [var_val_indices[entries[symmetry[iSymm][6]]]])))
                # case 3: central cell is bottom-right: 
                # (becomes top-left next time)
                g.write('%s,%s,_a1,_b1,_a2,_b2,_a3,%s,%s,%s    # BR -> TL\n'\
                    %(as_not_top_left(vars[entries[symmetry[iSymm][3]]]\
                    [var_val_indices[entries[symmetry[iSymm][3]]]]),\
                    as_not_top_left(vars[entries[symmetry[iSymm][1]]]\
                    [var_val_indices[entries[symmetry[iSymm][1]]]]),\
                    as_not_top_left(vars[entries[symmetry[iSymm][2]]]\
                    [var_val_indices[entries[symmetry[iSymm][2]]]]),\
                    as_top_left(vars[entries[symmetry[iSymm][0]]]\
                    [var_val_indices[entries[symmetry[iSymm][0]]]]),\
                    as_top_left(vars[entries[symmetry[iSymm][7]]]\
                    [var_val_indices[entries[symmetry[iSymm][7]]]])))
            # increment the variable values (or break out if done)
            var_val_to_change = 0
            while var_val_to_change<len(vars_used):
                var_label = vars_used[var_val_to_change]
                if var_val_indices[var_label]<len(vars[var_label])-1:
                    var_val_indices[var_label] += 1
                    break
                else:
                    var_val_indices[var_label] = 0
                    var_val_to_change += 1
            if var_val_to_change >= len(vars_used):
                break

# we also need a generic case for no-change
g.write('# default: no change (but partition moves)\n')
for s in range(n_states):
    # case 0: central cell is the top-left (but not next time)
    g.write('%d,_b1,_b2,_b3,_b4,_b5,_b6,_b7,_b8,%d    # TL -> BR\n'\
        %(as_top_left(s),as_not_top_left(s)))
    # case 1: central cell is the top-right
    g.write('%d,_b1,_b2,_a1,_b3,_b4,_b5,_a2,_b6,%d    # TR -> BL\n'\
        %(as_not_top_left(s),as_not_top_left(s)))
    # case 2: central cell is the bottom-left
    g.write('%d,_a1,_b1,_b2,_b3,_a2,_b4,_b5,_b6,%d    # BL -> TR\n'\
        %(as_not_top_left(s),as_not_top_left(s)))
    # case 3: central cell is the bottom-right (becomes top-left next time)
    g.write('%d,_b1,_a1,_b2,_a2,_b3,_a3,_b4,_a4,%d    # BR -> TL\n'\
        %(as_not_top_left(s),as_top_left(s)))
g.close()
f.close()
golly.setrule(rule_name+'-emulated')

