from glife.RuleTree import *
import os

def EmulateOneDimensional(neighborhood,n_states,transitions,input_filename):
    rule_name = os.path.splitext(os.path.split(input_filename)[1])[0]
    # (we use no special suffix so that we pick up any existing .colors or .icons)
    tree = RuleTree(n_states,4)
    any_state = range(n_states)
    for t in transitions:
        tree.add_rule([t[0],any_state,t[2],t[1],any_state],t[3][0]) # C,S,E,W,N,C'
        # (last entry of the transition should be a single integer)
    tree.write( golly.getdir('rules')+rule_name+".tree" )
    return rule_name
