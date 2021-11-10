-- This script runs the current pattern and plays different piano notes
-- depending on how the population changes.  Hopefully a useful template
-- for people who want to experiment with CA-generated "music".

local g = golly()

if g.sound() == 0 then g.exit("Sound support is not enabled.") end
if g.sound() == 1 then g.exit("Sound support failed to initialize.") end
if g.empty() then g.exit("There is no pattern.") end

local soundsdir = g.getdir("app").."Scripts/Lua/sounds/piano/"
local sounds = {
    soundsdir.."C3.ogg",
    soundsdir.."D3.ogg",
    soundsdir.."E3.ogg",
    soundsdir.."F3.ogg",
    soundsdir.."G3.ogg",
    soundsdir.."A3.ogg",
    soundsdir.."B3.ogg",
    soundsdir.."C4.ogg",
    soundsdir.."D4.ogg",
    soundsdir.."E4.ogg",
    soundsdir.."F4.ogg",
    soundsdir.."G4.ogg",
    soundsdir.."A4.ogg",
    soundsdir.."B4.ogg",
    soundsdir.."C5.ogg",
    soundsdir.."D5.ogg",
    soundsdir.."E5.ogg",
    soundsdir.."F5.ogg",
    soundsdir.."G5.ogg",
    soundsdir.."A5.ogg",
    soundsdir.."B5.ogg",
    soundsdir.."C6.ogg",
    soundsdir.."D6.ogg",
    soundsdir.."E6.ogg",
}
local numsounds = #sounds
local minpop = tonumber(g.getpop())
local maxpop = minpop
local prevsound, samecount = 0, 0 -- used to detect a repeating sound
local genspersec = 5
local volume = 0.3

--------------------------------------------------------------------------------

function ShowHelp()
    g.note([[
Hit up/down arrows to change volume.
Hit -/+ keys to change gens per sec.
Hit enter/return to toggle generating.
Hit space bar to step 1 gen.
Hit H to see this help.
Hit escape to exit script.
]])
end

--------------------------------------------------------------------------------

function ShowInfo()
    g.show(string.format("gens/sec=%d volume=%.1f (hit H for help)", genspersec, volume))
end

--------------------------------------------------------------------------------

function PlaySound()
    -- the next (non-empty) generation has just been created
    -- so get the current population and update minpop and maxpop
    local currpop = tonumber(g.getpop())
    if currpop < minpop then minpop = currpop end
    if currpop > maxpop then maxpop = currpop end
    local poprange = maxpop - minpop
    if poprange == 0 then
        g.sound("play", sounds[numsounds//2], volume)
    else
        local p = (currpop-minpop) / poprange
        -- p is from 0.0 to 1.0
        local i = 1 + math.floor(p * (numsounds-1))
        g.sound("play", sounds[i], volume)

        -- occasionally play a chord
        if math.random() < 0.1 then
            -- two notes up will be a major or minor third
            -- since only white notes are used
            local j = i + 2
            if j > numsounds then j = 1 end
            g.sound("play", sounds[j], volume)
        end

        -- note that we can end up repeating the same sound
        -- (eg. if the initial pattern is a dense Life soup),
        -- so if that happens we reset minpop and maxpop
        if i == prevsound then
            samecount = samecount + 1
            if samecount == numsounds then
                minpop = currpop
                maxpop = currpop
                prevsound, samecount = 0, 0
            end
        else
            prevsound = i
            samecount = 0
        end
    end
end

--------------------------------------------------------------------------------

function EventLoop()
    local nextgen = 0
    local running = true
    while true do
        local space = false
        local event = g.getevent()
        if #event == 0 then
            g.sleep(5) -- don't hog CPU if idle
        elseif event == "key space none" then
            running = false
            space = true
            nextgen = 0
        elseif event == "key return none" then
            running = not running
        elseif event == "key h none" then
            ShowHelp()
        elseif event == "key up none" then
            volume = math.min(1.0, volume+0.1)
            ShowInfo()
        elseif event == "key down none" then
            volume = math.max(0.0, volume-0.1)
            ShowInfo()
        elseif event == "key = none" or event == "key + none" then
            genspersec = math.min(30, genspersec+1)
            ShowInfo()
        elseif event == "key - none" then
            genspersec = math.max(1, genspersec-1)
            ShowInfo()
        else
            g.doevent(event) -- might be a keyboard shortcut
        end
        if (running or space) and g.millisecs() >= nextgen then
            g.run(1)
            if g.empty() then g.exit("The pattern died.") end
            PlaySound()
            g.update()
            if not space then
                nextgen = g.millisecs() + 1000/genspersec
            end
        end
    end
end

--------------------------------------------------------------------------------

ShowInfo()
EventLoop()
