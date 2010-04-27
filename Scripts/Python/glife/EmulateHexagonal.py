import os
from glife.RuleTree import *

def EmulateHexagonal(neighborhood,n_states,transitions,input_filename):
    '''Emulate a hexagonal neighborhood rule table with a Moore neighborhood rule tree.'''
    rule_name = os.path.splitext(os.path.split(input_filename)[1])[0]
    tree = RuleTree(n_states,8)
    for t in transitions:
        # C,S,E,W,N,SE,SW,NE,NW
        tree.add_rule([t[0],t[4],t[3],t[6],t[1],range(n_states),t[5],t[2],range(n_states)],t[7][0])
    tree.write( golly.getdir('rules')+rule_name+".tree" )
    return rule_name
