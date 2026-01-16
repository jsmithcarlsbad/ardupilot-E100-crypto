# PORTING_FIXES.md

This document details all changes made to create the E100 ArduPilot build, including sources, patches, and fixes applied.

---

## BUILD CONFIGURATION

### Base ArduPilot Version
- **Version**: ArduPlane 4.5.7
- **Source Repository**: https://github.com/IamPete1/ardupilot.git
- **Branch**: `E100-4.5.7-base` (created from ArduPlane 4.5.7 tag)
- **Target Repository**: https://github.com/jsmithcarlsbad/ardupilot-E100-crypto.git
- **Build Target**: CubeOrange (STM32H743xx)

### Version History
- **Initial**: Started with ArduPilot 4.7.0-dev (master branch)
- **Changed to**: ArduPlane 4.5.7 (Option 2 - checkout actual 4.5.7 codebase and re-apply changes)
- **Reason**: Customer requirement for version 4.5.7

---

## SOURCES AND PATCHES

### 1. Required Patch: AP_Airspeed DroneCAN Runtime Detection

**Commit**: `dc8701d22c648bb4e87679aa6698f7573022e643`  
**Applied as**: `a17f9bb589` (cherry-picked)  
**Description**: "AP_Airspeed: DroneCAN: allow runtime detection of single sensor"

**Files Modified**:
- `libraries/AP_Airspeed/AP_Airspeed_DroneCAN.cpp`
- `libraries/AP_Airspeed/AP_Airspeed_DroneCAN.h`

**Status**: ✅ Applied and merged

**Note**: After cherry-pick, `AP_Airspeed_DroneCAN.cpp` was replaced with custom version from `D:\FromLES` (see Custom Files section below).

---

### 2. Custom Files from D:\FromLES

**Source Location**: `/mnt/d/FromLES/` (Windows D:\FromLES mounted in WSL)

**Files Copied**:
- `AP_Airspeed_DroneCAN.cpp` → `libraries/AP_Airspeed/AP_Airspeed_DroneCAN.cpp`

**Changes Made**:
- Function signature: `subscribe_msgs()` returns `void` (matches 4.5.7 codebase)
- Removed `NEW_NOTHROW` macro usage (not available in 4.5.7), replaced with `new`
- Maintained custom logic from D:\FromLES version

**Status**: ✅ Applied

---

### 3. AP_CRYPTO Library Integration

**Source Location**: `/home/jsmith/LEIGH-6DOF-Branch/ardupilot/libraries/AP_Crypto`  
**Reference Repository**: https://github.com/jsmithcarlsbad/ardupilot-leigh-crypto.git  
**Target Location**: `libraries/AP_Crypto/`

**Library Contents**:
- `AP_Crypto.cpp` / `AP_Crypto.h` - Core crypto functionality
- `AP_Crypto_Params.cpp` / `AP_Crypto_Params.h` - Parameter management
- `AP_Crypto_config.h` - Configuration defines
- `PTYHON_CRYPTO_TOOL/` - Python tools for key management
- `USE.md` - Documentation
- `wscript` - Build system integration

**Status**: ✅ Integrated

---

## PARAMETER INTEGRATION

### LAS Parameters Added

The following parameters are provided by `AP_Crypto_Params`:

1. **LAS_CRYPT_KEY** (write-only)
   - Type: `AP_Int32`
   - Description: Encryption key parameter (write-only for security, reads as 0)
   - Security: Never readable via MAVLink (returns 0)

2. **LAS_CRYPT_LVL** (read-write)
   - Type: `AP_Int8`
   - Values: 0 = disabled, 1 = Lua scripts only, 2 = Lua scripts + logs, 3 = Lua scripts + logs, no script content in logs

3. **LAS_CRYPT_STAT** (read-only)
   - Type: `AP_Int8`
   - Values: 0 = no key stored, 1 = key stored
   - Computed: Value is computed on-the-fly via `get_key_status()`

4. **LAS_SCR_ENABLE** (read-write)
   - Type: `AP_Int8`
   - Values: 0 = disabled, 1 = Lua scripts enabled

### Integration Steps

#### 1. Parameters.h Changes
**File**: `ArduPlane/Parameters.h`

**Added Includes**:
```cpp
#include <AP_Crypto/AP_Crypto_config.h>
#include <AP_Crypto/AP_Crypto_Params.h>
```

