try:
  set
except NameError:
  # use sets module if Python version is < 2.4
  from sets import Set as set
  
import golly

SupportedSymmetries = {
"vonNeumann":
{
'none':[[0,1,2,3,4,5]],
'rotate4':[[0,1,2,3,4,5],[0,2,3,4,1,5],[0,3,4,1,2,5],[0,4,1,2,3,5]],
'rotate4reflect':[[0,1,2,3,4,5],[0,2,3,4,1,5],[0,3,4,1,2,5],[0,4,1,2,3,5],
    [0,4,3,2,1,5],[0,3,2,1,4,5],[0,2,1,4,3,5],[0,1,4,3,2,5]],
'reflect_horizontal':[[0,1,2,3,4,5],[0,1,4,3,2,5]],
'reflect':[[0,1,2,3,4,5],[0,1,4,3,2,5]], # synonym for reflect_horizontal
},
"Moore":
{
'none':[[0,1,2,3,4,5,6,7,8,9]],
'rotate4':[[0,1,2,3,4,5,6,7,8,9],[0,3,4,5,6,7,8,1,2,9],[0,5,6,7,8,1,2,3,4,9],[0,7,8,1,2,3,4,5,6,9]],
'rotate8':[[0,1,2,3,4,5,6,7,8,9],[0,2,3,4,5,6,7,8,1,9],[0,3,4,5,6,7,8,1,2,9],[0,4,5,6,7,8,1,2,3,9],\
    [0,5,6,7,8,1,2,3,4,9],[0,6,7,8,1,2,3,4,5,9],[0,7,8,1,2,3,4,5,6,9],[0,8,1,2,3,4,5,6,7,9]],
'rotate4reflect':[[0,1,2,3,4,5,6,7,8,9],[0,3,4,5,6,7,8,1,2,9],[0,5,6,7,8,1,2,3,4,9],[0,7,8,1,2,3,4,5,6,9],\
    [0,8,7,6,5,4,3,2,1,9],[0,6,5,4,3,2,1,8,7,9],[0,4,3,2,1,8,7,6,5,9],[0,2,1,8,7,6,5,4,3,9]],
'rotate8reflect':[[0,1,2,3,4,5,6,7,8,9],[0,2,3,4,5,6,7,8,1,9],[0,3,4,5,6,7,8,1,2,9],[0,4,5,6,7,8,1,2,3,9],\
    [0,5,6,7,8,1,2,3,4,9],[0,6,7,8,1,2,3,4,5,9],[0,7,8,1,2,3,4,5,6,9],[0,8,1,2,3,4,5,6,7,9],\
    [0,8,7,6,5,4,3,2,1,9],[0,7,6,5,4,3,2,1,8,9],[0,6,5,4,3,2,1,8,7,9],[0,5,4,3,2,1,8,7,6,9],\
    [0,4,3,2,1,8,7,6,5,9],[0,3,2,1,8,7,6,5,4,9],[0,2,1,8,7,6,5,4,3,9],[0,1,8,7,6,5,4,3,2,9]],
'reflect_horizontal':[[0,1,2,3,4,5,6,7,8,9],[0,1,8,7,6,5,4,3,2,9]],
'reflect':[[0,1,2,3,4,5,6,7,8,9],[0,1,8,7,6,5,4,3,2,9]], # synonym for reflect_horizontal
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
    [1,0,3,2,5,4,7,6],[0,2,1,3,4,6,5,7],[2,3,0,1,6,7,4,5],[3,1,2,0,7,5,6,4]]
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
    [1,0,3,2,5,4,7,6],[0,2,1,3,4,6,5,7],[2,3,0,1,6,7,4,5],[3,1,2,0,7,5,6,4]]
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
    [1,0,3,2,5,4,7,6],[0,2,1,3,4,6,5,7],[2,3,0,1,6,7,4,5],[3,1,2,0,7,5,6,4]]
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
    [1,0,3,2,5,4,7,6],[0,2,1,3,4,6,5,7],[2,3,0,1,6,7,4,5],[3,1,2,0,7,5,6,4]]
},
"triangularVonNeumann":
{
'rotate':[[0,1,2,3,4],[0,3,1,2,4],[0,2,3,1,4]],
'rotate_reflect':[[0,1,2,3,4],[0,3,1,2,4],[0,2,3,1,4],[0,3,2,1,4],[0,1,3,2,4],[0,2,1,3,4]]
}
}

# TODO: detect bad input
def ReadRuleTable(filename):
    '''
    Return n_states, neighborhood, transitions
    e.g. 2, "vonNeumann", [[0],[1],[0],[0],[1],[1]]
    Transitions are expanded for symmetries and variables.
    '''
    f=open(filename,'r')
    vars={}
    symmetry = []
    n_states = 0
    neighborhood = ''
    transitions = []
    numParams = 0
    for line in f:
        if line[0]=='#' or line.strip()=='':
            pass
        elif line[0:4]=='var ':
            line = line[4:line.find('#')] # strip var keyword and any trailing comment
            # read each variable into a dictionary mapping string to list of ints
            entries = line.replace('=',' ').replace('{',' ').replace(',',' ').\
                replace(':',' ').replace('}',' ').replace('\n','').split()
            vars[entries[0]] = []
            for entry in entries[1:]:
                if entry in vars:
                    vars[entries[0]] += vars[entry] # vars permitted inside later vars
                else:
                    vars[entries[0]] += int(entry)
        elif line[0:9]=='n_states:':
            n_states = int(line[9:])
            # add each state to the variables dictionary 
            # (simplifies code in next section)
            for i in range(n_states):
                vars[str(i)] = [i]
        elif line[0:13]=='neighborhood:':
            neighborhood = line[13:].strip()
            if not neighborhood in SupportedSymmetries:
                golly.warn('Unknown neighborhood: '+neighborhood)
                golly.exit()
            numParams = len(SupportedSymmetries[neighborhood].items()[0][1][0])
        elif line[0:11]=='symmetries:':
            symmetry_string = line[11:].strip()
            if not symmetry_string in SupportedSymmetries[neighborhood]:
                golly.warn('Unknown symmetry: '+symmetry_string)
                golly.exit()
            symmetry = SupportedSymmetries[neighborhood][symmetry_string]
        else:
            # assume line is a transition
            entries = line.replace('\n','').replace(',',' ').replace(':',' ').split()[:numParams]
            # we need to expand the variables
            vars_used = list(set(entries)) # (removes duplicates)
            var_val_indices = dict(zip(vars_used,[0]*len(vars_used)))
            # iterate through all the possible values of each used variable
            # (this ensures that bound variables keep matching values)
            while True:
                # collect the transitions, expanded for symmetry
                for s in symmetry:
                    new_transition = [ [vars[entries[i]][var_val_indices[entries[i]]]] for i in s ]
                    if not new_transition in transitions: # an optimisation
                        transitions.append(new_transition)
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
    f.close()
    return n_states, neighborhood, transitions

