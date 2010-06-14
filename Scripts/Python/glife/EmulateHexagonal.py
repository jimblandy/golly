import os
from glife.RuleTree import *
from glife.WriteBMP import *

def HexagonalTransitionsToRuleTree(neighborhood,n_states,transitions,rule_name):
    '''Convert a set of hexagonal neighborhood transitions to a Moore neighborhood rule tree.'''
    tree = RuleTree(n_states,8)
    for t in transitions:
        # C,S,E,W,N,SE,(SW),(NE),NW
        tree.add_rule([t[0],t[4],t[2],t[5],t[1],t[3],range(n_states),range(n_states),t[6]],t[7][0])
    tree.write( golly.getdir('rules')+rule_name+".tree" )
    
def MakePlainHexagonalIcons(n_states,rule_name):
    '''Make some monochrome hexagonal icons.'''

    width = 15*(n_states-1)
    height = 22
    pixels = [[(0,0,0) for x in range(width)] for y in range(height)]

    big = [[0,0,0,1,1,0,0,0,0,0,0,0,0,0,0],
           [0,0,1,1,1,1,1,0,0,0,0,0,0,0,0],
           [0,1,1,1,1,1,1,1,1,0,0,0,0,0,0],
           [1,1,1,1,1,1,1,1,1,1,1,0,0,0,0],
           [1,1,1,1,1,1,1,1,1,1,1,1,0,0,0],
           [0,1,1,1,1,1,1,1,1,1,1,1,0,0,0],
           [0,1,1,1,1,1,1,1,1,1,1,1,1,0,0],
           [0,0,1,1,1,1,1,1,1,1,1,1,1,0,0],
           [0,0,1,1,1,1,1,1,1,1,1,1,1,1,0],
           [0,0,0,1,1,1,1,1,1,1,1,1,1,1,0],
           [0,0,0,1,1,1,1,1,1,1,1,1,1,1,1],
           [0,0,0,0,1,1,1,1,1,1,1,1,1,1,1],
           [0,0,0,0,0,0,1,1,1,1,1,1,1,1,0],
           [0,0,0,0,0,0,0,0,1,1,1,1,1,0,0],
           [0,0,0,0,0,0,0,0,0,0,1,1,0,0,0]]
    small = [[0,1,1,0,0,0,0],
             [1,1,1,1,1,0,0],
             [1,1,1,1,1,1,0],
             [0,1,1,1,1,1,0],
             [0,1,1,1,1,1,1],
             [0,0,1,1,1,1,1],
             [0,0,0,0,1,1,0]]
    fg = (255,255,255)
    bg = (0,0,0)
    for s in range(1,n_states):
        for row in range(15):
            for column in range(15):
                pixels[row][(s-1)*15+column] = [bg,fg][big[row][column]]
        for row in range(7):
            for column in range(7):
                pixels[15+row][(s-1)*15+column] = [bg,fg][small[row][column]]
                
    WriteBMP( pixels, golly.getdir('rules') + rule_name + ".icons" )

def EmulateHexagonal(neighborhood,n_states,transitions,input_filename):
    '''Emulate a hexagonal neighborhood rule table with a Moore neighborhood rule tree.'''
    rule_name = os.path.splitext(os.path.split(input_filename)[1])[0]
    HexagonalTransitionsToRuleTree(neighborhood,n_states,transitions,rule_name)
    MakePlainHexagonalIcons(n_states,rule_name)
    return rule_name

    
