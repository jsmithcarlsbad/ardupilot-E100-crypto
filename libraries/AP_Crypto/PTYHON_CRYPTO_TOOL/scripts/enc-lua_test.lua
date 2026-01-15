
local SCRIPT_NAME       = "Encrypted Lua Test File"

local MAV_SEVERITY = {EMERGENCY=0, ALERT=1, CRITICAL=2, ERROR=3, WARNING=4, NOTICE=5, INFO=6, DEBUG=7}

function update()
    gcs:send_text(1, "Encryption/Decryption Successful")
end

gcs:send_text(MAV_SEVERITY.ALERT, "Fuel_level v2 Lua loaded")

-- wrapper around update(). This calls update() and if update faults
-- then an error is displayed, but the script is not stopped
local function protected_wrapper()
    local success, err = pcall(update)
    if not success then
        gcs:send_text(MAV_SEVERITY.ERROR, "Internal Error: " .. err)
        -- when we fault we run the update function again after 1s, slowing it
        -- down a bit so we don't flood the console with errors
        return protected_wrapper, 1000
    end
    return protected_wrapper, 1000 -- DEBUG+++ ask for 1 per second
end

-- start running update loop
return protected_wrapper()