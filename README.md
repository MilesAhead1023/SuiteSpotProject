# SuiteSpot - BakkesMod Plugin

**Purpose:** Automated map loading for Rocket League with Freeplay, Training Pack, and Workshop map support.

**Build:** `msbuild SuiteSpotv2.0\SuiteSpot.sln /p:Configuration=Release /p:Platform=x64`

**Output:** `SuiteSpotv2.0\plugins\SuiteSpot.dll`

## Code Organization

| Module | Path | Purpose |
|--------|------|---------|
| Main Plugin | `SuiteSpot.h/cpp` | Lifecycle, hooks, state, persistence |
| Maps | `MapList.h/cpp` | RLMaps, RLTraining, RLWorkshop vectors |
| UI | `Source.cpp` | ImGui settings window |
| Loadouts | `LoadoutManager.h/cpp` | Car loadout switching |
| Base | `GuiBase.h/cpp` | Settings window infrastructure |

## Critical Patterns

**Thread Safety:** All game state access via `gameWrapper->SetTimeout()` or `Execute()`. Never store wrappers.

**Event Hooks:** 
- `EventMatchEnded` → triggers auto-queue
- `HandleMatchEnded` → collects post-match stats

**Data Path:** `%APPDATA%\bakkesmod\bakkesmod\data\SuiteTraining\`

**CVar Format:** `suitespot_*` naming convention. Register only in `onLoad()`.

**Persistence:** Call `SaveTrainingMaps()` after modifying `RLTraining` vector.

## Key References

- **Architecture:** `SuiteSpotDocuments/instructions/context/`
- **Design Decisions:** `DECISIONS.md`
- **Full API Docs:** `SuiteSpotDocuments/instructions/context/*.md`

**Version:** 1.0.0 (Build 312)
