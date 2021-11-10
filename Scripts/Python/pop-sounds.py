# This script runs the current pattern and plays different piano notes
# depending on how the population changes.  Hopefully a useful template
# for people who want to experiment with CA-generated "music".

import golly as g
from time import time, sleep
from random import random

if g.sound() == 0: g.exit("Sound support is not enabled.")
if g.sound() == 1: g.exit("Sound support failed to initialize.")
if g.empty(): g.exit("There is no pattern.")

soundsdir = g.getdir("app")+"Scripts/Lua/sounds/piano/"
sounds = [
    soundsdir+"C3.ogg",
    soundsdir+"D3.ogg",
    soundsdir+"E3.ogg",
    soundsdir+"F3.ogg",
    soundsdir+"G3.ogg",
    soundsdir+"A3.ogg",
    soundsdir+"B3.ogg",
    soundsdir+"C4.ogg",
    soundsdir+"D4.ogg",
    soundsdir+"E4.ogg",
    soundsdir+"F4.ogg",
    soundsdir+"G4.ogg",
    soundsdir+"A4.ogg",
    soundsdir+"B4.ogg",
    soundsdir+"C5.ogg",
    soundsdir+"D5.ogg",
    soundsdir+"E5.ogg",
    soundsdir+"F5.ogg",
    soundsdir+"G5.ogg",
    soundsdir+"A5.ogg",
    soundsdir+"B5.ogg",
    soundsdir+"C6.ogg",
    soundsdir+"D6.ogg",
    soundsdir+"E6.ogg",
]
numsounds = len(sounds)
minpop = int(g.getpop())
maxpop = minpop
prevsound, samecount = 0, 0 # used to detect a repeating sound
genspersec = 5
volume = 0.3

# ------------------------------------------------------------------------------

def ShowHelp():
    g.note('''
Hit up/down arrows to change volume.
Hit -/+ keys to change gens per sec.
Hit enter/return to toggle generating.
Hit space bar to step 1 gen.
Hit H to see this help.
Hit escape to exit script.
''')

# ------------------------------------------------------------------------------

def ShowInfo():
    g.show("gens/sec=%d volume=%.1f (hit H for help)" % (genspersec, volume))

# ------------------------------------------------------------------------------

def PlaySound():
    # the next (non-empty) generation has just been created
    # so get the current population and update minpop and maxpop
    global minpop, maxpop, prevsound, samecount
    currpop = int(g.getpop())
    if currpop < minpop: minpop = currpop
    if currpop > maxpop: maxpop = currpop
    poprange = maxpop - minpop
    if poprange == 0:
        g.sound("play", sounds[numsounds//2], volume)
    else:
        p = (currpop-minpop) / poprange
        # p is from 0.0 to 1.0
        i = int(p * (numsounds-1))
        g.sound("play", sounds[i], volume)
        
        # occasionally play a chord
        if random() < 0.1:
            # two notes up will be a major or minor third
            # since only white notes are used
            j = i + 2
            if j >= numsounds: j = 0
            g.sound("play", sounds[j], volume)
        
        # note that we can end up repeating the same sound
        # (eg. if the initial pattern is a dense Life soup),
        # so if that happens we reset minpop and maxpop
        if i == prevsound:
            samecount = samecount + 1
            if samecount == numsounds:
                minpop = currpop
                maxpop = currpop
                prevsound, samecount = 0, 0
        else:
            prevsound = i
            samecount = 0

# ------------------------------------------------------------------------------

def EventLoop():
    global volume, genspersec
    nextgen = 0.0
    running = True
    while True:
        space = False
        event = g.getevent()
        if len(event) == 0:
            sleep(0.005) # don't hog CPU if idle
        elif event == "key space none":
            running = False
            space = True
            nextgen = 0
        elif event == "key return none":
            running = not running
        elif event == "key h none":
            ShowHelp()
        elif event == "key up none":
            volume = min(1.0, volume+0.1)
            ShowInfo()
        elif event == "key down none":
            volume = max(0.0, volume-0.1)
            ShowInfo()
        elif event == "key = none" or event == "key + none":
            genspersec = min(30, genspersec+1)
            ShowInfo()
        elif event == "key - none":
            genspersec = max(1, genspersec-1)
            ShowInfo()
        else:
            g.doevent(event) # might be a keyboard shortcut
        if (running or space) and time() >= nextgen:
            g.run(1)
            if g.empty(): g.exit("The pattern died.")
            PlaySound()
            g.update()
            if not space: nextgen = time() + 1.0/genspersec

# ------------------------------------------------------------------------------

ShowInfo()
EventLoop()
