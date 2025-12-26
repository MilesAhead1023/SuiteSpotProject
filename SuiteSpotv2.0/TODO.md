# UI/UX TODO (SuiteSpot)

## Priority 1 � Layout clarity and correctness
- [ ] Align radio buttons (`Freeplay`, `Training`, `Workshop`, `Freemod`) to prevent label clipping/overlap with headers.
- [ ] Separate `Enable SuiteSpot` and `Open Standalone Window` controls with proper spacing/margins; un-cramp the toggle row.
- [ ] Add inline help and units for `Auto-Queue Next Match` delay; include min/max.
- [ ] Add inline help and min/max for `Delay Freeplay (sec)` and `Delay Queue (sec)`.
- [ ] Improve `Map Selection` row: label + dropdown + `Load Now` on one line, `Load Now` right-aligned.
- [ ] Add visual separation (padding) between section headers and controls to avoid overlap/bleed on dark backgrounds.
- [ ] Standardize control spacing (radio buttons, checkboxes, sliders, numeric steppers) into a grid to avoid clipping.
- [ ] Fix numeric input clipping/overlap (e.g., `Offset X/Y`, `Blue Hue`, `Orange Hue`, `Header Size` rows).
- [ ] Ensure scrollbar visibility when content overflows (long lists/forms).
- [ ] Resolve header bars/collapsible sections overlapping content (e.g., `Team Sections` vs `Column Positions`).
- [ ] Align table headers with data columns in `Prejump Packs`; prevent actions column crowding.

## Priority 2 � Guidance, feedback, and defaults
- [ ] Add tooltips/help for unclear controls (e.g., `Fade In/Out Time`, `Overall Alpha` vs `Header Alpha`, `Section Padding`).
- [ ] Show default ranges/units next to sliders/steppers; validate ranges for saturation/brightness/alpha consistency.
- [ ] Add subtle "changes saved" indicator (autosave confirmation) near footer.
- [ ] Add per-section "Reset to defaults" (not just global) to avoid wiping unrelated tweaks.
- [ ] Add inline status after applying loadout (e.g., "Applied successfully").
- [ ] Show loading/empty state in Loadout tab when current loadout is `<Unknown>`; reconcile with available count.

## Priority 3 � Loadout management UX
- [ ] Keep dropdown and buttons in a single aligned row with padding.
- [ ] Add "Save current loadout" action.
- [ ] Add "Delete loadout" action.
- [ ] Add per-loadout preview/description tooltip to handle truncated names.

## Priority 4 � Overlay/pack usability
- [ ] Add "Test Overlay" button inside overlay layout tab (not only main) for quick iteration.
- [ ] Make action buttons/header row sticky in pack list when scrolling.
- [ ] Add live preview toggle for fade effects/colors to confirm before applying globally.
- [ ] Add search/filter for settings or per-tab anchor links (quick nav).
- [ ] Route Prejump pack filtering UX changes through `PrejumpUI` once extracted.

## Priority 5 � Performance polish
- [ ] Defer or throttle repaint while dragging sliders; update on release to reduce jitter.
- [ ] Confirm autosave state visually (toast/checkmark) after change commits.

## Notes
- Autosave text currently sits very low; avoid clipping on smaller windows.
- Improve input background vs label contrast for readability.
