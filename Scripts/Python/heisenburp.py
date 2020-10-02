# Stable pseudo-Heisenburp device.
# Show several views of a multi-stage signal-processing circuit,
# optionally using Golly 1.2+'s layer-cloning system.
# Author: Dave Greene, 27 February 2007.
# Latest changes:
# - corrected timing of signal-tracking selection highlight
# - switched to three spaces per indent
# - replaced long instruction message with Help note
# - added rule() to be sure to run in the right universe (B3/S23)
# - removed "x = ..." lines from pattern strings

import golly as g
from glife import *
from time import sleep
import os
import sys

def burp():
    test_signal=pattern("""
    40bo$41bo$39b3o17$40bo4bo4bo4bo4bo$41bo4bo4bo4bo4bo$39b3o2b3o2b3o2b3o
    2b3o3$40bo4bo4bo4bo4bo$41bo4bo4bo4bo4bo$39b3o2b3o2b3o2b3o2b3o3$40bo4bo
    4bo4bo4bo$41bo4bo4bo4bo4bo$39b3o2b3o2b3o2b3o2b3o3$40bo4bo4bo4bo4bo$41b
    o4bo4bo4bo4bo$39b3o2b3o2b3o2b3o2b3o3$bo38bo4bo4bo4bo4bo18bo$2bo38bo4bo
    4bo4bo4bo18bo$3o36b3o2b3o2b3o2b3o2b3o16b3o37$40bo$41bo$39b3o!""")

    prepare_burp()
    ticks=0
    tickstep=5
    last_signal=-99999
    viewport_speed=16
    sel_speed=0.0
    delta_sel=0.0
    delay=-1.0
    run_flag=False
    ch=""
    selx=600.0
    sely=365.0
    place_signal=3
    helpstring="""Use ENTER and SPACE to run or halt the pattern;
 use + and - to change the step size or delay value;
 use arrow keys and mouse tools to pan and zoom in any pane;
 T toggles between a tiled view and a single-pane view;
 S creates another signal fleet near the detection mechanism;
 R resets the Heisenburp device to its initial state;
 Q quits out of the script and restores original settings."""

    instr="Press H for help. "
    g.show(instr + str(tickstep))
    g.select([selx,sely,50,50])

    # keyboard handling
    while ch!="q":
        if place_signal>0:
            if ticks-last_signal>1846:
                test_signal.put(-150,60)
                last_signal=ticks
                place_signal-=1

        if place_signal>0 and run_flag==True:
            show_status_text("Next signal placement in " \
            + str(1847 - ticks + last_signal) + " ticks. " + instr, delay, tickstep)
        else:
            show_status_text(instr,delay,tickstep)

        event = g.getevent()
        if event.startswith("key"):
            evt, ch, mods = event.split()
        else:
            ch = ""
        if ch=="r":
            prepare_burp()
            ticks=0
            last_signal=-99999
            viewport_speed=16
            sel_speed=0
            run_flag=False
            selx=600.0
            sely=365.0
            place_signal=3
            g.select([selx,sely,50,50])

        elif ch=="h":
            g.note(helpstring)
        elif ch=="t":
            g.setoption("tilelayers",1-g.getoption("tilelayers"))

        elif ch=="=" or ch=="+":
            if delay>.01:
                delay/=2
            elif delay==.01:
                delay=-1
            else:
                if tickstep==1:
                    tickstep=3
                elif tickstep<250:
                    tickstep=(tickstep-1)*2+1
        elif ch=="-" or ch=="_":
            if delay==-1:
                if tickstep==1:
                    delay=.01
                else:
                    if tickstep==3:
                        tickstep=1
                    else:
                        tickstep=(tickstep-1)//2+1
            else:
                if delay<1:
                    delay*=2

        elif ch=="space":
            run_flag=False
        elif ch=="return":
            run_flag=not run_flag
        elif ch=="s":
            place_signal+=1
        else:
            # just pass any other keyboard/mouse event through to Golly
            g.doevent(event)

        # generation and selection speed handling
        if ch=="space" or run_flag==True:
            g.run(tickstep)

            currlayer = g.getlayer()
            if currlayer != 4:
                # user has switched layer so temporarily change it back
                # so we only change location of bottom right layer
                g.check(False)
                g.setlayer(4)

            x, y = getposint()
            oldticks=ticks
            ticks+=tickstep
            delta_view=int(ticks/viewport_speed) - int(oldticks/viewport_speed)
            if delta_view != 0: # assumes diagonal motion for now
                setposint(x + delta_view, y + delta_view)
            sel = g.getselrect()
            if len(sel)!=0:
                x, y, w, h = sel
                if int(selx)!=x: # user changed selection
                    # prepare to move new selection instead (!?!)
                    # -- use floating-point selx, sely to account for fractional speeds
                    selx=x
                    sely=y
                else:
                    selx+=sel_speed*tickstep
                    sely+=sel_speed*tickstep
                    g.select([selx, sely, w, h])
            else:
                g.select([selx, sely, 50, 50])

            if currlayer != 4:
                g.setlayer(currlayer)
                g.check(True)

            # change viewport speed at appropriate times to follow the action
            if oldticks<4570 and ticks>=4570:
                sel_speed=.666667
                # one-time correction to catch up with the signal at high sim speeds
                g.select([600+(ticks-4570)*1.5, 365+(ticks-4570)*1.5, w, h])
            if selx>2125:
                sel_speed=0
                g.select([2125, sely+2125-selx, w, h])
            if oldticks<4750 and ticks>=4750: viewport_speed=1.5
            if oldticks<6125 and ticks>=6125: viewport_speed=16
            if oldticks>=11995: viewport_speed=99999.9

            # stop automatically after the last signal passes through the device
            if oldticks - last_signal<8705 and ticks - last_signal>=8705:
                run_flag=False
            if run_flag==True and delay>0:
                sleep(delay)

        g.update()

