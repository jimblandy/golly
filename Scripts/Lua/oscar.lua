-- Oscar is an OSCillation AnalyzeR for use with Golly.
-- Author: Andrew Trevorrow (andrew@trevorrow.com), Mar 2016.
-- 
-- This script uses Gabriel Nivasch's "keep minima" algorithm.
-- For each generation, calculate a hash value for the pattern.  Keep all of
-- the record-breaking minimal hashes in a list, with the oldest first.
-- For example, after 5 generations the saved hash values might be:
-- 
--   8 12 16 24 25,
-- 
-- If the next hash goes down to 13 then the list can be shortened:
-- 
--   8 12 13.
-- 
-- If the current hash matches one of the saved hashes, it is highly likely
-- the pattern is oscillating.  By keeping a corresponding list of generation
-- counts we can calculate the period.  We also keep lists of population
-- counts and bounding boxes to reduce the chance of spurious oscillator
-- detection due to hash collisions.  The bounding box info also allows us
-- to detect moving oscillators (spaceships/knightships).

local g = golly()

-- initialize lists
local hashlist = {}     -- for pattern hash values
local genlist = {}      -- corresponding generation counts
local poplist = {}      -- corresponding population counts
local boxlist = {}      -- corresponding bounding boxes

-- check if outer-totalistic rule has B0 but not S8
local r = g.getrule()
r = string.match(r, "^(.+):") or r
local hasB0notS8 = r:find("B0") == 1 and r:find("/") > 1 and r:sub(-1,-1) ~= "8"

--------------------------------------------------------------------------------

local function show_spaceship_speed(period, deltax, deltay)
    -- we found a moving oscillator
    if deltax == deltay or deltax == 0 or deltay == 0 then
        local speed = ""
        if deltax == 0 or deltay == 0 then
            -- orthogonal spaceship
            if deltax > 1 or deltay > 1 then
                speed = speed..(deltax + deltay)
            end
        else
            -- diagonal spaceship (deltax == deltay)
            if deltax > 1 then
                speed = speed..deltax
            end
        end
        if period == 1 then
            g.show("Spaceship detected (speed = "..speed.."c)")
        else
            g.show("Spaceship detected (speed = "..speed.."c/"..period..")")
        end
    else
        -- deltax != deltay and both > 0
        local speed = deltay..","..deltax
        if period == 1 then
            g.show("Knightship detected (speed = "..speed.."c)")
        else
            g.show("Knightship detected (speed = "..speed.."c/"..period..")")
        end
    end
end

--------------------------------------------------------------------------------

local function oscillating()
    -- return true if the pattern is empty, stable or oscillating
    
    -- first get current pattern's bounding box
    local pbox = g.getrect()
    if #pbox == 0 then
        g.show("The pattern is empty.")
        return true
    end
    
    local h = g.hash(pbox)
    
    -- determine where to insert h into hashlist
    local pos = 1
    local listlen = #hashlist
    while pos <= listlen do
        if h > hashlist[pos] then
            pos = pos + 1
        elseif h < hashlist[pos] then
            -- shorten lists and append info below
            for i = 1, listlen - pos + 1 do
                table.remove(hashlist)
                table.remove(genlist)
                table.remove(poplist)
                table.remove(boxlist)
            end
            break
        else
            -- h == hashlist[pos] so pattern is probably oscillating, but just in
            -- case this is a hash collision we also compare pop count and box size
            local rect = boxlist[pos]
            if tonumber(g.getpop()) == poplist[pos] and pbox[3] == rect[3] and pbox[4] == rect[4] then
                local period = tonumber(g.getgen()) - genlist[pos]
                
                if hasB0notS8 and (period % 2) > 0 and
                   pbox[1] == rect[1] and pbox[2] == rect[2] and
                   pbox[3] == rect[3] and pbox[4] == rect[4] then
                    -- ignore this hash value because B0-and-not-S8 rules are
                    -- emulated by using different rules for odd and even gens,
                    -- so it's possible to have identical patterns at gen G and
                    -- gen G+p if p is odd
                    return false
                end
                
                if pbox[1] == rect[1] and pbox[2] == rect[2] and
                   pbox[3] == rect[3] and pbox[4] == rect[4] then
                    -- pattern hasn't moved
                    if period == 1 then
                        g.show("The pattern is stable.")
                    else
                        g.show("Oscillator detected (period = "..period..")")
                    end
                else
                    local deltax = math.abs(rect[1] - pbox[1])
                    local deltay = math.abs(rect[2] - pbox[2])
                    show_spaceship_speed(period, deltax, deltay)
                end
                return true
            else
                -- look at next matching hash value or insert if no more
                pos = pos + 1
            end
        end
    end
    
    -- store hash/gen/pop/box info at same position in various lists
    table.insert(hashlist, pos, h)
    table.insert(genlist, pos, tonumber(g.getgen()))
    table.insert(poplist, pos, tonumber(g.getpop()))
    table.insert(boxlist, pos, pbox)
    
    return false
end

--------------------------------------------------------------------------------

local function fit_if_not_visible()
    -- fit pattern in viewport if not empty and not completely visible
    local r = g.getrect()
    if #r > 0 and not g.visrect(r) then g.fit() end
end

--------------------------------------------------------------------------------

g.show("Checking for oscillation... (hit escape to abort)")

local oldsecs = os.clock()
while not oscillating() do
    g.run(1)
    local newsecs = os.clock()
    if newsecs - oldsecs >= 1.0 then   -- show pattern every second
        oldsecs = newsecs
        fit_if_not_visible()
        g.update()
    end
end
fit_if_not_visible()
