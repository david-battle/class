// "class" - a roguelike where you pick your class (by analogy to rogue).
// Skeleton: draw one frame of the hooded-rogue atlas, centered, at native
// resolution. Arrow keys step through the 256 frames; the player never moves,
// the world will later move around us.
#include "raylib.h"
#include <string.h>

#define ATLAS_FILE "Hooded_Rogue_256_Frame_Sprite_Atlas.png"

// The atlas is 1254x1254 RGBA, 256 frames in 16 columns x 16 rows.
//
// Columns are a uniform 1254/16 = 78.375 px pitch: every column gap in the
// file (66-91, 144-169, ... 1163-1189) straddles k*1254/16.
//
// Rows are NOT uniform, so there is no single cell height. Frames 1-11 sit on
// a ~78 px pitch with ~9 px of padding; the bottom four rows are packed at
// ~67-72 px with only ~3-6 px of padding, so a uniform grid would drift ~30 px
// by row 16 and slice those frames straight across two sprites. ROW_Y holds
// the midpoints of the transparent gaps between rows, measured from the file;
// every frame's pixels sit strictly inside its cell (verified for all 256).
static const int COL_X[17] = {
    0, 78, 157, 235, 314, 392, 470, 549, 627,
    705, 784, 862, 940, 1019, 1097, 1176, 1254
};
static const int ROW_Y[17] = {
    8, 85, 162, 240, 318, 396, 475, 554, 631,
    712, 790, 866, 938, 1006, 1072, 1144, 1224
};
#define ATLAS_COLS 16
#define ATLAS_ROWS 16
#define ATLAS_PX   1254

// Pick the external monitor. On WSLg the built-in laptop panel and the
// external monitor are both named "rdp-N", so names alone can't tell them
// apart, but physical size can: the laptop panel is the smaller one. Native
// Linux built-ins (eDP/LVDS/...) are excluded by name.
static int SelectExternalMonitor(void) {
    int count = GetMonitorCount();
    if (count <= 1) return 0;

    int best = -1;
    long bestArea = -1;
    for (int i = 0; i < count; i++) {
        const char *name = GetMonitorName(i);
        if (name && (strstr(name, "eDP") || strstr(name, "LVDS") ||
                     strstr(name, "DSI") || strstr(name, "Built") ||
                     strstr(name, "Laptop") || strstr(name, "Internal"))) {
            continue;
        }
        int pw = GetMonitorPhysicalWidth(i);
        int ph = GetMonitorPhysicalHeight(i);
        long area = (pw > 0 && ph > 0) ? (long)pw * ph
                                       : (long)GetMonitorWidth(i) * GetMonitorHeight(i);
        if (area > bestArea) { bestArea = area; best = i; }
    }
    return (best < 0) ? 0 : best;
}

int main(void) {
    // WSLg's RAIL shell ignores SetWindowMonitor()/FLAG_FULLSCREEN_MODE and
    // pins fullscreen X11 windows to the main screen, so cover the chosen
    // monitor with an undecorated window (borderless-windowed fullscreen)
    // instead. Monitor geometry is only valid after InitWindow().
    SetConfigFlags(FLAG_WINDOW_UNDECORATED | FLAG_VSYNC_HINT);
    InitWindow(640, 480, "class");

    int ext = SelectExternalMonitor();
    Vector2 mp = GetMonitorPosition(ext);
    SetWindowSize(GetMonitorWidth(ext), GetMonitorHeight(ext));
    SetWindowPosition((int)mp.x, (int)mp.y);

    SetTargetFPS(60);

    Texture2D atlas = LoadTexture(ATLAS_FILE);
    if (atlas.id == 0) {
        // Wrong/missing file: the COL_X/ROW_Y tables would read garbage.
        TraceLog(LOG_ERROR, "failed to load %s (run from the repo root)", ATLAS_FILE);
        CloseWindow();
        return 1;
    }
    if (atlas.width != ATLAS_PX || atlas.height != ATLAS_PX)
        TraceLog(LOG_WARNING, "atlas is %dx%d, expected %dx%d - frame table may be stale",
                 atlas.width, atlas.height, ATLAS_PX, ATLAS_PX);

    int col = 0, row = 0;              // start on the first sprite

    while (!WindowShouldClose()) {
        // Arrow keys select a sprite. Each axis is clamped at both ends so
        // the source rectangle can never leave the atlas.
        if (IsKeyPressed(KEY_RIGHT) && col < ATLAS_COLS - 1) col++;
        if (IsKeyPressed(KEY_LEFT)  && col > 0)              col--;
        if (IsKeyPressed(KEY_DOWN)  && row < ATLAS_ROWS - 1) row++;
        if (IsKeyPressed(KEY_UP)    && row > 0)              row--;

        BeginDrawing();
            ClearBackground((Color){18,18,24,255});

            int sw = GetRenderWidth(), sh = GetRenderHeight();
            Rectangle src = {
                COL_X[col], ROW_Y[row],
                COL_X[col + 1] - COL_X[col],
                ROW_Y[row + 1] - ROW_Y[row]
            };
            // Always the middle of the screen, one sprite, one-to-one pixels.
            DrawTextureRec(atlas, src,
                           (Vector2){ (sw - (int)src.width) / 2.0f,
                                      (sh - (int)src.height) / 2.0f },
                           WHITE);

            int fs = sh / 50;
            if (fs < 14) fs = 14;
            DrawText(TextFormat("FRAME %d   col %d row %d / %d",
                                row * ATLAS_COLS + col + 1, col + 1, row + 1,
                                ATLAS_COLS * ATLAS_ROWS),
                     16, 16, fs, (Color){180,180,200,255});
            DrawText("ARROW KEYS select a sprite   ESC quits",
                     16, 16 + fs * 2, fs, (Color){120,120,145,255});
        EndDrawing();
    }

    UnloadTexture(atlas);
    CloseWindow();
    return 0;
}