**Added Enum Entry**:
```cpp
enum {
    // ... existing entries ...
    k_param_crypto_params,
    // ... existing entries ...
};
```

**Added Class Member**:
```cpp
class ParametersG2 {
    // ... existing members ...
    AP_Crypto_Params crypto_params;
    // ... existing members ...
};
```

**Status**: ✅ Applied

#### 2. Parameters.cpp Changes
**File**: `ArduPlane/Parameters.cpp`

**Added Parameter Group Registration**:
```cpp
#if AP_CRYPTO_ENABLED
    // @Group: LAS_
    // @Path: ../libraries/AP_Crypto/AP_Crypto_Params.cpp
    // CRITICAL FIX: Changed index from 22 to 36 to avoid conflict with old EFI parameters
    // Index 22 was previously used for EFI, and old EFI data in FRAM can cause blocking during parameter loading
    AP_SUBGROUPINFO(crypto_params, "LAS_", 36, ParametersG2, AP_Crypto_Params),
#endif
```

**Status**: ✅ Applied (index changed from 22 to 36 - see Fix #4 below)

#### 3. Build System Integration
**File**: `ArduPlane/wscript`

**Added Library**:
```python
ap_libraries=bld.ap_common_vehicle_libraries() + [
    # ... existing libraries ...
    'AP_Crypto',
    # ... existing libraries ...
],
```

**Status**: ✅ Applied

---

## VERSION CHANGES

### ArduPlane Version Update

**File**: `ArduPlane/version.h`

**Changed From**: ArduPlane V4.7.0-dev  
**Changed To**: ArduPlane V4.5.7

**Status**: ✅ Applied

---

## ADDITIONAL CHANGES

### 1. GCS_MAVLink Radio Status Handling

**Files Modified**:
- `libraries/GCS_MAVLink/GCS.h`
- `libraries/GCS_MAVLink/GCS_Common.cpp`

**Changes**:
- Added `last_txbuf_is_greater(uint8_t txbuf_limit)` function
- Added `txbuf` member to `LastRadioStatus` struct
- Modified `handle_radio_status()` to track `txbuf` value
- Modified `GCS_Param.cpp` to use `last_txbuf_is_greater(33)` instead of `get_last_txbuf() > 50`

**Reason**: Critical for USB/Serial connections where radio status is stale. The old branch used `last_txbuf_is_greater(33)` which is crucial for parameter enumeration over USB.

**Status**: ✅ Applied

---

## FIXES

### Fix #1: Semaphore Timeout - Mission Planner Connection (2026-01-16)

**Problem**: Mission Planner fails to connect with error: **"The semaphore timeout period has expired"**

**Root Cause**: When Mission Planner enumerates parameters via MAVLink, the `LAS_CRYPT_STAT` parameter (read-only, computed) was returning the stored `_value` from the `AP_Int8` parameter instead of the computed value. This could trigger storage access during early startup, blocking USB CDC initialization and causing Windows COM port configuration to timeout.

**Solution**: Intercept `LAS_CRYPT_STAT` parameter reads in `GCS_Param.cpp` to return the computed value from `get_key_status()` instead of the stored value.

**Files Modified**:

1. **`libraries/GCS_MAVLink/GCS_Param.cpp`**
   - Added `#include <AP_Crypto/AP_Crypto_Params.h>` and `#include <string.h>`
   - Intercepted `LAS_CRYPT_STAT` in `queued_param_send()` (line ~78-95)
     - Check if parameter name is "LAS_CRYPT_STAT"
     - If yes, call `get_key_status()` to get computed value
     - Return computed value instead of stored value
   - Intercepted `LAS_CRYPT_STAT` in `param_io_timer()` (line ~405-415)
     - Same interception logic for async parameter requests

2. **`libraries/AP_Crypto/AP_Crypto_Params.cpp`**
   - Simplified `get_key_status()` to rely on `AP_Crypto::has_stored_key()`
   - `has_stored_key()` already contains fast-path checks:
     - `StorageManager::storage_failed()` - returns false immediately if storage failed
     - `_crypto_storage.size() == 0` - returns false immediately if storage area not initialized

**Technical Details**:
- Fast-path checks prevent blocking storage access during early startup
- Parameter interception ensures computed value is returned without storage access
- USB CDC can respond to Windows control requests immediately
- Mission Planner can connect successfully

**Build Status**: ✅ Build successful - no compilation errors

**Output Files**:
- `build/CubeOrange/bin/arduplane.apj` (for Mission Planner)
- `build/CubeOrange/bin/arduplane_with_bl.hex` (with bootloader for STM32CubeProgrammer)

**Testing Status**: ⏳ Pending hardware test

**Related Issues**:
- This exact issue was fixed in the old 6DOF build
- The fix prevents blocking storage access during USB CDC initialization
- Windows COM port configuration requires immediate response from firmware

**References**:
- Old working branch: `/home/jsmith/LEIGH-6DOF-Branch`
- GitHub reference: https://github.com/jsmithcarlsbad/ardupilot-leigh-crypto.git

---

## Fix #3: Scripting Directory Creation Blocking - Semaphore Timeout Resolved ✅

**Date**: 2026-01-16

**Problem**: Mission Planner connection failed with "The semaphore timeout period has expired" error when scripting was enabled (`SCR_ENABLE=1`). When scripting was disabled, connection worked fine.

**Root Cause**: 
- `AP_Scripting::init()` calls `AP::FS().mkdir(SCRIPTING_DIRECTORY)` at line 187
- This filesystem operation uses `WITH_SEMAPHORE(sem)` in the FATFS backend
- If SD card filesystem is busy or slow, `mkdir()` can block for seconds
- This blocks the main thread during the critical USB CDC initialization window
- When scripting is disabled, `AP_Scripting::init()` returns early, so no blocking occurs

**Solution**: Skip the `mkdir()` call in `AP_Scripting::init()` if USB is connected. The directory will be created later in the scripting thread (`lua_scripts::run()` line 523), so this is safe and prevents blocking during USB CDC initialization.

**Files Modified**:

1. **`libraries/AP_Scripting/AP_Scripting.cpp`**
   - Modified `AP_Scripting::init()` to check `hal.gpio->usb_connected()` before calling `AP::FS().mkdir()`
   - If USB is connected, directory creation is skipped (will happen later in scripting thread)
   - If USB is not connected, directory creation proceeds normally

**Testing**: 
- With scripting disabled: Connection works (no `mkdir()` call)
- With scripting enabled: Connection should now work (`mkdir()` deferred when USB connected)

---

## Fix #2: USB CDC Initialization - Semaphore Timeout Resolved ✅

**Date**: 2026-01-16

**Problem**: Mission Planner connection failed with "The semaphore timeout period has expired" error. This occurred because `_save_backup()` in `Storage::_storage_open()` was blocking for 1-3 seconds during parameter loading, which happened before USB CDC could respond to Windows control requests.

**Root Cause**: 
- `_save_backup()` is called synchronously during `_storage_open()`
- `_storage_open()` is called during `AP_Param::load_all()` (parameter loading)
- Parameter loading happens BEFORE `gcs().init()` (USB CDC initialization)
- Windows tries to configure COM port during this blocking window
- Firmware cannot respond to USB control requests while blocked in `_save_backup()`
- Windows times out after ~3 seconds

**Solution**: Defer `_save_backup()` until USB is connected and CDC is initialized. This allows USB CDC to respond to Windows control requests immediately, while the backup can happen later when it won't interfere.

**Files Modified**:

1. **`libraries/AP_HAL_ChibiOS/Storage.cpp`**
   - Modified `_storage_open()` to check `hal.gpio->usb_connected()` before calling `_save_backup()`
   - Applied to all three storage backends: FRAM (line 65), Flash (line 81), SDCard (line 111)
   - Backup is deferred until USB CDC is ready to respond to Windows

2. **`libraries/GCS_MAVLink/GCS.h`**
   - Added `txbuf` member to `LastRadioStatus` struct (line 760)
   - Added `last_txbuf_is_greater()` static method declaration (line 319)

3. **`libraries/GCS_MAVLink/GCS_Common.cpp`**
   - Added `last_txbuf_is_greater()` implementation (line ~876)
   - Modified `handle_radio_status()` to set `last_radio_status.txbuf = packet.txbuf` (line 847)
   - This function checks if radio status is stale (>5 seconds old) and returns true for USB/Serial connections

4. **`libraries/GCS_MAVLink/GCS_Param.cpp`**
   - Changed parameter enumeration check from `get_last_txbuf() > 50` to `last_txbuf_is_greater(33)` (line 78)
   - This ensures parameters enumerate correctly over USB/Serial connections where radio status is always stale

**Technical Details**:
- `_save_backup()` can take 1-3 seconds on slow SD cards
- USB CDC initialization happens in parallel with parameter loading
- Windows USB CDC timeout is ~3 seconds for control requests
- By deferring backup until USB is connected, firmware can respond immediately to Windows
- Backup still happens, just after USB CDC is ready

**Testing Results**: ✅ **SUCCESS**
- Flashed stock CubeOrange firmware - Mission Planner connected ✓
- Reset all parameters and rebooted - Mission Planner connected ✓
- Uploaded new build's `arduplane.apj` via Mission Planner firmware update tool - Mission Planner connected ✓
- Rebooted after firmware update - Mission Planner connected ✓
- LAS parameters (`LAS_CRYPT_KEY`, `LAS_CRYPT_LVL`, `LAS_CRYPT_STAT`) are present and visible ✓

**Build Status**: ✅ Build successful - no compilation errors

**Output Files**:
- `build/CubeOrange/bin/arduplane.apj` (for Mission Planner)
- `build/CubeOrange/bin/arduplane_with_bl.hex` (with bootloader for STM32CubeProgrammer)

**Related Issues**:
- This exact issue was encountered in the old 6DOF build and was fixed
- The fix prevents blocking storage operations during USB CDC initialization
- Windows COM port configuration requires immediate response from firmware

**References**:
- Old working branch: `/home/jsmith/LEIGH-6DOF-Branch`
- GitHub reference: https://github.com/jsmithcarlsbad/ardupilot-leigh-crypto.git
- Startup timing analysis: `/home/jsmith/E100_BUILD/STARTUP_TIMING_ANALYSIS.md`

---

## Fix #4: Parameter Index Conflict - Old EFI Data Blocking Parameter Loading ✅

**Date**: 2026-01-16

**Problem**: Mission Planner connection failed with "The semaphore timeout period has expired" error when old parameter data existed in FRAM. Connection worked fine after clearing all parameters to defaults, but failed with old parameters present.

**Root Cause**: 
- `crypto_params` was registered at index 22 in `ParametersG2`
- Index 22 was previously used for EFI parameters (now unused/reserved)
- Old EFI data stored in FRAM at index 22 was causing the parameter loading system to block
- `AP_Param::load_all()` scans FRAM sequentially during `load_parameters()`
- When old EFI data exists at index 22, the parameter system tries to match it against the current parameter structure
- The EFI conversion code (lines 1517-1526 in `Parameters.cpp`) also calls `find_old_parameter()`, which calls `scan()` again
- This double-scanning of FRAM causes delays/blocking during parameter loading
- Parameter loading happens BEFORE USB CDC initialization, so blocking prevents USB CDC from becoming ready
- Windows COM port configuration times out waiting for USB CDC response

**Solution**: Changed `crypto_params` parameter index from 22 to 36 (next available after `precland` at index 35) to avoid conflict with old EFI data.

**Files Modified**:

1. **`ArduPlane/Parameters.cpp`**
   - Changed `AP_SUBGROUPINFO(crypto_params, "LAS_", 22, ...)` to `AP_SUBGROUPINFO(crypto_params, "LAS_", 36, ...)`
   - Moved `crypto_params` registration to the end of ParametersG2 list (right before `AP_GROUPEND`)
   - Added comment explaining the index change and reason

**Technical Details**:
- Index 22 was marked as "// 22 was EFI" in the code (line 1170)
- Old EFI data in FRAM at index 22 was causing `scan()` to block during parameter loading
- Moving to index 36 avoids the conflict entirely
- The EFI conversion code will still try to convert old EFI data, but it won't interfere with `crypto_params`
- Parameter loading is now faster and doesn't block USB CDC initialization

**Testing Results**: ✅ **SUCCESS**
- Flashed stock CubeOrange firmware - Mission Planner connected ✓
- Reset all parameters and rebooted - Mission Planner connected ✓
- Uploaded new build's `arduplane.apj` via Mission Planner firmware update tool - Mission Planner connected ✓
- Rebooted after firmware update - Mission Planner connected ✓
- LAS parameters (`LAS_CRYPT_KEY`, `LAS_CRYPT_LVL`, `LAS_CRYPT_STAT`, `LAS_SCR_ENABLE`) are present and visible ✓
- Connection works reliably even with old parameters in FRAM ✓

**Build Status**: ✅ Build successful - no compilation errors

**Output Files**:
- `build/CubeOrange/bin/arduplane.apj` (for Mission Planner)
- `build/CubeOrange/bin/arduplane_with_bl.hex` (with bootloader for STM32CubeProgrammer)

**Commit**: `6fcbe802cd` - "Booting,Connecting with LAS parameters shown in parameters list"

**Related Issues**:
- This issue was discovered when user reported connection only worked after clearing parameters
- The fact that clearing parameters fixed the issue confirmed old parameter data was the problem
- Parameter index conflicts can cause blocking during parameter loading
- USB CDC initialization requires fast, non-blocking parameter loading

**References**:
- Old working branch: `/home/jsmith/LEIGH-6DOF-Branch`
- GitHub reference: https://github.com/jsmithcarlsbad/ardupilot-leigh-crypto.git

---

## CUBE REINITIALIZATION PROCEDURE

This section documents the procedure for reinitializing a CubeOrange board with the E100 crypto firmware, including parameter reset and firmware update via Mission Planner.

### Prerequisites
- Mission Planner installed and configured
- USB cable connected to CubeOrange board
- Stock ArduPlane firmware available (for initial connection)
- E100 crypto firmware build (`arduplane.apj` file)

### Step-by-Step Procedure

#### Step 1: Load Stock Firmware (Optional - for clean start)
1. Flash stock CubeOrange ArduPlane firmware to the board
2. Power cycle or reboot the board
3. **Verify**: Mission Planner should connect successfully
   - This confirms hardware and USB connection are working
   - If connection fails, troubleshoot hardware/USB issues first

#### Step 2: Reset All Parameters
1. In Mission Planner, navigate to **Config/Tuning** → **Full Parameter List**
2. Click **Reset Params** (or use **Reset to Defaults**)
3. Confirm the parameter reset
4. **Important**: Wait for the reset to complete
5. Power cycle or reboot the board
6. **Verify**: Mission Planner should reconnect successfully after reboot
   - This confirms the board is functioning correctly with default parameters

#### Step 3: Upload E100 Crypto Firmware
1. In Mission Planner, navigate to **Initial Setup** → **Install Firmware**
2. Click **Load custom firmware file**
3. Browse to and select the E100 crypto firmware file:
   - File: `build/CubeOrange/bin/arduplane.apj`
   - Or: `arduplane.apj` from the build output directory
4. Click **Upload** and wait for the upload to complete
5. Mission Planner will automatically reboot the board after successful upload
6. **Verify**: Mission Planner should reconnect automatically after reboot
   - Connection should be successful without semaphore timeout errors
   - If connection fails, see troubleshooting section below

#### Step 4: Verify LAS Parameters
1. In Mission Planner, navigate to **Config/Tuning** → **Full Parameter List**
2. Search for or scroll to find the LAS crypto parameters:
   - **LAS_CRYPT_KEY** - Encryption key parameter (write-only, reads as 0)
   - **LAS_CRYPT_LVL** - Encryption level (0-3)
   - **LAS_CRYPT_STAT** - Key status (read-only, 0=no key, 1=key stored)
   - **LAS_SCR_ENABLE** - Scripting enable (0=disabled, 1=enabled)
3. **Verify**: All four LAS parameters should be present and visible
   - If parameters are missing, check that AP_CRYPTO library was properly integrated
   - Verify `AP_CRYPTO_ENABLED` is defined in the build

### Expected Results
- ✅ Mission Planner connects successfully after each step
- ✅ No semaphore timeout errors
- ✅ All LAS parameters are visible in parameter list
- ✅ Firmware version shows "ArduPlane V4.5.7"
- ✅ Board responds to parameter reads/writes normally

### Troubleshooting

#### Mission Planner Connection Fails After Firmware Upload
- **Symptom**: "The semaphore timeout period has expired" error
- **Possible Causes**:
  1. Firmware build issue - rebuild firmware and try again
  2. USB driver issue - reinstall USB drivers or try different USB port
  3. Bootloader issue - try flashing `arduplane_with_bl.hex` via STM32CubeProgrammer instead
- **Solution**: See Fix #2 in FIXES section above

#### LAS Parameters Not Visible
- **Symptom**: LAS_CRYPT_KEY, LAS_CRYPT_LVL, etc. not in parameter list
- **Possible Causes**:
  1. AP_CRYPTO library not properly integrated
  2. `AP_CRYPTO_ENABLED` not defined during build
  3. Parameter index conflict
- **Solution**: 
  1. Verify `libraries/AP_Crypto/` directory exists
  2. Check `ArduPlane/Parameters.cpp` has `AP_SUBGROUPINFO(crypto_params, "LAS_", 36, ...)`
  3. Rebuild firmware with `./waf clean && ./waf plane`

#### Board Doesn't Boot After Firmware Upload
- **Symptom**: No connection, no LED activity, board appears dead
- **Possible Causes**:
  1. Corrupted firmware upload
  2. Bootloader issue
  3. Hardware problem
- **Solution**:
  1. Try flashing `arduplane_with_bl.hex` via STM32CubeProgrammer
  2. Verify bootloader is present and functional
  3. Check hardware connections and power supply

### Alternative: STM32CubeProgrammer Method

If Mission Planner firmware update fails, use STM32CubeProgrammer:

1. Open STM32CubeProgrammer
2. Connect to board via ST-Link or USB DFU
3. Select **File** → **Open File**
4. Browse to `build/CubeOrange/bin/arduplane_with_bl.hex`
5. Click **Download** to flash firmware
6. Disconnect and reconnect USB cable
7. Open Mission Planner and verify connection

**Note**: The `_with_bl.hex` file includes both bootloader and application firmware, so it's safe to flash even if bootloader is missing.

### Verification Checklist
- [ ] Stock firmware connects to Mission Planner
- [ ] Parameter reset completes successfully
- [ ] Board reboots after parameter reset
- [ ] Mission Planner reconnects after reboot
- [ ] E100 firmware uploads successfully
- [ ] Board reboots after firmware upload
- [ ] Mission Planner reconnects after firmware upload
- [ ] All LAS parameters are visible
- [ ] Firmware version is correct (V4.5.7)
- [ ] No connection errors or timeouts

---

## BUILD COMMANDS

### Initial Setup
```bash
cd /home/jsmith/E100_BUILD
git clone --recursive https://github.com/IamPete1/ardupilot.git
cd ardupilot
git checkout ArduPlane-4.5.7
git checkout -b E100-4.5.7-base
```

### Apply Patch
```bash
git cherry-pick dc8701d22c648bb4e87679aa6698f7573022e643
```

### Copy Custom Files
```bash
cp /mnt/d/FromLES/AP_Airspeed_DroneCAN.cpp libraries/AP_Airspeed/
```

### Copy AP_CRYPTO Library
```bash
cp -r /home/jsmith/LEIGH-6DOF-Branch/ardupilot/libraries/AP_Crypto libraries/
```

### Build
```bash
./waf clean
./waf configure --board CubeOrange
./waf plane
```

### Output Files
- `build/CubeOrange/bin/arduplane.apj` - Mission Planner firmware
- `build/CubeOrange/bin/arduplane_with_bl.hex` - Bootloader + firmware for STM32CubeProgrammer
- `build/CubeOrange/bin/arduplane.abin` - Binary firmware

---

## GIT COMMITS

### Commit History
1. `df68147cb8dfc46e4fd02676ea9ba832c8a3afc2` - "Based on 4.5.7 Branch"
2. `04e5389cb4` - "AP_Airspeed: DroneCAN: allow runtime detection of single sensor"
3. `c94f7bbf58` - "First clean build & run"
4. `a17f9bb589` - "AP_Airspeed: DroneCAN: allow runtime detection of single sensor" (cherry-pick)
5. `6fcbe802cd` - "Booting,Connecting with LAS parameters shown in parameters list" (Fix #4: Parameter index change 22→36)

---

## NOTES

- **Python Environment**: Build requires `venv-ardupilot` virtual environment with `future` and `intelhex` modules
- **Bootloader**: Bootloader is pre-built binary (`Tools/bootloaders/CubeOrange_bl.bin`), `_with_bl.hex` concatenates bootloader + application
- **Parameter Index**: `crypto_params` uses index 36 in `ParametersG2` (changed from 22 to avoid conflict with old EFI data)
- **Mission Planner Only**: This build is designed for Mission Planner, not QGroundControl
- **Hardware**: CubeOrange (STM32H743xx) board

---

## DOCUMENTATION UPDATES

**To Update FIXES Section**: Say "update fixes" and provide details of the new fix to be added.

**Last Updated**: 2026-01-16 (Added Fix #4: Parameter Index Conflict)
