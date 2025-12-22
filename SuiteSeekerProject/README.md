# SuiteSeeker - Heatseeker Training Plugin for BakkesMod

A standalone BakkesMod plugin that adds **Heatseeker mode** to freeplay training in Rocket League.

## Features

- ✅ **Auto-Enable in Freeplay**: Automatically activates Heatseeker when you enter freeplay
- ✅ **Adjustable Speed**: Configure ball speed with a multiplier (0.5x - 3.0x)
- ✅ **Manual Toggle**: Enable/disable Heatseeker on-the-fly with console commands
- ✅ **Settings UI**: Full ImGui integration in BakkesMod settings menu
- ✅ **Thread-Safe**: Follows BakkesMod best practices for game state access

## Installation

1. **Build the plugin**:
   ```bash
   MSBuild SuiteSeeker.sln /p:Configuration=Release /p:Platform=x64
   ```

2. **Output location**:
   ```
   SuiteSeekerProject\plugins\SuiteSeeker.dll
   ```

3. **Install to BakkesMod**:
   - Copy `SuiteSeeker.dll` to:
     ```
     %APPDATA%\bakkesmod\bakkesmod\plugins\
     ```

4. **Load in Rocket League**:
   - Press `F6` to open console
   - Type: `plugin load suiteseeker`

## Usage

### In-Game

1. Launch Rocket League with BakkesMod
2. Enter **Freeplay** mode
3. Heatseeker will activate automatically (if enabled)
4. The ball will now seek goals like in Heatseeker mode!

### Console Commands

```
plugin load suiteseeker          # Load the plugin
plugin unload suiteseeker        # Unload the plugin
suiteseeker_toggle               # Toggle Heatseeker on/off
```

### Settings (F2 → Plugins → SuiteSeeker)

| Setting | Description | Range |
|---------|-------------|-------|
| **Enable SuiteSeeker** | Master on/off toggle | On/Off |
| **Auto-enable in Freeplay** | Activate when entering freeplay | On/Off |
| **Speed Multiplier** | Adjust ball seeking speed | 0.5x - 3.0x |
| **Horizontal Speed** | Fine-tune horizontal curve | 500 - 3000 |
| **Vertical Speed** | Fine-tune vertical curve | 200 - 2000 |

## CVars (Console Variables)

```
suiteseeker_enabled "0"                   # Enable plugin (0=off, 1=on)
suiteseeker_auto_enable "1"               # Auto-enable in freeplay
suiteseeker_speed_multiplier "1.0"        # Speed multiplier
suiteseeker_horizontal_speed "1500.0"     # Horizontal seek speed
suiteseeker_vertical_speed "800.0"        # Vertical seek speed
```

## Development

### Project Structure

```
SuiteSeekerProject/
├── SuiteSeeker.h            # Plugin header
├── SuiteSeeker.cpp          # Plugin implementation
├── pch.h / pch.cpp          # Precompiled headers
├── GuiBase.h / GuiBase.cpp  # Settings UI base
├── logging.h                # Logging utilities
├── version.h                # Version info
├── resource.h / .rc         # Windows resources
├── BakkesMod.props          # BakkesMod SDK config
├── SuiteSeeker.vcxproj      # Visual Studio project
├── SuiteSeeker.sln          # Visual Studio solution
└── IMGUI/                   # ImGui library files
```

### Build Requirements

- **Visual Studio 2022** (v143 toolset)
- **Windows SDK 10.0**
- **BakkesMod SDK** (installed and registered in registry)
- **C++20** support

### Build Commands

```powershell
# Clean build
MSBuild SuiteSeeker.sln /t:Clean /p:Configuration=Release /p:Platform=x64

# Build
MSBuild SuiteSeeker.sln /p:Configuration=Release /p:Platform=x64

# Rebuild
MSBuild SuiteSeeker.sln /t:Rebuild /p:Configuration=Release /p:Platform=x64
```

### Hot Reloading (Development)

While testing in Rocket League:

