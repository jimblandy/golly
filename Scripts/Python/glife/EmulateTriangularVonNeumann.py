# Inspired by Andrew Trevorrow's work on TriLife.py

import golly
import os
from glife.RuleTree import *
from glife.WriteBMP import *

# for N states:
# we get N*N Golly states: each j*N+i where i is the lower triangle, j is the upper triangle
# each i,j in (0,N]
def encode(lower,upper,N): 
    return upper*N+lower
    
def EmulateTriangularVonNeumann(neighborhood,n_states,tri_transitions,input_filename,rule_name):
    transitions = [[e[0] for e in t] for t in tri_transitions]
    tree = RuleTree(n_states*n_states,4)
    # now work through the transitions, taking them in pairs
    for i,t1 in enumerate(transitions): # lower
        golly.show("Building rule tree... ("+str(100*i/len(transitions))+"%)") 
        for t2 in transitions: # upper
            # lower and upper transitions must be compatible if we're to apply them both
            if not t1[0]==t2[1] or not t2[0]==t1[1]:
                continue
            inputs = [[encode(t1[0],t2[0],n_states)],[],[],[],[]] # C,S,E,W,N
            for s in range(n_states):
                inputs[1].append(encode(s,t1[2],n_states)) # south: upper triangle specified
                inputs[2].append(encode(t2[3],s,n_states)) # east: lower triangle specified
                inputs[3].append(encode(s,t1[3],n_states)) # west: upper triangle specified
                inputs[4].append(encode(t2[2],s,n_states)) # north: lower triangle specified
            tree.add_rule(inputs,encode(t1[4],t2[4],n_states))
    # finally apply each transition to an individual triangle, leaving the other unchanged
    for t in transitions:
        # as lower triangle:
        inputs = [[encode(t[0],t[1],n_states)],[],range(n_states*n_states),[],range(n_states*n_states)] # C,S,E,W,N
        for s in range(n_states):
            inputs[1].append(encode(s,t[2],n_states)) # south: upper triangle specified
            inputs[3].append(encode(s,t[3],n_states)) # west: upper triangle specified
        tree.add_rule(inputs,encode(t[4],t[1],n_states))
        # as upper triangle:
        inputs = [[encode(t[1],t[0],n_states)],range(n_states*n_states),[],range(n_states*n_states),[]] # C,S,E,W,N
        for s in range(n_states):
            inputs[2].append(encode(t[3],s,n_states)) # east: lower triangle specified
            inputs[4].append(encode(t[2],s,n_states)) # north: lower triangle specified
        tree.add_rule(inputs,encode(t[1],t[4],n_states))
        
    # output the rule tree
    golly.show("Compressing rule tree and saving to file...")
    tree.write(golly.getdir('rules') + rule_name + '.tree')
    
    # also save a color .icons file
    golly.show("Generating icons...")

    # read rule_name+'.colors' file if it exists
    force_background = False
    background_color = [0,0,0]
    cfn = os.path.split(input_filename)[0]+"/"+os.path.splitext(os.path.split(input_filename)[1])[0]+".colors"
    try:
        cf = open(cfn,'r')
    except IOError:
        # use Golly's default random colours
        random_colors=[[0,0,0],[0,255,127],[127,0,255],[148,148,148],[128,255,0],[255,0,128],[0,128,255],[1,159,0],
            [159,0,1],[255,254,96],[0,1,159],[96,255,254],[254,96,255],[126,125,21],[21,126,125],[125,21,126],
            [255,116,116],[116,255,116],[116,116,255],[228,227,0],[28,255,27],[255,27,28],[0,228,227],
            [227,0,228],[27,28,255],[59,59,59],[234,195,176],[175,196,255],[171,194,68],[194,68,171],
            [68,171,194],[72,184,71],[184,71,72],[71,72,184],[169,255,188],[252,179,63],[63,252,179],
            [179,63,252],[80,9,0],[0,80,9],[9,0,80],[255,175,250],[199,134,213],[115,100,95],[188,163,0],
            [0,188,163],[163,0,188],[203,73,0],[0,203,73],[73,0,203],[94,189,0],[189,0,94]]
        colors = dict(zip(range(len(random_colors)),random_colors))
    else:
        # read from the .colors file
        colors = {0:[0,0,0]} # background is black
        for line in cf:
            if line[0:6]=='color ':
                entries = map(int,line[6:].replace('=',' ').replace('\n',' ').split())
                if len(entries)<4:
                    continue # too few entries, ignore
                if entries[0]==0:
                    force_background = True
                    background_color = [entries[1],entries[2],entries[3]]
                else:
                    colors.update({entries[0]:[entries[1],entries[2],entries[3]]})
        # (we don't support gradients in .colors)

    width = 15*(n_states*n_states-1)
    if force_background:
        width+=15
    height = 22
    pixels = [[(0,0,0) for x in range(width)] for y in range(height)]

    for row in range(height):
        for column in range(width):
            if force_background and column>=width-15:
                # add extra 'icon' filled with the intended background color
                pixels[row][column] = background_color
            else:
                # decide if this pixel is a lower or upper triangle
                iState = int(column/15)
                upper = int((iState+1) / n_states)
                lower = (iState+1) - upper*n_states
                is_upper = False
                is_lower = False
                if row<15: # big icon
                    if (column-iState*15) > row:
                        is_upper = True
                    elif (column-iState*15) < row:
                        is_lower = True
                elif (column-iState*15)<7: # little icon
                    if (column-iState*15) > row-15:
                        is_upper = True
                    elif (column-iState*15) < row-15:
                        is_lower = True
                if is_upper:
                    pixels[row][column] = colors[upper]
                elif is_lower:
                    pixels[row][column] = colors[lower]
                else:
                    pixels[row][column] = 0,0,0
                    
    WriteBMP( pixels, golly.getdir('rules') + rule_name + ".icons" )

