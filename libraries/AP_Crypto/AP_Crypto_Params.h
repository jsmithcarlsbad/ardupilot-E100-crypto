#pragma once

#include "AP_Crypto_config.h"

#if AP_CRYPTO_ENABLED

#include <AP_Param/AP_Param.h>
#include <stdint.h>

/*
  AP_Crypto_Params - Parameter-based key management for AP_Crypto
  
  Provides LAS_CRYPT_KEY parameter for setting encryption key via MAVLink
*/
class AP_Crypto_Params : public AP_Param
{
public:
    AP_Crypto_Params(void);
    
    static const struct AP_Param::GroupInfo var_info[];
    
    // Handle key setting from parameter
    static void handle_key_set(int32_t key_value);
    
    // Check if encryption is enabled (instance method)
    bool encryption_enabled(void) const { return _crypto_enable != 0; }
    
    // Check if encryption is enabled (static method, uses parameter system)
    // Returns true if LAS_CRYPT_LVL >= 1 (Lua script encryption enabled)
    static bool is_encryption_enabled(void);
    
    // Check if log file encryption is enabled (static method, uses parameter system)
    // Returns true if LAS_CRYPT_LVL == 2 or 3 (log file encryption enabled)
    static bool is_log_encryption_enabled(void);
    
    // Check if Lua script contents should be excluded from log files
    // Returns true if LAS_CRYPT_LVL == 3 (do NOT include lua script contents in ANY log files)
    static bool exclude_lua_script_content_from_logs(void);
    
    // Get key status (1 = key stored, 0 = no key) - for LAS_CRYPT_STAT parameter
    int8_t get_key_status(void) const;
    
    // Get the actual LAS_CRYPT_KEY parameter value (bypasses MAVLink security)
    int32_t get_key_value(void) const { return _key_param.get(); }
    
    // Derive key directly from LAS_CRYPT_KEY parameter value
    // This allows decryption even if key storage failed
    static bool derive_key_from_param(uint8_t key[32]);
    
private:
    AP_Int32 _key_param;      // LAS_CRYPT_KEY parameter (write-only, reads as 0)
    AP_Int8 _crypto_enable;   // LAS_CRYPT_LVL parameter (0 = disabled, 1 = Lua scripts only, 2 = Lua scripts + logs, 3 = Lua scripts + logs, no script content in logs)
    AP_Int8 _key_status;      // LAS_CRYPT_STAT parameter (read-only, 1 = key stored, 0 = no key)
};

#endif  // AP_CRYPTO_ENABLED
