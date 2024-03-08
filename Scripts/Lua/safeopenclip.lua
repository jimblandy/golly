-- Safely open a pattern from the clipboard in a benign 256 state rule
-- If the pattern has a known legacy rule then convert it
--
-- Author: Chris Rowett, March 2024
--   some code adapted from toChangeState.lua by Dave Greene

local g = golly()
local gp = require "gplus"

local tempname = "safeopenclip.rle"                    -- temporary pattern file name
local saferule = "Display256"                    -- benign 256 state rule
local safeheader = "x = 1, y = 1, rule = "..saferule.."\n"    -- safe rule header

-- list of legacy rule to new rule mappings with optional list of state conversion pairs
--   [<legacy name>] = {rule = <new name>, map = {<from>, <to>, ...}}
-- note: <legacy name> must be in lower case
local mappings = {
    ["extendedlife"] = {rule = "B3/S23Investigator", map = {2, 9, 4, 14, 6, 4}},
    ["extendedtlife"] = {rule = "B3/S23-i34qInvestigator", map = {2, 9, 4, 14, 6, 4}},
    ["stateinvestigator"] = {rule = "B3/S23Investigator", map = {}},
    ["leapinvestigator"] = {rule = "B2n3/S23-qInvestigator", map = {}},
    ["eightinvestigator"] = {rule = "B3/S238Investigator", map = {}},
    ["pedestrianinvestigator"] = {rule = "B38/S23Investigator", map = {}},
    ["merryinvestigator"] = {rule = "B3-eq4ciqt5ky/S2-c3-k4yz5i8Investigator", map = {}}
}

--------------------------------------------------------------------------------

local function swapcells(rect, map)
    -- check if there are cells to change and a mapping
    if #rect > 0 and #map > 0 then
        -- build the map
        local i
        local newstate = {}
        for i = 0, 255 do
            newstate[i] = i
        end

        i = 1
        while i < #map do
            newstate[map[i]] = map[i + 1]
            i = i + 2
        end

        -- get the bounding box
        local minx, miny, sizex, sizey = table.unpack(rect)
        local maxx = minx + sizex - 1
        local maxy = miny + sizey - 1

        -- get the list of non-zero cells
        local cells = g.getcells(rect)
        local numcells = #cells
        local newstate0 = newstate[0]

        -- if state 0 has changed then need to use the gaps in the cell list
        if newstate0 ~= 0 then
            local x = minx
            local y = miny
            local nextx, nexty
            local total = sizex * sizey
            local oldsecs = os.clock()
            local newsecs

            for i = 1, numcells, 3 do
                -- get the next non-zero cell location
                nextx = cells[i]
                nexty = cells[i + 1]

                -- cell states are 0 until the next non-zero cell is reached
                while x < nextx or y < nexty do
                    g.setcell(x, y, newstate0)
                    x = x + 1
                    if x > maxx then
                        x = minx
                        y = y + 1

                        -- might be large pattern so show percentage done each second
                        newsecs = os.clock()
                        if newsecs - oldsecs >= 1.0 then
                            oldsecs = newsecs
                            g.show("Changing cell states: "..string.format("%.1f", 100 * ((y - miny) * sizex) / total).."%")
                            g.update()
                        end
                    end
                end
      
                -- convert the non-zero cell
                g.setcell(nextx, nexty, newstate[cells[i + 2]])
                x = x + 1
                if x > maxx then
                    x = minx
                    y = y + 1
                end
            end
    
            -- process any final state 0 cells on the last line
            if y <= maxy then
                while x <= maxx do
                    g.setcell(x, y, newstate0)
                    x = x + 1
                end
            end
        else
            -- no state 0 change so just convert each non-zero cell
            for i = 1, numcells, 3 do
                g.setcell(cells[i], cells[i + 1], newstate[cells[i + 2]])
            end
        end
    end
end

--------------------------------------------------------------------------------

local function getrule(text)
    local rule = ""

    -- search for a rule definition
    local pos = text:find("rule%s*=")
    if pos ~= nil then
        -- if found then find the end of line following it
        local newline = text:find("\n", pos)
        if newline ~= nil then
            -- isolate the rule definition
            rule = text:sub(pos, newline - 1):gsub("rule%s*=%s*", ""):gsub(" *$", "")
        end
    end

    return rule
end

--------------------------------------------------------------------------------

local function safeopen()
    -- get the clipboard text
    local text = g.getclipstr()
    local rule = ""

    -- check if the clipboard can be loaded
    if not pcall(g.getclip) then
        -- pattern not valid so see if the rule is defined
        rule = getrule(text)

        -- remove leading blank lines
        text = text:gsub("^%s+", "")

        -- start position of pattern body
        local startpos = 1

        -- search for an RLE header line (x followed by space or =)
        if text:sub(1, 1) == "x" and (text:sub(2, 2) == " " or text:sub(2, 2) == "=") then
            -- header found so find start of next line
            startpos = text:find("\n") + 1
        end
    
        -- prefix pattern body with the valid header
        text = safeheader..text:sub(startpos)
    end
    
    -- write the contents out to a temporary file
    local filename = g.getdir("temp")..tempname
    file, msg = io.open(filename, "w")
    if file == nil then
        g.note("Could not create temporary file!n\n"..msg)
    else
        file:write(text)
        file:close()

        -- open temporary file containing pattern
        g.open(filename)

        -- if a rule was found then see if it is in the list to convert
        if rule ~= "" then
            local mapping = mappings[rule:lower()]
            if mapping ~= nil then
                -- mapping found so convert states and switch to new rule
                swapcells(g.getrect(), mapping.map)
                g.setrule(mapping.rule)

                -- display the canonical name of the new rule
                g.show("Converted pattern from rule "..rule.." to "..g.getrule()..".")
            end
        end
    end
end

--------------------------------------------------------------------------------

local status, err = xpcall(safeopen, gp.trace)
if err then g.continue(err) end
-- the following code is always executed

-- ensure the following code *completes*, even if user quits Golly
g.check(false)