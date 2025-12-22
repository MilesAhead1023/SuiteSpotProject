# SuiteSeeker Plugin Architecture

## Overview

SuiteSeeker is a standalone BakkesMod plugin that implements Heatseeker training mode for Rocket League freeplay. This document explains the technical architecture and design decisions.

## Design Philosophy

### 1. **Thread Safety First**
Following BakkesMod best practices, all game state access is wrapped in `gameWrapper->SetTimeout()` to ensure execution on the game thread:

```cpp
// ✅ CORRECT - Thread-safe
gameWrapper->SetTimeout([this](GameWrapper* gw) {
    ServerWrapper server = gw->GetCurrentGameState();
    if (!server) return;  // Always null-check
    // Safe game state access here
}, 0.0f);

// ❌ WRONG - Not thread-safe
ServerWrapper server = gameWrapper->GetCurrentGameState();
```

**Why?** BakkesMod plugins run on a separate thread from the game. Accessing game objects directly can cause crashes or race conditions.

### 2. **Wrapper Lifetime Management**
Never store wrappers across frames - always get fresh ones:

```cpp
// ✅ CORRECT
gameWrapper->SetTimeout([this](GameWrapper* gw) {
    BallWrapper ball = gw->GetCurrentGameState().GetBall();
    if (!ball) return;
    // Use immediately
}, 0.0f);

// ❌ WRONG - Wrapper may become invalid
BallWrapper cachedBall = gameWrapper->GetCurrentGameState().GetBall();
```

**Why?** Wrappers are lightweight pointers to game objects that can be destroyed/moved by the game engine at any time.

### 3. **Event-Driven Architecture**
The plugin reacts to game events rather than polling:

```cpp
gameWrapper->HookEvent("Function TAGame.Ball_TA.Spawned",
    std::bind(&SuiteSeeker::OnBallSpawned, this, std::placeholders::_1));
```

**Why?** More efficient than polling, and ensures we react to events at the right time.

## Component Breakdown

### Core Components

```
SuiteSeeker (Main Plugin Class)
│
├── CVar System (Configuration)
│   ├── suiteseeker_enabled
│   ├── suiteseeker_auto_enable
│   ├── suiteseeker_speed_multiplier
│   ├── suiteseeker_horizontal_speed
│   └── suiteseeker_vertical_speed
│
├── Event Hooks (Game State Monitoring)
│   ├── OnFreeplayLoad()
│   ├── OnFreeplayDestroy()
│   └── OnBallSpawned()
│
├── Core Logic (Heatseeker Control)
│   ├── EnableHeatseeker()
│   ├── DisableHeatseeker()
│   └── ApplyHeatseekerPhysics()
│
└── Settings UI (ImGui)
    └── RenderSettings()
```

### Data Flow

```
User enters freeplay
    ↓
InitGame event fires
    ↓
OnFreeplayLoad() detects freeplay mode
    ↓
[If auto_enable=true AND enabled=true]
    ↓
EnableHeatseeker() executes console command
    ↓
Ball spawns → OnBallSpawned() hook
    ↓
Heatseeker physics applied to ball
    ↓
Ball seeks nearest goal!
```

## Implementation Details

### Heatseeker Activation

**Current Approach**: Console Command
```cpp
cvarManager->executeCommand("sv_soccar_gamemode heatseeker", false);
```

**Pros**:
- Simple, one-line implementation
- Uses built-in game mutator
- Handles all physics automatically

**Cons**:
- May not work in all freeplay modes
- Limited customization

**Future Approach**: Manual Physics Manipulation
```cpp
// Get ball as BallHauntedWrapper
BallWrapper ball = server.GetBall();
// Would need to cast/convert to BallHauntedWrapper
// Then manually set physics each tick:
// ballHaunted.SetSeekTarget(goalPosition);
// ballHaunted.SetHorizontalSpeed(horizontalSpeed * speedMultiplier);
// ballHaunted.SetVerticalSpeed(verticalSpeed * speedMultiplier);
```

**Pros**:
- Full control over physics
- Custom targets (not just goals)
- Works in all modes

