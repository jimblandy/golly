-- Safely open a pattern from the clipboard in a safe 256 state rule
-- If the pattern has a known legacy rule then convert it
--
-- Author: Chris Rowett, March 2024
--   some code adapted from toChangeState.lua by Dave Greene
--   v.1.1:  include in added comments a link to likely LifeWiki location for missing rule, when appropriate
--   v.1.2:  support bounded grids, detect invalid pattern data, help window with LifeWiki rule download link

local g = golly()
local gp = require "gplus"

local tempname = "safeopenclip.rle"   -- temporary pattern file name
local helpname = "safeopenclip.html"  -- rule help html file name
local saferule = "Display256"         -- safe 256 state rule

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

-- help text used when the clipboard contains a pattern with an unsupported rule
local helptext =
[[
<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
</head>
<body bgcolor="#FFFFCE">
<title>Rule Help</title>
<h2>Unsupported Rule</h2>
<p>
The clipboard contains a pattern with an unsupported rule <b>RULENAME</b> so it has been opened in a safe rule.
</p>
<p>
If you want Golly to support this rule then it may already exist in the <a href="https://www.conwaylife.com/w/index.php?title=Special:AllPages&namespace=3794&from=">LifeWiki rule repository</a>.
</p>
<p>
Clicking the link below will download and install the rule if it exists or display "Web request failed" if it doesn't.
</p>
<p>
<a href=get:https://www.conwaylife.com/rules/RULENAME.rule>https://www.conwaylife.com/rules/RULENAME.rule</a>
</p>
</body>
</html>
]]

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

local function getclipboard()
    -- the optional non-zero parameter to g.getclip suppresses the Golly
    -- warning if the clipboard can not be opened
    g.getclip(1)
end

--------------------------------------------------------------------------------

local function openrulehelp(rule)
    -- create an HTML page
    local filename = g.getdir("temp")..helpname
    local file, msg = io.open(filename, "w")
    if file == nil then
        g.note("Could not create temporary help file!\n"..msg)
    else
        -- put the rule name into the help text and write to the HTML file
        helptext = helptext:gsub("RULENAME", rule)
        file:write(helptext)
        file:close()

        -- open temporary file containing rule help
        g.open(filename)
    end
end

--------------------------------------------------------------------------------

local function openpattern(text, rule, bounded, message)
    -- write the contents out to a temporary file
    local filename = g.getdir("temp")..tempname
    local file, msg = io.open(filename, "w")
    if file == nil then
        g.note("Could not create temporary pattern file!\n"..msg)
    else
        file:write(text)
        file:close()

        -- open temporary file containing pattern
        g.open(filename)

        -- if a rule was found then see if it is in the list to convert
        if rule ~= "" then
            -- see if there is a mapping for the rule
            local mapping = mappings[rule:lower()]
            if mapping ~= nil then
                -- mapping found so convert states and switch to new rule
                swapcells(g.getrect(), mapping.map)

                -- add back any bounded grid definition to the new rule
                g.setrule(mapping.rule..bounded)

                -- display the canonical name of the new rule
		    local newrule = g.getrule()
		    local boundedpos = newrule:find(":")
		    if newrule:find(":") ~= nil then
                    newrule = newrule:sub(1, boundedpos - 1)
		    end
                message = "Converted pattern from rule "..rule.." to "..newrule..".  "..message
            end
        end
    end

    -- update status if needed
    if message ~= "" then
        g.show(message)
    end
end

--------------------------------------------------------------------------------

local function checkpattern(text, rule, bounded)
    local result = false
    local original = text

    -- check if the pattern contains a rule
    if rule ~= "" then
        -- create a safe pattern in the rule
        text = "x=1,y=1,rule="..rule..bounded.."\no!"

        -- set the clipboard to the modified pattern
        g.setclipstr(text)

        -- check if the clipboard can be opened
        if pcall(getclipboard) then
            result = true
        end

        -- restore the original clipboard contents
        g.setclipstr(original)
    end

    return result
end

--------------------------------------------------------------------------------

local function trynewrule(text, rule, headerstartpos, headerendpos, rulepos)
    local result = false
    local original = text

    -- replace the original rule with the new one
    text = text:sub(1, headerstartpos + rulepos - 2).."rule="..rule..text:sub(headerendpos)

    -- set the clipboard to the modified pattern
    g.setclipstr(text)

    -- check if the clipboard can be opened
    if pcall(getclipboard) then
        result = true
    end

    -- restore the original clipboard contents
    g.setclipstr(original)

    return result
end

--------------------------------------------------------------------------------

