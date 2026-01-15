--[[
   print_leigh_key.lua - Display LAS_CRYPT_KEY parameter value
   
   This script demonstrates that encrypted Lua files can be loaded and executed,
   and can access the LAS_CRYPT_KEY parameter.
   
   Note: LAS_CRYPT_KEY returns 0 over MAVLink for security, but the actual
   value is stored and can be read by Lua scripts running on the device.
   Use LAS_CRYPT_STAT to verify if a key is stored (1 = key stored, 0 = no key).
--]]

local MAV_SEVERITY = {EMERGENCY=0, ALERT=1, CRITICAL=2, ERROR=3, WARNING=4, NOTICE=5, INFO=6, DEBUG=7}

-- Send initial message to confirm script is loaded (before any function calls)
gcs:send_text(MAV_SEVERITY.INFO, "print_E100_key: Script loaded from encrypted file!")

-- Get parameter values at top level (not in a function)
local key_param = Parameter("LAS_CRYPT_KEY")
local key_stat_param = Parameter("LAS_CRYPT_STAT")
local crypto_lvl_param = Parameter("LAS_CRYPT_LVL")

-- Get parameter values
local key_value = key_param:get()
local key_stat = key_stat_param:get()
local crypto_lvl = crypto_lvl_param:get()

-- Display information
gcs:send_text(MAV_SEVERITY.INFO, "=== LAS Crypto Key Status ===")

-- LAS_CRYPT_KEY value (stored in parameter, but returns 0 over MAVLink for security)
if key_value ~= nil then
    gcs:send_text(MAV_SEVERITY.INFO, string.format("LAS_CRYPT_KEY: %d", key_value))
else
    gcs:send_text(MAV_SEVERITY.ERROR, "LAS_CRYPT_KEY: parameter not found")
end

-- LAS_CRYPT_STAT shows if key is stored (1 = stored, 0 = not stored)
if key_stat ~= nil then
    if key_stat == 1 then
        gcs:send_text(MAV_SEVERITY.INFO, "LAS_CRYPT_STAT: 1 (key is stored)")
    else
        gcs:send_text(MAV_SEVERITY.WARNING, "LAS_CRYPT_STAT: 0 (no key stored)")
    end
else
    gcs:send_text(MAV_SEVERITY.EMERGENCY, "LAS_CRYPT_STAT: parameter not found")
end

-- LAS_CRYPT_LVL shows encryption level (0 = disabled, 1 = Lua scripts only, 2 = scripts + logs, 3 = scripts + logs, no script content in logs)
if crypto_lvl ~= nil then
    if crypto_lvl == 0 then
        gcs:send_text(MAV_SEVERITY.INFO, "LAS_CRYPT_LVL: 0 (encryption disabled)")
    elseif crypto_lvl == 1 then
        gcs:send_text(MAV_SEVERITY.INFO, "LAS_CRYPT_LVL: 1 (Lua scripts only)")
    elseif crypto_lvl == 2 then
        gcs:send_text(MAV_SEVERITY.INFO, "LAS_CRYPT_LVL: 2 (scripts + logs)")
    elseif crypto_lvl == 3 then
        gcs:send_text(MAV_SEVERITY.INFO, "LAS_CRYPT_LVL: 3 (scripts + logs, no script content in logs)")
    else
        gcs:send_text(MAV_SEVERITY.INFO, string.format("LAS_CRYPT_LVL: %d", crypto_lvl))
    end
else
    gcs:send_text(MAV_SEVERITY.WARNING, "LAS_CRYPT_LVL: parameter not found")
end

gcs:send_text(MAV_SEVERITY.INFO, "print_E100_key: Completed successfully!")

