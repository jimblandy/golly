from copy import deepcopy

def WriteRuleTable(neighborhood,n_states,transitions,output_filename):
    '''Write a rule table format file for the given rule.'''

    def toName(i,output=''):
        '''Convert [0,1,2,...,26,...] to ['a','b','c',...,'aa',...].'''
        output = chr(ord('a')+i%26) + output
        if i<26: return output
        return toName(i//26-1,output)

    f = open(output_filename,'w')
    f.write('neighborhood:'+neighborhood+'\n')
    f.write('symmetries:none\n') # our transitions list is expanded for symmetries
    f.write('n_states:'+str(n_states)+'\n\n')

    vars = {} # e.g. (0,1,2):['aa','bf'] (need multiple names to avoid binding)
    iNextName = 0

    # pass 1: collect and output the variables, replacing inputs with strings
    transitions_with_strings = deepcopy(transitions) # don't want to change our input list
    for t in transitions_with_strings:
        for i,inputs in enumerate(t):
            if len(inputs)==1:
                t[i]=str(inputs[0])
            else:
                # do we need to make a new variable for this input?
                ok = False
                if tuple(inputs) in vars:
                    for var_name in vars[tuple(inputs)]:
                        if not var_name in t:
                            ok = True
                            # can use an existing variable
                            break
                if not ok:
                    # must make a new variable
                    var_name = toName(iNextName)
                    iNextName += 1
                    if tuple(inputs) in vars:
                        vars[tuple(inputs)] += [var_name]
                    else:
                        vars[tuple(inputs)] = [var_name]
                    f.write('var '+var_name+'={'+','.join(map(str,inputs))+'}\n')
                t[i] = var_name

    # pass 2: output the transitions
    f.write('\n')
    for t in transitions_with_strings:
        f.write(','.join(t)+'\n')

    f.flush()
    f.close()
