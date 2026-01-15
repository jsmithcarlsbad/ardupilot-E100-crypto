#pragma once

#include <AP_HAL/AP_HAL_Boards.h>

// AP_Crypto is always enabled by default
// This can be overridden by defining AP_CRYPTO_ENABLED=0 before including this header
#ifndef AP_CRYPTO_ENABLED
#define AP_CRYPTO_ENABLED 1
#endif

// Simple XOR-based encryption
// Note: XOR encryption provides basic obfuscation but is NOT cryptographically secure
// File format: [Header:4 bytes "XOR1"][XOR-encrypted data]
