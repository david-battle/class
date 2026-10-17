// "class" - a roguelike where you pick your class (by analogy to rogue).
// Skeleton: draw one frame of the hooded-rogue atlas, centered, at native
// resolution, inside a lazily generated maze. The keypad steps the eight
// facings one tile at a time; the avatar stays in the middle and the world
// moves around it. ENTER opens the door being faced.
#include "raylib.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

// Screen-space step vector per facing (y grows downward, so S is +y).
static const int DIR_DX[8] = { 0,-1,-1,-1, 0, 1, 1, 1 };
static const int DIR_DY[8] = { 1, 1, 0,-1,-1,-1, 0, 1 };

// ---------------------------------------------------------------- world ----
// Model-A maze lattice: even-even tiles are rooms, odd-even/even-odd tiles
// are corridors (sometimes doors), odd-odd is always solid. The maze is
// grown lazily from a stack of pending rooms, biased toward the avatar, so
// what is carved stays a connected perfect maze as you explore.
#define TILE     64
#define MAZE_GROWTH_BIAS 6 // out of 10: chance a carve heads toward the avatar

enum { T_FLOOR = 1, T_DOOR = 2, T_THRESHOLD = 3 };

typedef struct { uint64_t key; uint8_t tile; } TileSlot;
static TileSlot *gTiles;              // open-addressed hash; absent = wall
static int gTilesCap, gTilesCount;
static uint64_t *gStack;              // rooms carved but not fully expanded
static int gStackN, gStackCap;

static uint64_t PackXY(int x, int y) {
    return ((uint64_t)(uint32_t)x << 32) | (uint32_t)y;
}
static int UnpackX(uint64_t k) { return (int)(int32_t)(k >> 32); }
static int UnpackY(uint64_t k) { return (int)(int32_t)k; }

