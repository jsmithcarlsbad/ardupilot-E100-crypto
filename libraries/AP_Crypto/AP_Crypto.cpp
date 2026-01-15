#include "AP_Crypto.h"

#if AP_CRYPTO_ENABLED

#include <AP_HAL/AP_HAL.h>
#include <StorageManager/StorageManager.h>
#include <AP_Filesystem/AP_Filesystem.h>
#include <GCS_MAVLink/GCS.h>
#include <AP_Crypto/AP_Crypto_Params.h>
#include <string.h>
#include <sys/stat.h>

#if defined(HAVE_MONOCYPHER) && defined(AP_CHECK_FIRMWARE_ENABLED)
#include <AP_CheckFirmware/monocypher.h>
#endif

extern const AP_HAL::HAL& hal;

// Key storage using StorageManager
#define CRYPTO_KEY_STORAGE_OFFSET 0  // Offset within StorageKeys area
static StorageAccess _crypto_storage(StorageManager::StorageKeys);

// XOR encode with "XOR1" header
int AP_Crypto::xor_encode_raw(const uint8_t raw_key[32], 
                              const uint8_t *plaintext, size_t plaintext_len,
                              uint8_t *ciphertext, size_t ciphertext_max)
{
    if (raw_key == nullptr || plaintext == nullptr || ciphertext == nullptr) {
        return -1;
    }
    
    size_t total_len = 4 + plaintext_len; // "XOR1" header + data
    if (ciphertext_max < total_len) {
        return -1;
    }
    
    // Write header
    memcpy(ciphertext, "XOR1", 4);
    
    // XOR encrypt data
    for (size_t i = 0; i < plaintext_len; i++) {
        ciphertext[4 + i] = plaintext[i] ^ raw_key[i % 32];
    }
    
    return (int)total_len;
}

// XOR decode (reads "XOR1" header)
int AP_Crypto::xor_decode_raw(const uint8_t raw_key[32], 
                              const uint8_t *ciphertext, size_t ciphertext_len,
                              uint8_t *plaintext, size_t plaintext_max)
{
    if (raw_key == nullptr || ciphertext == nullptr || plaintext == nullptr) {
        return -1;
    }
    
    if (ciphertext_len < 4) {
        return -1; // Too short for header
    }
    
    // Verify header
    if (memcmp(ciphertext, "XOR1", 4) != 0) {
        return -1; // Invalid header
    }
    
    size_t data_len = ciphertext_len - 4;
    if (plaintext_max < data_len) {
        return -1;
    }
    
    // XOR decrypt data
    for (size_t i = 0; i < data_len; i++) {
        plaintext[i] = ciphertext[4 + i] ^ raw_key[i % 32];
    }
    
    return (int)data_len;
}

bool AP_Crypto::generate_key(uint8_t key_out[32])
{
    if (key_out == nullptr) {
        return false;
    }
    
    // Use HAL random number generator
    if (!hal.util->get_random_vals(key_out, 32)) {
        return false;
    }
    
    return true;
}

bool AP_Crypto::streaming_encrypt_init_xor(StreamingEncrypt *ctx, const uint8_t raw_key[32])
{
    if (ctx == nullptr || raw_key == nullptr) {
        return false;
    }
    
    memcpy(ctx->key, raw_key, 32);
    ctx->bytes_encrypted = 0;
    ctx->initialized = true;
    
    return true;
}

bool AP_Crypto::streaming_encrypt_write_header_xor(StreamingEncrypt *ctx, int fd)
{
    if (ctx == nullptr || !ctx->initialized || fd < 0) {
        return false;
    }
    
    const uint8_t header[4] = {'X', 'O', 'R', '1'};
    ssize_t written = AP::FS().write(fd, header, 4);
    
    return (written == 4);
}

