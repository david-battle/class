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
  at 1:1 pixels in the center of the screen; arrow keys move a clamped
  (col,row) cursor through the 256 frames, so the source rectangle can never
  leave the atlas. When a world exists the player stays centered and the world
  moves around them — keep the sprite drawn at screen center.
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
- **Center-only drawing.** The sprite is always drawn at screen center at
  native resolution, one frame at a time. Don't scale it to the window, and
  don't add camera/position state until there is a world to scroll.
- **Clamped cursor.** Arrow keys clamp at the atlas edges rather than wrapping;
  the source rectangle must never read outside the texture.
