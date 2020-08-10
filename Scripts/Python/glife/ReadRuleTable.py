try:
    set
except NameError:
    # use sets module if Python version is < 2.4
    from sets import Set as set

import golly

# generate permutations where the input list may have duplicates
# e.g. [1,2,1] -> [[1, 2, 1], [1, 1, 2], [2, 1, 1]]
# (itertools.permutations would yield 6 and removing duplicates afterwards is less efficient)
# http://code.activestate.com/recipes/496819/ (PSF license)
def permu2(xs):
    if len(xs)<2: yield xs
    else:
        h = []
        for x in xs:
            h.append(x)
            if x in h[:-1]: continue
            ts = xs[:]; ts.remove(x)
            for ps in permu2(ts):
                yield [x]+ps

# With some neighborhoods we permute after building each transition, to avoid
# creating lots of copies of the same rule. The others are permuted explicitly
# because we haven't worked out how to permute the Margolus neighborhood while
# allowing for duplicates.
PermuteLater = ['vonNeumann','Moore','hexagonal','triangularVonNeumann',
    'triangularMoore','oneDimensional']

SupportedSymmetries = {
"vonNeumann":
{
'none':[[0,1,2,3,4,5]],
'rotate4':[[0,1,2,3,4,5],[0,2,3,4,1,5],[0,3,4,1,2,5],[0,4,1,2,3,5]],
'rotate4reflect':[[0,1,2,3,4,5],[0,2,3,4,1,5],[0,3,4,1,2,5],[0,4,1,2,3,5],
    [0,4,3,2,1,5],[0,3,2,1,4,5],[0,2,1,4,3,5],[0,1,4,3,2,5]],
'reflect_horizontal':[[0,1,2,3,4,5],[0,1,4,3,2,5]],
'permute':[[0,1,2,3,4,5]], # (gets done later)
},
"Moore":
{
'none':[[0,1,2,3,4,5,6,7,8,9]],
'rotate4':[[0,1,2,3,4,5,6,7,8,9],[0,3,4,5,6,7,8,1,2,9],[0,5,6,7,8,1,2,3,4,9],[0,7,8,1,2,3,4,5,6,9]],
'rotate8':[[0,1,2,3,4,5,6,7,8,9],[0,2,3,4,5,6,7,8,1,9],[0,3,4,5,6,7,8,1,2,9],[0,4,5,6,7,8,1,2,3,9],\
    [0,5,6,7,8,1,2,3,4,9],[0,6,7,8,1,2,3,4,5,9],[0,7,8,1,2,3,4,5,6,9],[0,8,1,2,3,4,5,6,7,9]],
'rotate4reflect':[[0,1,2,3,4,5,6,7,8,9],[0,3,4,5,6,7,8,1,2,9],[0,5,6,7,8,1,2,3,4,9],[0,7,8,1,2,3,4,5,6,9],\
    [0,1,8,7,6,5,4,3,2,9],[0,7,6,5,4,3,2,1,8,9],[0,5,4,3,2,1,8,7,6,9],[0,3,2,1,8,7,6,5,4,9]],
'rotate8reflect':[[0,1,2,3,4,5,6,7,8,9],[0,2,3,4,5,6,7,8,1,9],[0,3,4,5,6,7,8,1,2,9],[0,4,5,6,7,8,1,2,3,9],\
    [0,5,6,7,8,1,2,3,4,9],[0,6,7,8,1,2,3,4,5,9],[0,7,8,1,2,3,4,5,6,9],[0,8,1,2,3,4,5,6,7,9],\
    [0,8,7,6,5,4,3,2,1,9],[0,7,6,5,4,3,2,1,8,9],[0,6,5,4,3,2,1,8,7,9],[0,5,4,3,2,1,8,7,6,9],\
    [0,4,3,2,1,8,7,6,5,9],[0,3,2,1,8,7,6,5,4,9],[0,2,1,8,7,6,5,4,3,9],[0,1,8,7,6,5,4,3,2,9]],
'reflect_horizontal':[[0,1,2,3,4,5,6,7,8,9],[0,1,8,7,6,5,4,3,2,9]],
'permute':[[0,1,2,3,4,5,6,7,8,9]] # (gets done later)
},
"Margolus":
{
'none':[[0,1,2,3,4,5,6,7]],
'reflect_horizontal':[[0,1,2,3,4,5,6,7],[1,0,3,2,5,4,7,6]],
'reflect_vertical':[[0,1,2,3,4,5,6,7],[2,3,0,1,6,7,4,5]],
'rotate4':
    [[0,1,2,3,4,5,6,7],[2,0,3,1,6,4,7,5],[3,2,1,0,7,6,5,4],[1,3,0,2,5,7,4,6]],
'rotate4reflect':[
    [0,1,2,3,4,5,6,7],[2,0,3,1,6,4,7,5],[3,2,1,0,7,6,5,4],[1,3,0,2,5,7,4,6],
    [1,0,3,2,5,4,7,6],[0,2,1,3,4,6,5,7],[2,3,0,1,6,7,4,5],[3,1,2,0,7,5,6,4]],
'permute':[p+[x+4 for x in p] for p in permu2(list(range(4)))]
},
"square4_figure8v": # same symmetries as Margolus
{
'none':[[0,1,2,3,4,5,6,7]],
'reflect_horizontal':[[0,1,2,3,4,5,6,7],[1,0,3,2,5,4,7,6]],
'reflect_vertical':[[0,1,2,3,4,5,6,7],[2,3,0,1,6,7,4,5]],
'rotate4':
    [[0,1,2,3,4,5,6,7],[2,0,3,1,6,4,7,5],[3,2,1,0,7,6,5,4],[1,3,0,2,5,7,4,6]],
'rotate4reflect':[
    [0,1,2,3,4,5,6,7],[2,0,3,1,6,4,7,5],[3,2,1,0,7,6,5,4],[1,3,0,2,5,7,4,6],
    [1,0,3,2,5,4,7,6],[0,2,1,3,4,6,5,7],[2,3,0,1,6,7,4,5],[3,1,2,0,7,5,6,4]],
'permute':[p+[x+4 for x in p] for p in permu2(list(range(4)))]
},
"square4_figure8h": # same symmetries as Margolus
{
'none':[[0,1,2,3,4,5,6,7]],
'reflect_horizontal':[[0,1,2,3,4,5,6,7],[1,0,3,2,5,4,7,6]],
'reflect_vertical':[[0,1,2,3,4,5,6,7],[2,3,0,1,6,7,4,5]],
'rotate4':
    [[0,1,2,3,4,5,6,7],[2,0,3,1,6,4,7,5],[3,2,1,0,7,6,5,4],[1,3,0,2,5,7,4,6]],
'rotate4reflect':[
    [0,1,2,3,4,5,6,7],[2,0,3,1,6,4,7,5],[3,2,1,0,7,6,5,4],[1,3,0,2,5,7,4,6],
    [1,0,3,2,5,4,7,6],[0,2,1,3,4,6,5,7],[2,3,0,1,6,7,4,5],[3,1,2,0,7,5,6,4]],
'permute':[p+[x+4 for x in p] for p in permu2(list(range(4)))]
},
"square4_cyclic": # same symmetries as Margolus
{
'none':[[0,1,2,3,4,5,6,7]],
'reflect_horizontal':[[0,1,2,3,4,5,6,7],[1,0,3,2,5,4,7,6]],
'reflect_vertical':[[0,1,2,3,4,5,6,7],[2,3,0,1,6,7,4,5]],
'rotate4':
    [[0,1,2,3,4,5,6,7],[2,0,3,1,6,4,7,5],[3,2,1,0,7,6,5,4],[1,3,0,2,5,7,4,6]],
'rotate4reflect':[
    [0,1,2,3,4,5,6,7],[2,0,3,1,6,4,7,5],[3,2,1,0,7,6,5,4],[1,3,0,2,5,7,4,6],
    [1,0,3,2,5,4,7,6],[0,2,1,3,4,6,5,7],[2,3,0,1,6,7,4,5],[3,1,2,0,7,5,6,4]],
'permute':[p+[x+4 for x in p] for p in permu2(list(range(4)))]
},
"triangularVonNeumann":
{
'none':[[0,1,2,3,4]],
'rotate':[[0,1,2,3,4],[0,3,1,2,4],[0,2,3,1,4]],
'rotate_reflect':[[0,1,2,3,4],[0,3,1,2,4],[0,2,3,1,4],[0,3,2,1,4],[0,1,3,2,4],[0,2,1,3,4]],
'permute':[[0,1,2,3,4]] # (gets done later)
},
"triangularMoore":
{
'none':[[0,1,2,3,4,5,6,7,8,9,10,11,12,13]],
'rotate':[[0,1,2,3,4,5,6,7,8,9,10,11,12,13],
          [0,2,3,1,7,8,9,10,11,12,4,5,6,13],
          [0,3,1,2,10,11,12,4,5,6,7,8,9,13]],
'rotate_reflect':[[0,1,2,3,4,5,6,7,8,9,10,11,12,13],
                  [0,2,3,1,7,8,9,10,11,12,4,5,6,13],
                  [0,3,1,2,10,11,12,4,5,6,7,8,9,13],
                  [0,3,2,1,9,8,7,6,5,4,12,11,10,13],
                  [0,2,1,3,6,5,4,12,11,10,9,8,7,13],
                  [0,1,3,2,12,11,10,9,8,7,6,5,4,13]],
'permute':[[0,1,2,3,4,5,6,7,8,9,10,11,12,13]], # (gets done later)
},
"oneDimensional":
{
'none':[[0,1,2,3]],
'reflect':[[0,1,2,3],[0,2,1,3]],
'permute':[[0,1,2,3]], # (gets done later)
},
"hexagonal":
{
'none':[[0,1,2,3,4,5,6,7]],
'rotate2':[[0,1,2,3,4,5,6,7],[0,4,5,6,1,2,3,7]],
'rotate3':[[0,1,2,3,4,5,6,7],[0,3,4,5,6,1,2,7],[0,5,6,1,2,3,4,7]],
'rotate6':[[0,1,2,3,4,5,6,7],[0,2,3,4,5,6,1,7],[0,3,4,5,6,1,2,7],
           [0,4,5,6,1,2,3,7],[0,5,6,1,2,3,4,7],[0,6,1,2,3,4,5,7]],
'rotate6reflect':[[0,1,2,3,4,5,6,7],[0,2,3,4,5,6,1,7],[0,3,4,5,6,1,2,7],
                   [0,4,5,6,1,2,3,7],[0,5,6,1,2,3,4,7],[0,6,1,2,3,4,5,7],
                   [0,6,5,4,3,2,1,7],[0,5,4,3,2,1,6,7],[0,4,3,2,1,6,5,7],
                   [0,3,2,1,6,5,4,7],[0,2,1,6,5,4,3,7],[0,1,6,5,4,3,2,7]],
'permute':[[0,1,2,3,4,5,6,7]], # (gets done later)
},
}

