# Bill Gosper's pure-period p1100 double MWSS gun, circa 1984

use strict;

g_new("P1100 gun");
g_setalgo("HashLife");
g_setrule("B3/S23");

# update status bar now so we don't see different colors when
# g_show is called
g_update();

my $glider = g_parse('bo$bbo$3o!');
my $block = g_parse('oo$oo!');
my $eater = g_parse('oo$bo$bobo$bboo!');
my $bhept = g_parse('bbo$boo$oo$boo!');

my $centinal = $eater;
push @$centinal,@{g_transform($bhept,8,3)};
push @$centinal,@{g_evolve(g_transform($centinal,51,0,-1,0,0,1),1)};
push @$centinal,@{g_transform($centinal,0,16,1,0,0,-1)};

my @temp = (@{g_transform(g_evolve($centinal,1),16,3,0,-1,1,0)},
            @{g_transform(g_evolve($centinal,19),52,0,0,-1,1,0)},
            @{g_transform(g_evolve($centinal,81),55,51,0,1,-1,0)},
            @{g_transform(g_evolve($centinal,99),91,54,0,1,-1,0)});
my $overpass = \@temp;

# build the source signal -- the most compact set of gliders
# from which all other gliders and spaceships can be generated.
# Also add the first four centinals to guide the recipe gliders
my @MWSSrecipes = ( @{g_transform($glider,5759,738,0,1,-1,0)},
                    @{g_transform($glider,6325,667,-1,0,0,1)},
                    @{g_transform(g_evolve($glider,3),5824,896,-1,0,0,1)},
                    @{g_transform(g_evolve($glider,2),5912,1264)},
                    @{g_transform(g_evolve($glider,2),6135,1261,-1,0,0,1)},
                    @{g_transform(g_evolve($glider,1),5912,1490,1,0,0,-1)},
                    @{g_transform($glider,6229,4717,-1,0,0,1)},
                    @{g_transform(g_evolve($glider,1),6229,5029,-1,0,0,-1)},
                    @{g_transform(g_evolve($glider,1),5920,5032,1,0,0,-1)},
                    @{g_transform($glider,6230,5188,-1,0,0,-1)},
                    @{g_transform(g_evolve($glider,3),6230,5306,-1,0,0,-1)},
                    @{g_transform(g_evolve($glider,3),5959,5309,1,0,0,-1)},
                    @{g_transform(g_evolve($centinal,44),6185,5096,-1,0,0,1)},
                    @{g_transform(g_evolve($centinal,73),5897,1066)},
                    @{g_transform(g_evolve($centinal,42),5782,690)},
                    @{g_transform(g_evolve($centinal,25),5793,897,0,-1,1,0)},
                    6095,-65,6075,228); # suppress outbound MWSSes with two extra cells

#  to change behavior at center, comment out one of the options below:
# true p1100 gun
#my @all = (@{g_transform($eater,6018,5924,0,1,-1,0)},
#      @{g_transform($eater,6037,5943,0,1,-1,0)});
# synch center to recreate original p1100x5 gun pattern:
my @all = @{g_transform($block,6018,6787)};

# generate the MWSSes for the glider-fanout ladder
for (my $i = 0; $i < 7; $i += 1) {
    g_show("Building rung $i of ladder...");
    push @all, @MWSSrecipes;
    @all = @{g_evolve(\@all,1100)};
}

# add the actual glider-fanout ladder -- six rungs
for (my $i = 0; $i < 6; $i += 1) {
    push @all,  @{g_transform($glider,6030,1706+550*$i,0,-1,-1,0)},
                @{g_transform(g_evolve($centinal,15),6102,1585+550*$i)},
                @{g_transform($block,6029,1721+550*$i)},
                @{g_transform(g_evolve($centinal,34),5996,1725+550*$i,0,1,-1,0)},
                @{g_transform($block,6087,1747+550*$i)},
                @{g_transform(g_evolve($centinal,87),6122,1745+550*$i,0,-1,1,0)};
}

