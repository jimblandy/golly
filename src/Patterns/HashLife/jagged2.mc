[M2] (golly 2.0)
#C A variant of the jagged line pattern in which some of the gliders
#C from the LWSS collision undergo 3 kickback reactions.
#C 
#C The pattern looks roughly like this; view in a fixed-width font:
#C 
#C                                        F
#C                                        .
#C                                       . .
#C                                       . .
#C                                       . .
#C                                      .   .
#C                                      .   .
#C                                      .   .
#C                                     .     .
#C                                     .     .
#C                                     .     .
#C                                    .       .
#C                                    .       .
#C                                    .       .
#C                                   .         .
#C                                   .         .
#C                                   .         .
#C                     I            .           .            J
#C                     .            .           .            .
#C                      .           .           . L         .
#C                       .         .        ......         .
#C                        .        .  ......   . .        .
#C                         .     K ...        .  .       .
#C                          .     .   ...     .   .     .
#C                           .    .      ..  .    .    .
#C                            .   .        ...    .   .
#C                             . .          . ...  . .
#C                              ..         .     ....
#C                             G ..        .       . H
#C                              . ..      .       . .
#C                              .  ..     .      .  .
#C                              .   ..   .      .   .
#C                             .     .. .      .     .
#C                             .      ...     .      .
#C                             .       ..    .       .
#C                            .       . ..  .         .
#C                            .      .   ...          .
#C  .............................................................................
#C  A                         B     .     C           D                         E
#C                                 .
#C                                 .
#C                                .
#C                               .
#C                              .
#C                              .
#C                             .
#C                            .
#C                            .
#C                           .
#C                          .
#C                         .
#C                         .
#C                        .
#C                       .
#C                       .
#C                      .
#C                     .
#C                     M
#C 
#C The lines AE, BF, and DF are straight; the others are jagged.  CG is
#C actually two jagged lines, which sometimes cross.
#C 
#C BF and DF are lines of gliders headed southwest and southeast.
#C 
#C AC and CD are lines of LWSSs headed toward C.  At C, they crash to form
#C gliders headed northwest (CI) and northeast (CJ).
#C 
#C At H some of the gliders reflect toward the northwest, forming HK. The
#C rest escape, forming HJ.
#C 
#C At K the gliders reflect again, toward the northeast, forming KL.
#C 
#C At L they reflect again, toward the southwest, forming LM.
#C 
#C At G some of the northwestward gliders hit gliders in BF, forming
#C blinkers which are the other part of CG.  The rest escape, forming GI.
#C 
#C In generation t, some of the coordinates are (asymptotically):
#C 
#C     A=(-t/2,0)      B=(-t/6,0)     D=(t/6,0)    E=(t/2,0)    F=(0,t/2)
#C     I=(-t/4,t/4)    J=(t/4,t/4)    M=(-t/4,-t/4)
#C 
#C If the collision point C didn't drift, then the coordinates of the
#C other points would be:
#C 
#C     C=(0,0)           G=(-t/8,t/8)    H=(t/8,t/8)    K=(-t/10,t/5)
#C     L=(t/11,5t/22)
#C 
#C But it does drift, and I don't know if it oscillates back and forth,
#C or moves slowly but steadily in one direction.  (In gen 17409 it's moved
#C 260 units west; in gen 72609 it's 700 units east.)
#C 
#C In addition to the headon LWSS crashes at C, the glider+LWSS crashes
#C at B and D, the kickbacks at H, K, and L, and the glider+glider ->
#C blinker collisions at G, there are some other reactions that occur
#C occasionally:
#C 
#C When C drifts far enough to the left, one of the gliders on CH may hit
#C one of the blinkers, turning it into a pond; this first happens at gen
#C 11986.  A subsequent glider on the same path turns it into a block (first
#C happens at 12056), and a third glider deletes it (first at 12300).
#C 
#C Sometimes a glider on LM will hit a glider on CG, deleting both (first at
#C gen 11808).  And sometimes an LM glider will hit an eastward LWSS on BC;
#C this is a ternary reaction which deletes the glider and 2 LWSSs (first at
#C gen 5393).
#C 
#C Unless I've overlooked something, that's all that ever happens.
#C 
#C The open question here is what happens to the collision point C.  Does it
#C drift infinitely far west?  Infinitely far east?  Both?
#C 
#C Dean Hickerson (dean.hickerson@yahoo.com) 7 May 2005
$$$$$$$.....***$
4 0 0 0 1
$$$$$$$...***$
4 0 0 0 3
5 0 0 2 4
6 0 0 0 5
7 0 0 6 0
$$$$$$$****$
$$$$$$$.**$
4 0 0 8 9
$$$$$$.......*$
4 0 0 0 11
5 0 0 10 12
.....*$.....*$.....*$......*$
......*$.....***$....**.*$....***$....***$.....**$
4 0 14 0 15
*$....***$...*..*$...*...*$..**..*$....*$.....**$.....***$
...*..*$...*$...*$....*$*$$*$....*$
.....***$$$$$.....*$.....**$....*..*$
...***$...*.**$....***$....**$
4 17 18 19 20
$$$$.***$*****$*.***$**$
4 0 0 22 0
......**$
$$$$$$......*$.....***$
4 24 0 0 25
5 16 21 23 26
*...*$*$.*$$$...**$..*..*$..*..*$
**.**$.****$..**$$.....*$.....**$.......*$..*****$
..**.**$....**$$$$****$*...*$*$
..****$..*$$$...***$..*****$.**.***$..**$
4 28 29 30 31
$$$$$...*$....**$...**$
4 0 33 0 0
.*$
4 35 0 0 0
$...**$..****$.**.**$..**$$$.......*$
.....***$....*.**$...**$....***$.....***$$$...**$
*$***$...*$*...*$...*$..*$
4 37 0 38 39
5 32 34 36 40
$$$$$$$.....**$
4 0 0 42 0
....**.*$....***$....***$.....**$
4 0 44 0 0
....****$...**.**$....**$
$$$$$$$.***$
4 46 0 0 47
$$.......*$......**$.......*$
$$*$.***$****$***...*$.....*.*$......*$
4 0 0 49 50
5 43 45 48 51
6 13 27 41 52
$$....*$...***$...*.**$....***$....**$
$$$$...*$..***$.**..*$.*...*$
$$$...***$..*..*$.....*$.....*$..*.*$
.*.**$.**$
4 54 55 56 57
$$..*$.***$.*.**$..***$..**$
$$$$.***$.*..*$.*$.*$
$$$$.***$*****$***.**$...**$
4 59 0 60 61
5 0 0 58 62
$$$$$$$..**$
$$$$$$$.****$
4 0 0 64 65
5 0 0 0 66
..*$.*.**$.*..*$$.*$*.*$...*$
4 0 68 0 0
.*$..*$
$$$$$$$......**$
4 70 11 71 0
$$.***$.*..*$.*$.*$.*$..*$
$$....**$.***.**$.*****$..***$
4 73 0 74 0
.....***$.....**$.......*$
*$**$*$
4 76 77 47 0
5 69 72 75 78
$$$$$.*$*$**$
$$$$.......*$......**$.....*$......**$
.......*$$$$.......*$......**$......**$
4 80 81 0 82
**.**$****$.**$$$$.......*$***....*$
*...*$....*$...*$$$**$..*$..*$
***...**$..*....*$$$**$***$*.**$.**$
.**$*$$$$.****$*...*$....*$
4 84 85 86 87
$$.......*$.......*$$$$.....*$
$**$***$*.**$.**$
....****$..*****$.*$*...*.**$.*...***$..*$
$*$**$*$$$$**$
4 89 90 91 92
...*$
4 0 94 0 0
5 83 88 93 95
6 63 67 79 96
$$..****$..*...*$..*$...*$
$$...**$..**.**$...****$....**$
.....*$....*$....*$....**$......*$
....*$....**$...**$..**$$**$
4 98 99 100 101
..****$.**.**$..**$
..****$..*...*$..*$...*$
$.......*$.......*$$$$..***$..*$
4 103 104 0 105
4 104 0 0 0
$....*$...*$...*...*$...****$
4 108 94 0 0
5 102 106 107 109
$$$$....***$....*$.....*$
$**$$*$
4 111 70 112 0
5 113 0 0 0
6 110 114 0 0
...*$..*$$$......**$$.......*$
.......*$......*$$$*$*$
$...***$.....*$....*$
4 116 117 0 118
5 0 119 0 0
***....*$..*....*$..*$.*$
***$*.**$.**$......**$......**$.......*$
$$$$$$***$..*$
$.......*$
4 121 122 123 124
$$**.....*$.**...*$**$*$
$$***$..*$..*$.*$
*......*$*$**$.**....*$......*$...**$
$*$*$*$
4 126 127 128 129
.*$$$.....*$......**$
$*$.*$.*$**$
4 131 132 0 0
.......*$......*$
***$..*$..*$.*$
4 134 135 0 0
5 125 130 133 136
6 120 137 0 0
7 53 97 115 138
8 0 7 0 139
