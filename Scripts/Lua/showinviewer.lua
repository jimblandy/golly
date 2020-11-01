-- Launch LifeViewer in the default browser with the current Golly pattern
--
-- Author:
--   Chris Rowett (crowett@gmail.com)

local g = golly()
local gp = require "gplus"
local gpl = require "gplus.lifeviewer"

--------------------------------------------------------------------------------

local _, err = xpcall(gpl.launch, gp.trace)
if err then g.continue(err) end

-- this code is always executed, even after escape/error;
g.check(false)
