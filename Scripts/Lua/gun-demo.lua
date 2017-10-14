-- Based on gun_demo.py from PLife (http://plife.sourceforge.net/).

local g = golly()
local gp = require "gplus"
local gpo = require "gplus.objects"

-- best to create empty universe before setting rule
-- to avoid converting an existing pattern (slow if large)
g.new("gun-demo")
g.setrule("B3/S23")

-- load gplus.guns AFTER setting rule to Life (for evolving phases)
local gpg = require "gplus.guns"
local gun24 = gpg.gun24
local gun30 = gpg.gun30
local gun46 = gpg.gun46

--------------------------------------------------------------------------------

local function mirror_gun(glider_gun)
    -- mirror an 'aligned' glider gun and preserve its timing
    return glider_gun[2].t(-1, 0, gp.swap_xy)
end

--------------------------------------------------------------------------------

local function compose_lwss_gun(glider_gun, A, B, C)
    -- construct an lwss gun using 3 copies of an 'aligned' glider gun
    -- where A, B and C are distances from the glider collision point to
    -- NE, NW and SW guns respectively
    local m = math.min(A, B, C)
    local a = A - m
    local b = B - m
    local c = C - m
    return glider_gun[4 * a].t( A, -A - 3, gp.flip_x) +
           glider_gun[4 * b].t(-B + 2, -B + 1) +
           glider_gun[4 * c + 1].t(-C + 6, C, gp.flip_y)
end

--------------------------------------------------------------------------------

local lwss_eater = gpo.eater.t(4, 2, gp.swap_xy)

local all =
    compose_lwss_gun(mirror_gun(gun24), 12, 11, 12).t(0, -100) + lwss_eater.t(100, -100) +
    compose_lwss_gun(gun24, 11, 13, 4) + lwss_eater.t(100, 0) +
    compose_lwss_gun(gun30, 13, 11, 4).t(0,  70) + lwss_eater.t(100,  70) +
    compose_lwss_gun(gun46, 22, 13, 3).t(0, 130) + lwss_eater.t(100, 130)

all.display("") -- don't change name
