#include "AP_Crypto_Params.h"

#if AP_CRYPTO_ENABLED

#include <AP_Crypto/AP_Crypto.h>
#include <AP_HAL/AP_HAL.h>
#include <AP_Param/AP_Param.h>
#include <StorageManager/StorageManager.h>
#include <GCS_MAVLink/GCS.h>

extern const AP_HAL::HAL& hal;

AP_Crypto_Params::AP_Crypto_Params(void)
{
    AP_Param::setup_object_defaults(this, var_info);
}

const struct AP_Param::GroupInfo AP_Crypto_Params::var_info[] = {
    // @Param: CRYPT_KEY
    // @DisplayName: Encryption Key
    // @Description: Encryption key for AP_Crypto (write-only for security, reads as 0). Must be set explicitly via Mission Planner or MAVLink. Default value of 0 means no key is set.
    // @User: Advanced
    // @Range: -2147483648 2147483647
    AP_GROUPINFO("CRYPT_KEY", 1, AP_Crypto_Params, _key_param, 0),
    
    // @Param: CRYPT_LVL
    // @DisplayName: Encryption Level
    // @Description: Encryption level for Lua scripts and logs (0=disabled/no encryption, 1=encrypted .lua files (.enc), 2=encrypted .lua files AND encrypted log files, 3=encrypted .lua files AND encrypted log files, do NOT include lua script contents in ANY log files). Defaults to 2.
    // @User: Advanced
    // @Values: 0:Disabled,1:Lua Scripts Only,2:Lua Scripts + Logs,3:Lua Scripts + Logs (No Script Content in Logs)
    AP_GROUPINFO("CRYPT_LVL", 2, AP_Crypto_Params, _crypto_enable, 2),
    
    // @Param: CRYPT_STAT
    // @DisplayName: Encryption Key Status
    // @Description: Read-only status indicating if encryption key is stored (1=key stored, 0=no key stored)
    // @User: Advanced
    // @ReadOnly: 1
    // @Values: 0:No Key,1:Key Stored
    AP_GROUPINFO("CRYPT_STAT", 3, AP_Crypto_Params, _key_status, 0),
    
    AP_GROUPEND
};

void AP_Crypto_Params::handle_key_set(int32_t key_value)
{
    if (key_value == 0) {
        return;  // Ignore zero values
    }
    
    // Safety checks: don't access storage if it has failed or isn't ready
    // This prevents blocking or crashes during MAVLink parameter handling
    if (StorageManager::storage_failed()) {
        return;  // Storage not available, skip key storage
    }
    
    // Derive 32-byte key from INT32 value
    // EXACTLY matches Python tool derive_key_from_leigh_key_simple()
    uint8_t key[32];
    uint32_t uval = (uint32_t)key_value;  // Equivalent to & 0xFFFFFFFF for positive values
    const uint8_t *uval_bytes = (const uint8_t*)&uval;  // Little-endian bytes
    for (int i = 0; i < 32; i++) {
        // Explicitly mask to 8 bits to match Python's bytearray assignment behavior
        key[i] = (uint8_t)(uval_bytes[i % 4] ^ ((i * 0x73) & 0xFF));
    }
    
    // ALWAYS save the parameter value so it can be used for key derivation
    // even if key storage fails (LAS_CRYPT_STAT=0)
    enum ap_var_type ptype;
    AP_Int32 *key_param = (AP_Int32*)AP_Param::find("LAS_CRYPT_KEY", &ptype);
    if (key_param != nullptr) {
        key_param->set(key_value);
        key_param->save();
    }
    
    // Try to store the derived key in secure storage
    bool stored = AP_Crypto::store_key(key);
    if (stored) {
        gcs().send_text(MAV_SEVERITY_INFO, "Crypto key stored");
        // Update status parameter
        AP_Crypto_Params *params = (AP_Crypto_Params*)AP_Param::find("LAS_CRYPT_STAT", &ptype);
        if (params != nullptr) {
            params->_key_status.set(1);
        }
    } else {
        gcs().send_text(MAV_SEVERITY_WARNING, "Crypto key storage failed - using parameter value");
    }
}