ssize_t AP_Crypto::streaming_encrypt_write_xor(StreamingEncrypt *ctx, int fd,
                                               const uint8_t *plaintext, size_t plaintext_len)
{
    if (ctx == nullptr || !ctx->initialized || fd < 0 || plaintext == nullptr) {
        return -1;
    }
    
    // Encrypt in place (we'll use a temporary buffer)
    uint8_t encrypted[256];
    size_t total_written = 0;
    
    for (size_t offset = 0; offset < plaintext_len; offset += 256) {
        size_t chunk_len = (plaintext_len - offset > 256) ? 256 : (plaintext_len - offset);
        
        // XOR encrypt chunk
        for (size_t i = 0; i < chunk_len; i++) {
            size_t key_idx = (ctx->bytes_encrypted + i) % 32;
            encrypted[i] = plaintext[offset + i] ^ ctx->key[key_idx];
        }
        
        // Write encrypted chunk
        ssize_t written = AP::FS().write(fd, encrypted, chunk_len);
        if (written < 0 || (size_t)written != chunk_len) {
            return -1;
        }
        
        ctx->bytes_encrypted += chunk_len;
        total_written += written;
    }
    
    return total_written;
}

bool AP_Crypto::streaming_encrypt_finalize_xor(StreamingEncrypt *ctx, int fd)
{
    if (ctx == nullptr || !ctx->initialized || fd < 0) {
        return false;
    }
    
    // Sync file
    AP::FS().fsync(fd);
    
    return true;
}

void AP_Crypto::streaming_encrypt_cleanup(StreamingEncrypt *ctx)
{
    if (ctx == nullptr) {
        return;
    }
    
    memset(ctx->key, 0, 32);
    ctx->bytes_encrypted = 0;
    ctx->initialized = false;
}

bool AP_Crypto::streaming_decrypt_init_xor(StreamingDecrypt *ctx, const uint8_t raw_key[32], int fd)
{
    if (ctx == nullptr || raw_key == nullptr || fd < 0) {
        return false;
    }
    
    // Read and verify header
    uint8_t header[4];
    ssize_t read_bytes = AP::FS().read(fd, header, 4);
    if (read_bytes != 4) {
        return false;
    }
    
    if (memcmp(header, "XOR1", 4) != 0) {
        return false;
    }
    
    memcpy(ctx->key, raw_key, 32);
    ctx->bytes_decrypted = 0;
    ctx->initialized = true;
    
    return true;
}

ssize_t AP_Crypto::streaming_decrypt_read_xor(StreamingDecrypt *ctx, int fd,
                                             uint8_t *plaintext, size_t plaintext_max)
{
    if (ctx == nullptr || !ctx->initialized || fd < 0 || plaintext == nullptr) {
        return -1;
    }
    
    // Read encrypted chunk
    uint8_t encrypted[256];
    size_t read_len = (plaintext_max > 256) ? 256 : plaintext_max;
    ssize_t read_bytes = AP::FS().read(fd, encrypted, read_len);
    
    if (read_bytes <= 0) {
        return read_bytes;
    }
    
    // XOR decrypt
    for (ssize_t i = 0; i < read_bytes; i++) {
        size_t key_idx = (ctx->bytes_decrypted + i) % 32;
        plaintext[i] = encrypted[i] ^ ctx->key[key_idx];
    }
    
    ctx->bytes_decrypted += read_bytes;
    
    return read_bytes;
}

bool AP_Crypto::streaming_decrypt_finalize_xor(StreamingDecrypt *ctx, int fd)
{
    if (ctx == nullptr || !ctx->initialized || fd < 0) {
        return false;
    }
    
    // Nothing special needed for XOR
    return true;
}

void AP_Crypto::streaming_decrypt_cleanup(StreamingDecrypt *ctx)
{
    if (ctx == nullptr) {
        return;
    }
    
    memset(ctx->key, 0, 32);
    ctx->bytes_decrypted = 0;
    ctx->initialized = false;
}

