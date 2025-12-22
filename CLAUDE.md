# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

SuiteSpot is a **BakkesMod plugin** for Rocket League that provides automated map loading and queuing after matches. It supports Freeplay maps, Training Packs, and Workshop maps with configurable delays and shuffle options.

**Main plugin code:** `SuiteSpotv2.0/`

## Build Commands

**Development Environment:**
- Visual Studio 2022 Professional
- MSBuild Path: `C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe`

```powershell
# Build the plugin (Release|x64 required)
# From project root directory:
& 'C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe' SuiteSpotv2.0\SuiteSpot.sln /p:Configuration=Release /p:Platform=x64 /v:minimal

# Or use Visual Studio 2022 with Ctrl+Shift+B
```

**Output:** `SuiteSpotv2.0\plugins\SuiteSpot.dll`
**Toolset:** v143 (Visual Studio 2022)
**SDK:** BAKKESMOD_PLUGIN_API_VERSION 95

**Build Notes:**
- Conversion warnings (C4244, C4267, C4305, C4099) are suppressed in project settings
- Build should complete with zero warnings
- Build must complete successfully (errors not tolerated)
- Post-build automatically patches DLL and generates settings file

## Testing

```
# In Rocket League console (F6):
plugin load suitespot     # Load
plugin unload suitespot   # Unload
plugin show suitespot     # View logs
```

---

## DOCUMENTATION REFERENCE RULE

**ALWAYS consult the documentation in `SuiteSpotDocuments/` before making changes.** The documentation contains critical patterns, API references, and safety rules.

### Documentation Index

All documentation is in `SuiteSpotDocuments/instructions/`:

#### Context (API References)
| File | Purpose |
|------|---------|
| `context/architecture.instructions.md` | SuiteSpot architecture, components, CVars, data flow |
| `context/bakkesmod.instructions.md` | BakkesMod SDK overview, wrapper system, plugin types |
| `context/gamewrapper-api.md` | GameWrapper API - game state, event hooks, SetTimeout |
| `context/serverwrapper-api.md` | ServerWrapper API - match state, cars, ball, PRIs |
| `context/carwrapper-api.md` | CarWrapper API - car control, boost, components |
| `context/ballwrapper-api.md` | BallWrapper API - ball physics and state |
| `context/priwrapper-api.md` | PriWrapper API - player data, stats, loadouts |
| `context/teamwrapper-api.md` | TeamWrapper API - team data, colors, scores |
| `context/event-hooking.instructions.md` | Type-safe event hooking patterns (HookEventWithCaller) |
| `context/event-hook-registry.instructions.md` | 50+ event hook paths with caller types |
| `context/thread-safety.instructions.md` | **CRITICAL** - SetTimeout, Execute, thread-safe patterns |
| `context/cvarmanagerwrapper-api.md` | CVar registration and management |
| `context/canvaswrapper-api.md` | Canvas drawing API |
| `context/camerawrapper-api.md` | Camera control API |
| `context/playercontroller-api.md` | PlayerController API |
| `context/vehiclesim-api.instructions.md` | VehicleSim physics API |
| `context/flipcar-component.md` | Flip/jump mechanics |
| `context/boost-component.md` | Boost component |
| `context/jump-component.md` | Jump component |
| `context/dodge-component.md` | Dodge component |
| `context/goalwrapper-api.md` | Goal wrapper |
| `context/boost-pickup-wrapper.md` | Boost pickup pads |
| `context/mmr-ranking-api.instructions.md` | MMR/ranking queries |
| `context/items-cosmetics-api.instructions.md` | Items and cosmetics |
| `context/replay-system-api.instructions.md` | Replay system |
| `context/streaming-networking-api.instructions.md` | HTTP requests |
| `context/wheel-wrapper-api.instructions.md` | Wheel physics |
| `context/rbactor-wrapper.md` | RigidBody actor base |
| `context/actorwrapper-api.md` | Actor base class |
| `context/objectwrapper-api.md` | Object base class |
| `context/plugin-creation.md` | Plugin creation guide |
| `context/architecture.md` | Additional architecture notes |

#### Development Guides
| File | Purpose |
|------|---------|
| `development/development-guide.md` | Build system, lifecycle, CVars, hooks, GUI |
| `development/deployment-guide.md` | Plugin deployment, hot-reloading |
| `development/Imgui-windows.md` | ImGui window creation |
| `development/logging-utility.md` | Logging utilities |

#### Reference
| File | Purpose |
|------|---------|
| `reference/codemap.md` | Project structure overview, file locations |

---

## Critical Development Rules

### Thread Safety (MANDATORY - read `thread-safety.instructions.md`)

BakkesMod runs on the game thread. **Always use SetTimeout or Execute for game state access:**