**Cons**:
- More complex
- Need to hook physics tick
- Performance considerations

### Freeplay Detection

```cpp
bool SuiteSeeker::IsInFreeplayMode()
{
    ServerWrapper server = gameWrapper->GetCurrentGameState();
    if (!server) return false;

    // Freeplay = single car, no online match
    auto cars = server.GetCars();
    if (cars.Count() <= 1) {
        return true;
    }

    return false;
}
```

**Why this works**: Freeplay always has 1 or fewer cars, while matches have 2+.

## Thread Safety Patterns

### Pattern 1: Immediate Execution
```cpp
gameWrapper->SetTimeout([this](GameWrapper* gw) {
    // Execute immediately on game thread
}, 0.0f);
```

### Pattern 2: Delayed Execution
```cpp
gameWrapper->SetTimeout([this](GameWrapper* gw) {
    // Execute after delay (e.g., wait for server init)
}, 0.5f);  // 500ms delay
```

### Pattern 3: Lambda Captures
```cpp
// ✅ Capture by value (safe)
float speed = speedMultiplier;
gameWrapper->SetTimeout([this, speed](GameWrapper* gw) {
    // Use 'speed' safely
}, 0.0f);

// ❌ Capture by reference (dangerous)
gameWrapper->SetTimeout([this, &speed](GameWrapper* gw) {
    // 'speed' may be invalid by the time this runs!
}, 0.0f);
```

## CVar System

### Registration Pattern
```cpp
cvarManager->registerCvar(
    "cvar_name",                    // Unique identifier
    "default_value",                // Initial value
    "Description shown in UI",      // Help text
    true, true, 0, true, 10        // (searchable, has_min, min, has_max, max)
)
.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) {
    // React to value changes
    memberVariable = cvar.getBoolValue();
});
```

### CVar Types

| Type | Getter | Example |
|------|--------|---------|
| Bool | `getBoolValue()` | `"0"` or `"1"` |
| Int | `getIntValue()` | `"42"` |
| Float | `getFloatValue()` | `"1.5"` |
| String | `getStringValue()` | `"text"` |

## Settings UI (ImGui)

### Render Flow
```cpp
void SuiteSeeker::RenderSettings()
{
    // 1. Show status
    ImGui::TextColored(color, "STATUS: ACTIVE");

    // 2. Controls linked to CVars
    CVarWrapper cvar = cvarManager->getCvar("suiteseeker_enabled");
    bool value = cvar.getBoolValue();
    if (ImGui::Checkbox("Enable", &value)) {
        cvar.setValue(value);  // Triggers onValueChanged callback
    }

    // 3. Tooltips for user guidance
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Description here");
    }
}
```

**Key Points**:
- UI controls modify CVars, not member variables directly
- CVar callbacks update member variables
- Ensures persistence across sessions (BakkesMod saves CVars)

## File Structure

```
SuiteSeeker.h          - Plugin class definition
SuiteSeeker.cpp        - Implementation
├── Lifecycle          (onLoad, onUnload)
├── CVar Registration  (RegisterCVars)
├── Event Hooks        (RegisterHooks)
├── Event Handlers     (OnFreeplayLoad, etc.)
├── Core Logic         (EnableHeatseeker, etc.)
├── Helpers            (IsInFreeplayMode)
└── UI                 (RenderSettings)

pch.h/cpp              - Precompiled headers (faster builds)
GuiBase.h/cpp          - Settings window base class
logging.h              - LOG() and DEBUGLOG() macros
version.h              - VERSION_MAJOR/MINOR/PATCH/BUILD
resource.h/.rc         - Windows resource file
BakkesMod.props        - SDK paths and build config
```

## Build Process

### Compilation Flow
```
1. update_version.ps1  → Updates version.h with build number
2. pch.h compiled      → Precompiled header created
3. *.cpp compiled      → Using pch.h
4. Link with pluginsdk.lib → BakkesMod SDK
5. bakkesmod-patch.exe → Post-build patching
6. Output: SuiteSeeker.dll
```

