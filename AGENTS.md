# class

Roguelike in C on raylib ("class" by analogy to rogue: not just rogue, you can
pick your class). Single source file, own git repo, own GitHub repo
(`david-battle/class`, public). Differentiator vs. rogue: animations and sound
effects, which are not built yet. Keep changes small.

## Build / run

- `make` — builds `./class`. Links statically against the sibling raylib clone
  at `~/raylib` (`src/raylib.h`, `src/libraylib.a`). That clone is upstream
  `raysan5/raylib`, not personal; do not push it.
- `./run.sh` — builds if needed, then launches the game detached via `setsid`
  and returns immediately. Run GUI programs this way: a plain foreground launch
  inherits the agent tool's stdout pipe, and the tool's timeout then kills the
  whole process group (this looked like the game "dying" but was the launcher).
- Binary `class` and `class.log` are gitignored; commit source and assets only.
- Paths are relative (`Hooded_Rogue_256_Frame_Sprite_Atlas.png` in the repo
  root), so run the binary from the repo root — the run script does.

## Files

- `class.c` — the whole program. Borderless-windowed fullscreen on the external
  monitor (same WSLg handling as `~/tetris`: `FLAG_WINDOW_UNDECORATED`, sized
  over the chosen monitor after `InitWindow()`, since RAIL ignores
  `SetWindowMonitor()`/`FLAG_FULLSCREEN_MODE`). Draws exactly one atlas frame
  at 1:1 pixels in the center of the screen, inside a lazily generated maze of
  64 px tiles (see **World** below). Keypad `7 8 9 / 4 6 / 1 2 3` steps one
  tile per press in that facing and alternates the step pose (walk row,
  column 0) with a standing frame (idle row); walls and closed doors block
  movement (diagonals also need both adjacent cardinals open — no corner
  cutting). `ENTER` opens the door or bars being faced (they become a walkable
  stone threshold). `0` stops into the standing row and cycles idle frames.
  Arrows
  alias the four cardinals for keyboards without a pad. The avatar is always
  drawn at screen center; the world scrolls beneath it one whole tile per
  step — no sub-tile camera.
- `Hooded_Rogue_256_Frame_Sprite_Atlas.png` — 1254x1254 RGBA, 256 frames,
  16 columns x 16 rows.

## Atlas geometry (measured from the file, do not re-derive casually)

- Columns are a uniform 1254/16 = 78.375 px pitch; `COL_X` rounds those
  boundaries, each of which lands inside a transparent column gap.
- **Rows are not uniform.** The first 11 rows sit on a ~78 px pitch with ~9 px
  padding; the bottom four are packed at ~67-72 px with ~3-6 px padding. A
  uniform `k * cell` row table drifts ~30 px by row 16 and slices those frames
  across two sprites. `ROW_Y` holds the measured midpoints of the transparent
  row gaps; all 256 frames were verified to sit strictly inside their cell.
  Frame content is ~44x68 px, centered in its cell — cell rects are safe to
  draw 1:1.

## Atlas semantics (read off the art; `DIR_NAME`/`KP_DIR` in class.c)

- Rows 0-7 are the eight idle facings clockwise from south — S, SW, W, NW, N,
  NE, E, SE — and rows 8-15 are the matching *step* poses (row = facing + 8).
  Basis: skin/hand pixels sit left of center in rows 1-3 and right in 6-7; the
  face vanishes in rows 3-5; walk rows match their idle partner's head-skin
  profile. Keypad digits map straight onto the compass (8=N … 1=SW … 7=NW).
- **A walk row is not a cycle.** All 16 of its columns are the same mid-step
  pose with idle-animation jitter (frame IoU vs column 0 is 0.85–1.00 in every
  walk row). Walking is faked by alternating the standing frames with the step
  pose per keypress; only column 0 of a walk row is ever drawn. An idle row's
  columns, by contrast, are a real standing animation cycle.
- GLFW resolves keypad keys by physical scancode (see
  `glfw/src/external/glfw/src/x11_init.c`), so raylib's `KEY_KP_*` fire with
  NumLock on or off; this was verified empirically with xdotool under both
  states. Do not add numlock handling.

## World (pre-generated levels, sparse tiles)

- Levels are plain-text files (`level1.map`: `format 1`, `size W H`,
  `start X Y`, one `tile X Y T` line per non-wall tile). `./class gen`
  rewrites the default level deterministically (hand-placed, no RNG); the
  game loads the file at startup and errors if it is missing.
- Tiles still live in the open-addressed hash map (`PackXY`/`TileGet`/
  `TileSet`); absent = solid ship wall. Outside `[0,W)x[0,H)` renders as
  ocean (unwalkable). `T_THRESHOLD` (opened door/bars) is walkable.
- Level 1 is a prison ship: start in a cell with `T_BARS` (left unlocked —
  locking it is a planned revision), cross mess/cargo/galley/crew, reach the
  `T_WHEEL` on the stern deck to finish (movement freezes, HUD banner).
- Tile art is generated at startup with `GenImageColor` + `ImageDraw*`:
  light-wood deck floor, dark-wood walls/doors (a ship, not stone), bars,
  ocean, wheel, stone threshold. No asset files. (This raylib's
  `ImageDrawLine` takes ints, not Vector2s.)

## Conventions

- If the user sends any message after a program was started, assume they exited
  it — check with `pgrep` instead of assuming it is still running. If a fresh
  copy must run and the old one is alive, kill it silently first; never ask and
  never offer to kill it.

## Things not to "simplify"

- **The row table.** Do not replace `ROW_Y` with a computed uniform grid (or
  `ATLAS_PX/16`) because "atlases are square grids". This one is not, and the
  failure is silent: rows 13-16 draw the wrong/partial sprites. Same for
  `COL_X`'s rounding.
- **Center-only drawing.** The avatar is always drawn at screen center at
  native resolution, one frame at a time; the world scrolls beneath it in
  whole-tile steps. Don't scale the sprite to the window, and don't add a
  sub-tile camera until there's a reason to.
- **The direction table.** `DIR_NAME`/`KP_DIR` encode the row reading above;
  don't re-derive it from pixels or "fix" the odd clockwise order, and don't
  turn the animation rows back into a free-form 16x16 cursor.
