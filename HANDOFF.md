# Handoff (2026-10-17)

Snapshot for whoever picks this up next. Conventions, atlas warnings, and
the "do not simplify" list live in AGENTS.md; the original brief is
PROMPT.md. Repo: github.com/david-battle/class, public, main pushed.

## Current state

- Single-file roguelike (class.c), builds warning-free, links the sibling
  `~/raylib` static library. Game left stopped at handoff.
- Level 1 loads from level1.map (48x28 prison ship). The committed file
  is the deterministic gen layout plus ~253 hand-painted floors and 1
  door from the owner's play sessions. That paintwork was lost once by a
  careless `./class gen` and painstakingly repainted; it is committed
  now. Treat local level1.map changes as precious: check
  `git diff level1.map`, and never regenerate without asking.
- Play: keypad 8-way, hold to auto-repeat, ENTER opens door/bars into a
  walkable threshold, `0` stops/cycles idle, the wheel finishes the level.
- Edit mode: E toggles it; keypad 1-6 pick FLOOR/DOOR/THRESHOLD/BARS/
  WHEEL/WATER, 0 picks WALL (delete); click or drag paints; S saves to
  the loaded file. Save keeps the original `start` and writes
  runtime-opened doors/bars as their original types, never as thresholds
  (OpenedNote/OpenedOrig in class.c).

## Planned revisions

- Lock the cell bars at spawn (currently openable from inside; noted in
  AGENTS.md World section).
- Sound effects and richer animations - the differentiators vs rogue;
  nothing built beyond walk/idle frames (see PROMPT.md).

## Verification recipe (WSLg / xdotool)

- Launch via `./run.sh` (detached; never foreground). Kill with
  `pkill -x class`.
- `xdotool search --name class | head -1` for the window id, then
  `xdotool windowfocus $WID` and `xdotool key --window $WID <key>`.
  Keys are flaky under WSLg: retry presses and confirm effects via
  class.log (run.sh truncates it per launch, so lines are unambiguous
  per game instance). Keypad enter keysym is KP_Enter (KP_ENTER is
  rejected); KP_0..KP_9 work as-is.
- Mouse math: game window client origin is abs (0,27) per xwininfo
  (xdotool getwindowgeometry lies); game coords = abs - origin; render
  size equals window size.

## Last verified

- Edit mode end-to-end: pick/click/drag/erase, out-of-bounds ignored,
  save round-trip.
- Save fidelity: bars opened in play and saved stayed `tile 6 12 4` in
  the file, not `3`.
- Git at 8ff3915 (painted map) + handoff note, all pushed.
