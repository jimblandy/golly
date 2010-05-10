# Inspired by Andrew Trevorrow's work on TriLife.py

try:
  set
except NameError:
  # use sets module if Python version is < 2.4
  from sets import Set as set

import golly
import os
from glife.RuleTree import *
from glife.WriteBMP import *

#  triangularVonNeumann -> vonNeumann:
#
#      lower         upper 
#       +--+          +--+        
#       |\ |          |\ |        
#       | \|          |2\|          (any rotation would work too)
#    +--+--+--+    +--+--+--+
#    |\3|\1|\ |    |\ |\0|\ |
#    | \|0\| \|    | \|1\|3\|
#    +--+--+--+    +--+--+--+
#       |\2|          |\ |
#       | \|          | \|
#       +--+          +--+
#
# triangularMoore -> Moore:
#
#      lower         upper 
#    +--+--+--+    +--+--+--+       
#    |\B|\ |\ |    |\6|\7|\ |       where A=10, B=11, C=12
#    |A\|C\| \|    |5\|2\|8\|       
#    +--+--+--+    +--+--+--+
#    |\3|\1|\ |    |\4|\0|\9|
#    |9\|0\|4\|    | \|1\|3\|
#    +--+--+--+    +--+--+--+
#    |\8|\2|\5|    |\ |\C|\A|
#    | \|7\|6\|    | \| \|B\|
#    +--+--+--+    +--+--+--+

def TriangularTransitionsToRuleTree(neighborhood,n_states,transitions_list,rule_name):

    # each square cell is j*N+i where i is the lower triangle, j is the upper triangle
    # each i,j in (0,N]
    # (lower and upper are lists)
    def encode(lower,upper): 
        return [ up*n_states+low for up in upper for low in lower ]

    # what neighbors of the lower triangle overlap neighbors of the upper triangle?
    lower2upper = { 
        "triangularVonNeumann": [(0,1),(1,0)],
        "triangularMoore": [(0,1),(1,0),(2,12),(3,4),(4,3),(5,10),(6,11),(10,5),(11,6),(12,2)],
    }
    numNeighbors = { "triangularVonNeumann":4, "triangularMoore":8 }

    # convert transitions to list of list of sets for speed
    transitions = [[set(e) for e in t] for t in transitions_list]
    tree = RuleTree(n_states*n_states,numNeighbors[neighborhood])
    # for each transition pair, see if we can apply them both at once to a square
    for i,t1 in enumerate(transitions): # as lower
        golly.show("Building rule tree... (pass 1 of 2: "+str(100*i/len(transitions))+"%)") 
        for t2 in transitions: # as upper
            # we can only apply both rules at once if they overlap to some extent
            if any( t1[j].isdisjoint(t2[k]) for j,k in lower2upper[neighborhood] ):
                continue
            # take the intersection of their inputs
            if neighborhood=="triangularVonNeumann":
                tree.add_rule( [ encode(t1[0]&t2[1],t1[1]&t2[0]), # C
                     encode(range(n_states),t1[2]), # S
                     encode(t2[3],range(n_states)), # E
                     encode(range(n_states),t1[3]), # W
                     encode(t2[2],range(n_states)) ], # N
                     encode(t1[4],t2[4])[0] ) # C'
            elif neighborhood=="triangularMoore":
                tree.add_rule( [ encode(t1[0]&t2[1],t1[1]&t2[0]), # C
                             encode(t1[7],t1[2]&t2[12]), # S
                             encode(t1[4]&t2[3],t2[9]), # E
                             encode(t1[9],t1[3]&t2[4]), # W
                             encode(t1[12]&t2[2],t2[7]), # N
                             encode(t1[6]&t2[11],t1[5]&t2[10]), # SE
                             encode(range(n_states),t1[8]), # SW
                             encode(t2[8],range(n_states)), # NE
                             encode(t1[10]&t2[5],t1[11]&t2[6]) ], # NW
                           encode(t1[13],t2[13])[0] ) # C'
    # apply each transition to an individual triangle, leaving the other unchanged
    for i,t in enumerate(transitions):
        golly.show("Building rule tree... (pass 2 of 2: "+str(100*i/len(transitions))+"%)") 
        for t_1 in t[1]:
            if neighborhood=="triangularVonNeumann":
                # as lower triangle:
                tree.add_rule( [ encode(t[0],[t_1]), # C
                                 encode(range(n_states),t[2]), # S
                                 range(n_states*n_states), # E
                                 encode(range(n_states),t[3]), # W
                                 range(n_states*n_states) ], # N
                                 encode(t[4],[t_1])[0] ) # C'
                # as upper triangle:
                tree.add_rule( [ encode([t_1],t[0]), # C
                                 range(n_states*n_states), # S
                                 encode(t[3],range(n_states)), # E
                                 range(n_states*n_states), # W
                                 encode(t[2],range(n_states)) ], # N
                                 encode([t_1],t[4])[0] ) # C'
            elif neighborhood=="triangularMoore":
                # as lower triangle:
                tree.add_rule( [encode(t[0],[t_1]), # C
                    encode(t[7],t[2]), # S
                    encode(t[4],range(n_states)), # E
                    encode(t[9],t[3]), # W
                    encode(t[12],range(n_states)), # N
                    encode(t[6],t[5]), # SE
                    encode(range(n_states),t[8]), # SW
                    range(n_states*n_states), # NE
                    encode(t[10],t[11]) ], # NW
                    encode(t[13],[t_1])[0] ) # C'
                # as upper triangle:
                tree.add_rule( [encode([t_1],t[0]),
                    encode(range(n_states),t[12]), # S
                    encode(t[3],t[9]), # E
                    encode(range(n_states),t[4]), # W
                    encode(t[2],t[7]), # N
                    encode(t[11],t[10]), # SE
                    range(n_states*n_states), # SW
                    encode(t[8],range(n_states)), # NE
                    encode(t[5],t[6]) ], # NW
                    encode([t_1],t[13])[0] ) # C'
        
    # output the rule tree
    golly.show("Compressing rule tree and saving to file...")
    tree.write(golly.getdir('rules') + rule_name + '.tree')
    
def MakeTriangularIcons(n_states,colors,force_background,rule_name):

    width = 15*(n_states*n_states-1)
    if force_background and n_states>2:
        width+=15
    height = 22
    pixels = [[(0,0,0) for x in range(width)] for y in range(height)]

    for row in range(height):
        for column in range(width):
            if force_background and n_states>2 and column>=width-15:
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


def EmulateTriangular(neighborhood,n_states,transitions_list,input_filename):
    '''Emulate a triangularVonNeumann or triangularMoore neighborhood rule table with a rule tree.'''
    
    rule_name = os.path.splitext(os.path.split(input_filename)[1])[0]+'_emulated'
    # (we use a special suffix to avoid picking up any existing .colors or .icons)
    
    # make a rule tree
    TriangularTransitionsToRuleTree(neighborhood,n_states,transitions_list,rule_name)

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
        
    MakeTriangularIcons(n_states,colors,force_background,rule_name)

    if n_states==2:    
        # the icons we wrote are monochrome, so we need a .colors file to avoid
        # them all having different colors or similar from Golly's preferences
        c = open(golly.getdir('rules')+rule_name+".colors",'w')
        if force_background:
            c.write('color = 0 '+' '.join(map(str,background_color))+'\n')
        for i in range(1,n_states*n_states):
            c.write('color = '+str(i)+' '+' '.join(map(str,colors[1]))+'\n')
        c.flush()
        c.close()
    
    return rule_name

