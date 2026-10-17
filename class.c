// "class" - a roguelike where you pick your class (by analogy to rogue).
// Skeleton: draw one frame of the hooded-rogue atlas, centered, at native
// resolution. The keypad walks the eight facings: each direction key
// alternates that facing's standing frame with its step frame, 0 stops. The
// player never moves; the world will later move around us.
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

// Atlas semantics, read off the art (skin/hand pixels sit left of centre in
// rows 1-3 and right in 6-7; the face disappears in rows 3-5): rows 0-7 are
// the eight idle facings clockwise from south - S, SW, W, NW, N, NE, E, SE -
// and rows 8-15 are the matching "step" poses (facing + 8). An idle row's 16
// columns are a standing animation cycle; a walk row's columns are all the
// SAME mid-step pose with idle-animation jitter (verified: frame IoU vs
// col 0 is 0.85-1.00 within every walk row), so walking is faked by
// alternating the standing frames with the step pose - see the input code.
#define NC 16
static const char *DIR_NAME[8] = { "S", "SW", "W", "NW", "N", "NE", "E", "SE" };

// Keypad digit -> direction index, keypad layout 7 8 9 / 4 6 / 1 2 3:
// 8=N, 9=NE, 6=E, 3=SE, 2=S, 1=SW, 4=W, 7=NW. 0 (stop) and 5 (centre) are -1.
static const int KP_DIR[10] = { -1, 1, 0, 7, 2, -1, 6, 3, 4, 5 };

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

    // dir = facing 0..7 (DIR_NAME, atlas row dir when standing, dir+8 when
    // stepping); idleCol is the standing cycle.
    int dir = 0, stepping = 0, col = 0, idleCol = 0;

    while (!WindowShouldClose()) {
        // Walking is an alternation: a direction press shows that facing's
        // step pose (walk row, column 0), the next press of the same key
        // shows a standing frame (idle row), then step again. 0 stops into
        // the standing row and, while standing, cycles its frames.
        // GLFW resolves keypad keys by physical scancode, so KEY_KP_*
        // fires with NumLock on or off alike - no numlock handling needed.
        int d = -1;
        for (int k = 1; k <= 9; k++)
            if (IsKeyPressed(KEY_KP_0 + k)) d = KP_DIR[k];
        if (d < 0) { // arrows: the four cardinals, for keyboards without a pad
            if (IsKeyPressed(KEY_UP))    d = 4;
            if (IsKeyPressed(KEY_RIGHT)) d = 6;
            if (IsKeyPressed(KEY_DOWN))  d = 0;
            if (IsKeyPressed(KEY_LEFT))  d = 2;
        }
        if (IsKeyPressed(KEY_KP_0)) { // stop, or cycle the idle row while stopped
            if (stepping) { stepping = 0; col = idleCol; }
            else { idleCol = (idleCol + 1) % NC; col = idleCol; }
        } else if (d >= 0) {
            if (d == dir && stepping) { // already stepping: take a stand break
                stepping = 0;
                col = idleCol;
                idleCol = (idleCol + 1) % NC;
            } else {                    // new facing, or re-step after standing
                stepping = 1;
                dir = d;
                col = 0;                // the step pose sits in column 0
            }
        }

        int row = stepping ? dir + 8 : dir;

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
            DrawText(TextFormat("FRAME %d   facing %s   %s %d/%d",
                                row * ATLAS_COLS + col + 1, DIR_NAME[dir],
                                stepping ? "STEP" : "IDLE", col + 1, NC),
                     16, 16, fs, (Color){180,180,200,255});
            DrawText("KEYPAD 7-9/4 6/1-3 step/stand   0 stop + cycle idle   ARROWS cardinals   ESC quits",
                     16, 16 + fs * 2, fs, (Color){120,120,145,255});
        EndDrawing();
    }

    UnloadTexture(atlas);
    CloseWindow();
    return 0;
}