bool AP_Crypto_Params::is_encryption_enabled(void)
{
    // Safety check: don't access parameters if system not initialized
    if (!AP_Param::initialised()) {
        return false;  // Default to disabled if param system not ready
    }
    
    enum ap_var_type ptype;
    AP_Int8 *crypto_enable = (AP_Int8*)AP_Param::find("LAS_CRYPT_LVL", &ptype);
    if (crypto_enable != nullptr) {
        // Level 1, 2, or 3 enables Lua script encryption
        int8_t level = crypto_enable->get();
        return (level == 1 || level == 2 || level == 3);
    }
    // Default to disabled if parameter not found
    return false;
}

bool AP_Crypto_Params::is_log_encryption_enabled(void)
{
    // Safety check: don't access parameters if system not initialized
    if (!AP_Param::initialised()) {
        return false;  // Default to disabled if param system not ready
    }
    
    enum ap_var_type ptype;
    AP_Int8 *crypto_enable = (AP_Int8*)AP_Param::find("LAS_CRYPT_LVL", &ptype);
    if (crypto_enable != nullptr) {
        // Level 2 or 3 enables log file encryption
        int8_t level = crypto_enable->get();
        return (level == 2 || level == 3);
    }
    // Default to disabled if parameter not found
    return false;
}

bool AP_Crypto_Params::exclude_lua_script_content_from_logs(void)
{
    // Safety check: don't access parameters if system not initialized
    if (!AP_Param::initialised()) {
        return false;  // Default to false if param system not ready
    }
    
    enum ap_var_type ptype;
    AP_Int8 *crypto_enable = (AP_Int8*)AP_Param::find("LAS_CRYPT_LVL", &ptype);
    if (crypto_enable != nullptr) {
        // Level 3 means do NOT include lua script contents in ANY log files
        return (crypto_enable->get() == 3);
    }
    // Default to false if parameter not found
    return false;
}

int8_t AP_Crypto_Params::get_key_status(void) const
{
    // Check if key is actually stored in storage
    return AP_Crypto::has_stored_key() ? 1 : 0;
}

bool AP_Crypto_Params::derive_key_from_param(uint8_t key[32])
{
    if (key == nullptr) {
        return false;
    }
    
    if (!AP_Param::initialised()) {
        return false;
    }
    
    // COMPLETELY NEW APPROACH: Force load entire AP_Crypto_Params object from storage
    // This ensures _key_param is loaded from storage before we read it
    enum ap_var_type ptype;
    AP_Crypto_Params *crypto_params = (AP_Crypto_Params*)AP_Param::find("LAS_CRYPT_STAT", &ptype);
    if (crypto_params == nullptr) {
        crypto_params = (AP_Crypto_Params*)AP_Param::find("LAS_CRYPT_LVL", &ptype);
    }
    if (crypto_params == nullptr) {
        return false;
    }
    
    // Force load entire object from storage - this loads ALL parameters in the group
    AP_Param::load_object_from_eeprom(crypto_params, var_info);
    
    // Now read _key_param directly from the instance - it should be loaded from storage
    int32_t key_value = crypto_params->_key_param.get();
    
    // Value must be non-zero for encryption
    if (key_value == 0) {
        return false;
    }
    
    // Key derivation: EXACTLY matches Python tool derive_key_from_leigh_key_simple()
    // Python: uval = leigh_key_value & 0xFFFFFFFF
    //         uval_bytes = struct.pack('<I', uval)  # little-endian uint32
    //         key[i] = uval_bytes[i % 4] ^ (i * 0x73)  # Python truncates to 8 bits automatically
    uint32_t uval = (uint32_t)key_value;  // Equivalent to & 0xFFFFFFFF for positive values
    const uint8_t *uval_bytes = (const uint8_t*)&uval;  // Little-endian bytes (like struct.pack('<I', uval))
    for (int i = 0; i < 32; i++) {
        // Explicitly mask to 8 bits to match Python's bytearray assignment behavior
        key[i] = (uint8_t)(uval_bytes[i % 4] ^ ((i * 0x73) & 0xFF));
    }
    
    return true;
}

#endif  // AP_CRYPTO_ENABLED
