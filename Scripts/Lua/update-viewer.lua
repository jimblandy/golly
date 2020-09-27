-- Download the latest version of LifeViewer
--
-- Author:
--   Chris Rowett (crowett@gmail.com)

local g = golly()
local gp = require "gplus"

-- folder name for LifeViewer files
local lifeviewerfolder = "lifeviewer"

-- name of LifeViewer Javascript plugin
local lifeviewerfilename = "lv-plugin.js"

-- download URL for latest LifeViewer build
local lifeviewerdownload = "https://lazyslug.com/lifeview/plugin/js/release/"

-- path separator
local pathsep = g.getdir("app"):sub(-1)

--------------------------------------------------------------------------------

local function launchBrowser(uri)
    -- execute the file to open the browser
    local opersys = g.os()
    if opersys == "Windows" then
        os.execute("start \"\" \""..uri.."\"")
    elseif opersys == "Linux" then
        os.execute("xdg-open \""..uri.."\"")
    elseif opersys == "Mac" then
        os.execute("open \""..uri.."\"")
    end
end

--------------------------------------------------------------------------------

local function createDirectory(path)
    local opersys = g.os()
    if opersys == "Windows" then
        os.execute("mkdir \""..path.."\"")
    elseif opersys == "Linux" then
        os.execute("mkdir -p \""..path.."\"")
    elseif opersys == "Mac" then
        os.execute("mkdir -p \""..path.."\"")
    end
end

--------------------------------------------------------------------------------

local function downloadLifeViewer()
    -- check LifeViewer exists in Golly's Scripts/Lua/gplus folder
    local lifeviewerdir = g.getdir("data")..lifeviewerfolder..pathsep
    local lifeviewerpath = lifeviewerdir..lifeviewerfilename
    -- prompt to download LifeViewer (Cancel will abort script)
    g.note("Update LifeViewer\n\nClick OK to go to download page or Cancel to stop.\n\n"..lifeviewerdownload)

    -- attempt to create the LifeViewer folder
    createDirectory(lifeviewerdir)

    -- open the default browser at the download page
    launchBrowser(lifeviewerdownload.."?dir="..lifeviewerpath)
end

--------------------------------------------------------------------------------

local _, err = xpcall(downloadLifeViewer, gp.trace)
if err then g.continue(err) end

-- this code is always executed, even after escape/error;
g.check(false)
