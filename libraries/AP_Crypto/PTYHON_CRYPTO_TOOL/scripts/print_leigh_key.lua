-- print_leigh_key.lua - Simple script to print LAS_CRYPT_KEY parameter

-- Send initial message to confirm script is loaded
gcs:send_text(6, 'print_leigh_key.lua: Script loaded and starting')

function update()
    -- Get the current LAS_CRYPT_KEY parameter value
    local las_key_value = param:get('LAS_CRYPT_KEY')
    
    if las_key_value then
        -- Print the value to console
        local msg = string.format('LAS_CRYPT_KEY: %d', las_key_value)
        gcs:send_text(6, msg)
        -- Try to print to terminal if available
        if terminal_print then
            terminal_print(msg)
        end
    else
        -- Parameter not found or not set
        local msg = 'LAS_CRYPT_KEY: not found'
        gcs:send_text(6, msg)
        if terminal_print then
            terminal_print(msg)
        end
    end
    
    -- Return the update function with a 1000ms (1 second) delay
    -- This schedules the function to run again after 1 second
    return update, 1000
end

-- Start the update loop immediately
-- The function will run once, then reschedule itself every second
return update()