def ReadRuleTable(filename):
    '''
    Return n_states, neighborhood, transitions
    e.g. 2, "vonNeumann", [[0],[0,1],[0],[0],[1],[1]]
    Transitions are expanded for symmetries and bound variables.
    '''
    f=open(filename,'r')
    vars={}
    symmetry_string = ''
    symmetry = []
    n_states = 0
    neighborhood = ''
    transitions = []
    numParams = 0
    for line in f:
        if line[0]=='#' or line.strip()=='':
            pass
        elif line[0:9]=='n_states:':
            n_states = int(line[9:])
            if n_states<0 or n_states>256:
                golly.warn('n_states out of range: '+n_states)
                golly.exit()
        elif line[0:13]=='neighborhood:':
            neighborhood = line[13:].strip()
            if not neighborhood in SupportedSymmetries:
                golly.warn('Unknown neighborhood: '+neighborhood)
                golly.exit()
            numParams = len(list(SupportedSymmetries[neighborhood].items())[0][1][0])
        elif line[0:11]=='symmetries:':
            symmetry_string = line[11:].strip()
            if not symmetry_string in SupportedSymmetries[neighborhood]:
                golly.warn('Unknown symmetry: '+symmetry_string)
                golly.exit()
            symmetry = SupportedSymmetries[neighborhood][symmetry_string]
        elif line[0:4]=='var ':
            line = line[4:] # strip var keyword
            if '#' in line: line = line[:line.find('#')] # strip any trailing comment
            # read each variable into a dictionary mapping string to list of ints
            entries = line.replace('=',' ').replace('{',' ').replace(',',' ').\
                replace(':',' ').replace('}',' ').replace('\n','').split()
            vars[entries[0]] = []
            for e in entries[1:]:
                if e in vars: vars[entries[0]] += vars[e] # vars allowed in later vars
                else: vars[entries[0]].append(int(e))
        else:
            # assume line is a transition
            if '#' in line: line = line[:line.find('#')] # strip any trailing comment
            if ',' in line:
                entries = line.replace('\n','').replace(',',' ').replace(':',' ').split()
            else:
                entries = list(line.strip()) # special no-comma format
            if not len(entries)==numParams:
                golly.warn('Wrong number of entries on line: '+line+' (expected '+str(numParams)+')')
                golly.exit()
            # retrieve the variables that repeat within the transition, these are 'bound'
            bound_vars = [ e for e in set(entries) if entries.count(e)>1 and e in vars ]
            # iterate through all the possible values of each bound variable
            var_val_indices = dict(list(zip(bound_vars,[0]*len(bound_vars))))
            while True:
                ### AKT: this code causes syntax error in Python 2.3:
                ### transition = [ [vars[e][var_val_indices[e]]] if e in bound_vars \
                ###                else vars[e] if e in vars \
                ###                else [int(e)] \
                ###                for e in entries ]
                transition = []
                for e in entries:
                    if e in bound_vars:
                        transition.append([vars[e][var_val_indices[e]]])
                    elif e in vars:
                        transition.append(vars[e])
                    else:
                        transition.append([int(e)])
                if symmetry_string=='permute' and neighborhood in PermuteLater:
                    # permute all but C,C' (first and last entries)
                    for permuted_section in permu2(transition[1:-1]):
                        permuted_transition = [transition[0]]+permuted_section+[transition[-1]]
                        if not permuted_transition in transitions:
                            transitions.append(permuted_transition)
                else:
                    # expand for symmetry using the explicit list
                    for s in symmetry:
                        tran = [transition[i] for i in s]
                        if not tran in transitions:
                            transitions.append(tran)
                # increment the variable values (or break out if done)
                var_val_to_change = 0
                while var_val_to_change<len(bound_vars):
                    var_label = bound_vars[var_val_to_change]
                    if var_val_indices[var_label]<len(vars[var_label])-1:
                        var_val_indices[var_label] += 1
                        break
                    else:
                        var_val_indices[var_label] = 0
                        var_val_to_change += 1
                if var_val_to_change >= len(bound_vars):
                    break
    f.close()
    return n_states, neighborhood, transitions