bool AP_Crypto::streaming_encrypt_init_xor_from_params(StreamingEncrypt *ctx)
{
    if (ctx == nullptr) {
        return false;
    }
    
    uint8_t key[32];
    
    // Try to get key from storage first
    if (retrieve_key(key)) {
        return streaming_encrypt_init_xor(ctx, key);
    }
    
    // If not in storage, try to derive from parameter
    // This allows encryption to work even if key storage failed
    if (AP_Crypto_Params::derive_key_from_param(key)) {
        // Store the key for future use (non-blocking, happens asynchronously)
        store_key(key);
        return streaming_encrypt_init_xor(ctx, key);
    }
    
    // Fallback to board ID if parameter not set
    if (derive_key_from_board_id(key)) {
    return streaming_encrypt_init_xor(ctx, key);
    }
    
    return false;
}

bool AP_Crypto::streaming_decrypt_init_xor_from_params(StreamingDecrypt *ctx, int fd)
{
    if (ctx == nullptr || fd < 0) {
        return false;
    }
    
    uint8_t key[32];
    
    // Try to get key from storage first
    if (retrieve_key(key)) {
        return streaming_decrypt_init_xor(ctx, key, fd);
    }
    
    // If not in storage, try to derive from parameter
    // This allows decryption to work even if key storage failed
    if (AP_Crypto_Params::derive_key_from_param(key)) {
        // Store the key for future use (non-blocking, happens asynchronously)
        store_key(key);
        return streaming_decrypt_init_xor(ctx, key, fd);
    }
    
    // Fallback to board ID if parameter not set
    if (derive_key_from_board_id(key)) {
    return streaming_decrypt_init_xor(ctx, key, fd);
    }
    
    return false;
}

bool AP_Crypto::store_key(const uint8_t key[32])
{
    if (key == nullptr) {
        return false;
    }
    
    // Check if storage has failed
    if (StorageManager::storage_failed()) {
        return false;
    }
    
    // Safety check: ensure storage is initialized
    // StorageAccess constructor is safe, but write_block might fail if storage isn't ready
    // We check storage_failed() above, but also verify the storage area is valid
    if (_crypto_storage.size() == 0) {
        // Storage area not available
        return false;
    }
    
    // Store key at offset 0 in StorageKeys area
    // Note: This is a synchronous write that should be fast, but if it blocks
    // it could cause MAVLink issues. However, StorageManager writes are typically
    // buffered and shouldn't block for long.
    // We use write_block which is safe and non-blocking (buffered write)
    bool result = _crypto_storage.write_block(CRYPTO_KEY_STORAGE_OFFSET, key, 32);
    
    // Don't fail if write returns false - storage might be busy
    // We'll try again next time if needed
    return result;
}

bool AP_Crypto::retrieve_key(uint8_t key[32])
{
    if (key == nullptr) {
        return false;
    }
    
    // Read key from offset 0 in StorageKeys area
    return _crypto_storage.read_block(key, CRYPTO_KEY_STORAGE_OFFSET, 32);
}

bool AP_Crypto::has_stored_key(void)
{
    // Fast-path checks to avoid storage access if not available
    // This prevents blocking during parameter loading and USB CDC initialization
    if (StorageManager::storage_failed()) {
        return false;  // Storage not available - fast return
    }
    
    // Check if storage area is valid (size > 0)
    // This avoids slow storage operations if storage area not initialized
    if (_crypto_storage.size() == 0) {
        return false;  // Storage area not available - fast return
    }
    
    // Only perform actual storage read if storage is available
    uint8_t key[32];
    return retrieve_key(key);
}

bool AP_Crypto::generate_and_store_key(uint8_t key[32])
{
    uint8_t new_key[32];
    
    if (!generate_key(new_key)) {
        return false;
    }
    
    if (!store_key(new_key)) {
        return false;
    }
    
    if (key != nullptr) {
        memcpy(key, new_key, 32);
    }
    
    return true;
}

