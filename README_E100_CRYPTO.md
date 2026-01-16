# ArduPilot E100 Crypto Build

This is a specialized fork of ArduPilot for the E100 project with integrated AP_Crypto library and LAS parameter support.

## Repository Information

- **Fork Source**: https://github.com/IamPete1/ardupilot.git
- **Base Version**: ArduPlane 4.5.7
- **Branch**: `E100-4.5.7-base`
- **Target Hardware**: CubeOrange (STM32H743xx)

## What Makes This Fork Different

This fork includes the following E100-specific modifications:

### 1. AP_Crypto Library Integration
- **Location**: `libraries/AP_Crypto/`
- **Source**: https://github.com/jsmithcarlsbad/ardupilot-leigh-crypto.git
- **Purpose**: Provides encryption functionality for Lua scripts and log files

### 2. LAS Parameters
Four new parameters are available for crypto configuration:
- **LAS_CRYPT_KEY** (write-only): Encryption key parameter
- **LAS_CRYPT_LVL** (read-write): Encryption level (0-3)
- **LAS_CRYPT_STAT** (read-only): Key status indicator
- **LAS_SCR_ENABLE** (read-write): Scripting enable flag

**Parameter Index**: 36 in ParametersG2 (changed from 22 to avoid conflict with old EFI data)

### 3. Critical Fixes Applied

#### Fix #4: Parameter Index Conflict Resolution
- **Problem**: Old EFI data at index 22 was blocking parameter loading
- **Solution**: Changed `crypto_params` index from 22 to 36
- **Result**: Reliable connection even with old parameters in FRAM

#### Fix #3: Scripting Directory Creation Blocking
- **Problem**: `mkdir()` blocking USB CDC initialization
- **Solution**: Deferred directory creation to scripting thread
- **Result**: Connection works with scripting enabled

#### Fix #2: USB CDC Initialization - Storage Backup Blocking
- **Problem**: `_save_backup()` blocking during parameter loading
- **Solution**: Deferred backup until USB connected
- **Result**: Fast, non-blocking parameter loading

#### Fix #1: LAS_CRYPT_STAT Parameter Interception
- **Problem**: Parameter returning stored value instead of computed
- **Solution**: Intercept parameter reads in GCS_Param.cpp
- **Result**: Correct computed values returned

### 4. Additional Modifications
- **AP_Airspeed DroneCAN**: Runtime detection patch applied
- **Custom AP_Airspeed_DroneCAN.cpp**: From D:\FromLES
- **GCS MAVLink**: Radio status handling for USB/Serial connections
- **Storage**: Deferred backup operations for USB CDC compatibility

## Building

### Prerequisites
- Python 3.x with `venv-ardupilot` virtual environment
- Required Python modules: `future`, `intelhex`
- Build tools: `waf` (included in repository)

### Build Commands
```bash
cd ardupilot
./waf clean
./waf configure --board CubeOrange
./waf plane
```

### Output Files
- **Mission Planner**: `build/CubeOrange/bin/arduplane.apj`
- **STM32CubeProgrammer**: `build/CubeOrange/bin/arduplane_with_bl.hex`
- **Binary**: `build/CubeOrange/bin/arduplane.abin`

## Installation

### Via Mission Planner
1. Flash stock CubeOrange firmware first (optional, for clean start)
2. Reset all parameters to defaults
3. Use Mission Planner's firmware update tool
4. Select `arduplane.apj` file
5. Upload and reboot

### Via STM32CubeProgrammer
1. Connect via ST-Link or USB DFU
2. Open `arduplane_with_bl.hex`
3. Flash to board
4. Disconnect and reconnect USB
5. Connect via Mission Planner

## Verification

After installation, verify:
- ✅ Mission Planner connects successfully
- ✅ No semaphore timeout errors
- ✅ LAS parameters visible in Full Parameter List:
  - `LAS_CRYPT_KEY`
  - `LAS_CRYPT_LVL`
  - `LAS_CRYPT_STAT`
  - `LAS_SCR_ENABLE`
- ✅ Firmware version shows "ArduPlane V4.5.7"

## Documentation

- **PORTING_FIXES.md**: Complete documentation of all fixes and modifications
- **RESUME_DEBUGGING.md**: Checkpoint for resuming development

## Git Configuration

This repository is configured with two remotes:
- **origin**: https://github.com/IamPete1/ardupilot.git (upstream, fetch only)
- **target**: git@github.com:jsmithcarlsbad/ardupilot-E100-crypto.git (this fork, push enabled)

**Default push remote**: `target` (your fork)

## Key Commits

- `930774f9dc` - Minor formatting/comment updates
- `ad7a106d51` - Add RESUME_DEBUGGING.md checkpoint
- `831b52f6f7` - Update PORTING_FIXES.md with Fix #4
- `6fcbe802cd` - Booting,Connecting with LAS parameters shown in parameters list
- `df68147cb8` - Based on 4.5.7 Branch

## Support

For issues specific to this fork, refer to:
- **PORTING_FIXES.md**: Detailed fix documentation
- **RESUME_DEBUGGING.md**: Development checkpoint

For general ArduPilot support:
- Support Forum: https://discuss.ardupilot.org/
- Community Site: https://ardupilot.org

## License

This fork maintains the same GPL-3.0 license as the upstream ArduPilot project.

---

**Last Updated**: 2026-01-16  
**Maintainer**: E100 Project Team