static uint64_t Mix64(uint64_t z) { // splitmix64 finalizer
    z += 0x9e3779b97f4a7c15ULL;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

static uint8_t TileGet(int x, int y) {
    if (!gTilesCap) return 0;
    uint64_t k = PackXY(x, y);
    size_t i = (size_t)Mix64(k) & (size_t)(gTilesCap - 1);
    while (gTiles[i].tile) { // tile 0 marks an empty slot; (0,0) is a floor
        if (gTiles[i].key == k) return gTiles[i].tile;
        i = (i + 1) & (size_t)(gTilesCap - 1);
    }
    return 0;
}

static void TileSetRaw(uint64_t k, uint8_t t) {
    if (!gTilesCap) { // first carve: open-addressed table needs a power of two
        gTilesCap = 256;
        gTiles = calloc((size_t)gTilesCap, sizeof *gTiles);
    }
    size_t i = (size_t)Mix64(k) & (size_t)(gTilesCap - 1);
    while (gTiles[i].tile) {
        if (gTiles[i].key == k) { gTiles[i].tile = t; return; }
        i = (i + 1) & (size_t)(gTilesCap - 1);
    }
    gTiles[i].key = k;
    gTiles[i].tile = t;
    gTilesCount++;
    if (gTilesCount * 10 >= gTilesCap * 7) { // grow: rehash everything
        int newCap = gTilesCap * 2;
        TileSlot *ns = calloc((size_t)newCap, sizeof *ns);
        for (int j = 0; j < gTilesCap; j++) if (gTiles[j].tile) {
            size_t n = (size_t)Mix64(gTiles[j].key) & (size_t)(newCap - 1);
            while (ns[n].tile) n = (n + 1) & (size_t)(newCap - 1);
            ns[n] = gTiles[j];
        }
        free(gTiles);
        gTiles = ns;
        gTilesCap = newCap;
    }
}
static void TileSet(int x, int y, uint8_t t) { TileSetRaw(PackXY(x, y), t); }

static void StackPush(uint64_t k) {
    if (gStackN == gStackCap) {
        gStackCap = gStackCap ? gStackCap * 2 : 256;
        gStack = realloc(gStack, (size_t)gStackCap * sizeof *gStack);
    }
    gStack[gStackN++] = k;
}

static const int MDX[4] = { 0, 1, 0,-1 }; // maze growth: N E S W
static const int MDY[4] = {-1, 0, 1, 0 };

// Carve one room's worth of maze (one passage to a random unvisited
// neighbour, biased toward the avatar), or backtrack if none remain.
static void GrowMaze(int px, int py) {
    if (gStackN == 0) { TileSet(0, 0, T_FLOOR); StackPush(PackXY(0, 0)); }

    int best = 0, bd = 1 << 30; // expand the pending room nearest the avatar
    for (int i = 0; i < gStackN; i++) {
        int dx = UnpackX(gStack[i]) - px, dy = UnpackY(gStack[i]) - py;
        int d = dx * dx + dy * dy;
        if (d < bd) { bd = d; best = i; }
    }
    int cx = UnpackX(gStack[best]), cy = UnpackY(gStack[best]);

    int order[4] = { 0, 1, 2, 3 };
    for (int i = 3; i > 0; i--) {
        int j = rand() % (i + 1), t = order[i];
        order[i] = order[j];
        order[j] = t;
    }
    int unv[4], nu = 0;
    for (int i = 0; i < 4; i++)
        if (!TileGet(cx + 2 * MDX[order[i]], cy + 2 * MDY[order[i]]))
            unv[nu++] = order[i];
    if (nu == 0) { gStack[best] = gStack[--gStackN]; return; } // backtrack

    int pick = unv[rand() % nu];
    if (nu > 1 && rand() % 10 < MAZE_GROWTH_BIAS) {
        int td = 1 << 30;
        for (int i = 0; i < nu; i++) {
            int dx = cx + 2 * MDX[unv[i]] - px, dy = cy + 2 * MDY[unv[i]] - py;
            int d = dx * dx + dy * dy;
            if (d < td) { td = d; pick = unv[i]; }
        }
    }
    int mid = rand() % 6 == 0 ? T_DOOR : T_FLOOR; // corridor between rooms
    TileSet(cx + MDX[pick], cy + MDY[pick], (uint8_t)mid);
    TileSet(cx + 2 * MDX[pick], cy + 2 * MDY[pick], T_FLOOR);
    StackPush(PackXY(cx + 2 * MDX[pick], cy + 2 * MDY[pick]));
}

// Carve until every room within Chebyshev distance R of the avatar exists.
static void EnsureWorld(int px, int py, int R) {
    for (int n = 0; n < 3000; n++) {
        int ready = 1;
        for (int y = py - R; y <= py + R && ready; y++)
            for (int x = px - R; x <= px + R; x++)
                if (((x | y) & 1) == 0 && !TileGet(x, y)) { ready = 0; break; }
        if (ready) return;
        GrowMaze(px, py);
    }
}

static int Walkable(int x, int y) {
    uint8_t t = TileGet(x, y);
    return t == T_FLOOR || t == T_THRESHOLD; // closed doors block
}

// Procedural tile art: no asset files, just a little pixel scribbling.
static Texture2D FinishTile(Image *im) {
    Texture2D t = LoadTextureFromImage(*im);
    UnloadImage(*im);
    return t;
}
static Texture2D MakeFloorTex(void) { // light wood planks
    Image im = GenImageColor(TILE, TILE, (Color){205,170,120,255});
    Color seam = (Color){162,126,80,255};
    for (int y = 0; y < TILE; y += 16)
        ImageDrawLine(&im, 0, y, TILE - 1, y, seam);
    for (int y = 0, row = 0; y < TILE; y += 16, row++) {
        int jx = (row & 1) ? 22 : 48; // staggered plank ends
        ImageDrawLine(&im, jx, y, jx, y + 15, seam);
    }
    for (int i = 0; i < TILE * 3; i++)
        ImageDrawPixel(&im, rand() % TILE, rand() % TILE,
                       (Color){188,153,104,255}); // grain speckle
    return FinishTile(&im);
}
static Texture2D MakeWallTex(void) { // rough stone bricks
    Image im = GenImageColor(TILE, TILE, (Color){112,112,120,255});
    Color mortar = (Color){74,74,80,255};
    for (int y = 0; y < TILE; y += 16) {
        ImageDrawLine(&im, 0, y, TILE - 1, y, mortar);
        for (int x = ((y / 16) & 1) ? 0 : 16; x < TILE; x += 32)
            ImageDrawLine(&im, x, y, x, y + 15, mortar);
    }
    for (int i = 0; i < TILE * 4; i++) {
        int v = rand() % 36 - 18;
        ImageDrawPixel(&im, rand() % TILE, rand() % TILE,
                       (Color){112 + v, 112 + v, 120 + v, 255});
    }
    return FinishTile(&im);
}
static Texture2D MakeDoorTex(void) { // dark wood, planked, with a knob
    Image im = GenImageColor(TILE, TILE, (Color){86,52,30,255});
    Color plank = (Color){58,34,18,255};
    for (int x = 0; x < TILE; x += 12)
        ImageDrawLine(&im, x, 0, x, TILE - 1, plank);
    ImageDrawRectangleLines(&im, 1, 1, TILE - 2, TILE - 2, plank);
    ImageDrawRectangleLines(&im, 2, 2, TILE - 4, TILE - 4, plank);
    ImageDrawCircle(&im, TILE - 14, TILE / 2, 2, (Color){186,164,96,255});
    return FinishTile(&im);
}
static Texture2D MakeThreshTex(void) { // stone threshold for open doors
    Image im = GenImageColor(TILE, TILE, (Color){130,128,134,255});
    ImageDrawRectangleLines(&im, 2, 2, TILE - 4, TILE - 4, (Color){92,90,96,255});
    ImageDrawRectangleLines(&im, 3, 3, TILE - 6, TILE - 6, (Color){92,90,96,255});
    ImageDrawLine(&im, 3, TILE / 2, TILE - 4, TILE / 2, (Color){104,102,108,255});
    return FinishTile(&im);
}

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
    // stepping); idleCol is the standing cycle; (px,py) is the avatar's world
    // tile - the avatar never moves on screen, the world scrolls beneath it.
    int dir = 0, stepping = 0, col = 0, idleCol = 0;
    int px = 0, py = 0;
    srand((unsigned)time(NULL));
    Texture2D texFloor = MakeFloorTex(), texWall = MakeWallTex();
    Texture2D texDoor = MakeDoorTex(), texThresh = MakeThreshTex();

    while (!WindowShouldClose()) {
        // A direction press steps one tile (blocked by walls and closed
        // doors) and shows that facing's step pose; the next press of the
        // same key shows a standing frame, then step again. 0 stops into
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
            int tx = px + DIR_DX[d], ty = py + DIR_DY[d];
            int open = Walkable(tx, ty) &&
                       (DIR_DX[d] == 0 || DIR_DY[d] == 0 || // no corner cutting
                        (Walkable(px + DIR_DX[d], py) &&
                         Walkable(px, py + DIR_DY[d])));
            if (open) { px = tx; py = ty; }
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
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
            int fx = px + DIR_DX[dir], fy = py + DIR_DY[dir];
            if (TileGet(fx, fy) == T_DOOR) TileSet(fx, fy, T_THRESHOLD);
        }

        int row = stepping ? dir + 8 : dir;

        // Grow the maze around the avatar to comfortably past the view.
        int rw = GetRenderWidth() / TILE / 2 + 2;
        int rh = GetRenderHeight() / TILE / 2 + 2;
        EnsureWorld(px, py, rw > rh ? rw : rh);

        BeginDrawing();
            ClearBackground((Color){18,18,24,255});

            int sw = GetRenderWidth(), sh = GetRenderHeight();
            int cx0 = sw / 2 - TILE / 2, cy0 = sh / 2 - TILE / 2;
            int cols = sw / TILE / 2 + 2, rows = sh / TILE / 2 + 2;
            for (int wy = py - rows; wy <= py + rows; wy++) {
                for (int wx = px - cols; wx <= px + cols; wx++) {
                    const Texture2D *tex = &texWall;
                    uint8_t t = TileGet(wx, wy);
                    if (t == T_FLOOR) tex = &texFloor;
                    else if (t == T_DOOR) tex = &texDoor;
                    else if (t == T_THRESHOLD) tex = &texThresh;
                    DrawTexture(*tex, cx0 + (wx - px) * TILE,
                                cy0 + (wy - py) * TILE, WHITE);
                }
            }

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
            DrawText("KEYPAD move   ENTER open door   0 stop + cycle idle   ARROWS cardinals   ESC quits",
                     16, 16 + fs * 2, fs, (Color){120,120,145,255});
        EndDrawing();
    }

    UnloadTexture(texFloor); UnloadTexture(texWall);
    UnloadTexture(texDoor);  UnloadTexture(texThresh);
    UnloadTexture(atlas);
    free(gTiles);
    free(gStack);
    CloseWindow();
    return 0;
}