```cpp
// CORRECT - game thread execution
gameWrapper->SetTimeout([this](GameWrapper* gw) {
    ServerWrapper server = gw->GetCurrentGameState();
    if (server) { /* safe */ }
}, 0.0f);

// WRONG - may crash
ServerWrapper server = gameWrapper->GetCurrentGameState();
```

### Wrapper Lifetime

Never store wrappers across frames - get fresh ones each use:

```cpp
// WRONG: Stored wrapper may become invalid
ServerWrapper cachedServer;

// CORRECT: Fresh wrapper each time
gameWrapper->SetTimeout([this](GameWrapper* gw) {
    ServerWrapper server = gw->GetCurrentGameState();
    if (!server) return;
    // Use immediately
}, 0.0f);
```

### Lambda Captures

Always capture by value for SetTimeout:

```cpp
std::string mapCode = "Stadium_P";
gameWrapper->SetTimeout([this, mapCode](GameWrapper* gw) {
    cvarManager->executeCommand("load_freeplay " + mapCode);
}, 5.0f);
```

### Event Hooking (read `event-hooking.instructions.md`)

Use type-safe HookEventWithCaller:

```cpp
gameWrapper->HookEventWithCallerPost<ServerWrapper>(
    "Function TAGame.GameEvent_Soccar_TA.EventMatchEnded",
    bind(&SuiteSpot::OnMatchEnded, this,
         placeholders::_1, placeholders::_2, placeholders::_3));
```

### File Requirements

- Every `.cpp` starts with `#include "pch.h"`
- Null-check all wrappers immediately
- Register CVars only in `onLoad()`
- Call `SaveTrainingMaps()` after modifying `RLTraining`

---

## Architecture Quick Reference

### Core Files (in `SuiteSpotv2.0/`)
| File | Purpose |
|------|---------|
| `SuiteSpot.h/cpp` | Main plugin - lifecycle, hooks, persistence, overlay |
| `Source.cpp` | ImGui settings UI (`RenderSettings()`) |
| `MapList.h/cpp` | Map data: `RLMaps`, `RLTraining`, `RLWorkshop` vectors |
| `GuiBase.h/cpp` | Settings window base class |
| `LoadoutManager.h/cpp` | Car loadout management |

### Key Event Hooks

```cpp
"Function TAGame.GameEvent_Soccar_TA.EventMatchEnded"
"Function TAGame.AchievementManager_TA.HandleMatchEnded"
```

### Data Storage

```
%APPDATA%\bakkesmod\bakkesmod\data\SuiteTraining\SuiteSpotTrainingMaps.txt
%APPDATA%\bakkesmod\bakkesmod\data\SuiteTraining\SuiteShuffleBag.txt
%APPDATA%\bakkesmod\bakkesmod\data\SuiteTraining\prejump_packs.json
```

### CVar Pattern

```cpp
cvarManager->registerCvar("suitespot_name", "default", "Description",
    true, true, 0, true, 100)
    .addOnValueChanged([this](string oldValue, CVarWrapper cvar) {
        memberVar = cvar.getBoolValue();
    });
```

---

## SDK Resources

- **BakkesMod SDK:** `BakkesModSDK/include/bakkesmod/`
- **Plugin Template:** `BakkesmodPluginTemplate/`
- **Community Discord:** https://discord.gg/HMptXSzCvU

---

## AI Agent Workflow Guide

When working on SuiteSpot, follow this workflow to ensure high-quality iterations:

### Pre-Implementation Checklist

Before writing ANY code:

1. **Read architectural context:**
   - [ ] Check `DECISIONS.md` for relevant ADRs (Architecture Decision Records)
   - [ ] Review `SuiteSpotDocuments/instructions/context/architecture.instructions.md`
   - [ ] Verify the change aligns with existing patterns

2. **Understand the specific API:**
   - [ ] If touching game state: Read `thread-safety.instructions.md` (CRITICAL)
   - [ ] If adding event hooks: Read `event-hooking.instructions.md` + `event-hook-registry.instructions.md`
   - [ ] If using wrappers: Read the specific wrapper API doc (e.g., `serverwrapper-api.md`)

3. **Plan before coding:**
   - [ ] Ask user for clarification if requirements are ambiguous
   - [ ] Propose architecture/approach before implementation
   - [ ] Identify affected files using `reference/codemap.md`

### Implementation Rules

**MANDATORY checks for every code change:**

✅ **Thread Safety:**
- All game state access uses `gameWrapper->SetTimeout()` or `Execute()`
- No wrappers stored in member variables
- Lambda captures are by value (not reference)

✅ **Wrapper Usage:**
- Every wrapper is null-checked immediately after obtaining it
- No wrapper reuse across different code blocks
- Fresh wrapper obtained each time it's needed

✅ **File Structure:**
- Every `.cpp` starts with `#include "pch.h"` (FIRST LINE)
- No changes to precompiled header setup
- Follow existing file organization (UI in Source.cpp, logic in SuiteSpot.cpp)

