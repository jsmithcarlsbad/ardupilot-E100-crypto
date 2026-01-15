# AP_Crypto Usage Guide

## Overview

AP_Crypto provides XOR-based encryption functionality for ArduPilot, enabling encryption of Lua scripts and log files. This feature helps protect sensitive data stored on the flight controller.

**Important Security Note**: XOR encryption provides basic obfuscation but is **NOT cryptographically secure**. It should be used for basic protection against casual inspection, not for protecting highly sensitive data.

## Features

- **Lua Script Encryption**: Encrypt Lua scripts stored on the flight controller
- **Log File Encryption**: Encrypt log files written by Lua scripts
- **Parameter-based Key Management**: Set encryption keys via MAVLink parameters
- **Streaming Encryption**: Support for encrypting/decrypting large files efficiently

## File Format

Encrypted files use the following format:
- **Header**: 4 bytes containing "XOR1" magic string
- **Data**: XOR-encrypted content following the header

## Parameters

### LEIGH_CRYPT_KEY

- **Type**: INT32 (write-only)
- **Range**: -2147483648 to 2147483647
- **Description**: Encryption key for AP_Crypto. This parameter is write-only for security - it always reads as 0.
- **Usage**: Set this parameter to a non-zero integer value to configure the encryption key. The system will derive a 32-byte key from this value.

**Setting the Key**:
1. Connect via MAVLink (Mission Planner, QGroundControl, etc.)
2. Set the `LEIGH_CRYPT_KEY` parameter to your desired integer value (e.g., 12345)
3. The key is stored securely and cannot be read back (reads return 0)

**Important**: 
- The key is derived from the integer value you provide
- Use a unique, non-zero value for your key
- Remember the value you set - you'll need it to decrypt files later
- Zero values are ignored (cannot be used as a key)

### LEIGH_CRYPT_LVL

- **Type**: INT8
- **Values**: 
  - `0`: Disabled (no encryption)
  - `1`: Lua script encryption enabled (scripts are encrypted/decrypted)
  - `2`: Lua script encryption + log file encryption (both scripts and logs are encrypted)
  - Any non-zero value: Enables Lua script encryption
- **Default**: 0 (disabled)
- **Description**: Controls the encryption level:
  - **Level 0**: No encryption - all files are stored and read as plaintext
  - **Any non-zero value**: Lua scripts are encrypted/decrypted automatically
  - **Level 1**: Lua scripts are encrypted/decrypted automatically, but log files remain unencrypted
  - **Level 2**: Both Lua scripts and log files are encrypted - logs must be decrypted to be human-readable

## Setup Instructions

### Step 1: Enable Encryption

1. Connect to your flight controller via MAVLink
2. Set `LEIGH_CRYPT_LVL` to:
   - `1` for Lua script encryption only
   - `2` for both Lua script and log file encryption
3. Reboot the flight controller (or wait for parameter to take effect)

### Step 2: Set Encryption Key