```
plugin unload suiteseeker    # Unload current version
plugin load suiteseeker      # Load new build
```

## Architecture

### Thread Safety

All game state access uses `gameWrapper->SetTimeout()` to ensure thread-safe execution on the game thread:

```cpp
gameWrapper->SetTimeout([this](GameWrapper* gw) {
    ServerWrapper server = gw->GetCurrentGameState();
    if (!server) return;  // Always null-check!

    // Safe to access game state here
}, 0.0f);
```

### Event Hooks

The plugin hooks these events:

| Event | Purpose |
|-------|---------|
| `Function TAGame.GameEvent_Soccar_TA.InitGame` | Detect freeplay load |
| `Function TAGame.GameEvent_Soccar_TA.Destroyed` | Detect freeplay exit |
| `Function TAGame.Ball_TA.Spawned` | Apply physics to new balls |

### Heatseeker Implementation

**Current approach**: Uses `sv_soccar_gamemode heatseeker` console command to enable the built-in Heatseeker mutator.

**Future enhancement**: Manual physics manipulation using `BallHauntedWrapper`:
- `SetSeekTarget()` - Aim ball at nearest goal
- `SetHorizontalSpeed()` - Control lateral curve speed
- `SetVerticalSpeed()` - Control vertical curve speed

## Troubleshooting

### Ball not curving in freeplay?

The Heatseeker mutator may not be available in all freeplay modes. Try:

1. Spawn a ball after enabling the plugin
2. Use the manual toggle: `suiteseeker_toggle`
3. Check console (F6) for error messages

### Plugin not loading?

1. Verify BakkesMod SDK is installed correctly
2. Check that `SuiteSeeker.dll` is in the plugins folder
3. Look for errors in BakkesMod console (F6)
4. Try: `plugin show suiteseeker` to see logs

### Build errors?

1. Ensure Visual Studio 2022 (v143) is installed
2. Verify BakkesMod is installed and registry key exists:
   ```
   HKEY_CURRENT_USER\Software\BakkesMod\AppPath@BakkesModPath
   ```
3. Check that all IMGUI files are present

## API Reference

### Key BakkesMod Wrappers Used

- **ServerWrapper**: Game state control (`GetCurrentGameState()`)
- **BallWrapper**: Ball physics (`GetBall()`)
- **BallHauntedWrapper**: Heatseeker ball type (future enhancement)
- **GameWrapper**: Thread-safe execution (`SetTimeout()`)
- **CVarManagerWrapper**: Console variables (`registerCvar()`)

### Documentation

See `SuiteSpotDocuments/instructions/` in parent directory for:

- `thread-safety.instructions.md` - **CRITICAL** thread safety patterns
- `event-hooking.instructions.md` - Event hook best practices
- `ballwrapper-api.md` - Ball physics API
- `serverwrapper-api.md` - Game state API

## Known Limitations

1. **Freeplay Only**: Heatseeker mode works in freeplay, not private matches
2. **Console Command Dependency**: Currently relies on built-in mutator
3. **Ball Respawn**: May need to toggle after ball respawns
4. **No Custom Physics Yet**: Advanced control via `BallHauntedWrapper` not yet implemented

## Future Enhancements

- [ ] Manual physics tick-based implementation (no console commands)
- [ ] Custom target selection (specific goal, moving targets)
- [ ] Speed ramping (ball accelerates over time)
- [ ] Visual indicators for ball trajectory
- [ ] Training pack integration (auto-enable for specific packs)

## Credits

- **BakkesMod SDK**: https://github.com/bakkesmodorg/BakkesModSDK
- **ImGui**: https://github.com/ocornut/imgui
- **Rocket League**: Psyonix

## License

This plugin is provided as-is for educational and training purposes.

## Support

- BakkesMod Discord: https://discord.gg/HMptXSzCvU
- GitHub Issues: [Report bugs or request features]

---

**Version**: 1.0.0
**Author**: SuiteSeeker Development
**Last Updated**: 2025-12-20