# add the rest of the centinals to guide the ladder's output gliders
g_show("Adding centinal reflectors...");
push @all,  @{g_transform(g_evolve($centinal,88),5704,0)},
            @{g_transform(g_evolve($centinal,29),6423,295,-1,0,0,1)},
            @{g_transform(g_evolve($centinal,74),5616,298)},
            @{g_transform(g_evolve($centinal,40),6361,613,0,-1,1,0)},
            @{g_transform(g_evolve($centinal,23),6502,620,-1,0,0,1)},
            @{g_transform(g_evolve($centinal,36),5636,986)},
            @{g_transform(g_evolve($centinal,38),6370,1008,0,-1,1,0)},
            @{g_transform(g_evolve($centinal,19),5747,1347,0,-1,1,0)},
            @{g_transform(g_evolve($centinal,67),5851,1516)},
            @{g_transform($centinal,4061,2605,0,1,-1,0)},
            @{g_transform(g_evolve($centinal,10),5376,3908,0,1,-1,0)},
            @{g_transform(g_evolve($centinal,77),8191,4407,-1,0,0,1)},
            @{g_transform(g_evolve($centinal,6),4988,4606)},
            @{g_transform(g_evolve($centinal,34),6357,4608,-1,0,0,1)},
            @{g_transform(g_evolve($centinal,27),8129,4621,-1,0,0,1)},
            @{g_transform(g_evolve($centinal,92),5159,5051)},
            @{g_transform(g_evolve($centinal,53),7991,5201,-1,0,0,1)},
            @{g_transform(g_evolve($centinal,94),7038,5370,0,1,-1,0)},
            @{g_transform(g_evolve($centinal,13),5591,5379,0,1,-1,0)},
            @{g_transform(g_evolve($centinal,3),5858,5428,0,1,-1,0)},
            @{g_transform(g_evolve($centinal,87),7805,5511,-1,0,0,1)},
            @{g_transform(g_evolve($centinal,98),401,5557,0,1,-1,0)},
            @{g_transform(g_evolve($centinal,14),955,5561,0,1,-1,0)},
            @{g_transform(g_evolve($centinal,8),6592,5584,0,1,-1,0)},
            @{g_transform(g_evolve($centinal,39),6933,5698,-1,0,0,1)},
            @{g_transform(g_evolve($centinal,32),6230,5881)},
            @{g_transform(g_evolve($centinal,47),11676,5854,0,1,-1,0)},
            @{g_transform(g_evolve($centinal,68),0,5748,0,1,-1,0)},
            @{g_transform(g_evolve($centinal,89),6871,5912,-1,0,0,1)},
            @{g_transform(g_evolve($centinal,45),12095,6027,0,1,-1,0)},
            @{g_transform(g_evolve($centinal,86),6209,6134)},
            @{g_transform(g_evolve($centinal,55),6868,6357,-1,0,0,1)},
            @{g_transform(g_evolve($centinal,95),9939,6491,0,1,-1,0)},
            @{g_transform(g_evolve($centinal,23),8782,6548,0,1,-1,0)},
            @{g_transform(g_evolve($centinal,58),3066,6572,0,1,-1,0)},
            @{g_transform(g_evolve($centinal,21),9326,6596,0,1,-1,0)},
            @{g_transform(g_evolve($centinal,80),3628,6626,0,1,-1,0)},
            @{g_transform(g_evolve($centinal,45),6821,6528,-1,0,0,1)},
            @{g_transform(g_evolve($centinal,33),10373,6649,0,1,-1,0)},
            @{g_transform(g_evolve($centinal,16),2587,6685,0,1,-1,0)};

# reflect the symmetrical part and add the center asymmetric stuff --
push @all,  @{g_transform(\@all,0,13583,1,0,0,-1)},
            @{g_transform($block,1081,6791)},
            @{g_transform($block,2731,6791)},
            @{g_transform($block,3831,6791)},
            @{g_transform($block,9108,6791)},
            @{g_transform($block,10208,6791)},
            @{g_transform($block,11308,6791)},
            @{g_transform($overpass,8475,6737)},
            @{g_transform(g_evolve($overpass,39),3365,6737,-1,0,0,1)},
            @{g_transform($overpass,9575,6737)},
            @{g_transform(g_evolve($overpass,39),2265,6737,-1,0,0,1)},
            @{g_transform($overpass,10675,6737)},
            @{g_transform(g_evolve($overpass,39),1715,6737,-1,0,0,1)};

push @MWSSrecipes,@{g_transform(\@MWSSrecipes,0,13583,1,0,0,-1)};

g_putcells(\@all);
g_fit();

# Different glider paths are different lengths, so incomplete
# glider recipes must be overwritten for a while to prevent disaster.
for (my $i = 0; $i < 46; $i += 1) {
    my $t = 49500 - $i*1100;
    g_show("Filling glider tracks -- $t ticks left.");
    g_update();
    g_putcells(\@MWSSrecipes);
    g_run(1100);
}
g_show("");

# reset gen count to 0
g_setgen("0");