1. Set `LEIGH_CRYPT_KEY` to a non-zero integer value (e.g., 12345)
2. The key will be stored securely on the flight controller
3. The parameter will read back as 0 (this is normal - it's write-only for security)

### Step 3: Use Encrypted Files

Once encryption is enabled:
- **Lua Scripts (Level 1 or 2)**: Encrypted Lua scripts (with "XOR1" header) will be automatically decrypted when loaded
- **Log Files (Level 2 only)**: Log files written by Lua scripts will be encrypted and must be decrypted to be human-readable

## Usage Examples

### Encrypting Lua Scripts

To encrypt a Lua script before uploading:

1. **Set the encryption key first**: Set `LEIGH_CRYPT_KEY` to your desired integer value on the flight controller
2. Use a tool that supports AP_Crypto encryption (or implement using the AP_Crypto API)
3. **Encrypt the script with the same key value** you set in `LEIGH_CRYPT_KEY` - this is critical!
4. Upload the encrypted script (`.enc` file) to the flight controller
5. The script will be automatically decrypted when loaded (if encryption is enabled)

**Important**: For an `.enc` file to run, it **must** have been encrypted with a matching `LEIGH_CRYPT_KEY` value. If the key doesn't match, the file will fail to decrypt and won't run.

### Writing Encrypted Logs from Lua

Lua scripts can write encrypted logs using the encrypted log writer. The system automatically handles encryption when `LEIGH_CRYPT_LVL` is set to 2. At level 1, log files remain unencrypted.

### Decrypting Files Offline

To decrypt files on your computer:

1. Extract the encrypted file from the flight controller
2. Use a decryption tool that supports AP_Crypto format
3. Provide the same integer key value you used for `LEIGH_CRYPT_KEY`
4. The tool will decrypt the file using XOR decryption

## Key Derivation

The encryption key is derived from the `LEIGH_CRYPT_KEY` parameter value as follows:

1. The INT32 value is converted to a 32-byte key
2. The key derivation uses a simple algorithm: repeats the 4 bytes of the integer value with XOR operations
3. The derived 32-byte key is stored securely on the flight controller

## Security Considerations

1. **XOR Encryption Limitation**: XOR encryption is NOT cryptographically secure. It provides basic obfuscation only.

2. **Key Security**: 
   - The key parameter is write-only (reads return 0) for security
   - Store your key value securely - you'll need it to decrypt files
   - Use a unique key value for each flight controller

3. **File Format**: 
   - Encrypted files have a "XOR1" header for identification
   - The header is not encrypted, only the data following it

4. **Storage**: 
   - Keys are stored in persistent storage on the flight controller
   - Keys survive reboots and firmware updates (if storage is preserved)

## Troubleshooting

### Encryption Not Working

1. Verify `LEIGH_CRYPT_LVL` is set to `1` or `2` (not 0)
2. Ensure `LEIGH_CRYPT_KEY` is set to a non-zero value
3. Check that the file has the "XOR1" header (first 4 bytes)
4. Verify you're using the same key value for encryption and decryption
5. For Lua scripts: Level 1 or 2 enables script encryption
6. For log files: Level 2 is required for log encryption

### Cannot Read Key Parameter

- This is normal! `LEIGH_CRYPT_KEY` is write-only and always reads as 0 for security
- Keep a record of the value you set externally

### Files Not Decrypting

1. **Verify the key value matches**: The `LEIGH_CRYPT_KEY` value on the flight controller must exactly match the value used to encrypt the file
2. **Ensure key is set**: `LEIGH_CRYPT_KEY` must be set to a non-zero value before the file can be decrypted
3. Check that the file has the correct "XOR1" header
4. Ensure encryption is enabled:
   - For Lua scripts: `LEIGH_CRYPT_LVL >= 1` (any non-zero value)
   - For log files: `LEIGH_CRYPT_LVL >= 2`
5. **Key matching is required**: If the file was encrypted with a different key value, it will not decrypt and will not run

## API Reference

For developers integrating AP_Crypto into custom code:

- **AP_Crypto::xor_encode_raw()**: Encrypt data with a raw key
- **AP_Crypto::xor_decode_raw()**: Decrypt data with a raw key
- **AP_Crypto::streaming_encrypt_init_xor()**: Initialize streaming encryption
- **AP_Crypto::streaming_decrypt_init_xor()**: Initialize streaming decryption
- **AP_Crypto::store_key()**: Store a key in persistent storage
- **AP_Crypto::retrieve_key()**: Retrieve stored key
- **AP_Crypto_Params::is_encryption_enabled()**: Check if Lua script encryption is enabled (level >= 1)
- **AP_Crypto_Params::is_log_encryption_enabled()**: Check if log file encryption is enabled (level >= 2)

See `AP_Crypto.h` for complete API documentation.

## Related Files

- `AP_Crypto.h` - Main encryption API
- `AP_Crypto.cpp` - Encryption implementation
- `AP_Crypto_Params.h` - Parameter management
- `AP_Crypto_Params.cpp` - Parameter implementation
- `lua_encrypted_reader.h` - Lua script decryption
- `lua_encrypted_log_writer.h` - Encrypted log writing

## Notes

- Encryption is disabled by default (`LEIGH_CRYPT_LVL = 0`)
- The system gracefully handles missing keys or disabled encryption
- Files without the "XOR1" header are treated as unencrypted
- Encryption/decryption happens automatically when enabled
- **Level 1**: Only Lua scripts are encrypted/decrypted
- **Level 2**: Both Lua scripts and log files are encrypted
- Log files at level 2 must be decrypted before they can be read by standard log analysis tools
- **Key Matching Requirement**: For `.enc` files to run, they **must** have been encrypted with the same `LEIGH_CRYPT_KEY` value that is currently set on the flight controller. If the keys don't match, the file will fail to decrypt and won't execute.

