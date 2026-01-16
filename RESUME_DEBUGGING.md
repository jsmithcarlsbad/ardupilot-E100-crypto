# RESUME_DEBUGGING.md

**Checkpoint Date**: 2026-01-16  
**Purpose**: Resume debugging from this exact state

---

## CURRENT STATE SUMMARY

### ✅ **WORKING**
- **Firmware boots successfully** ✓
- **Mission Planner connects reliably** ✓
- **LAS parameters visible in parameter list** ✓
  - `LAS_CRYPT_KEY` (write-only)
  - `LAS_CRYPT_LVL` (read-write)
  - `LAS_CRYPT_STAT` (read-only, computed)
  - `LAS_SCR_ENABLE` (read-write)
- **No semaphore timeout errors** ✓
- **MAVLink heartbeat and messages transmitting** ✓

### ⚠️ **NOT YET TESTED**
- **Scripting enabled** (`SCR_ENABLE=1`) - Has fixes in place but not tested yet
  - Thread creation deferred when USB connected
  - Directory creation deferred to scripting thread
  - 5-second delay before thread creation

---

## GIT REPOSITORY STATE

### Repository Information
- **Location**: `/home/jsmith/E100_BUILD/ardupilot`
- **Branch**: `E100-4.5.7-base`
- **Base Version**: ArduPlane 4.5.7
- **Upstream Remote (origin)**: https://github.com/IamPete1/ardupilot.git (fetch only, push disabled)
- **Fork Remote (target)**: git@github.com:jsmithcarlsbad/ardupilot-E100-crypto.git (push enabled)
- **Default Push Remote**: `target` (your fork)
- **GitHub Repository**: https://github.com/jsmithcarlsbad/ardupilot-E100-crypto.git

### Recent Commits
```
1c92ecb362 Add README_E100_CRYPTO.md documenting fork-specific changes
930774f9dc Minor formatting/comment updates
ad7a106d51 Add RESUME_DEBUGGING.md checkpoint
831b52f6f7 Update PORTING_FIXES.md with Fix #4: Parameter index conflict resolution
6fcbe802cd Booting,Connecting with LAS parameters shown in parameters list
```

### Working Directory Status
- **All changes committed** ✓
- **Working tree clean** ✓
- **Branch synced with remote** ✓ (`target/E100-4.5.7-base`)

---

## KEY FIXES APPLIED

### Fix #4: Parameter Index Conflict (CRITICAL - Latest Fix)
- **Problem**: Old EFI data at index 22 blocking parameter loading
- **Solution**: Changed `crypto_params` index from 22 to 36
- **File**: `ArduPlane/Parameters.cpp` line 1274
- **Status**: ✅ **WORKING** - Connection works reliably

### Fix #3: Scripting Directory Creation Blocking
- **Problem**: `mkdir()` blocking USB CDC initialization
- **Solution**: Deferred directory creation to scripting thread
- **File**: `libraries/AP_Scripting/AP_Scripting.cpp` line 186-189
- **Status**: ✅ Fix applied, ⏳ Not tested with scripting enabled

### Fix #2: USB CDC Initialization - Storage Backup Blocking
- **Problem**: `_save_backup()` blocking during parameter loading
- **Solution**: Deferred backup until USB connected
- **File**: `libraries/AP_HAL_ChibiOS/Storage.cpp`
- **Status**: ✅ **WORKING**

### Fix #1: LAS_CRYPT_STAT Parameter Interception
- **Problem**: Parameter returning stored value instead of computed
- **Solution**: Intercept parameter reads in GCS_Param.cpp
- **File**: `libraries/GCS_MAVLink/GCS_Param.cpp`
- **Status**: ✅ **WORKING**

---

## CRITICAL CODE LOCATIONS

### Parameter Registration
**File**: `ArduPlane/Parameters.cpp`  
**Line**: ~1274
```cpp
#if AP_CRYPTO_ENABLED
    // @Group: LAS_
    // @Path: ../libraries/AP_Crypto/AP_Crypto_Params.cpp
    // CRITICAL FIX: Changed index from 22 to 36 to avoid conflict with old EFI parameters
    // Index 22 was previously used for EFI, and old EFI data in FRAM can cause blocking during parameter loading
    AP_SUBGROUPINFO(crypto_params, "LAS_", 36, ParametersG2, AP_Crypto_Params),
#endif
```

### Scripting Initialization
**File**: `libraries/AP_Scripting/AP_Scripting.cpp`  
**Line**: ~179-203
- `init()` defers thread creation if USB connected
- `update()` creates thread after 5-second delay

### Parameter Loading
**File**: `libraries/AP_Vehicle/AP_Vehicle.cpp`  
**Line**: ~287
- `load_parameters()` called before GCS initialization
- This is the correct order (matches working GitHub code)

---

## BUILD INFORMATION

### Build Commands
```bash
cd /home/jsmith/E100_BUILD/ardupilot
./waf clean
./waf configure --board CubeOrange
./waf plane
```

### Output Files
- **Mission Planner**: `build/CubeOrange/bin/arduplane.apj`
- **STM32CubeProgrammer**: `build/CubeOrange/bin/arduplane_with_bl.hex`
- **Binary**: `build/CubeOrange/bin/arduplane.abin`

