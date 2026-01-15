--[[
    test_script.lua
    
    DESCRIPTION:
    This Lua script continuously prints the current LAS_CRYPT_KEY parameter value
    to the console once per second. Useful for monitoring the encryption key
    parameter value in real-time.
    
    USAGE:
    1. Copy this script to your autopilot's SD card in the APM/scripts/ directory
    2. Ensure LAS_SCR_ENABLE = 1 to enable Lua scripting
    3. The script will run automatically and print LAS_CRYPT_KEY every second
    
    REQUIREMENTS:
    - ArduPilot firmware with Lua scripting enabled (LAS_SCR_ENABLE = 1)
    - AP_Crypto library with LAS_CRYPT_KEY parameter support
    - SD card with APM/scripts/ directory
    
    OUTPUT:
    The script will send messages to the GCS console (Mission Planner) every second:
    - "LAS_CRYPT_KEY: <value>" if the parameter is set
    - "LAS_CRYPT_KEY: not found" if the parameter doesn't exist or isn't set
    
    NOTES:
    - The script runs continuously until LAS_SCR_ENABLE is set to 0
    - Update interval is 1000ms (1 second)
    - Uses severity level 6 (INFO) for console messages
--]]

--[[
    Update function that runs periodically
    This function is called repeatedly with a 1 second delay
--]]
function update()
    -- Wrap in error handling to catch any runtime errors
    local success, err = pcall(function()
        -- Get the current LAS_CRYPT_KEY parameter value
        local las_key_value = param:get('LAS_CRYPT_KEY')
        local current_time = millis():toint()
        
        if las_key_value then
            -- Print the value to console with timestamp
            gcs:send_text(6, string.format('test_script: LAS_CRYPT_KEY = %d (time: %d ms)', las_key_value, current_time))
            -- Optional: Play a single beep when key is found (uncomment to enable)
            -- notify:play_tune("C")
        else
            -- Parameter not found or not set
            gcs:send_text(6, string.format('test_script: LAS_CRYPT_KEY = not found (time: %d ms)', current_time))
            -- Optional: Play warning beep pattern when key not found (uncomment to enable)
            -- notify:play_tune("L16CCC")  -- three fast beeps
        end
    end)
    
    if not success then
        -- Report any errors that occurred
        gcs:send_text(3, 'test_script.lua ERROR: ' .. tostring(err))
    end
    
    -- Return the update function with a 1000ms (1 second) delay
    -- This schedules the function to run again after 1 second
    return update, 1000
end

-- Send initial message to confirm script is loaded
local success, err = pcall(function()
    gcs:send_text(6, 'test_script.lua: Script loaded and starting')
    
    -- Try to play the Charge fanfare (using GWBasic/QBasic PLAY syntax)
    local tune_success, tune_err = pcall(function()
        if notify and notify.play_tune then
            -- Try a simple tune first to verify notify works
            notify:play_tune("C")
            gcs:send_text(6, 'test_script: Playing fanfare...')
            -- Play the Charge fanfare using GWBasic/QBasic PLAY syntax
            -- Format: T(tempo)O(octave)L(length)notes
            -- T120 = tempo 120, O4 = octave 4, L4 = quarter note length
            notify:play_tune("T120O4L4EGEL2C")
            gcs:send_text(6, 'test_script: Fanfare should have played')
        else
            gcs:send_text(4, 'test_script: WARNING - notify system not available')
        end
    end)
    
    if not tune_success then
        gcs:send_text(3, 'test_script: Fanfare error - ' .. tostring(tune_err))
    end
end)

if not success then
    -- If we can't even send a message, there's a serious problem
    -- Try with emergency severity
    gcs:send_text(0, 'test_script.lua: ERROR - ' .. tostring(err))
end

-- Start the update loop immediately
-- The function will run once, then reschedule itself every second
return update()