# -----------------------------------------------------

def prepare_burp():
    highway_robber=pattern("""
    143bo$143b3o$146bo$145b2o2$126bo42bo$126b3o38b3o$129bo36bo$128b2o11b2o
    23b2o7b2o$141b2o32bo$108bo64bobo$108b3o6b2o54b2o$111bo5b2o$110b2o3$
    111b2o$111b2o3b2o$116b2o35b2o23bo$154bo22bobo$151b3o8b2o14bo$151bo11bo
    $98bo7bo53b3o$86bo11b3o5b3o51bo$84b3o14bo7bo32b2o$68bo14bo16b2o6b2o32b
    obo30b2o$68b3o12b2o59bo30bobo$71bo72b2o31bo$70b2o105b2o$159bob2o$159b
    2obo$71b2o$71b2o17b2o76b2o$90b2o76b2o5$104b2o$87b2o14bobo$87bo15bo74b
    2o$88b3o11b2o74bo$90bo85bobo$84b2o90b2o$84bo$85b3o$87bo2$158b2o$159bo$
    68b2o89bobo$67bobo90b2o$67bo25b2o$66b2o25bo$74b2o15bobo$74b2o15b2o98b
    2o6bo$187bobo2bo4b3o$185b3ob2o5bo$184bo11b2o$185b3ob2o$177b2o8bob2o$
    177b2o2$161b2o32b2o3b2o$161b2o32b2o3b2o$153b2o$88b2o2b2ob2o57bo18b2ob
    2o$73b2o13bobo2bobo58bobo16b2obo$72bobo16b2o3bo58b2o21bo26bo$72bo19bob
    2o76bob5o25bobo$71b2o16bo2bobo77b2o31bo$88bobobobo80b2o$89b2ob2o82bo$
    173b3o$173bo33bo$171bobo31b3o$171b2o31bo$187b2o14bobo$187b2o15bo$83b2o
    $83b2o$202b2o$202b2o2$175b2o$175b2o$72b2o123b2o$73bo19b2o102bo$73bobo
    17bo76b2o26b3o$74b2o15bobo60b2o14b2o28bo$86bo4b2o62bo$85bobo64b3o$54bo
    30bobo64bo$54b3o17b2o10bo$57bo15bobo$56b2o15bo116b2o$72b2o116bo$36b2o
    49b2o102b3o$29b2o5b2o49bo105bo$29b2o57b3o6bo$90bo4b3o$94bo$31b2o17b2o
    29b2o11b2o14b2o$31b2o17bo30b2o27b2o$25b2o21bobo$25b2o21b2o$116b2o$116b
    2o36b2o$112b2o41bo$112b2o41bobo$156b2o$56b2o122b2o$56b2o11b2o109b2o$
    69bo47b2o$70b3o44b2o$72bo2$22bo$22b3o55b2o96b2o$25bo53bobo96bo$24b2o
    47b2o4bo99b3o$73b2o3b2o101bo$179b2o$76b4o99bo$75bo3bo97bobo$75b2o100b
    2o$53b2o33bo22b2o$19b2o32b2o33b3o20bo$20bo70bo20b3o$20bobo67b2o22bo44b
    o$21b2o136b3o$162bo$115bo45b2o$35b2o28b2o46b3o$35bobo27b2o45bo$37bo41b
    2o6b2o23b2o$37b2o40b2o6bo2b2o$83b2o3b2obo$83b2o4bo89b2o$17bob2o68bo89b
    o$17b2obo67b2obob2o18b2o62bobo$87bo2b2ob2o18bobo61b2o$26b2o60bo26bo$
    26b2o33b2o26b3ob2o20b2o$60bo2bo27bobo66b2o$61bobo29bo66b2o15b2o$62bo
    29b2ob2o80bobo$94bobo82bo$94bo84b2o$93b2o3$160b2o$160bo$158bobo$158b2o
    2$16bo$16b3o$19bo33bo121b2obo$18b2o33b3o53b2o64bob2o$56bo52b2o$11bo43b
    obo110b2o$11b3o42bo111b2o$14bo123bo$13b2o121b3o$57b2o19bo56bo$57b2o17b
    3o19b2o20bo14b2o$75bo23bo19bobo$75b2o22bobo17bobo$100b2o15bobobobo34b
    2o$10b2o19b2o79bo4b2o3b2o10bo24bo$10b2o19b2o47b2o29bobo19bobo23bobo$
    80b2o29bobo19bobo24b2o$4bo95b2o10bo21bo4b2o$4b3o25b2o65bobo20b2o15bobo
    $2o5bo24bo57b2o7bo21bobo17bo$bo4b2o25b3o54bo7b2o21bo19b2o36bo$bobo31bo
    52bobo22b2o5b2o55b3o$2b2o29b2o53b2o23bo62bo$33bo17b2o61b3o28b2o29b2o$
    31bobo4bo11bobo63bo28bo$31b2o3b3o11bo92bobo$35bo13b2o92b2o$35b2o94b2o$
    61b2o68b2o$61b2o$b2o178b2o$b2o178bo$36b2o141bobo$36b2o141b2o3$27b2o37b
    2o97b2o$27b2o36bobo51b2o43bobo$65bo54bo43bo$64b2o54bobo40b2o$121b2o2$
    181b2obo$181bob2o2$40bo133b2o$38b3o133b2o$37bo$37b2o$138b2o$138b2o$42b
    2o$42b2o78b2o$122b2o$114b2o$52b2o61bo18b2ob2o$52bo62bobo16b2obo$50bobo
    63b2o21bo$50b2o81bob5o$13b2o118b2o$12bobo121b2o$12bo124bo$11b2o99bobo
    19b3o$112b2obo18bo$23b2o90b3o$23b2o87b2o4bo$112bob5o$114bo$113bo3b2o$
    108bo3bo3bobo$106b3o3b2o3bo$105bo$28b2o75b2o29b2o39b2o$27bobo106b2o38b
    o2bo$27bo149b2o$26b2o2$131b2o$131b2o$135b2o$14b2o119b2o$14b2o35bo$49b
    3o$48bo80b2o$48b2o63b2o14b2o$79bo33bo$68b2o7b3o34b3o$69bo6bo39bo$69bob
    o4b2o$70b2o$109b2o$109bobo$111bo$111b2o7$103b2o$103bo$104b3o$106bo$13b
    o19b2o$12bobo10b2o6bo5b2o41b2o$11bo2bo10bo8b3obobo41bo$12b2o9bobo10bob
    o7b2o28b2obo3b3o$23b2o12b2o7b2o28b2ob4o2bo$82bo$64b2o6b2o2b2ob2o$63bob
    o6bo4bobo$63bo9b4o2bo$62b2o7bobo2bobo$71b2o4bo!""")

    connecting_neck=pattern("""
    24bo$22b3o$21bo$21b2o$6b2o$7bo$7bobo$8b2o10bo$19bobo$19bobo$20bo4b2o$
    8b2o15bobo$7bobo17bo$7bo19b2o$6b2o6$17b2o$17b2o7$23b2ob2o$22bobobobo$
    5b2o16bo2bobo$6bo19bob2o$6bobo16b2o3bo$7b2o13bobo2bobo$22b2o2b2ob2o11$
    8b2o15b2o$8b2o15bobo$2o25bo$bo25b2o$bobo$2b2o4$21bo107bo$19b3o106bobo$
    18bo109bobo$18b2o106b3ob2o$24bo100bo$22b3o12bo84bo2b4ob2o$21bo15b3o82b
    3o3bob2o$21b2o17bo84bo$39b2o83b2o26b2o$152bo$150bobo$150b2o2$24b2o$5b
    2o17b2o76bo$5b2o95b3o6b2o$105bo5b2o41b2obo$104b2o48bob2o$4b2o$5bo141b
    2o$2b3o12b2o86b2o40b2o$2bo14bo16b2o6b2o61b2o3b2o$18b3o14bo7bo66b2o$20b
    o11b3o5b3o$32bo7bo$78bo51b2o3b2o$76b3o13bo38bo3bo$75bo16b3o33b3o5b3o$
    50b2o23b2o18bo32bo9bo$45b2o3b2o42b2o42bobo$45b2o92b2o3$44b2o76b2o$45bo
    5b2o69b2o$42b3o6b2o104b2o$42bo41b2o71bo$84b2o69bobo$61b2o92b2o$60bobo
    62bo$60bo62b3o$59b2o10b2o49bo$72bo48bobo$69b3o9b2o39bo43b2o$69bo11bo
    19b2o56b2o6bo$82bo17bobo56bo6bo$81b2o17bo19b2o35bobo6b2o$99b2o19b2o35b
    2o$116b2o$116bo$117b3o24b2o$119bo23bobo$143bo25b2o$142b2o25b2o4$146b2o
    $145bobo$145bo4b2o$144b2o5bo$148b3o5bo$148bo6bobo$156bo!""")

    transmitter2c3=pattern("""
    180b2o$180b2o$219bo$219b3o$178b2o42bo$116bo61b2o41b2o$115bobo47b2o99b
    2o14bo$116bo49bo74b2o24bo14b3o$166bobo73bo13b2o6b3o18bo$167b2o73bobo
    11b2o6bo19b2o12bo$110b2o51b2o78b2o53b3o$110b2o23b2o26b2o136bo$102b2o
    31bo164b2o$103bo29bobo$103bobo27b2o$104b2o4b2o$110b2o162b2o$274b2o$
    144b2o30b2o$102b2o40bo31b2o$102bo39bobo$104bob2o34b2o43b2o144b2o$103b
    2ob2o78bo2bo143b2o$124b2o60bobo23b2o57b2o$103b2ob2o16b2o61bo20bo3b2o
    57bo20b2o$104bobo100bobo26b2o14b2o3b2o13bo20bo$104bobo99bobo21b2o5bo
    15bo3bo13b2o17b3o61b2o$105bo100bo23bobob3o13b3o5b3o29bo63b2o5b2o$205b
    2o16b2o7bobo15bo9bo100b2o$223b2o7b2o152b2o6bo$169b2o125b2o84bobo2bo4b
    3o$169b2o124bobo61b2o19b3ob2o5bo$295bo63b2o18bo11b2o$294b2o69b2o13b3ob
    2o$365b2o15bob2o$131b2o151bo$131b2o149b3o12b2o$281bo16bo91b2o3b2o$271b
    2o8b2o12b3o92b2o3b2o$272bo22bo$272bobo27b2o30b2o$273b2o28bo17b2o11b2o$
    39bo82bo136bo27b2o11b3o19bo77bo$38bobo79b3o136b3o25b2o11bo18b3o77bobo$
    39bo79bo142bo56bo80bo$108bo10b2o140b2o$37b5o65bobo228b2o$37bo4bo64bobo
    177b2o50bo28b2o$40bo2bo61b3ob2o20bo155b2o47b3o29bo$12bo27b2obo60bo24b
    3o204bo29bobo$12b3o10b2o10bo5bob2obo49bo6b3ob2o17bo104bo132b2o$15bo9b
    2o9bobo4bobob2o49b3o6bob2o17b2o103b3o146b2o$4b2o8b2o20bo2bo2b2obo30b2o
    23bo134bo145b2o$5bo31b2o6bo31bo6b2o14b2o133b2o11b2o13b2o$5bobo37b3o29b
    obo4b2o39b2o121b2o13b2o$6b2o40bo23b2o4b2o44bo2bo87bo73b2o$47b2o23bobo
    50b2o88b3o5b2o64bo$70bobob3o141bo4b2o62bobo$66b2o2b2o5bo139b2o68b2o81b
    2o$4bob2o58b2o8b2o292b2o$2b3ob2o384b2o$bo216b2o172bo$2b3ob2o80b2o128b
    2o3b2o123b2o15b2o26b3o$4bobo30b2o49b2o23b2o108b2o122bobo15b2o28bo$4bob
    o30b2o74bo233bo$5bo108b3o229b2o$116bo77b2o7b2o$100b2o24b2o66b2o7bobo$
    28b2o66bo3b2o24bo30b2o42bobob3o$28b2o65bobo29b3o2b2o24bo42b2o5bo40b2o
    18b2obo112b2o$34b2o58bobo32bo3bo13b2o6b3o49b2o40bobo17bob2o112bo$34bo
    19b2o38bo38bobo11b2o6bo95bo134b3o$30bo5bo16bobo37b2o24b2o13b2o115b2o
    135bo$29bobo3b2o16bo65b2o164b2obo$30bo21b2o231bob2o$31b3o$33bo79b2o
    163b2o$113b2o163b2o$117b2o46b2o$66b2o49b2o46b2o$66b2o$349b2o$350bo$71b
    2o39b2o98b2o136bobo$70bobo39b2o99bo54b2o81b2o$70bo91b2o46b3o56bo105b2o
    $69b2o91bo47bo58bobo103b2o$143b2o3b2o13bo106b2o$82b2o60bo3bo13b2o$82b
    2o57b3o5b3o$141bo9bo40b2o$193bo$27bo162b3o180b2o$26bobo161bo182bo$26bo
    2bo262b2o80b3o$27b2o263bo83bo$118b2o22bo147bobo81b2o$119bo20b3o99bo18b
    o28b2ob2o79bo$116b3o20bo102b3o7b2o5b3o31bobo76bobo$116bo22b2o104bo6b2o
    4bo34bobo76b2o$5b2o237b2o12b2o10b2o20b2ob2o$5b2o264bo24bo$9b2o104bo
    155bobo18b2obo$9b2o104b3o139b2o13b2o18b2obobo56bo$23b2o93bo133b2o3b2o
    37b2o56b3o$23b2o92b2o23b2o108b2o103bo$139b2o2bo212b2o28b2o$139bob2o71b
    2o56b2o112bo$141bo71bobo55bobo110bobo$141bo71bo49bo7bo108b2o2b2o$116b
    2o18b2obob2o69b2o47b3o5b3o108b2o$35b2o78bobo18b2ob2o2bo116bo7bo$35b2o
    78bo26bo117b2o6b2o$13b2o99b2o20b2ob3o$14bo122bobo239b2o4b2o$11b3o123bo
    241b2o4b2o$11bo90b2o30b2ob2o$102bo31bobo$100bobo5b2o26bo$99bobo6b2o26b
    2o140b2o$95b2o3bo177b2o72b2o35bo$95b2o254bobo33b3o$20b2o69b2o194b2o62b
    o34bo$21bo70bo194bo62b2o34b2o$18b3o71bobo193b3o63b2o$18bo74b2o195bo63b
    2o3bo$233bo47b2o75bobo6b2o$233b3o46bo76bobo5b2o$236bo44bo79bo$235bobo
    43b2o78b2o$120b2o4b2o108bo30bo$120b2o4b2o139b3o$270bo$237b2o30b2o8b2o
    74bo$237b2o40bo61bo13b3o35b2o$121b2o154bobo59b3o16bo34b2o$121b2o2b2o
    150b2o59bo18b2o$125bobo93bo41b2o73b2o$127bo92bobo40b2o26b2o$97b2o28b2o
    91bobo68bo23bo72b2o$98bo119b3ob2o65bobo23b3o70b2o$95b3o119bo71b2o27bo
    73b2o$95bo118bo2b4ob2o39b2o52b2o73b2o$214b3o3bob2o39b2o$217bo130b2o$
    175bo40b2o26b2o102b2o36b2o$113b2o60b3o66bo126b2o13b2o$113bobo46b2o3b2o
    9bo14b2o47bobo126bobo$77bo37bo2bo2bo40b2o3b2o8b2o15bo47b2o43b2o84bo$
    77b3o35b7o71bo93b2o72b2o10b2o$80bo112b2o113b2o51bo$38b2o39b2o36b5o33b
    2o151b2o41b2o9b3o$38b2o77bo4bo2b2o29bo174b2o19bo11bo$120bo2bo2bo29bobo
    87b2obo81bobo17bo$120b2obobo31b2o87bob2o83bo17b2o$117bo5bob2o194b2o10b
    2o$105b2o9bobo4bo72b2o41b2o80bo$105b2o9bo2bo2b2o72b2o41b2o70b2o9b3o$
    117b2o193bo11bo$96b2obo211bo$96bob2o211b2o$160b2o$61b2o98bo60b2o3b2o$
    61b2o8b2o85b3o28b2o32bo3bo$72bo85bo31bo15b2o12b3o5b3o29b2o$69b3o117bo
    17bo12bo9bo28bobo$47b2o20bo119b2o13b3o23bobo26bo$48bo155bo26b2o25b2o
    14b2o$10b2o33b3o70b2o153bo2bo$10b2o33bo29b2o41b2o154b2o$6bo50b2o15bobo
    $6b3o48bo16bo98b2o$9bo48b3o12b2o98b2o54b2o$8b2o11bo38bo103b2o64bo$20bo
    bo7b2o131bobo51b2o11bobo$20bobo7b2o44b2o85bo54bo12b2o$21bo55bo84b2o54b
    obo$74b3o142b2o2b2o$74bo148b2o$81b2o34b2o$82bo34bobo$18b2o59b3o37bo$
    18b2o59bo39b2o97b2o4b2o$4b2o212b2o4b2o$4b2o$2o$2o162bo$107bob2o3b2o48b
    3o$105b3ob2o3bo52bo$104bo10b3o48b2o$105b3ob2o6bo101b2o$107b2o2bo106bob
    o$110b2o106bo$217b2o2$164b2o$164b2o$188b2o$188bobo$190bo$24b2o164b2o$
    23bobo$24bo4$132bo4b2o$131bobo2bobo7b2o$130bo2b4o9bo$26b2obo100bobo4bo
    6bobo$26bob2o99b2ob2o2b2o6b2o$127bo$19b2o103bo2b4ob2o$19b2o103b3o3bob
    2o$127bo40b2o$126b2o39bobo$52b2o103bo9bo$52b2o49b2o52b3o5b3o54b2o$104b
    o55bo3bo57b2o$91bo11bo55b2o3b2o$9b2o39b2o39b3o9b2o$10bo39b2o42bo$10bob
    o24b2o42b2o10b2o$11b2o25bo43bo150b2o$38bobo41bobo91b2o55bo$39b2o42b2o
    91b2o53bobo$35b2o69b2o123b2o$35b2o69b2o75bob2o$29b2o152b2obo$29bo188b
    2o$27bobo187bobo$27b2o109b2o77bo$132b2o4bobo75b2o$133bo6bo38b2o$48b2o
    66b2o12b3o7b2o37bobo$48b2o47b2o18bo12bo50bo$97bo16b3o36b2o26b2o$59b2o
    37b3o13bo39bo$58bo2bo38bo50b3o3bob2o59b2o$58bobo23b2o65bo2b4ob2o58bobo
    $59bo20bo3b2o68bo64bo$79bobo73b3ob2o57b2o$7b2o69bobo76b2o2bo$7b2o17b2o
    50bo81b2o84bo$26b2o49b2o165b3o$243bo$41b2o200b2o$41b2o2$219bo$219b3o$
    222bo$221b2o23b2o$243b2o2bo$243bob2o$245bo$245bo$220b2o18b2obob2o$219b
    obo18b2ob2o2bo$219bo26bo$218b2o20b2ob3o$241bobo$51bo189bo$12b2o35b3o
    186b2ob2o$12b2o34bo189bobo$48b2o190bo$81bo158b2o$81b3o$17b2o65bo$17b2o
    64b2o$13b2o$13b2o2$57b2o159b2o$19b2o36b2o158bobo$19b2o13b2o181bo$33bob
    o180b2o$33bo181bo$32b2o10b2o74bo94b3o$45bo72b3o97bo$42b3o9b2o61bo99b2o
    $42bo11bo20b2o40b2o17bo39bo$55bo20bo59b3o37b3o$54b2o17b3o63bo39bo$73bo
    64b2o38b2o12bo$192b3o$159bo35bo19b2o$79b2o78b3o32b2o19b2o$78bobo81bo
    76b2o$78bo82b2o11b2o63bobo$77b2o95b2o65bo$241b2o2$80b2o$81bo$78b3o$78b
    o$85b2o30b2o$86bo17b2o11b2o$83b3o19bo80b2o$83bo18b3o82bo$102bo81b3o$
    184bo2$153b2o$147b2o5bo20b2o13b2o$147bobob3o21bobo11bobo$140b2o7bobo
    25bo11bo$140b2o7b2o26b2o9b2o3$191b2o$192bo$189b3o19b2o21b2o$189bo20bob
    o21b2o$196b2o12bo17b2o$197bo11b2o17b2o$194b3o$194bo$230b2o$223b2o5b2o$
    223b2o5$89b2o$89b2o8$63bo$62bobo$62bobo$61b2ob2o16b2o$82b2o$61b2ob2o$
    62bob2o34b2o$60bo39bobo$60b2o40bo$102b2o2$68b2o$62b2o4b2o$61bobo27b2o$
    61bo29bobo$60b2o31bo$68b2o23b2o$68b2o$86b2o$82bo3b2o$74bo6bobo$73bobo
    4bobo$74bo5bo$79b2o!""")

    head2c3=pattern("8b2o$3bo2bo2bo$3b6o2$3b6o$2bo6bo$2bo2b5o$obobo$2o2bo$4bo$3b2o!")
    body2c3=pattern("6bo$b6o$o$o2b6o$obo6bo$obo2b5o$b2obo$4bo$4bo$3b2o!")
    tail2c3=pattern("5b2o$5b2o2$b6o$o5bo$o2b3o$obo$o2bo$b2o!")

    wire2c3=head2c3(625,388) + tail2c3(2143,1905)
    for i in range(631,2142,6): # 251 body segments
        wire2c3+=body2c3(i,i-236) # first one at (631, 395)

    receiver2c3=pattern("""
    208bo$207bobo$208bo3$213b2o$188b2o23b2o$189bo31b2o$189bobo29bo$190b2o
    27bobo$213b2o4b2o$213b2o2$179b2o$180bo40b2o$180bobo39bo$181b2o34b2obo$
    217b2ob2o$199b2o$199b2o16b2ob2o$218bobo$218bobo$219bo8$192b2o$192b2o8$
    55b2o$48b2o5b2o$48b2o$85bo$83b3o$50b2o17b2o11bo$50b2o17bo12b2o$44b2o
    21bobo20bo$44b2o21b2o19b3o$87bo$87b2o3$90b2o9b2o26b2o7b2o$90bo11bo25bo
    bo7b2o$88bobo11bobo21b3obobo$88b2o13b2o20bo5b2o39bo$125b2o45b3o$175bo
    14bo$95bo78b2o12b3o$41bo51b3o91bo$41b3o48bo94b2o$44bo47b2o$43b2o$186b
    2o$167b2o17b2o$167b2o3$61b2o$61bo$59bobo42b2o$59b2o43b2o11b2o51b2o$
    117bo53bo$84b2o32b3o48bo$42b2o40bo35bo48b2o$42b2o15b2o24b3o85b2o$59bob
    o25bo12b2o38b2o32bo$61bo38bo39bo30b3o$61b2o38b3o37b3o27bo$103bo39bo3$
    42b2o$42bo$40bobo$40b2o4$57b2obo$57bob2o2$50b2o$50b2o122b2o$173bobo$
    173bo29b2o$172b2o29bobo$205bo$178b2o25b2o$177bobo4b2o$40b2o135bo7bo$
    41bo134b2o4b3o$41bobo138bo$42b2o146b2o$189bobo$189bo$188b2o$61bo$59b3o
    $58bo$58b2o$206b2o$182bo23bobo$63b2o117b3o23bo$63b2o120bo22b2o$184b2o
    14b2o$200b2o$73b2o$73bo$71bobo$71b2o$34b2o$33bobo$33bo$32b2o2$44b2o$
    44b2o$160b2o7b2o30b2o$92b2o66b2o7bobo29bobo$92b2o45bo27bobob3o29bo$
    139b3o25b2o5bo28b2o$142bo30b2o$90b2o49b2o$49b2o39b2o$48bobo26b2o43bo$
    48bo29bo43b3o$47b2o29bobo44bo$79b2o43b2o11b2o$75b2o60b2o$75b2o4$177b2o
    21b2o$66b2o108bobo21b2o$66bo109bo17b2o$64bobo21b2o85b2o17b2o$48b2o14b
    2o22b2o$48b2o93b2o$52bo46b2o39b2o2bo2b2o47b2o$48b2o2b3o43bo2bo38b2obo
    3bo41b2o5b2o$48b2o5bo42bobo23b2o17bobobo10b2o29b2o$54b2o43bo20bo3b2o
    14b2obob2o12bo$119bobo12b2o4bo2bo12b3o$118bobo13b2o6b2o12bo$67b2o49bo$
    66bobo48b2o$67bo$81b2o$81b2o4$47b2o$47b2o5$49bo$19b2o28b3o$12b2o5b2o
    31bo92bo$12b2o37b2o90b3o11bo$47b2o93bo14b3o$47bo94b2o16bo14bo$14b2o33b
    o109b2o12b3o$14b2o32b2o69bo52bo$8b2o109b3o50b2o$8b2o112bo$121b2o$171b
    2o$152b2o17b2o$45b2o105b2o$45b2o17b2o$64b2o3$65b2o$65bo89b2o$52b2o12b
    3o41bo23b2o20bo$35b2o16bo14bo40bobo22bo19bo$35bo14b3o57bo14b2o8b3o16b
    2o$28bo7b3o11bo74bo11bo20b2o$26b3o9bo87b3o30bo$25bo102bo27b3o$25b2o
    129bo$17b2o93b2o$17b2o92bobo$111bo$110b2o62b2o$126b2obo44bobo$126bob2o
    46bo$176b2o$34b2o83b2o$34bo84b2o$32bobo$32b2o3$166b2o$166b2o2$157b2obo
    $157bob2o2$36b2o$36bobo$38bo91b2o44b2o$38b2o90bo44bobo$28b2o98bobo44bo
    $28b2o98b2o44b2o$9bob2o$9b2obo2$18b2o135b2o$18b2o136bo$156bobo$157b2o
    15b2o$114b2o58b2o$114bo2b2o$115b2obo$116bo40b2o$28b2o86bobo4b2o31bobo$
    28bo88b2o5bo31bo$26bobo94bo31b2o$26b2o68b2o25b2o$70bo25bo$58bo11b3o21b
    obo$56b3o14bo20b2o$40bo14bo16b2o$40b3o12b2o70b2o44b2o$43bo58b2o24bo44b
    o$4b2o36b2o59bo13b2o6b3o46b3o$5bo97bobo11b2o6bo50bo$5bobo96b2o$3b2ob2o
    35b2o$2bobo38b2o17b2o$2bobo57b2o$b2ob2o20b2o$bo24bo$2bob2o18bobo108b2o
    $obob2o18b2o109b2o$2o$59b2o32b2o$59bo20b2o11b2o$24b2o35bo19bo67b2o21b
    2o$24bobo33b2o16b3o67bobo21b2o$26bo29b2o20bo53b2o14bo17b2o$26b3o27bo
    75bo14b2o17b2o$29bo27b3o37b2o14b2o3b2o13bo$28b2o29bo38bo15bo3bo13b2o$
    95b3o13b3o5b3o46b2o$95bo15bo9bo39b2o5b2o$161b2o4$18b2o$18b2o2$9b2o$10b
    o$7b3o45b2o$7bo47b2o$15b2o32b2o$15bo33b2o$16bo$15b2o$51b2o$44b2o5b2o$
    44b2o!""")

    inserter2c3=pattern("""
    51bo$49b3o$23b2o23bo$24bo23b2o$24bobo$25b2o2b2o37bo$29b2o35b3o15bo9bo$
    65bo18b3o5b3o$52b2o11b2o20bo3bo$52b2o32b2o3b2o8$23b2o52b2o$22bobo16b2o
    34b2o$22bo18bobo45b2o$21b2o20bo44bo2bo$37b2o4b2o44b2o4b2o$37bo2bo54bob
    o$39b2o56bo$30b2o65b2o$30b2o20b2o33b2o$53bo34bo$50b3o32b3o$50bo34bo2$
    19bo$19b3o$22bo$21b2o2$85b2o$85bo$86b3o$88bo2$40b2o$40bo$38bobo$38b2o
    12$20b2o15b2o$19bobo15b2o$19bo$18b2o6$25bo$25b3o$28bo$27b2o28b2o$57bo$
    55bobo$51b2o2b2o$51b2o4$50b2o4b2o$50b2o4b2o5$23b2o35bo$22bobo33b3o$22b
    o34bo$21b2o34b2o$25b2o$25b2o3bo$29bobo6b2o$30bobo5b2o$32bo$32b2o$24b2o
    $25bo$25bobo$26b2o2b2o$30b2o32b2o$64b2o4$59b2o$59b2o$63b2o$63b2o3$24b
    2o31b2o$23bobo16b2o13b2o$23bo18bobo$22b2o20bo$38b2o4b2o$38bo2bo$40b2o$
    31b2o$31b2o7$21b2o$22bo$22bobo$23b2o$39bo$37b3o$36bo$36b2o3$10b2o$10b
    2o7$43b2o$43b2o4$38b2o$38b2o$2o40b2o$2o40b2o3$36b2o$21b2o13b2o$21bobo$
    23bo$23b2o!""")

    all=highway_robber(86,0) + connecting_neck(195,262) + transmitter2c3(347,219) \
          + wire2c3 + receiver2c3(2103,1763) + inserter2c3(2024,2042)
    while g.numlayers()>1: g.dellayer()

    all.display("Stable Pseudo-Heisenburp Device")
    g.setmag(0)
    setposint(120,200)

    g.setname("Highway Robber")
    g.clone()
    g.setname("2c/3 Transmitter")
    setposint(500,400)
    g.clone()
    g.setname("2c/3 Receiver")
    setposint(2175,2000)
    g.clone()
    g.setname("Stable Pseudo-Heisenburp Device")
    g.clone()
    g.setname("Glider Fleet")
    setposint(330,290)

    # since the tiles change size depending on how many layers have been created,
    # have to create all five layers before checking visibility of components --
    # now go back and check that the critical areas are all visible:
    g.setlayer(0)
    while not g.visrect([100,100,150,175]): g.setmag(g.getmag()-1)
    g.setlayer(1)
    while not g.visrect([350,225,400,350]): g.setmag(g.getmag()-1)
    g.setlayer(2)
    while not g.visrect([2100,1750,225,300]): g.setmag(g.getmag()-1)
    g.setlayer(3)
    g.fit()
    g.setlayer(4)
    while not g.visrect([0,200,300,400]): g.setmag(g.getmag()-1)
    g.update()

# -----------------------------------------------------

def show_status_text(s, d, t):
    if d==-1:
        if t==1:
            g.show(s + "Speed is " + str(t) + " tick per step.")
        else:
            g.show(s + "Speed is " + str(t) + " ticks per step.")
    else:
        g.show(s + "Delay between ticks is " + str(d) + " seconds.")

# -----------------------------------------------------

# if there are multiple layers, get permission to remove them
if g.numlayers() > 1:
    answer = g.getstring("All existing layers will be removed. OK?")
    if answer[:1].lower() == "n":
        g.exit()
oldswitch = g.setoption("switchlayers", True) # allow user to switch layers
oldtile = g.setoption("tilelayers", True)
rule()
try:
    burp()
finally:
    # remove the cloned layers added by the script
    while g.numlayers() > 1: g.dellayer()
    g.setname("Stable Pseudo-Heisenburp Device")
    g.setoption("tilelayers", oldtile)
    g.setoption("switchlayers", oldswitch)
    g.show("")
