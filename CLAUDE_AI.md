# AI Development Reference

## Build
```powershell
msbuild SuiteSpotv2.0\SuiteSpot.sln /p:Configuration=Release /p:Platform=x64
```
Output: `SuiteSpotv2.0\plugins\SuiteSpot.dll`

## Critical Constraints

**Thread Safety (MANDATORY):**
- All game state: `gameWrapper->SetTimeout([this](GameWrapper* gw) { ... }, 0.0f)`
- Never store wrappers
- Never capture by reference in lambdas
- See: `SuiteSpotDocuments/instructions/context/thread-safety.instructions.md`

**Wrapper Pattern:**
```cpp
gameWrapper->SetTimeout([this](GameWrapper* gw) {
    ServerWrapper server = gw->GetCurrentGameState();
    if (!server) return;
    // Use fresh wrapper immediately
}, 0.0f);
```

**CVars:**
- Register only in `onLoad()`
- Naming: `suitespot_*`
- All updates go to member vars in callback

**Persistence:**
- `SaveTrainingMaps()` after modifying `RLTraining`
- Location: `%APPDATA%\bakkesmod\bakkesmod\data\SuiteTraining\`

**File Format:**
- Every `.cpp` starts: `#include "pch.h"`
- Tabs, not spaces
- No raw `new`/`delete` without matching pair

## Architecture

| Module | File | Purpose |
|--------|------|---------|
| Plugin | `SuiteSpot.h/cpp` | Lifecycle, state, hooks |
| Maps | `MapList.h/cpp` | RLMaps, RLTraining, RLWorkshop |
| UI | `Source.cpp` | ImGui settings |
| Loadouts | `LoadoutManager.h/cpp` | Car loadout switching |

## Key APIs

**GameWrapper:** `GetCurrentGameState()`, `SetTimeout(lambda, delay)`, `HookEventWithCallerPost()`

**ServerWrapper:** `GetBall()`, `GetCars()`, `GetTeams()`, `GetPlayers()`

**Event Hooks:**
- `Function TAGame.GameEvent_Soccar_TA.EventMatchEnded` → auto-queue
- `Function TAGame.AchievementManager_TA.HandleMatchEnded` → post-match stats

See: `SuiteSpotDocuments/instructions/context/` for full API docs

## Design Rules (from DECISIONS.md)

- **ADR-001:** Shuffle bag algorithm for non-repeating maps
- **ADR-002:** SetTimeout for ALL game state access

## Common Tasks

**Add CVar:**
```cpp
cvarManager->registerCvar("suitespot_name", "default", "Desc", true, true, 0, true, 100)
    .addOnValueChanged([this](string oldValue, CVarWrapper cvar) {
        memberVar = cvar.getBoolValue();
    });
```

**Load maps after change:**
```cpp
SaveTrainingMaps();
LoadTrainingMaps();
```

**Hook event:**
```cpp
gameWrapper->HookEventWithCallerPost(
    "Function TAGame.GameEvent_Soccar_TA.EventMatchEnded",
    bind(&SuiteSpot::OnMatchEnded, this, placeholders::_1, placeholders::_2, placeholders::_3));
```

## Testing
```
plugin load suitespot    # Load in RL console (F6)
plugin show suitespot    # View logs
```

## Reference

- **Docs:** `SuiteSpotDocuments/instructions/context/`
- **Decisions:** `DECISIONS.md`
- **Build Issues:** `BUILD_TROUBLESHOOTING.md`
- **Release:** `DEPLOYMENT.md`