bool AP_Crypto::derive_key_from_board_id(uint8_t key[32])
{
    if (key == nullptr) {
        return false;
    }
    
    // Simple key derivation from board ID
    // Get board ID (system ID)
    uint8_t board_id[12];
    uint8_t board_id_len = sizeof(board_id);
    if (!hal.util->get_system_id_unformatted(board_id, board_id_len)) {
        // Fallback: use a hardcoded key if board ID not available
        memset(key, 0x42, 32);
        return true;
    }
    
    // Simple hash-based key derivation
    // Use BLAKE2b if available, otherwise simple hash
    #if defined(HAVE_MONOCYPHER) && defined(AP_CHECK_FIRMWARE_ENABLED)
    uint8_t salt[16] = "ArduPilotCrypto";
    crypto_blake2b(key, 32, board_id, board_id_len, salt, sizeof(salt));
    #else
    // Simple hash: XOR board ID bytes and repeat
    for (int i = 0; i < 32; i++) {
        key[i] = board_id[i % board_id_len] ^ (i * 0x37);
    }
    #endif
    
    return true;
}

bool AP_Crypto::read_and_decrypt_file(const char *filename, uint8_t **plaintext, size_t *plaintext_len)
{
    if (filename == nullptr || plaintext == nullptr || plaintext_len == nullptr) {
        return false;
    }
    
    *plaintext = nullptr;
    *plaintext_len = 0;
    
    // Open file
    int fd = AP::FS().open(filename, O_RDONLY);
    if (fd < 0) {
        return false;
    }
    
    // Get file size
    struct stat st;
    if (AP::FS().stat(filename, &st) != 0) {
        AP::FS().close(fd);
        return false;
    }
    
    size_t file_size = st.st_size;
    if (file_size < 4) {
        AP::FS().close(fd);
        return false;  // Too small to be encrypted
    }
    
    // Read entire file
    uint8_t *encrypted_data = (uint8_t*)hal.util->malloc_type(file_size, AP_HAL::Util::MEM_DMA_SAFE);
    if (encrypted_data == nullptr) {
        AP::FS().close(fd);
        return false;
    }
    
    ssize_t read_bytes = AP::FS().read(fd, encrypted_data, file_size);
    AP::FS().close(fd);
    
    if (read_bytes != (ssize_t)file_size) {
        hal.util->free_type(encrypted_data, file_size, AP_HAL::Util::MEM_DMA_SAFE);
        return false;
    }
    
    // Check for "XOR1" header
    if (file_size < 4 || memcmp(encrypted_data, "XOR1", 4) != 0) {
        hal.util->free_type(encrypted_data, file_size, AP_HAL::Util::MEM_DMA_SAFE);
        return false;  // Not an encrypted file
    }
    
    // Get decryption key
    uint8_t key[32];
    if (!retrieve_key(key)) {
        if (!derive_key_from_board_id(key)) {
            hal.util->free_type(encrypted_data, file_size, AP_HAL::Util::MEM_DMA_SAFE);
            return false;
        }
    }
    
    // Decrypt
    size_t plaintext_size = file_size - 4;
    uint8_t *decrypted = (uint8_t*)hal.util->malloc_type(plaintext_size, AP_HAL::Util::MEM_DMA_SAFE);
    if (decrypted == nullptr) {
        hal.util->free_type(encrypted_data, file_size, AP_HAL::Util::MEM_DMA_SAFE);
        return false;
    }
    
    int decrypted_len = xor_decode_raw(key, encrypted_data, file_size, decrypted, plaintext_size);
    hal.util->free_type(encrypted_data, file_size, AP_HAL::Util::MEM_DMA_SAFE);
    
    if (decrypted_len < 0) {
        hal.util->free_type(decrypted, plaintext_size, AP_HAL::Util::MEM_DMA_SAFE);
        return false;
    }
    
    *plaintext = decrypted;
    *plaintext_len = decrypted_len;
    return true;
}

