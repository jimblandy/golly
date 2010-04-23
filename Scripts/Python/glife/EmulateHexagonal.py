from glife.RuleTree import *

def EmulateHexagonal(neighborhood,n_states,transitions,input_filename):
    rule_name = os.path.splitext(os.path.split(input_filename)[1])[0]
    # (we use no special suffix so that we pick up any existing .colors or .icons)
    tree = RuleTree(n_states,8)
    any_state = range(n_states)
    for t in transitions:
        # C,S,E,W,N,SE,SW,NE,NW
        tree.add_rule([t[0],t[4],t[3],t[6],t[1],any_state,t[5],t[2],any_state],t[7][0])
        # (last entry in transition should be a single integer)
    tree.write( golly.getdir('rules')+rule_name+".tree" )
    return rule_name
