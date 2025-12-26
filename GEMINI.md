# SuiteSpot: Gemini Context & Development Guide

**Target Audience:** AI Coding Agents & Human Developers
**Purpose:** Single source of truth for codebase structure, constraints, patterns, and architectural decisions.

---

## 1. Project Overview
**SuiteSpot** is a BakkesMod plugin for Rocket League designed to automate map loading and enhance training workflows.
- **Core Features:** Auto-loading Freeplay/Training/Workshop maps after matches.
- **Advanced Logic:** Shuffle bag randomization (no repeats), auto-queueing, and post-match analysis overlay.
- **Integration:** Scrapes and loads training packs from Prejump.com.
- **Tech Stack:** C++20, BakkesMod SDK, ImGui, PowerShell (tooling).

## 2. Quick Start

### Build System
- **Platform:** Windows x64 (Release build required for production).
- **Command (Standard):**
  ```powershell
  & "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe" SuiteSpotv2.0\SuiteSpot.sln /p:Configuration=Release /p:Platform=x64
  ```
- **Output:** `SuiteSpotv2.0\plugins\SuiteSpot.dll`
- **Auto-Deploy:** Post-build event automatically copies the DLL to `%APPDATA%\bakkesmod\bakkesmod\plugins\` if detected.

### Manual Installation
1. Copy `SuiteSpot.dll` to your BakkesMod plugins folder.
2. Open Rocket League, press F6, run `plugin load suitespot`.

## 3. Architecture & File Manifest

| File/Module | Responsibilities | Key Components |
| :--- | :--- | :--- |
| **`SuiteSpot.cpp`** | **Core Logic.** Plugin lifecycle, event hooks, CVar management, persistence IO, and overlay rendering. | `onLoad`, `onUnload`, `GameEndedEvent`, `RenderPostMatchOverlay` |
| **`Source.cpp`** | **Settings UI.** ImGui rendering for the main settings window and Prejump browser. | `RenderSettings`, `RenderPrejumpPacksTab` |
| **`GuiBase.cpp`** | **UI Infrastructure.** Base classes for managing ImGui contexts and window state. | `SettingsWindowBase`, `PluginWindowBase` |
| **`MapList.cpp`** | **Data Model.** Static and dynamic lists of maps. | `RLMaps` (Freeplay), `RLTraining` (Custom), `RLWorkshop` |
| **`LoadoutManager.cpp`** | **Feature Logic.** Thread-safe wrapper for switching car presets. | `SwitchLoadout`, `QueryLoadoutNamesInternal` |
| **`project.json`** | **Meta-Config.** Machine-readable project constraints. | Build targets, critical file lists. |
| **`decisions.md`** | **Architecture.** Records of key design decisions (ADRs). | *See Section 7 below.* |

## 4. Critical Constraints (MANDATORY)

**Violating these rules causes crashes.**

### A. Thread Safety (The Golden Rule)
- **Constraint:** BakkesMod wrappers (`ServerWrapper`, etc.) are **NOT** thread-safe.
- **Requirement:** ALL game state access must occur on the game thread via `gameWrapper->SetTimeout` or `Execute`.
- **Anti-Pattern:** `auto server = gameWrapper->GetCurrentGameState();` (Crash if outside game thread).
- **Correct Pattern:** 
  ```cpp
  gameWrapper->SetTimeout([this](GameWrapper* gw) {
      if (!gw) return;
      // Safe logic here
  }, 0.0f);
  ```

### B. Wrapper Lifetime
- **Constraint:** Wrappers are transient handles. They become invalid between frames/matches.
- **Requirement:** **NEVER** store a wrapper as a class member variable. Always retrieve a fresh one inside the local scope.

### C. File Structure
- **Constraint:** Every `.cpp` file must begin with `#include "pch.h"` to use precompiled headers.

### D. Data Persistence
- **Path:** `%APPDATA%\bakkesmod\bakkesmod\data\SuiteTraining\`
- **Requirement:** Call `SaveTrainingMaps()` immediately after modifying the `RLTraining` vector.

### E. Documentation First (Inviolable Rule)
- **Constraint:** Before writing ANY code, you **MUST** search `SuiteSpotv2.0\SuiteSpotDocuments\instructions` for the corresponding `.md` file (e.g., `carwrapper-api.md`, `thread-safety.instructions.md`).
- **Requirement:** You must explicitly confirm you have read the relevant doc or state "No specific documentation found for [topic]" prior to generating code.

## 5. CVar Reference (Configuration)

All CVars are registered in `SuiteSpot::onLoad()`.

| CVar Name | Type | Purpose |
| :--- | :--- | :--- |
| `suitespot_enabled` | Bool | Master toggle for all automation features. |
| `suitespot_map_type` | Int | 0=Freeplay, 1=Training, 2=Workshop. |
| `suitespot_auto_queue` | Bool | Automatically queue for next match after game end. |
| `suitespot_training_shuffle` | Bool | Enable shuffle bag logic for training packs. |
| `suitespot_delay_queue_sec` | Int | Delay before queuing (independent of map load). |
| `suitespot_delay_[type]_sec` | Int | Delays for loading Freeplay/Training/Workshop. |
| `suitespot_current_[type]_index` | Int | Persists selected map index for each category. |
| `suitespot_toggle_window` | Notifier | Toggles the standalone ImGui debug/control window. |

## 6. Implementation Patterns

### Safe Game State Access
```cpp
gameWrapper->SetTimeout([this](GameWrapper* gw) {
    if (!gw) return;
    ServerWrapper server = gw->GetCurrentGameState();
    if (!server) return; // Always check null! 
    
    BallWrapper ball = server.GetBall();
    if (ball) {
        // logic...
    }
}, 0.0f);
```

### Event Hooking
```cpp
// In LoadHooks()
gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.EventMatchEnded", 
    [this](std::string eventName) {
        this->GameEndedEvent(eventName);
    });
```

## 7. Architectural Decisions (ADR Summary)

*Refer to `DECISIONS.md` for full context.*

- **ADR-001 (Shuffle Bag):** We use a shuffle bag (draw until empty, then refill) instead of pure random to prevent repeated maps.
- **ADR-002 (SetTimeout):** We mandate `SetTimeout` for ALL wrapper access to guarantee thread safety, even if it adds a frame of latency.
- **ADR-003 (UI Separation):** `Source.cpp` handles ImGui; `SuiteSpot.cpp` handles logic. Communication happens via CVars or direct method calls, but logic never depends on UI state directly.
- **ADR-004 (Text Persistence):** Training maps are stored in a simple CSV text file (`SuiteSpotTrainingMaps.txt`) for human readability and easy editing.

## 8. Tooling & LSP

This project uses `clangd` for IntelliSense.
- **Config:** `.clangd` and `.clang-format` are present in the root.
- **Update:** Run `.\generate_compile_commands.ps1` if you add files or change includes.
- **Verification:** If `pch.h` errors appear in the editor, ensure `compile_commands.json` is up to date.

## 9. Verification Checklist

Before verifying a task complete:
1. [ ] **Thread Safety:** Is every wrapper access inside `SetTimeout`?
2. [ ] **Null Checks:** Are `server`, `car`, and `ball` wrappers checked for null?
3. [ ] **PCH:** Is `#include "pch.h"` the first line of every `.cpp`?
4. [ ] **Persistence:** Did I save data if I modified a list?
5. [ ] **Build:** Does `msbuild ... /p:Configuration=Release` succeed?

---
*End of Gemini Context Guide*
