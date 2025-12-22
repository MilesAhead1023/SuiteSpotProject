# SuiteSeeker - Quick Start Guide

Get Heatseeker training mode running in 5 minutes!

## Prerequisites

✅ **Rocket League** installed
✅ **BakkesMod** installed and working
✅ **Visual Studio 2022** with C++ tools

## Build & Install

### Step 1: Build the Plugin

Open PowerShell in the `SuiteSeekerProject` directory:

```powershell
# Build the plugin
MSBuild SuiteSeeker.sln /p:Configuration=Release /p:Platform=x64
```

**Expected output**:
```
Build succeeded.
    0 Warning(s)
    0 Error(s)
```

**Output file**: `plugins\SuiteSeeker.dll`

### Step 2: Install to BakkesMod

```powershell
# Copy DLL to BakkesMod plugins folder
Copy-Item plugins\SuiteSeeker.dll "$env:APPDATA\bakkesmod\bakkesmod\plugins\"
```

### Step 3: Load in Rocket League

1. Launch **Rocket League**
2. Press **F6** to open BakkesMod console
3. Type:
   ```
   plugin load suiteseeker
   ```

**Expected output**:
```
SuiteSeeker loaded
SuiteSeeker: Plugin loaded!
SuiteSeeker: Event hooks registered
SuiteSeeker: Initialization complete
```

## Usage

### Automatic Mode (Recommended)

1. Go to **BakkesMod Settings** (F2)
2. Navigate to **Plugins → SuiteSeeker**
3. Enable these settings:
   - ✅ **Enable SuiteSeeker**
   - ✅ **Auto-enable in Freeplay**
4. Enter **Freeplay** mode
5. The ball will automatically seek goals! 🎯

### Manual Mode

If you prefer manual control:

1. Disable "Auto-enable in Freeplay"
2. Press **F6** in freeplay
3. Type: `suiteseeker_toggle`
4. Press Enter

## Verification

### Test 1: Plugin Loaded?
```
Press F6
Type: plugin show suiteseeker
```

**Should show**:
```
SuiteSeeker loaded
SuiteSeeker: Plugin loaded!
```

### Test 2: Settings Visible?
```
Press F2 → Plugins tab
Look for "SuiteSeeker"
```

**Should see**: Settings panel with toggles and sliders

### Test 3: Heatseeker Active?
```
1. Enter Freeplay
2. Hit the ball
3. Ball should curve toward goals
```

## Troubleshooting

### ❌ Build Fails

**Error**: `Cannot find BakkesMod SDK`

**Fix**:
1. Install BakkesMod
2. Verify registry key exists:
   ```powershell
   Get-ItemProperty "HKCU:\Software\BakkesMod" -Name AppPath
   ```
3. Rebuild

---

**Error**: `v143 toolset not found`

**Fix**:
1. Install Visual Studio 2022
2. Include "Desktop development with C++"
3. Rebuild

### ❌ Plugin Won't Load

**Symptom**: `plugin load suiteseeker` → no response

**Fix**:
1. Check DLL location:
   ```powershell
   Test-Path "$env:APPDATA\bakkesmod\bakkesmod\plugins\SuiteSeeker.dll"
   ```
2. Should return `True`
3. If `False`, copy DLL again

---

**Symptom**: `SuiteSeeker.dll is not a valid plugin`

**Fix**:
1. Rebuild in **Release** mode (not Debug)
2. Ensure **Platform is x64** (not x86)

### ❌ Ball Not Curving

**Symptom**: Ball acts normal in freeplay

**Fixes**:

1. **Check if enabled**:
   ```
   F6 → Type: suiteseeker_toggle
   Should say "Heatseeker mode ACTIVE!"
   ```

2. **Spawn a new ball**:
   - Reset shot (default: R key)
   - Try after ball spawns

3. **Check console for errors**:
   ```
   F6 → Look for red text
   ```

4. **Try manual toggle**:
   ```
   F6 → Type: sv_soccar_gamemode heatseeker
   ```

### ❌ Settings Not Saving

**Symptom**: Settings reset after restarting RL

**This is normal!** CVars are saved by BakkesMod. If settings don't persist:

1. Close Rocket League completely
2. Reopen and check settings
3. BakkesMod saves on clean exit

## Console Commands Cheat Sheet

```bash
# Plugin management
plugin load suiteseeker          # Load plugin
plugin unload suiteseeker        # Unload plugin
plugin show suiteseeker          # Show plugin logs

# Heatseeker control
suiteseeker_toggle               # Toggle on/off

# Settings (manually)
suiteseeker_enabled 1            # Enable (0=off, 1=on)
suiteseeker_auto_enable 1        # Auto-enable in freeplay
suiteseeker_speed_multiplier 1.5 # Set speed (0.5-3.0)

# Alternative (if plugin fails)
sv_soccar_gamemode heatseeker    # Direct mutator command
sv_soccar_gamemode soccar        # Reset to normal
```

## Next Steps

### Customize Settings

**Slower Heatseeker** (easier):
```
Speed Multiplier: 0.5x
```

**Faster Heatseeker** (harder):
```
Speed Multiplier: 2.0x - 3.0x
```

### Advanced Configuration

See `README.md` for:
- Full CVar reference
- Advanced physics settings
- Development guide

### Join the Community

- **BakkesMod Discord**: https://discord.gg/HMptXSzCvU
- Share your Heatseeker training clips!

## Tips & Tricks

💡 **Training Packs + Heatseeker**:
1. Load a training pack
2. Enable Heatseeker
3. Practice redirects with seeking balls!

💡 **Speed Training**:
- Start at 0.5x speed
- Gradually increase to 2.0x
- Builds reaction time

💡 **Defense Training**:
- Let ball spawn in corner
- Practice saves as it seeks your goal

## Known Limitations

⚠️ Currently works best in **Freeplay**
⚠️ May need to toggle after ball respawn
⚠️ Some freeplay modes might not support mutator

## Getting Help

1. **Check logs**: `F6` → `plugin show suiteseeker`
2. **Read docs**: `README.md` and `ARCHITECTURE.md`
3. **Ask community**: BakkesMod Discord
4. **File bug**: GitHub Issues (if available)

---

🚀 **You're all set!** Enjoy Heatseeker training mode!

**Version**: 1.0.0
**Last Updated**: 2025-12-20
