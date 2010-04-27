import os
from glife.RuleTree import *

def EmulateOneDimensional(neighborhood,n_states,transitions,input_filename):
    '''Emulate a oneDimensional neighborhood rule table with a vonNeumann neighborhood rule tree.'''
    rule_name = os.path.splitext(os.path.split(input_filename)[1])[0]
    tree = RuleTree(n_states,4)
    for t in transitions:
        tree.add_rule([t[0],range(n_states),t[2],t[1],range(n_states)],t[3][0]) # C,S,E,W,N,C'
    tree.write( golly.getdir('rules')+rule_name+".tree" )
    return rule_name