bool AP_Crypto::read_decrypt_and_display_file(const char *filename, size_t max_display_len)
{
    if (filename == nullptr) {
        return false;
    }
    
    uint8_t *plaintext = nullptr;
    size_t plaintext_len = 0;
    
    if (!read_and_decrypt_file(filename, &plaintext, &plaintext_len)) {
        gcs().send_text(MAV_SEVERITY_ERROR, "Failed to decrypt file: %s", filename);
        return false;
    }
    
    if (plaintext == nullptr || plaintext_len == 0) {
        gcs().send_text(MAV_SEVERITY_WARNING, "Decrypted file is empty: %s", filename);
        if (plaintext != nullptr) {
            hal.util->free_type(plaintext, plaintext_len, AP_HAL::Util::MEM_DMA_SAFE);
        }
        return false;
    }
    
    // Limit display length to prevent message overflow
    // MAVLink STATUSTEXT messages are limited to 50 characters per message
    const size_t MAX_CHUNK_SIZE = 40;  // Leave room for message prefix
    size_t display_len = plaintext_len;
    if (max_display_len > 0 && display_len > max_display_len) {
        display_len = max_display_len;
    }
    
    // Send file info
    gcs().send_text(MAV_SEVERITY_INFO, "Decrypted file: %s (%u bytes)", filename, (unsigned)plaintext_len);
    
    // Display in chunks (MAVLink messages are limited to 50 chars)
    size_t offset = 0;
    uint16_t chunk_num = 0;
    while (offset < display_len) {
        size_t chunk_size = display_len - offset;
        if (chunk_size > MAX_CHUNK_SIZE) {
            chunk_size = MAX_CHUNK_SIZE;
        }
        
        // Create null-terminated string for display
        char chunk[MAX_CHUNK_SIZE + 1];
        memcpy(chunk, plaintext + offset, chunk_size);
        chunk[chunk_size] = '\0';
        
        // Replace non-printable characters with '.'
        for (size_t i = 0; i < chunk_size; i++) {
            if (chunk[i] < 32 || chunk[i] > 126) {
                if (chunk[i] == '\n') {
                    chunk[i] = '\\';
                    // Insert 'n' after backslash if there's room
                    if (i + 1 < chunk_size) {
                        memmove(chunk + i + 2, chunk + i + 1, chunk_size - i - 1);
                        chunk[i + 1] = 'n';
                        chunk_size++;
                        if (chunk_size > MAX_CHUNK_SIZE) {
                            chunk_size = MAX_CHUNK_SIZE;
                        }
                    }
                } else if (chunk[i] == '\r') {
                    chunk[i] = '.';
                } else if (chunk[i] == '\t') {
                    chunk[i] = ' ';
                } else {
                    chunk[i] = '.';
                }
            }
        }
        chunk[chunk_size] = '\0';
        
        // Send chunk
        if (display_len > MAX_CHUNK_SIZE) {
            gcs().send_text(MAV_SEVERITY_INFO, "[%u] %s", chunk_num, chunk);
        } else {
            gcs().send_text(MAV_SEVERITY_INFO, "%s", chunk);
        }
        
        offset += chunk_size;
        chunk_num++;
        
        // Limit total messages to prevent flooding
        if (chunk_num >= 100) {
            gcs().send_text(MAV_SEVERITY_WARNING, "... (truncated, %u bytes shown)", (unsigned)offset);
            break;
        }
    }
    
    if (display_len < plaintext_len) {
        gcs().send_text(MAV_SEVERITY_INFO, "... (showing %u of %u bytes)", (unsigned)display_len, (unsigned)plaintext_len);
    }
    
    // Free decrypted data
    hal.util->free_type(plaintext, plaintext_len, AP_HAL::Util::MEM_DMA_SAFE);
    
    return true;
}

#endif  // AP_CRYPTO_ENABLED