local function safeopen()
    -- get the clipboard text
    local text = g.getclipstr()
    local rulepos = nil
    local rule = ""
    local boundedpos = nil
    local bounded = ""
    local headerstartpos = nil
    local headerendpos = nil
    local header = ""

    -- remove leading blank lines
    text = text:gsub("^%s+", "")

    -- find the header line
    headerstartpos = text:find("x[= ]")
    if headerstartpos ~= nil then
        headerendpos = text:find("[\r\n]", headerstartpos)
        if headerendpos == nil then
            headerendpos = #text + 1
        end
        header = text:sub(headerstartpos, headerendpos - 1)

        -- find the rule in the header line
        rulepos = header:find("rule%s*=")
        if rulepos ~= nil then
            rule = header:sub(rulepos):gsub("rule%s*=%s*", ""):gsub(" *$", "")

            -- check for bounded grid
            boundedpos = rule:find(":")
            if boundedpos ~= nil then
                bounded = rule:sub(boundedpos)
                rule = rule:sub(1, boundedpos - 1)
            end

            header = header:sub(1, rulepos - 1)
        end
    end

    -- check if the clipboard can be loaded
    if not pcall(getclipboard) then
        -- check if the pattern data is valid
        if rule == "" or not checkpattern(text, saferule, "") then
            -- pattern data is invalid
            g.note("The pattern in the clipboard contains invalid data and can not be opened.\n\nClick OK to see details or Cancel to abort.")

            -- if the user didn't cancel then attempt to open the clipboard so the detailed Golly error can be seen
            openpattern(text, rule, bounded, "")
        else
            -- check if there is a mapping available for the rule
            local mapping = mappings[rule:lower()]
            if mapping ~= nil then
                -- check if the clipboard can be loaded with the mapping
                if trynewrule(text, mapping.rule..bounded, headerstartpos, headerendpos, rulepos) then
                    -- the pattern is valid with the mapping so open it
                    text = text:sub(1, headerstartpos + rulepos - 2).."rule="..mapping.rule..bounded..text:sub(headerendpos)
                    openpattern(text, rule, bounded, "")
                else
                    -- check if there was a bounded grid and if so attempt to open the clipboard without it
                    if bounded ~= "" and trynewrule(text, mapping.rule, headerstartpos, headerendpos, rulepos) then
                        -- the pattern is valid with the mapping but without the bounded grid so ask for permission to change
                        g.note("The pattern in the clipboard contains a legacy rule with an invalid bounded grid definition.\n\nRule: "..rule..bounded.."\n\nPress OK to convert to a supported rule without the bounded grid or Cancel to abort.")

                        -- if the user didn't cancel then open the pattern without the bounded grid
                        text = text:sub(1, headerstartpos + rulepos - 2).."rule="..mapping.rule..text:sub(headerendpos)
                        openpattern(text, rule, "", "Invalid bounded grid definition removed ("..bounded..").")
                    end
                end
           else
                -- no mapping available so check if the clipboard can be loaded without the bounded grid
                if bounded ~= "" and trynewrule(text, rule, headerstartpos, headerendpos, rulepos) then
                    -- the pattern is valid without the bounded grid so ask for permission to change
                    g.note("The pattern in the clipboard contains a rule with an invalid bounded grid definition\n\nRule: "..rule..bounded.."\n\nPress OK to open without the bounded grid or Cancel to abort.")

                    -- if the user didn't cancel then open the pattern without the bounded grid
                    text = text:sub(1, headerstartpos + rulepos - 2).."rule="..rule..text:sub(headerendpos)
                    openpattern(text, rule, "", "Invalid bounded grid definition removed ("..bounded..").")
                else
                    -- if there is a bounded grid definition then check the safe rule can be opened with it
                    if bounded ~= "" and not trynewrule(text, saferule..bounded, headerstartpos, headerendpos, rulepos) then
                        -- the pattern is valid with the mapping but without the bounded grid so ask for permission to change
                        g.note("The pattern in the clipboard contains an unsupported rule with an invalid bounded grid definition.\n\nRule: "..rule..bounded.."\n\nPress OK to convert to a safe rule without the bounded grid or Cancel to abort.")

                        -- if the user didn't cancel then open the pattern without the bounded grid
                        text = text:sub(1, headerstartpos + rulepos - 2).."rule="..saferule..text:sub(headerendpos)
                        openpattern(text, saferule, "", "Opened pattern in a safe rule. Invalid bounded grid definition removed ("..bounded..").")
                    else
                        -- the clipboard can't be opened so ask for permission to convert to the safe rule
                        g.note("The pattern in the clipboard contains an unsupported rule.\n\nRule: "..rule..bounded.."\n\nPress OK to open in a safe rule or Cancel to abort.")

                        -- if the user didn't cancel then open the pattern in the safe rule
                        text = text:sub(1, headerstartpos + rulepos - 2).."rule="..saferule..bounded..text:sub(headerendpos)
                        openpattern(text, saferule, bounded, "Opened pattern in safe rule.")
                    end

                    -- show Rule Help if the rule looks like a named rule rather than a rulestring
                    if rule ~= "" then
                        local ruleadvice = ""
                        if rule:find("[^%w%-%^%+_]") == nil then
                            -- make the allowable characters URL-safe
                            ruleurlsafe = rule:gsub("%+", "%%%%2B"):gsub("%^", "%%%%5E")

                            -- open the help page
                            openrulehelp(ruleurlsafe)
                        end
                    end
                end
            end
        end
    else
        -- clipboard contains a valid pattern so open it
        openpattern(text, rule, bounded, "")
    end
end

--------------------------------------------------------------------------------

local status, err = xpcall(safeopen, gp.trace)
if err then g.continue(err) end
-- the following code is always executed

-- ensure the following code *completes*, even if user quits Golly
g.check(false)