✅ **CVars:**
- Registered only in `onLoad()`, never elsewhere
- Follow naming convention: `suitespot_*`
- Include proper callbacks for value changes

✅ **Data Persistence:**
- Call `SaveTrainingMaps()` after modifying `RLTraining` vector
- Never modify data file formats without user approval
- Maintain backward compatibility with existing save files

### Code Review Checklist

After writing code, verify:

- [ ] No thread safety violations (see `thread-safety.instructions.md`)
- [ ] All wrappers null-checked
- [ ] Event hooks use correct caller types (see `event-hook-registry.instructions.md`)
- [ ] Lambda captures are safe (by value, not reference)
- [ ] No memory leaks (proper RAII, no raw `new` without `delete`)
- [ ] Follows existing code style (tabs, not spaces)
- [ ] Build tested: `MSBuild SuiteSpotv2.0\SuiteSpot.sln /p:Configuration=Release /p:Platform=x64`

### Common Pitfalls to Avoid

❌ **DON'T DO THIS:**

```cpp
// ❌ Direct game state access (not thread-safe)
ServerWrapper server = gameWrapper->GetCurrentGameState();

// ❌ Storing wrappers (becomes invalid)
this->cachedServer = server;

// ❌ Capture by reference in SetTimeout
std::string mapCode = "X";
gameWrapper->SetTimeout([this, &mapCode](GameWrapper* gw) { ... }, 1.0f);

// ❌ Missing null check
server.GetBall(); // crashes if server is null

// ❌ Wrong event caller type
gameWrapper->HookEventWithCallerPost<CarWrapper>( // should be ServerWrapper
    "Function TAGame.GameEvent_Soccar_TA.EventMatchEnded", ...);

// ❌ Forgetting pch.h
#include <vector>  // pch.h must be FIRST
#include "pch.h"
```

✅ **DO THIS INSTEAD:**

```cpp
// ✅ Thread-safe game state access
gameWrapper->SetTimeout([this](GameWrapper* gw) {
    ServerWrapper server = gw->GetCurrentGameState();
    if (!server) return; // ✅ Null check
    // Use server immediately, don't store it
}, 0.0f);

// ✅ Capture by value
std::string mapCode = "X";
gameWrapper->SetTimeout([this, mapCode](GameWrapper* gw) { ... }, 1.0f);

// ✅ Correct event caller type
gameWrapper->HookEventWithCallerPost<ServerWrapper>(
    "Function TAGame.GameEvent_Soccar_TA.EventMatchEnded", ...);

// ✅ pch.h first
#include "pch.h"
#include <vector>
```

### When Uncertain

If you're unsure about:
- **API usage:** Check `SuiteSpotDocuments/instructions/context/[wrapper]-api.md`
- **Patterns:** Look at existing code in `SuiteSpot.cpp` for examples
- **Architecture:** Ask user before making structural changes
- **Safety:** When in doubt, use SetTimeout and null-check everything

### Documentation Updates

After implementing features:
- [ ] Update `DECISIONS.md` if you made architectural choices
- [ ] Add comments for non-obvious BakkesMod API usage
- [ ] Update CLAUDE.md if you discovered new patterns/gotchas

### Success Criteria

A change is ready when:
1. ✅ Builds without errors: `MSBuild ...`
2. ✅ Follows all thread safety rules
3. ✅ All wrappers null-checked
4. ✅ Matches existing code style
5. ✅ Tested in Rocket League (if applicable)
6. ✅ No violations of architectural decisions in `DECISIONS.md`

---

## Quick Reference: File Locations

**When implementing features, know where code goes:**

| Task | File to Modify |
|------|---------------|
| Add UI elements | `Source.cpp` (RenderSettings function) |
| Add game logic | `SuiteSpot.cpp` |
| Add event hook | `SuiteSpot.cpp` (onLoad for registration) |
| Add CVar | `SuiteSpot.cpp` (onLoad function) |
| Add map data | `MapList.cpp` (RLMaps/RLTraining/RLWorkshop vectors) |
| Add loadout feature | `LoadoutManager.cpp` |
| Add new class/component | Create in `SuiteSpotv2.0/`, update `.vcxproj` |

---

## AI Agent Self-Check

Before submitting code, ask yourself:

1. **Did I read the relevant documentation?** (Not just guessed based on naming)
2. **Is this thread-safe?** (Used SetTimeout? No stored wrappers?)
3. **Did I null-check?** (Every wrapper checked before use?)
4. **Does this match existing patterns?** (Looked at similar code?)
5. **Will this build?** (pch.h included first? Syntax correct?)
6. **Is this what the user actually wants?** (Clarified if ambiguous?)

If you can answer "yes" to all six, proceed. Otherwise, stop and address the issue.
