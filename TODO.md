# SuiteSpot Development TODO

## 1. Standalone Window Reliability Issues (Highest Priority)

**Context:**
`SuiteSpot Control` is a standalone ImGui window that can be toggled via `togglemenu SuiteSpot` or the BakkesMod settings tab. Currently, it opens sporadically and fails when hovering other windows or when BakkesMod UI is focused.

**Reference Documentation:**
- `SuiteSpotv2.0/SuiteSpotDocuments/instructions/development/Imgui-windows.md`
- `SuiteSpotv2.0/SuiteSpotDocuments/instructions/context/plugin-creation.md`

### Current Behavior (Problem Statement)
- Opens sporadically.
- Fails to open if mouse is hovering BakkesMod UI (Settings, Console, etc.).
- Fails to open if BakkesMod Console/Settings is focused.
- Sometimes fails even with no other windows open.
- **Rendering is correct** when it *does* open.

### Expected Behavior
- `togglemenu SuiteSpot` must **always** toggle the window.
- Independent of mouse hover state.
- Independent of ImGui focus.
- Independent of other BakkesMod windows.
- Hotkeys must work reliably.

### Developer Tasks
- [ ] **Audit ImGui Window Logic:**
    - Ensure `ImGui::Begin()` is not restricted by hover/focus checks in `GuiBase.cpp` or `Source.cpp`.
- [ ] **Verify CVAR Execution Path:**
    - Confirm `togglemenu SuiteSpot` maps to a single, deterministic toggle function.
- [ ] **Ensure Visibility State:**
    - Variable governing visibility (e.g., `isWindowOpen_`) must be independent of BakkesMod's internal menu state.
- [ ] **Validate Hotkey Context:**
    - Confirm callbacks execute regardless of UI focus.
- [ ] **Root Cause Analysis:**
    - Provide an explanation of *why* it was failing before applying the fix.

---

## 2. CVAR Cleanup and Standardization

**Context:**
We need to remove redundant CVARs and introduce a dedicated one for the Test Overlay.

**Reference Documentation:**
- `SuiteSpotv2.0/SuiteSpotDocuments/instructions/context/cvarmanagerwrapper-api.md`

### Changes Required
- [ ] ❌ **Remove `suitespot_toggle_window`**
    - Redundant. Remove registration, callbacks, and references.
- [ ] ✅ **Retain `togglemenu SuiteSpot`**
    - This is the sole method for toggling the control window.
- [ ] ➕ **Add `ss_testoverlay`**
    - **Purpose:** Toggle the Test Overlay independently of the main window or UI state.
    - **Constraints:** Must work via console, hotkey, and scripts.

### Developer Tasks
- [ ] Implement `ss_testoverlay` as a single-responsibility CVAR.
- [ ] Ensure it toggles `postMatch.active` (or equivalent) reliably.
- [ ] Add logging to confirm execution and state transitions.

---

## 3. Move "Shuffle" UI + Logic to Prejump Packs Tab

**Context:**
Shuffle logic currently exists outside the Prejump Packs tab, causing fragmentation. It applies specifically to Prejump packs and should be co-located.

**Status:** **Design Review Required**

### Current Issues
- UI fragmentation.
- Poor discoverability.
- Conceptual mismatch (Shuffle is for Prejump packs).

### Developer Tasks
- [ ] **Design Proposal (Mandatory First Step):**
    - Explain UI restructuring strategy.
    - Define state ownership and data flow.
    - Decide if Shuffle logic is centralized or moved into `Prejump` module.
    - Plan for migrating existing user settings.
- [ ] **Implementation (Await Approval):**
    - Move Shuffle UI controls to Prejump Packs tab.
    - Move Shuffle logic to Prejump module.

---

## 4. Header Metadata Improvements (Low Risk)

**Context:**
The main window header lacks credit and version info.

### Target Format
```text
SuiteSpot
By: Flicks Creations
Version: x.y.z
```

### Developer Tasks
- [ ] Source version from `version.h` or plugin metadata.
- [ ] Update `RenderSettings()` header layout.
- [ ] Ensure clean visual consistency.

---

## Priority Order
1. Standalone window reliability and focus issues.
2. CVAR cleanup + new `ss_testoverlay`.
3. Shuffle UI + logic relocation (Design First).
4. Header metadata (Author + Version).
