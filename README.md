# SuiteSpot

**SuiteSpot** is a BakkesMod plugin for Rocket League that provides automated map loading and queuing after matches. It supports Freeplay maps, Training Packs, and Workshop maps with configurable delays and shuffle options.

## Features

- 🎮 **Auto-Queue:** Automatically load the next map after a match ends
- 📍 **Map Types:** Freeplay, Training Packs, or Workshop maps
- 🎲 **Shuffle Bag:** Smart random selection ensuring variety (no immediate repeats)
- ⏱️ **Configurable Delays:** Separate delay controls for queue, freeplay, training, and workshop
- 🧠 **Post-Match Overlay:** Beautiful stats overlay showing match results and player stats
- 🎯 **Prejump Packs:** Integration with Prejump training pack library
- 🎨 **Custom Loadouts:** Automatic car loadout switching on map load

## Build

**Requirements:**
- Visual Studio 2022 Professional
- BakkesMod SDK (included in `BakkesModSDK/`)
- Windows 10+ with x64 architecture

**Build Command:**
```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe' `
  SuiteSpotv2.0\SuiteSpot.sln `
  /p:Configuration=Release /p:Platform=x64 /v:minimal
```

Or use Visual Studio 2022: **Ctrl+Shift+B**

**Output:** `SuiteSpotv2.0\plugins\SuiteSpot.dll`

## Installation

1. Ensure BakkesMod is installed and running
2. Copy `SuiteSpotv2.0\plugins\SuiteSpot.dll` to your BakkesMod plugins directory
3. Run `plugin load suitespot` in the Rocket League console (F6)

## Usage

### In-Game Console (F6)

```
plugin load suitespot      # Load the plugin
plugin unload suitespot    # Unload the plugin
plugin show suitespot      # View plugin logs
```

### Settings UI

Access plugin settings in BakkesMod's UI to configure:
- Enable/disable auto-queue
- Select map type (Freeplay/Training/Workshop)
- Set delays for each mode
- Configure training pack shuffle
- Customize post-match overlay appearance

## Project Structure

```
SuiteSpotv2.0/
├── SuiteSpot.h/cpp          # Main plugin class
├── MapList.h/cpp            # Map data structures
├── Source.cpp               # ImGui settings UI
├── LoadoutManager.h/cpp     # Car loadout management
├── GuiBase.h/cpp            # Settings window base
├── IMGUI/                   # ImGui implementation
├── BakkesMod.props          # Build configuration
└── SuiteSpot.sln            # Visual Studio solution

SuiteSpotDocuments/
├── instructions/context/    # API reference docs
├── instructions/development/# Development guides
└── instructions/reference/  # Codemap and structure

DECISIONS.md                  # Architecture Decision Records
CLAUDE.md                     # AI workflow guide
```

## Architecture

### Thread Safety
All game state access uses `gameWrapper->SetTimeout()` or `Execute()` to ensure thread-safe access to BakkesMod wrappers. Never store wrappers across frames.

### Data Persistence
Configuration and maps are saved to:
```
%APPDATA%\bakkesmod\bakkesmod\data\SuiteTraining\
├── SuiteSpotTrainingMaps.txt
├── SuiteShuffleBag.txt
└── prejump_packs.json
```

### Event Hooks
- `Function TAGame.GameEvent_Soccar_TA.EventMatchEnded` - Triggers auto-queue on match end
- `Function TAGame.AchievementManager_TA.HandleMatchEnded` - Collects post-match stats

## Development

### Key Files to Know

| File | Purpose |
|------|---------|
| `SuiteSpot.h` | Plugin interface and state |
| `SuiteSpot.cpp` | Game hooks, persistence, core logic |
| `Source.cpp` | ImGui settings window |
| `MapList.cpp` | Freeplay, training, and workshop maps |
| `LoadoutManager.cpp` | Car loadout switching |

### Important Patterns

**SetTimeout for Game State:**
```cpp
gameWrapper->SetTimeout([this](GameWrapper* gw) {
    ServerWrapper server = gw->GetCurrentGameState();
    if (!server) return;
    // Use server safely here
}, 0.0f);
```

**CVar Registration (onLoad only):**
```cpp
cvarManager->registerCvar("suitespot_name", "default", "Description",
    true, true, 0, true, 100)
    .addOnValueChanged([this](string oldValue, CVarWrapper cvar) {
        memberVar = cvar.getBoolValue();
    });
```

**Saving Map Data:**
```cpp
SaveTrainingMaps();  // Call after modifying RLTraining vector
```

### Documentation

Comprehensive API documentation is in `SuiteSpotDocuments/instructions/`:
- **context/** - BakkesMod API references (GameWrapper, ServerWrapper, etc.)
- **development/** - Build system, lifecycle, logging
- **reference/** - Project structure and codemap

See `CLAUDE.md` for detailed development workflow and safety rules.

## Version

Current version: **1.0.0** (Build 312)

## License

This project uses the BakkesMod Plugin SDK. See BakkesMod documentation for licensing terms.

## Support

For issues or feature requests, consult the documentation in `SuiteSpotDocuments/` and review `DECISIONS.md` for architectural context.