### Key Build Settings

| Setting | Value | Why |
|---------|-------|-----|
| Platform Toolset | v143 | Visual Studio 2022 |
| C++ Standard | C++20 | Modern language features |
| Runtime Library | MultiThreaded | Static linking (no DLL deps) |
| Precompiled Header | Use pch.h | Faster builds |
| Output | DLL | BakkesMod plugin format |

## Dependencies

### SDK Headers (from BakkesMod)
```cpp
#include "bakkesmod/plugin/bakkesmodplugin.h"    // Plugin base
#include "bakkesmod/plugin/PluginSettingsWindow.h" // Settings UI
```

### Wrappers Used
- `GameWrapper` - Main game interface
- `ServerWrapper` - Game state control
- `BallWrapper` - Ball physics
- `CVarManagerWrapper` - Console variables
- `BallHauntedWrapper` - Heatseeker ball (future)

### External Libraries
- **ImGui** - Settings UI rendering
- **fmt** - String formatting (via logging.h)
- **Windows SDK** - Platform APIs

## Error Handling

### Null-Check Pattern
```cpp
ServerWrapper server = gw->GetCurrentGameState();
if (!server) {
    LOG("SuiteSeeker: No server found");
    return;  // Fail gracefully
}
```

**Why**: Wrappers can be null if:
- Not in a game
- Game state transitioning
- Object destroyed

### Logging Levels
```cpp
LOG("Normal message");           // Always shown
DEBUGLOG("Debug info");          // Only if DEBUG_LOG=true in logging.h
cvarManager->log("User message"); // Shows in console + BM overlay
```

## Performance Considerations

### Event Hook Overhead
- Hooks are called **every time** the event fires
- Keep handlers fast and lightweight
- Use `SetTimeout` for heavy operations

### SetTimeout Cost
- Minimal overhead (async callback queue)
- Prefer batching operations in one timeout over many

### Memory Management
- No manual `new`/`delete` needed
- Wrappers are lightweight (just pointers)
- STL containers (`std::string`, `std::vector`) handled automatically

## Future Enhancements

### Phase 1: Enhanced Physics (v1.1)
- Manual `BallHauntedWrapper` control
- Custom target selection
- Speed ramping over time

### Phase 2: Advanced Features (v1.2)
- Multiple ball support
- Training pack integration
- Visual trajectory indicators

### Phase 3: Customization (v1.3)
- Configurable seek behavior
- Ball color changes
- Sound effects

## Testing Strategy

### Manual Testing
1. Load plugin: `plugin load suiteseeker`
2. Enter freeplay
3. Verify ball seeks goals
4. Toggle on/off: `suiteseeker_toggle`
5. Adjust speed in settings
6. Exit freeplay, re-enter

### Edge Cases
- [ ] Ball respawn handling
- [ ] Plugin disabled mid-game
- [ ] Multiple balls (rumble mode)
- [ ] Demolition/explosion
- [ ] Goal scored reset

## Common Pitfalls (Avoided)

### ❌ Storing Wrappers
```cpp
// WRONG - wrapper becomes invalid!
this->cachedBall = server.GetBall();
```

### ❌ Direct Game Access
```cpp
// WRONG - not thread-safe!
BallWrapper ball = gameWrapper->GetCurrentGameState().GetBall();
ball.SetVelocity(...);
```

### ❌ Missing Null Checks
```cpp
// WRONG - crashes if server is null!
ServerWrapper server = gw->GetCurrentGameState();
server.GetBall();  // CRASH!
```

### ❌ Reference Captures
```cpp
// WRONG - 'speed' may be destroyed!
gameWrapper->SetTimeout([this, &speed](GameWrapper* gw) {
    // Dangerous!
}, 1.0f);
```

## Resources

- **BakkesMod SDK**: https://github.com/bakkesmodorg/BakkesModSDK
- **Discord**: https://discord.gg/HMptXSzCvU
- **SuiteSpot Docs**: `../SuiteSpotDocuments/instructions/`

---

**Last Updated**: 2025-12-20
**Architecture Version**: 1.0