### Build Status
- ✅ **Last build successful** (no compilation errors)
- ✅ **Firmware size**: Within limits
- ✅ **All libraries compiled**: AP_Crypto integrated

---

## TESTING PROCEDURE

### Current Working Procedure
1. Flash stock CubeOrange firmware
2. Reset all parameters to defaults
3. Reboot and verify connection
4. Upload `arduplane.apj` via Mission Planner firmware update tool
5. Reboot and verify connection
6. Verify LAS parameters are visible

### Next Steps to Test
1. **Test scripting enabled**:
   - Set `SCR_ENABLE=1`
   - Reboot
   - Verify connection still works
   - Check that scripting thread starts after 5 seconds

2. **Test with old parameters**:
   - Don't clear parameters
   - Flash firmware
   - Verify connection works (should work now with index 36)

---

## KNOWN ISSUES

### None Currently
- All known issues have been resolved
- Connection works reliably
- Parameters visible and functional

---

## IMPORTANT NOTES

### Parameter Index Change
- **OLD**: Index 22 (conflicted with old EFI data)
- **NEW**: Index 36 (no conflict)
- **Reason**: Old EFI data in FRAM at index 22 was causing blocking during parameter loading

### Scripting Status
- Scripting is **disabled** by default (`SCR_ENABLE=0`)
- Fixes are in place for when scripting is enabled
- Not yet tested with scripting enabled

### Storage Backend
- Parameters stored in **FRAM** (RAMTRON)
- Storage initialization deferred when USB connected
- Backup operations deferred to prevent blocking

---

## FILES MODIFIED IN LATEST COMMIT

### Commit: `6fcbe802cd` - "Booting,Connecting with LAS parameters shown in parameters list"
1. `ArduPlane/Parameters.cpp` - Parameter index changed 22→36
2. `libraries/AP_BoardConfig/AP_BoardConfig.cpp`
3. `libraries/AP_Crypto/AP_Crypto_Params.cpp`
4. `libraries/AP_HAL_ChibiOS/Storage.cpp`
5. `libraries/AP_HAL_ChibiOS/sdcard.cpp`
6. `libraries/AP_Scripting/AP_Scripting.cpp`
7. `libraries/AP_Scripting/AP_Scripting.h`
8. `libraries/AP_Scripting/lua_scripts.cpp`
9. `libraries/GCS_MAVLink/GCS.h`
10. `libraries/GCS_MAVLink/GCS_Common.cpp`
11. `libraries/GCS_MAVLink/GCS_Param.cpp`

### Commit: `831b52f6f7` - "Update PORTING_FIXES.md with Fix #4"
1. `PORTING_FIXES.md` - Documentation updated

### Commit: `ad7a106d51` - "Add RESUME_DEBUGGING.md checkpoint"
1. `RESUME_DEBUGGING.md` - This checkpoint file

### Commit: `930774f9dc` - "Minor formatting/comment updates"
1. `ArduPlane/Parameters.cpp` - Minor formatting updates
2. `libraries/AP_HAL_ChibiOS/Storage.cpp` - Minor comment updates

### Commit: `1c92ecb362` - "Add README_E100_CRYPTO.md documenting fork-specific changes"
1. `README_E100_CRYPTO.md` - Fork documentation

---

## RESUME INSTRUCTIONS

### To Resume Debugging:
1. **Verify git state**:
   ```bash
   cd /home/jsmith/E100_BUILD/ardupilot
   git status
   git log --oneline -5
   ```
   Should show clean working tree and latest commit `1c92ecb362`
   
2. **Verify remotes**:
   ```bash
   git remote -v
   ```
   Should show:
   - `origin` → IamPete1 (fetch only)
   - `target` → jsmithcarlsbad (push enabled)

3. **Rebuild if needed**:
   ```bash
   ./waf clean
   ./waf configure --board CubeOrange
   ./waf plane
   ```

4. **Test connection**:
   - Flash firmware to CubeOrange
   - Connect via Mission Planner
   - Verify LAS parameters visible

5. **Next testing**:
   - Test scripting enabled (`SCR_ENABLE=1`)
   - Test with old parameters (don't clear before flashing)

---

## REFERENCE DOCUMENTATION

- **README_E100_CRYPTO.md**: Fork-specific documentation and build instructions
- **PORTING_FIXES.md**: Complete documentation of all fixes
- **RESUME_DEBUGGING.md**: This checkpoint file
- **Working Branch**: `/home/jsmith/LEIGH-6DOF-Branch`
- **GitHub Fork**: https://github.com/jsmithcarlsbad/ardupilot-E100-crypto.git
- **GitHub Reference**: https://github.com/jsmithcarlsbad/ardupilot-leigh-crypto.git

---

## CHECKPOINT VALIDATION

**Checkpoint Created**: 2026-01-16  
**Last Updated**: 2026-01-16 (after README_E100_CRYPTO.md creation)  
**Git State**: Clean, all changes committed and pushed to fork  
**Build Status**: Successful  
**Connection Status**: ✅ Working  
**Parameters Status**: ✅ Visible  
**Repository Status**: ✅ Synced with https://github.com/jsmithcarlsbad/ardupilot-E100-crypto.git  

**Ready to Resume**: ✅ YES

---

**END OF CHECKPOINT**
