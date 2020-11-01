-- Download the latest version of LifeViewer
--
-- Author:
--   Chris Rowett (crowett@gmail.com)

local g = golly()
local gp = require "gplus"
local gpl = require "gplus.lifeviewer"

--------------------------------------------------------------------------------

local _, err = xpcall(gpl.download, gp.trace)
if err then g.continue(err) end

-- this code is always executed, even after escape/error;
g.check(false)
