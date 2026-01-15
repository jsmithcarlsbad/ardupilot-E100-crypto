#!/usr/bin/env python3
"""
Simple XOR encryption/decryption tool for AP_Crypto
Format: [Header:4 bytes "XOR1"][XOR-encrypted data]

Key can be provided as:
  - INT32 value (e.g., 12345) - will be derived to 32-byte key using same algorithm as ArduPilot
  - 32-byte hex string (64 hex characters)
  - Path to file containing 32-byte key
"""

import sys
import argparse
import struct

def derive_key_from_int32(key_value):
    """
    Derive 32-byte key from INT32 value (matches ArduPilot's handle_key_set algorithm)
    
    Args:
        key_value: Integer value (will be converted to uint32_t)
    
    Returns:
        32-byte key as bytes
    """
    # Convert to uint32_t (handle negative values)
    uval = key_value & 0xFFFFFFFF
    
    # Pack as 4 bytes (little-endian, matching C++ behavior)
    uval_bytes = struct.pack('<I', uval)
    
    # Derive 32-byte key using same algorithm as AP_Crypto_Params::handle_key_set
    key = bytearray(32)
    for i in range(32):
        key[i] = uval_bytes[i % 4] ^ (i * 0x73)
    
    return bytes(key)

def xor_encrypt(plaintext, key):
    """XOR encrypt data with key"""
    key_len = len(key)
    ciphertext = bytearray()
    ciphertext.extend(b"XOR1")  # Header
    for i, byte in enumerate(plaintext):
        ciphertext.append(byte ^ key[i % key_len])
    return bytes(ciphertext)

def xor_decrypt(ciphertext, key):
    """XOR decrypt data with key"""
    if len(ciphertext) < 4:
        raise ValueError("File too short")
    if ciphertext[:4] != b"XOR1":
        raise ValueError("Invalid header - not XOR1 format")
    key_len = len(key)
    plaintext = bytearray()
    for i, byte in enumerate(ciphertext[4:]):
        plaintext.append(byte ^ key[i % key_len])
    return bytes(plaintext)

def main():
    parser = argparse.ArgumentParser(description='XOR encrypt/decrypt files for AP_Crypto')
    parser.add_argument('mode', choices=['encrypt', 'decrypt'], help='Operation mode')
    parser.add_argument('input', help='Input file')
    parser.add_argument('output', help='Output file')
    parser.add_argument('--key', required=True, help='Key: INT32 value, 32-byte hex string (64 chars), or path to 32-byte key file')
    args = parser.parse_args()
    
    # Determine key type and read/derive key
    key = None
    
    # Try as INT32 first (if it's a number)
    try:
        key_value = int(args.key)
        key = derive_key_from_int32(key_value)
        print(f"Using INT32 key derivation: {key_value} -> {len(key)} bytes", file=sys.stderr)
    except ValueError:
        # Not an integer, try other formats
        pass
    
    # If not INT32, try as hex string (64 hex characters = 32 bytes)
    if key is None and len(args.key) == 64:
        try:
            key = bytes.fromhex(args.key)
            print(f"Using hex key: {len(key)} bytes", file=sys.stderr)
        except ValueError:
            pass
    
    # If still not found, try as file path
    if key is None:
        try:
            with open(args.key, 'rb') as f:
                key = f.read(32)
            print(f"Read key from file: {len(key)} bytes", file=sys.stderr)
        except (IOError, OSError):
            pass
    
    if key is None or len(key) != 32:
        print("Error: Key must be:", file=sys.stderr)
        print("  - An INT32 value (e.g., 12345)", file=sys.stderr)
        print("  - A 32-byte hex string (64 hex characters)", file=sys.stderr)
        print("  - Path to a file containing exactly 32 bytes", file=sys.stderr)
        sys.exit(1)
    
    # Read input
    with open(args.input, 'rb') as f:
        data = f.read()
    
    # Process
    if args.mode == 'encrypt':
        result = xor_encrypt(data, key)
    else:
        result = xor_decrypt(data, key)
    
    # Write output
    with open(args.output, 'wb') as f:
        f.write(result)
    
    print(f"Success: {args.mode}ed {len(data)} bytes -> {len(result)} bytes")

if __name__ == '__main__':
    main()
