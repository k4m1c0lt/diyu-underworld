/* =========================================================================
 *  大闹阴曹 · 归山  (A Little Monkey's Escape from the Chinese Underworld)
 *  一个用纯 C 写的轻度肉鸽 / 实时地牢动作游戏
 *
 *  编译:  MSVC : cl /nologo /O2 /utf-8 /D_CRT_SECURE_NO_WARNINGS diyu.c
 *         MinGW: gcc -O2 -std=c99 diyu.c -o diyu.exe
 *
 *  运行:  diyu.exe            正常游玩（建议 Windows Terminal，UTF-8）
 *         diyu.exe --sim 20000  无界面机器人自检（开发用）
 * ========================================================================= */
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifdef _WIN32
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "kernel32.lib")
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/time.h>
#include <ctype.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define W 58                 /* 地图总宽（含边墙） */
#define H 16                 /* 地图总高（含边墙） */
#define FRAME_LINES 24
#define FRAME_COLS 78
#define MAX_ENEMIES 26
#define MAX_BULLETS 90
#define MAX_ORBS 140
#define MAX_FX 80
#define TICK_DT 0.033

static void end_run(int win);
static void next_room(void);

/* ------------------------------ 枚举 ------------------------------------ */
enum { W_STAFF = 0, W_SPEAR, W_BLADE, W_SWORD };
enum { E_GHOST = 0, E_SOLDIER, E_RANGED, E_ELITE, E_BOSS };
enum { ST_TITLE = 0, ST_STORY, ST_PLAY, ST_BOON, ST_SHOP, ST_PAUSE,
       ST_DEAD, ST_WIN, ST_HELP, ST_META };
enum { K_UP=0,K_DOWN,K_LEFT,K_RIGHT,K_ATK,K_DASH,K_POT,K_E,
       K_1,K_2,K_3,K_4,K_5,K_6,K_7,K_8,K_ENTER,K_ESC,K_P,K_H,K_X,K_COUNT };

/* ------------------------------ 数据表 ---------------------------------- */
typedef struct {
    const char *name, *desc;
    int cost;
    double dmg, range, arc, width, cd, kb;
    int col;
} WeaponDef;

static const WeaponDef WD[4] = {
    {"齐眉棍", "横扫八方 · 攻守兼备",   0,  24, 3.0, 1.15, 1.8, 0.40, 0.8, 93},
    {"追魂枪", "一寸长一寸强 · 击退",  45,  30, 5.4, 0.75, 0.9, 0.54, 1.8, 96},
    {"断业刀", "重若千钧 · 大范围",   100,  48, 3.1, 1.45, 2.1, 0.85, 1.6, 91},
    {"青冥剑", "轻灵迅捷 · 连击如风",  70,  17, 2.5, 1.00, 1.2, 0.22, 0.4, 97},
};

static const char *STAGE_NAME[4] = {"黄泉路", "奈何桥", "枉死城", "阎罗殿"};
static const int   STAGE_CHAMBERS[4] = {3, 3, 3, 1};
static const char *STAGE_SUB[4] = {
    "彼岸花开，孤魂拦路",
    "忘川水寒，桥上无退路",
    "冤魂不散，鬼差列阵",
    "阎罗升殿，生死簿上无名"
};
static const char *BOON_NAME[8] = {
    "判官朱印", "风火轮", "还魂草", "阴阳眼",
    "摄魂诀", "缩地符", "铁布衫", "孟婆慈悲"
};
static const char *BOON_DESC[8] = {
    "攻击伤害 +18%",
    "移动速度 +12%，冲刺冷却 -15%",
    "生命上限 +25，立即回复 25",
    "暴击率 +12%",
    "灵魂获取 +30%，吸血 +4%",
    "攻击间隔 -12%",
    "受到的伤害 -12%",
    "回复 45 生命，并获得 1 枚回魂丹"
};

/* ------------------------------ 结构体 ---------------------------------- */
typedef struct {
    double x, y, hp, maxhp, facing;
    double atkCd, dashCd, dashTime, dashVx, dashVy, invuln;
    double dmgMul, speedMul, cdr, crit, lifesteal, soulMul, dmgTaken, dashMul;
    int weapon, unlocked[4], wlevel[4], potions, maxPotions, souls, blessings;
} Player;

typedef struct {
    int type;
    double x, y, vx, vy, hp, maxhp;
    int dmg, worth;
    double timer, atkCd, hitFlash;
    int state, phase;
} Enemy;

typedef struct { double x, y, vx, vy, life; int dmg, ch, col; } Bullet;
typedef struct { double x, y, vx, vy, life; int val; } Orb;
typedef struct { double x, y, life; int col; char txt[8]; } Fx;
typedef struct { int yuanhun, hpLv, atkLv, soulLv, potLv, runs, best; } Save;

typedef struct {
    int state, stage, chamber, wave, waveTotal, roomClear;
    int shopMode, boonChoices[3], lastGain;
    int nEnemies, nBullets, nOrbs, nFx, kills;
    int earnedSouls;
    double time, msgTimer, waveTimer;
    int msgCol;
    char msg[160];
    Player p;
    Save save;
    Enemy enemies[MAX_ENEMIES];
    Bullet bullets[MAX_BULLETS];
    Orb orbs[MAX_ORBS];
    Fx fx[MAX_FX];
    struct { double life, ang; int weapon; } swing;
    char tile[H][W];
} Game;

static Game G;
static char gTile[H][W];
static char gGlyph[H][W];
static int  gCol[H][W];
static unsigned char gKey[K_COUNT], gPrev[K_COUNT];
static double gHold[K_COUNT];
static int gQuit = 0;
static int gSimStage = -1;
static int gSimMode = 0;
static int gTrace = 0;
static int gGodMode = 0;
static int gWinCount = 0;
static int gSimRender = 0;

/* --------------------------- 平台 / 输入 -------------------------------- */
static double now_sec(void) {
#ifdef _WIN32
    static LARGE_INTEGER freq;
    LARGE_INTEGER t;
    if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart / (double)freq.QuadPart;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
#endif
}

#ifndef _WIN32
static struct termios gOldTerm;
static int gTermRaw = 0;
#endif

static void plat_init(void) {
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD m = 0;
    SetConsoleOutputCP(CP_UTF8);
    if (GetConsoleMode(h, &m))
        SetConsoleMode(h, m | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    _setmode(_fileno(stdout), _O_BINARY);
    /* 尝试调整控制台尺寸；失败也不影响（默认 80x25 亦容纳得下 24 行） */
    {
        COORD c; SMALL_RECT r;
        c.X = 100; c.Y = 34;
        if (SetConsoleScreenBufferSize(h, c)) {
            r.Left = 0; r.Top = 0; r.Right = 99; r.Bottom = 33;
            SetConsoleWindowInfo(h, TRUE, &r);
        }
    }
#else
    struct termios t;
    tcgetattr(0, &gOldTerm);
    t = gOldTerm;
    t.c_lflag &= ~(ICANON | ECHO);
    t.c_cc[VMIN] = 0; t.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &t);
    gTermRaw = 1;
#endif
    fputs("\033[?25l", stdout);   /* 隐藏光标 */
    fflush(stdout);
}

static void plat_shutdown(void) {
    fputs("\033[?25h\033[0m", stdout);
    fflush(stdout);
#ifndef _WIN32
    if (gTermRaw) tcsetattr(0, TCSANOW, &gOldTerm);
#endif
}

static void plat_sleep_ms(int ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

static void plat_poll(void) {
    memcpy(gPrev, gKey, sizeof(gKey));
#ifdef _WIN32
    gKey[K_UP]    = (GetAsyncKeyState('W') & 0x8000) || (GetAsyncKeyState(VK_UP) & 0x8000);
    gKey[K_DOWN]  = (GetAsyncKeyState('S') & 0x8000) || (GetAsyncKeyState(VK_DOWN) & 0x8000);
    gKey[K_LEFT]  = (GetAsyncKeyState('A') & 0x8000) || (GetAsyncKeyState(VK_LEFT) & 0x8000);
    gKey[K_RIGHT] = (GetAsyncKeyState('D') & 0x8000) || (GetAsyncKeyState(VK_RIGHT) & 0x8000);
    gKey[K_ATK]   = (GetAsyncKeyState('J') & 0x8000) || (GetAsyncKeyState(VK_SPACE) & 0x8000);
    gKey[K_DASH]  = (GetAsyncKeyState('K') & 0x8000) || (GetAsyncKeyState(VK_SHIFT) & 0x8000);
    gKey[K_POT]   = (GetAsyncKeyState('Q') & 0x8000) != 0;
    gKey[K_E]     = (GetAsyncKeyState('E') & 0x8000) != 0;
    gKey[K_1]     = (GetAsyncKeyState('1') & 0x8000) != 0;
    gKey[K_2]     = (GetAsyncKeyState('2') & 0x8000) != 0;
    gKey[K_3]     = (GetAsyncKeyState('3') & 0x8000) != 0;
    gKey[K_4]     = (GetAsyncKeyState('4') & 0x8000) != 0;
    gKey[K_5]     = (GetAsyncKeyState('5') & 0x8000) != 0;
    gKey[K_6]     = (GetAsyncKeyState('6') & 0x8000) != 0;
    gKey[K_7]     = (GetAsyncKeyState('7') & 0x8000) != 0;
    gKey[K_8]     = (GetAsyncKeyState('8') & 0x8000) != 0;
    gKey[K_ENTER] = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;
    gKey[K_ESC]   = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
    gKey[K_P]     = (GetAsyncKeyState('P') & 0x8000) != 0;
    gKey[K_H]     = (GetAsyncKeyState('H') & 0x8000) != 0;
    gKey[K_X]     = (GetAsyncKeyState('X') & 0x8000) != 0;
#else
    {
        double now = now_sec();
        unsigned char buf[64];
        int i, n, k;
        for (k = 0; k < K_COUNT; k++)
            if (gKey[k] && now > gHold[k]) gKey[k] = 0;
        n = (int)read(0, buf, sizeof(buf));
        for (i = 0; i < n; i++) {
            unsigned char c = buf[i];
            k = -1;
            if (c == 27) {
                if (i + 1 < n && buf[i + 1] == '[') {
                    i += 2;
                    if (i < n) {
                        switch (buf[i]) {
                        case 'A': k = K_UP; break;
                        case 'B': k = K_DOWN; break;
                        case 'C': k = K_RIGHT; break;
                        case 'D': k = K_LEFT; break;
                        }
                    }
                } else k = K_ESC;
            } else {
                c = (unsigned char)tolower(c);
                switch (c) {
                case 'w': k = K_UP; break;
                case 's': k = K_DOWN; break;
                case 'a': k = K_LEFT; break;
                case 'd': k = K_RIGHT; break;
                case 'j': case ' ': k = K_ATK; break;
                case 'k': k = K_DASH; break;
                case 'q': k = K_POT; break;
                case 'e': k = K_E; break;
                case '1': k = K_1; break;
                case '2': k = K_2; break;
                case '3': k = K_3; break;
                case '4': k = K_4; break;
                case '5': k = K_5; break;
                case '6': k = K_6; break;
                case '7': k = K_7; break;
                case '8': k = K_8; break;
                case '\r': case '\n': k = K_ENTER; break;
                case 'p': k = K_P; break;
                case 'h': k = K_H; break;
                case 'x': k = K_X; break;
                }
            }
            if (k >= 0) { gKey[k] = 1; gHold[k] = now + 0.13; }
        }
    }
#endif
}

static int held(int k) { return gKey[k] != 0; }
static int pressed(int k) { return gKey[k] && !gPrev[k]; }

/* --------------------------- 帧缓冲 / UI -------------------------------- */
static char gFB[1 << 18];
static size_t gFBLen;
static int gLines;

static void fb_add(const char *fmt, ...) {
    va_list ap;
    int n, room;
    if (gFBLen >= sizeof(gFB) - 1024) return;
    room = (int)(sizeof(gFB) - gFBLen);
    va_start(ap, fmt);
    n = vsnprintf(gFB + gFBLen, (size_t)room, fmt, ap);
    va_end(ap);
    if (n > 0) {
        if (n >= room) n = room - 1;
        gFBLen += (size_t)n;
    }
}

static void fb_start(void) {
    gFBLen = 0; gLines = 0;
    fb_add("\033[H\033[40m\033[97m\033[22m");
}

static void fb_line(void) { fb_add("\033[K\r\n"); gLines++; }

static void fb_finish(void) {
    while (gLines < FRAME_LINES) { fb_add("\033[K\r\n"); gLines++; }
    fwrite(gFB, 1, gFBLen, stdout);
    fflush(stdout);
}

static int text_width(const char *s) {
    int w = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        unsigned char c = *p;
        int n = c < 0x80 ? 1 : (c < 0xE0 ? 2 : (c < 0xF0 ? 3 : 4));
        w += (n >= 3) ? 2 : 1;
        p += n;
    }
    return w;
}

static void ui_center(const char *s, int col) {
    int i, w = text_width(s);
    int pad = (FRAME_COLS - w) / 2;
    if (pad < 0) pad = 0;
    fb_add("\033[%dm", col);
    for (i = 0; i < pad; i++) fb_add(" ");
    fb_add("%s", s);
    fb_line();
}

static void ui_left(const char *s, int col, int pad) {
    int i;
    fb_add("\033[%dm", col);
    for (i = 0; i < pad; i++) fb_add(" ");
    fb_add("%s", s);
    fb_line();
}

static void ui_blank(void) { fb_add("\033[K"); fb_line(); }

static void ui_rule(void) {
    char line[100];
    int i;
    for (i = 0; i < 68; i++) line[i] = '=';
    line[68] = 0;
    ui_center(line, 90);
}

/* ------------------------------ 工具 ------------------------------------ */
static double dist2(double x, double y) { return sqrt(x * x + y * y); }

static double angdiff(double a, double b) {
    double d = a - b;
    while (d > M_PI) d -= 2 * M_PI;
    while (d < -M_PI) d += 2 * M_PI;
    return d;
}

static void add_msg(int col, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(G.msg, sizeof(G.msg), fmt, ap);
    va_end(ap);
    G.msgCol = col;
    G.msgTimer = 3.2;
}

static void save_path(char *buf, int n) {
#ifdef _WIN32
    char exe[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, exe, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        char *slash = strrchr(exe, '\\');
        if (slash) {
            *slash = 0;
            snprintf(buf, (size_t)n, "%s\\difu_save.dat", exe);
            return;
        }
    }
#endif
    snprintf(buf, (size_t)n, "difu_save.dat");
}

static void save_load(void) {
    char path[512];
    FILE *f;
    memset(&G.save, 0, sizeof(G.save));
    save_path(path, sizeof(path));
    f = fopen(path, "r");
    if (f) {
        if (fscanf(f, "%d %d %d %d %d %d %d", &G.save.yuanhun, &G.save.hpLv,
                   &G.save.atkLv, &G.save.soulLv, &G.save.potLv,
                   &G.save.runs, &G.save.best) != 7)
            memset(&G.save, 0, sizeof(G.save));
        fclose(f);
    }
}

static void save_store(void) {
    char path[512];
    FILE *f;
    save_path(path, sizeof(path));
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "%d %d %d %d %d %d %d\n", G.save.yuanhun, G.save.hpLv,
            G.save.atkLv, G.save.soulLv, G.save.potLv, G.save.runs, G.save.best);
    fclose(f);
}

static int meta_cost(int i) {
    switch (i) {
    case 0: return 20 + G.save.hpLv * 20;
    case 1: return 25 + G.save.atkLv * 25;
    case 2: return 15 + G.save.soulLv * 15;
    default: return 40 + G.save.potLv * 40;
    }
}
static int meta_level(int i) {
    switch (i) {
    case 0: return G.save.hpLv;
    case 1: return G.save.atkLv;
    case 2: return G.save.soulLv;
    default: return G.save.potLv;
    }
}
static int meta_max(int i) { return i == 3 ? 2 : 5; }

/* ------------------------------ 地图 ------------------------------------ */
static int solid_tile(int x, int y) {
    char c;
    if (x < 0 || x >= W || y < 0 || y >= H) return 1;
    c = gTile[y][x];
    return c == '#' || c == '~';
}

static int solid_point(double x, double y) {
    return solid_tile((int)(x + 0.5), (int)(y + 0.5));
}

static int blocked_body(double x, double y, double r) {
    return solid_point(x - r, y - r) || solid_point(x + r, y - r) ||
           solid_point(x - r, y + r) || solid_point(x + r, y + r);
}

/* 分步移动：冲刺速度很快，必须切成小步，否则会一步跨过一格厚的墙 */
static void move_ent(double *x, double *y, double vx, double vy, double dt, double r) {
    double stepX = vx * dt, stepY = vy * dt;
    int steps = (int)(dist2(stepX, stepY) / 0.18) + 1;
    int i;
    if (steps > 12) steps = 12;
    for (i = 0; i < steps; i++) {
        double nx = *x + stepX / steps, ny = *y + stepY / steps;
        if (!blocked_body(nx, *y, r)) *x = nx;
        if (!blocked_body(*x, ny, r)) *y = ny;
    }
}

/* 万一角色已经挤进墙体（例如异常传送），把它吸附到最近的可站格中心 */
static int unstick(double *x, double *y, double r) {
    int cx = (int)(*x + 0.5), cy = (int)(*y + 0.5), dx, dy, ok = 0;
    double bestD = 1e9, bx = *x, by = *y;
    for (dy = -3; dy <= 3; dy++) for (dx = -3; dx <= 3; dx++) {
        int tx = cx + dx, ty = cy + dy;
        double d;
        if (solid_tile(tx, ty)) continue;
        if (blocked_body((double)tx, (double)ty, r)) continue;
        d = dist2((double)tx - *x, (double)ty - *y);
        if (d < bestD) { bestD = d; bx = (double)tx; by = (double)ty; ok = 1; }
    }
    if (ok) { *x = bx; *y = by; }
    return ok;
}

/* 生物专用移动：正面被挡住时沿墙侧滑，仍不行则小幅游走，避免卡在石柱上 */
static void move_creature(double *x, double *y, double vx, double vy, double dt, double r) {
    double ox = *x, oy = *y;
    move_ent(x, y, vx, vy, dt, r);
    if (dist2(*x - ox, *y - oy) > 0.0001) return;
    move_ent(x, y, -vy, vx, dt, r);
    if (dist2(*x - ox, *y - oy) > 0.0001) return;
    move_ent(x, y, vy, -vx, dt, r);
    if (dist2(*x - ox, *y - oy) > 0.0001) return;
    {
        double a = (rand() % 628) / 100.0;
        move_ent(x, y, cos(a) * 4.0, sin(a) * 4.0, dt, r);
    }
    if (dist2(*x - ox, *y - oy) <= 0.0001) unstick(x, y, r);
}

static void cells_clear(void) {
    int y, x;
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++) { gGlyph[y][x] = 0; gCol[y][x] = -1; }
}

static void cell_put(int x, int y, char c, int col) {
    if (x < 0 || x >= W || y < 0 || y >= H) return;
    gGlyph[y][x] = c;
    gCol[y][x] = col;
}

static int tile_color(char c) {
    switch (c) {
    case '#': return 37;
    case '.': return 90;
    case '~': return 94;
    case '=': return 33;
    case 'o': return 95;
    case 'O': return 96;
    case '+': return 91;
    default: return 90;
    }
}

static void put_pillar(int x, int y, int w, int h) {
    int i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            if (x + i > 1 && x + i < W - 2 && y + j > 0 && y + j < H - 1)
                gTile[y + j][x + i] = '#';
}

static void tile_generate(void) {
    int x, y, i;
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++) gTile[y][x] = '.';
    for (x = 0; x < W; x++) { gTile[0][x] = '#'; gTile[H - 1][x] = '#'; }
    for (y = 0; y < H; y++) { gTile[y][0] = '#'; gTile[y][W - 1] = '#'; }
    gTile[0][0] = gTile[0][W - 1] = gTile[H - 1][0] = gTile[H - 1][W - 1] = '+';

    if (G.stage == 1) { /* 奈何桥：上下忘川水，中间一条桥 */
        for (y = 1; y < H - 1; y++)
            for (x = 1; x < W - 1; x++)
                gTile[y][x] = (y < 4 || y > 11) ? '~' : '=';
        put_pillar(16, 5, 2, 1);
        put_pillar(30, 10, 2, 1);
        put_pillar(43, 5, 2, 1);
        for (i = 0; i < 10; i++) {
            x = 2 + rand() % (W - 4); y = 4 + rand() % 8;
            if (gTile[y][x] == '=' && !(x < 8 && y > 6 && y < 10)) gTile[y][x] = 'o';
        }
    } else if (G.stage == 0) { /* 黄泉路：断碑与彼岸花 */
        for (i = 0; i < 7; i++) {
            x = 10 + rand() % (W - 24); y = 2 + rand() % (H - 5);
            put_pillar(x, y, 1 + rand() % 2, 1 + rand() % 2);
        }
        for (i = 0; i < 26; i++) {
            x = 2 + rand() % (W - 4); y = 1 + rand() % (H - 2);
            if (gTile[y][x] == '.') gTile[y][x] = 'o';
        }
    } else if (G.stage == 2) { /* 枉死城：牢笼石柱阵 */
        int xs[4] = {13, 24, 35, 46};
        int ys[3] = {3, 7, 11};
        for (i = 0; i < 4; i++) {
            int j;
            for (j = 0; j < 3; j++) put_pillar(xs[i], ys[j], 2, 2);
        }
        for (i = 0; i < 18; i++) {
            x = 2 + rand() % (W - 4); y = 1 + rand() % (H - 2);
            if (gTile[y][x] == '.') gTile[y][x] = 'o';
        }
    } else { /* 阎罗殿：对称大殿 */
        put_pillar(11, 3, 2, 2);
        put_pillar(11, 11, 2, 2);
        put_pillar(W - 13, 3, 2, 2);
        put_pillar(W - 13, 11, 2, 2);
        gTile[3][W / 2] = '+';
        gTile[H - 4][W / 2] = '+';
        for (i = 0; i < 14; i++) {
            x = 2 + rand() % (W - 4); y = 1 + rand() % (H - 2);
            if (gTile[y][x] == '.') gTile[y][x] = 'o';
        }
    }
    /* 保证出生点、出口与一条贯穿大殿的中轴路畅通 */
    for (y = H / 2 - 1; y <= H / 2 + 1; y++) {
        for (x = 2; x <= 6; x++)
            if (gTile[y][x] == '#') gTile[y][x] = (G.stage == 1) ? '=' : '.';
        for (x = W - 6; x <= W - 2; x++)
            if (gTile[y][x] == '#') gTile[y][x] = (G.stage == 1) ? '=' : '.';
    }
    for (x = 4; x <= W - 3; x++)
        if (gTile[H / 2][x] == '#') gTile[H / 2][x] = (G.stage == 1) ? '=' : '.';

    /* 把出生点走不到的孤立地板封成石壁，杜绝敌人刷进死胡同 */
    {
        static int q[W * H];
        static unsigned char seen[W * H];
        static const int ddx[4] = {1, -1, 0, 0}, ddy[4] = {0, 0, 1, -1};
        int head = 0, tail = 0, start = (H / 2) * W + 4, i;
        memset(seen, 0, sizeof(seen));
        q[tail++] = start; seen[start] = 1;
        while (head < tail) {
            int cur = q[head++], cx = cur % W, cy = cur / W;
            for (i = 0; i < 4; i++) {
                int xx = cx + ddx[i], yy = cy + ddy[i], id;
                if (xx < 0 || xx >= W || yy < 0 || yy >= H) continue;
                id = yy * W + xx;
                if (seen[id] || solid_tile(xx, yy)) continue;
                seen[id] = 1; q[tail++] = id;
            }
        }
        for (y = 1; y < H - 1; y++)
            for (x = 1; x < W - 1; x++)
                if (!seen[y * W + x] && !solid_tile(x, y)) gTile[y][x] = '#';
    }
}

static int rand_walkable(double *ox, double *oy, double minDist) {
    int i;
    for (i = 0; i < 400; i++) {
        int x = 2 + rand() % (W - 4);
        int y = 2 + rand() % (H - 4);
        if (solid_tile(x, y)) continue;
        if (dist2((double)x - G.p.x, (double)y - G.p.y) < minDist) continue;
        *ox = (double)x; *oy = (double)y;
        return 1;
    }
    *ox = (double)(W - 8); *oy = (double)(H / 2);
    return 0;
}

/* ------------------------------ 特效 ------------------------------------ */
static void add_fx(double x, double y, int col, double life, const char *fmt, ...) {
    Fx *f;
    va_list ap;
    if (G.nFx >= MAX_FX) return;
    f = &G.fx[G.nFx++];
    f->x = x; f->y = y; f->life = life; f->col = col;
    va_start(ap, fmt);
    vsnprintf(f->txt, sizeof(f->txt), fmt, ap);
    va_end(ap);
}

static void spawn_orb(double x, double y, int val) {
    Orb *o;
    if (G.nOrbs >= MAX_ORBS || val <= 0) return;
    o = &G.orbs[G.nOrbs++];
    o->x = x + (rand() % 100 - 50) / 100.0;
    o->y = y + (rand() % 100 - 50) / 100.0;
    o->vx = (rand() % 100 - 50) / 60.0;
    o->vy = (rand() % 100 - 50) / 60.0;
    o->val = val;
    o->life = 30.0;
}

static void spawn_bullet(double x, double y, double vx, double vy,
                         int dmg, int ch, int col) {
    Bullet *b;
    if (G.nBullets >= MAX_BULLETS) return;
    b = &G.bullets[G.nBullets++];
    b->x = x; b->y = y; b->vx = vx; b->vy = vy;
    b->dmg = dmg; b->ch = ch; b->col = col; b->life = 4.0;
}

/* ------------------------------ 敌人 ------------------------------------ */
static void init_enemy(Enemy *e, int t) {
    int s = G.stage;
    memset(e, 0, sizeof(*e));
    e->type = t;
    e->timer = 0.8 + (rand() % 100) / 100.0;
    e->atkCd = 0.6;
    switch (t) {
    case E_GHOST:
        e->maxhp = 28 + s * 9; e->dmg = 7 + s * 2; e->worth = 5 + s * 2; break;
    case E_SOLDIER:
        e->maxhp = 52 + s * 14; e->dmg = 11 + s * 2; e->worth = 9 + s * 3; break;
    case E_RANGED:
        e->maxhp = 34 + s * 9; e->dmg = 9 + s; e->worth = 10 + s * 3; break;
    case E_ELITE:
        e->maxhp = 130 + s * 28; e->dmg = 16 + s * 3; e->worth = 22 + s * 6; break;
    default:
        e->maxhp = 560; e->dmg = 18; e->worth = 140; e->timer = 2.0; break;
    }
    e->hp = e->maxhp;
}

static void spawn_enemy(int t, double x, double y) {
    Enemy *e;
    if (G.nEnemies >= MAX_ENEMIES) return;
    e = &G.enemies[G.nEnemies++];
    init_enemy(e, t);
    e->x = x; e->y = y;
    add_fx(x, y - 1, 95, 0.5, "+");
}

static void spawn_wave(void) {
    int i, n;
    G.wave++;
    if (G.stage == 3) {
        spawn_enemy(E_BOSS, (double)(W - 12), (double)(H / 2));
        for (i = 0; i < 2; i++) {
            double x, y;
            rand_walkable(&x, &y, 10);
            spawn_enemy(E_GHOST, x, y);
        }
        add_msg(91, "阎罗升殿：生死簿上，查无此猴！");
        return;
    }
    n = 3 + G.stage + (G.wave - 1) * 2;
    if (n > 11) n = 11;
    for (i = 0; i < n; i++) {
        int r = rand() % 100, t;
        double x, y;
        if (G.stage == 0) t = r < 65 ? E_GHOST : (r < 88 ? E_SOLDIER : E_RANGED);
        else if (G.stage == 1) t = r < 38 ? E_GHOST : (r < 68 ? E_SOLDIER : (r < 90 ? E_RANGED : E_ELITE));
        else t = r < 28 ? E_GHOST : (r < 56 ? E_SOLDIER : (r < 80 ? E_RANGED : E_ELITE));
        rand_walkable(&x, &y, 8);
        spawn_enemy(t, x, y);
    }
    add_msg(91, "阴风骤起 —— 第 %d/%d 波鬼差拦路！", G.wave, G.waveTotal);
}

static void hurt_player(int dmg) {
    if (G.p.invuln > 0 || G.p.dashTime > 0) return;
    dmg = (int)(dmg * G.p.dmgTaken);
    if (dmg < 1) dmg = 1;
    G.p.hp -= dmg;
    G.p.invuln = 0.72;
    add_fx(G.p.x, G.p.y - 1, 91, 0.8, "-%d", dmg);
    add_msg(91, "受创 -%d！", dmg);
    if (G.p.hp <= 0) { G.p.hp = 0; end_run(0); }
}

static void kill_enemy(int idx) {
    Enemy *e = &G.enemies[idx];
    int parts, i, val, each;
    double x = e->x, y = e->y;
    int t = e->type;
    val = e->worth + rand() % 5;
    parts = 2 + rand() % 3;
    each = val / parts;
    if (each < 1) each = 1;
    for (i = 0; i < parts; i++) spawn_orb(x, y, each);
    add_fx(x, y - 1, 93, 0.6, "魂");
    G.kills++;
    if (t == E_ELITE) add_msg(93, "牛头马面伏诛，魂魄四散！");
    if (t == E_BOSS) { end_run(1); return; }
    G.enemies[idx] = G.enemies[--G.nEnemies];
}

static void update_enemy(Enemy *e, double dt) {
    double dx = G.p.x - e->x, dy = G.p.y - e->y;
    double d = dist2(dx, dy);
    double ux = d > 0.001 ? dx / d : 1.0, uy = d > 0.001 ? dy / d : 0.0;
    double speed = 1.0;

    e->timer -= dt;
    e->atkCd -= dt;
    if (e->hitFlash > 0) e->hitFlash -= dt;

    switch (e->type) {
    case E_GHOST:
        speed = 3.2;
        move_creature(&e->x, &e->y, ux * speed, uy * speed, dt, 0.3);
        break;
    case E_SOLDIER:
    case E_ELITE:
        speed = (e->type == E_ELITE) ? 2.9 : 2.5;
        if (e->state == 0) {
            move_creature(&e->x, &e->y, ux * speed, uy * speed, dt, 0.35);
            if (d < 8 && e->timer <= 0) {
                e->state = 1; e->timer = 0.5;
                add_fx(e->x, e->y - 1, 91, 0.5, "!");
            }
        } else if (e->state == 1) {
            if (e->timer <= 0) {
                e->state = 2; e->timer = 0.34;
                e->vx = ux * (e->type == E_ELITE ? 15.0 : 13.0);
                e->vy = uy * (e->type == E_ELITE ? 15.0 : 13.0);
            }
        } else {
            move_creature(&e->x, &e->y, e->vx, e->vy, dt, 0.35);
            if (e->timer <= 0) { e->state = 0; e->timer = 1.1; }
        }
        break;
    case E_RANGED:
        if (d < 6.5) move_creature(&e->x, &e->y, -ux * 2.6, -uy * 2.6, dt, 0.3);
        else if (d > 9.5) move_creature(&e->x, &e->y, ux * 2.2, uy * 2.2, dt, 0.3);
        else move_creature(&e->x, &e->y, -uy * 1.2, ux * 1.2, dt, 0.3);
        if (e->timer <= 0) {
            spawn_bullet(e->x, e->y, ux * 7.0, uy * 7.0, e->dmg, '*', 95);
            e->timer = 1.75;
        }
        break;
    case E_BOSS:
        if (e->state == 2) {
            move_creature(&e->x, &e->y, e->vx, e->vy, dt, 0.6);
            if (e->timer <= 0) { e->state = 0; e->timer = 1.5; }
        } else {
            if (d > 3.4) move_creature(&e->x, &e->y, ux * 2.0, uy * 2.0, dt, 0.6);
            if (e->timer <= 0) {
                int pat = (e->phase++) % 4;
                double base = atan2(dy, dx);
                e->timer = 2.1;
                if (pat == 0) { /* 五鬼扇 */
                    int k;
                    for (k = -2; k <= 2; k++) {
                        double a = base + k * 0.23;
                        spawn_bullet(e->x, e->y, cos(a) * 7.5, sin(a) * 7.5, 10, '*', 95);
                    }
                    add_msg(95, "阎罗抖袖 · 五鬼扇！");
                } else if (pat == 1) { /* 万魂环 */
                    int k;
                    for (k = 0; k < 12; k++) {
                        double a = k * M_PI / 6.0;
                        spawn_bullet(e->x, e->y, cos(a) * 5.6, sin(a) * 5.6, 9, '+', 91);
                    }
                    add_msg(95, "万魂环 · 四面皆鬼！");
                } else if (pat == 2) { /* 召阴兵 */
                    int k;
                    for (k = 0; k < 2; k++) {
                        double x, y;
                        rand_walkable(&x, &y, 5);
                        spawn_enemy(E_GHOST, x, y);
                    }
                    add_msg(95, "阎罗拍案 · 阴兵听令！");
                } else { /* 阎罗冲 */
                    e->state = 2; e->timer = 0.45;
                    e->vx = ux * 13.0; e->vy = uy * 13.0;
                    add_fx(e->x, e->y - 1, 91, 0.5, "!!");
                }
            }
        }
        break;
    }

    /* 接触伤害 */
    if (dist2(G.p.x - e->x, G.p.y - e->y) < 1.05 && e->atkCd <= 0) {
        if (G.p.invuln <= 0 && G.p.dashTime <= 0) {
            hurt_player(e->dmg);
            e->atkCd = 0.95;
        }
    }
}

/* ------------------------------ 玩家 ------------------------------------ */
/* 软锁定：出手时自动把朝向微调到射程内最近的敌人，贴身也能打到 */
static void aim_assist(double maxRange) {
    int i, found = 0;
    double best = 1e9, bx = 0, by = 0;
    for (i = 0; i < G.nEnemies; i++) {
        double d = dist2(G.enemies[i].x - G.p.x, G.enemies[i].y - G.p.y);
        if (d < best) { best = d; bx = G.enemies[i].x; by = G.enemies[i].y; found = 1; }
    }
    if (found && best <= maxRange && best > 0.05)
        G.p.facing = atan2(by - G.p.y, bx - G.p.x);
}

static void player_attack(void) {
    const WeaponDef *w = &WD[G.p.weapon];
    int i, hits = 0;
    double dmg;
    aim_assist(w->range + 1.6);
    G.p.atkCd = w->cd * G.p.cdr;
    G.swing.life = 0.17;
    G.swing.ang = G.p.facing;
    G.swing.weapon = G.p.weapon;
    dmg = w->dmg * (1.0 + 0.28 * (G.p.wlevel[G.p.weapon] - 1)) * G.p.dmgMul;
    for (i = 0; i < G.nEnemies; i++) {
        Enemy *e = &G.enemies[i];
        double dx = e->x - G.p.x, dy = e->y - G.p.y;
        double d = dist2(dx, dy), a, diff, perp;
        int crit;
        double out;
        if (d > w->range + 0.6) continue;
        a = atan2(dy, dx);
        diff = angdiff(a, G.p.facing);
        perp = fabs(d * sin(diff));
        if (fabs(diff) > w->arc || perp > w->width) continue;
        crit = (rand() % 1000) < (int)(G.p.crit * 1000);
        out = dmg * (crit ? 1.8 : 1.0);
        e->hp -= out;
        e->hitFlash = 0.16;
        if (w->kb > 0 && d > 0.01)
            move_ent(&e->x, &e->y, (dx / d) * w->kb * 6.0,
                     (dy / d) * w->kb * 6.0, 0.06, 0.35);
        add_fx(e->x, e->y - 1, crit ? 93 : 97, 0.65, crit ? "!%d" : "%d", (int)out);
        if (G.p.lifesteal > 0) {
            G.p.hp += out * G.p.lifesteal;
            if (G.p.hp > G.p.maxhp) G.p.hp = G.p.maxhp;
        }
        hits++;
        if (e->hp <= 0) { kill_enemy(i); i--; if (G.state != ST_PLAY) return; }
    }
    if (hits == 0 && (rand() % 3) == 0) add_fx(G.p.x + cos(G.p.facing) * 2,
                                               G.p.y + sin(G.p.facing) * 2, 90, 0.25, ".");
}

static void use_potion(void) {
    if (G.p.potions <= 0) { add_msg(90, "没有回魂丹了。"); return; }
    if (G.p.hp >= G.p.maxhp) { add_msg(90, "生命已满，不必浪费。"); return; }
    G.p.potions--;
    G.p.hp += 42;
    if (G.p.hp > G.p.maxhp) G.p.hp = G.p.maxhp;
    add_fx(G.p.x, G.p.y - 1, 92, 0.9, "+42");
    add_msg(92, "服下回魂丹，回复 42 点生命。");
}

static void switch_weapon(int i) {
    if (i < 0 || i > 3) return;
    if (!G.p.unlocked[i]) { add_msg(90, "%s 尚未解锁（可在孟婆亭购买，%d 灵魂）。", WD[i].name, WD[i].cost); return; }
    if (G.p.weapon == i) return;
    G.p.weapon = i;
    G.p.atkCd = 0.16;
    add_msg(WD[i].col, "换上 %s —— %s", WD[i].name, WD[i].desc);
}

static void update_player(double dt) {
    Player *p = &G.p;
    double dx = (held(K_RIGHT) ? 1.0 : 0.0) - (held(K_LEFT) ? 1.0 : 0.0);
    double dy = (held(K_DOWN) ? 1.0 : 0.0) - (held(K_UP) ? 1.0 : 0.0);
    double len = dist2(dx, dy);
    int i;

    if (p->atkCd > 0) p->atkCd -= dt;
    if (p->dashCd > 0) p->dashCd -= dt;
    if (p->invuln > 0) p->invuln -= dt;
    if (p->dashTime > 0) p->dashTime -= dt;

    if (blocked_body(p->x, p->y, 0.32)) unstick(&p->x, &p->y, 0.32);

    if (len > 0) { dx /= len; dy /= len; p->facing = atan2(dy, dx); }
    else { dx = cos(p->facing); dy = sin(p->facing); }

    if (pressed(K_DASH) && p->dashCd <= 0) {
        p->dashTime = 0.19;
        p->dashCd = 1.55 * p->dashMul * (1.0 - (1.0 - p->cdr) * 0.5);
        if (p->dashCd < 0.5) p->dashCd = 0.5;
        p->dashVx = dx * 24.0; p->dashVy = dy * 24.0;
        p->invuln = p->invuln > 0.22 ? p->invuln : 0.22;
        add_fx(p->x, p->y + 1, 96, 0.35, "~");
        add_msg(96, "缩地成寸！");
    }

    if (p->dashTime > 0)
        move_ent(&p->x, &p->y, p->dashVx, p->dashVy, dt, 0.32);
    else if (len > 0)
        move_ent(&p->x, &p->y, dx * 8.4 * p->speedMul, dy * 8.4 * p->speedMul, dt, 0.32);

    if (held(K_ATK) && p->atkCd <= 0) player_attack();
    if (pressed(K_POT)) use_potion();
    for (i = 0; i < 4; i++) if (pressed(K_1 + i)) switch_weapon(i);

    if (pressed(K_E) && G.roomClear) next_room();
    if (pressed(K_P) || pressed(K_ESC)) { G.state = ST_PAUSE; }
}

/* --------------------------- 关卡 / 经济 -------------------------------- */
static void roll_boons(void) {
    int i, j, t, dup;
    for (i = 0; i < 3; i++) {
        do {
            t = rand() % 8; dup = 0;
            for (j = 0; j < i; j++) if (G.boonChoices[j] == t) dup = 1;
        } while (dup);
        G.boonChoices[i] = t;
    }
}

static void apply_boon(int b) {
    Player *p = &G.p;
    switch (b) {
    case 0: p->dmgMul += 0.18; break;
    case 1: p->speedMul += 0.12; p->dashMul *= 0.85; break;
    case 2: p->maxhp += 25; p->hp += 25; if (p->hp > p->maxhp) p->hp = p->maxhp; break;
    case 3: p->crit += 0.12; break;
    case 4: p->soulMul += 0.30; p->lifesteal += 0.04; break;
    case 5: p->cdr *= 0.88; break;
    case 6: p->dmgTaken *= 0.88; break;
    case 7: p->hp += 45; if (p->hp > p->maxhp) p->hp = p->maxhp;
            if (p->potions < p->maxPotions) p->potions++; break;
    }
    p->blessings++;
    add_msg(93, "得赐福【%s】—— %s", BOON_NAME[b], BOON_DESC[b]);
}

static void start_room(void) {
    int y, x;
    tile_generate();
    G.nEnemies = G.nBullets = G.nOrbs = G.nFx = 0;
    G.wave = 0;
    G.waveTotal = (G.stage == 3) ? 1 : (G.stage < 2 ? 2 : 3);
    G.waveTimer = 1.15;
    G.roomClear = 0;
    G.swing.life = 0;
    G.p.x = 4.0; G.p.y = (double)(H / 2); G.p.facing = 0.0;
    G.p.dashTime = 0; G.p.dashCd = 0; G.p.atkCd = 0; G.p.invuln = 1.1;
    for (y = 0; y < H; y++) for (x = 0; x < W; x++) G.tile[y][x] = gTile[y][x];
    add_msg(93, "【%s · 第 %d/%d 殿】%s", STAGE_NAME[G.stage],
            G.chamber + 1, STAGE_CHAMBERS[G.stage], STAGE_SUB[G.stage]);
    if (G.stage == 3) add_msg(91, "阎罗殿前，判官提笔，牛头马面列于两侧……");
}

static void next_room(void) {
    G.chamber++;
    if (G.chamber >= STAGE_CHAMBERS[G.stage]) {
        G.chamber = 0; G.stage++;
        if (G.stage > 3) { end_run(1); return; }
        if (G.stage < 3) G.p.souls += 12 + G.stage * 6;
        add_msg(93, "踏入 %s……", STAGE_NAME[G.stage]);
    }
    start_room();
    G.state = ST_PLAY;
}

static void start_run(void) {
    Player *p = &G.p;
    memset(p, 0, sizeof(*p));
    p->maxhp = 100 + G.save.hpLv * 15;
    p->hp = p->maxhp;
    p->dmgMul = 1.0 + G.save.atkLv * 0.08;
    p->speedMul = 1.0; p->cdr = 1.0; p->crit = 0.05;
    p->lifesteal = 0.0; p->soulMul = 1.0; p->dmgTaken = 1.0; p->dashMul = 1.0;
    p->maxPotions = 4;
    p->potions = 1 + G.save.potLv;
    p->souls = 15 * G.save.soulLv;
    p->unlocked[W_STAFF] = 1;
    p->wlevel[0] = p->wlevel[1] = p->wlevel[2] = p->wlevel[3] = 1;
    p->weapon = W_STAFF;
    if (gSimMode && gGodMode) {
        p->maxhp = 100000.0; p->hp = p->maxhp;
        p->dmgMul = 300.0; p->souls = 99999; p->potions = 9;
    }
    G.stage = 0; G.chamber = 0;
    G.kills = 0; G.earnedSouls = 0; G.lastGain = 0;
    G.time = 0.0; G.shopMode = 0;
    start_room();
}

static void end_run(int win) {
    int gain = (int)(G.earnedSouls * 0.15) + (win ? 90 : 10 + G.stage * 12);
    if (win) gWinCount++;
    G.save.yuanhun += gain;
    G.save.runs++;
    if (G.stage + 1 > G.save.best) G.save.best = G.stage + 1;
    save_store();
    G.lastGain = gain;
    G.state = win ? ST_WIN : ST_DEAD;
}

static void room_cleared(void) {
    int i;
    G.roomClear = 1;
    for (i = 0; i < G.nBullets; i++) G.bullets[i].life = 0;
    gTile[H / 2][W - 3] = 'O';
    G.tile[H / 2][W - 3] = 'O';
    G.p.souls += 10 + G.stage * 5;
    G.earnedSouls += 10 + G.stage * 5;
    roll_boons();
    G.state = ST_BOON;
}

static void shop_buy(int idx) {
    Player *p = &G.p;
    int w, cost;
    if (G.shopMode == 1) {
        if (idx == 4) { G.shopMode = 0; return; }
        w = idx;
        if (w < 0 || w > 3) return;
        if (p->unlocked[w]) { p->weapon = w; add_msg(WD[w].col, "换上 %s。", WD[w].name); }
        else if (p->souls >= WD[w].cost) {
            p->souls -= WD[w].cost;
            p->unlocked[w] = 1; p->weapon = w;
            add_msg(WD[w].col, "解锁 %s！%s", WD[w].name, WD[w].desc);
        } else add_msg(90, "灵魂不足，还差 %d。", WD[w].cost - p->souls);
        return;
    }
    switch (idx) {
    case 0:
        if (p->potions >= p->maxPotions) { add_msg(90, "葫芦已满。"); return; }
        if (p->souls < 25) { add_msg(90, "灵魂不足。"); return; }
        p->souls -= 25; p->potions++;
        add_msg(92, "孟婆递来一枚回魂丹。");
        break;
    case 1:
        w = p->weapon;
        if (p->wlevel[w] >= 4) { add_msg(90, "%s 已淬炼至极。", WD[w].name); return; }
        cost = 30 + 25 * (p->wlevel[w] - 1);
        if (p->souls < cost) { add_msg(90, "灵魂不足，还差 %d。", cost - p->souls); return; }
        p->souls -= cost; p->wlevel[w]++;
        add_msg(93, "%s 淬炼至 Lv%d，威力大增！", WD[w].name, p->wlevel[w]);
        break;
    case 2:
        G.shopMode = 1;
        break;
    case 3:
        if (p->souls < 60) { add_msg(90, "灵魂不足。"); return; }
        p->souls -= 60; p->maxhp += 20; p->hp += 20;
        if (p->hp > p->maxhp) p->hp = p->maxhp;
        add_msg(92, "阴司淬体，生命上限 +20。");
        break;
    case 4:
        if (p->souls < 40) { add_msg(90, "灵魂不足。"); return; }
        if (p->hp >= p->maxhp) { add_msg(90, "生命已满。"); return; }
        p->souls -= 40; p->hp = p->maxhp;
        add_msg(92, "一碗孟婆汤，伤势尽消。");
        break;
    case 5:
        G.shopMode = 0;
        G.state = ST_PLAY;
        add_msg(96, "此殿已清，按 E 前往下一处。");
        break;
    }
}

/* ------------------------------ 更新 ------------------------------------ */
static void update_play(double dt) {
    int i;

    if (G.swing.life > 0) G.swing.life -= dt;
    if (G.msgTimer > 0) G.msgTimer -= dt;

    update_player(dt);
    if (G.state != ST_PLAY) return;

    for (i = 0; i < G.nEnemies; i++) {
        update_enemy(&G.enemies[i], dt);
        if (G.state != ST_PLAY) return;
    }

    /* 敌方弹幕 */
    for (i = 0; i < G.nBullets; i++) {
        Bullet *b = &G.bullets[i];
        b->x += b->vx * dt; b->y += b->vy * dt; b->life -= dt;
        if (b->life <= 0 || solid_point(b->x, b->y)) {
            G.bullets[i] = G.bullets[--G.nBullets]; i--; continue;
        }
        if (dist2(b->x - G.p.x, b->y - G.p.y) < 0.8) {
            if (G.p.invuln <= 0 && G.p.dashTime <= 0) hurt_player(b->dmg);
            G.bullets[i] = G.bullets[--G.nBullets]; i--;
            if (G.state != ST_PLAY) return;
        }
    }

    /* 灵魂汲取 */
    for (i = 0; i < G.nOrbs; i++) {
        Orb *o = &G.orbs[i];
        double dx = G.p.x - o->x, dy = G.p.y - o->y, d = dist2(dx, dy);
        o->life -= dt;
        if (d < 4.5 && d > 0.01) {
            o->vx += dx / d * 22 * dt; o->vy += dy / d * 22 * dt;
        }
        o->vx *= 0.93; o->vy *= 0.93;
        move_ent(&o->x, &o->y, o->vx, o->vy, dt, 0.05);
        if (d < 0.95) {
            int v = (int)(o->val * G.p.soulMul);
            if (v < 1) v = 1;
            G.p.souls += v; G.earnedSouls += v;
            add_fx(o->x, o->y - 1, 93, 0.6, "+%d", v);
            G.orbs[i] = G.orbs[--G.nOrbs]; i--; continue;
        }
        if (o->life <= 0) { G.orbs[i] = G.orbs[--G.nOrbs]; i--; }
    }

    /* 飘字 */
    for (i = 0; i < G.nFx; i++) {
        G.fx[i].life -= dt;
        G.fx[i].y -= dt * 1.1;
        if (G.fx[i].life <= 0) { G.fx[i] = G.fx[--G.nFx]; i--; }
    }

    /* 波次 / 清场 */
    if (!G.roomClear) {
        if (G.nEnemies == 0) {
            if (G.wave < G.waveTotal) {
                G.waveTimer -= dt;
                if (G.waveTimer <= 0) { spawn_wave(); G.waveTimer = 1.5; }
            } else if (G.stage == 3) {
                end_run(1);
            } else {
                room_cleared();
            }
        }
    }
}

static void game_update(double dt) {
    G.time += dt;
    if (G.msgTimer > 0 && G.state != ST_PLAY) G.msgTimer -= dt;

    switch (G.state) {
    case ST_TITLE:
        if (pressed(K_1)) {
            start_run();
            if (gSimMode && gSimStage >= 0 && gSimStage <= 3) {
                G.stage = gSimStage; G.chamber = 0; start_room();
            }
            G.state = ST_STORY;
        }
        else if (pressed(K_2)) G.state = ST_META;
        else if (pressed(K_3) || pressed(K_H)) G.state = ST_HELP;
        else if (pressed(K_4) || pressed(K_ESC)) gQuit = 1;
        break;
    case ST_STORY:
        if (pressed(K_ENTER) || pressed(K_E) || pressed(K_1)) G.state = ST_PLAY;
        break;
    case ST_HELP:
        if (pressed(K_ENTER) || pressed(K_ESC) || pressed(K_1)) G.state = ST_TITLE;
        break;
    case ST_META: {
        int i;
        for (i = 0; i < 4; i++) {
            if (pressed(K_1 + i)) {
                int c = meta_cost(i);
                if (meta_level(i) >= meta_max(i)) add_msg(90, "此项已修至圆满。");
                else if (G.save.yuanhun < c) add_msg(90, "元魂不足，还差 %d。", c - G.save.yuanhun);
                else {
                    G.save.yuanhun -= c;
                    if (i == 0) G.save.hpLv++;
                    else if (i == 1) G.save.atkLv++;
                    else if (i == 2) G.save.soulLv++;
                    else G.save.potLv++;
                    save_store();
                    add_msg(93, "阴德加身，来世更强。");
                }
            }
        }
        if (pressed(K_5) || pressed(K_ESC) || pressed(K_ENTER)) G.state = ST_TITLE;
        break;
    }
    case ST_BOON: {
        int i;
        for (i = 0; i < 3; i++) {
            if (pressed(K_1 + i)) {
                apply_boon(G.boonChoices[i]);
                G.state = ST_SHOP;
                G.shopMode = 0;
                break;
            }
        }
        break;
    }
    case ST_SHOP: {
        int i;
        if (G.shopMode == 0) {
            for (i = 0; i < 6; i++) if (pressed(K_1 + i)) { shop_buy(i); break; }
        } else {
            for (i = 0; i < 5; i++) if (pressed(K_1 + i)) { shop_buy(i); break; }
        }
        break;
    }
    case ST_PAUSE:
        if (pressed(K_P) || pressed(K_ESC) || pressed(K_ENTER)) G.state = ST_PLAY;
        else if (pressed(K_X)) end_run(0);
        break;
    case ST_DEAD:
    case ST_WIN:
        if (pressed(K_ENTER) || pressed(K_1)) G.state = ST_TITLE;
        break;
    case ST_PLAY:
        update_play(dt);
        break;
    }
}

/* ------------------------------ 渲染 ------------------------------------ */
static void draw_map_cells(void) {
    int x, y, i;
    const WeaponDef *w;
    cells_clear();

    /* 挥击残影 */
    if (G.swing.life > 0) {
        w = &WD[G.swing.weapon];
        for (y = 1; y < H - 1; y++) for (x = 1; x < W - 1; x++) {
            double dx = x - G.p.x, dy = y - G.p.y, d = dist2(dx, dy);
            double a, diff, perp;
            char c;
            if (d < 0.5 || d > w->range + 0.6) continue;
            a = atan2(dy, dx);
            diff = angdiff(a, G.swing.ang);
            perp = fabs(d * sin(diff));
            if (fabs(diff) > w->arc || perp > w->width) continue;
            c = (G.swing.weapon == W_STAFF || G.swing.weapon == W_BLADE)
                  ? '*' : (fabs(cos(a)) > fabs(sin(a)) ? '-' : '|');
            cell_put(x, y, c, w->col);
        }
    }

    /* 灵魂 */
    for (i = 0; i < G.nOrbs; i++)
        cell_put((int)(G.orbs[i].x + 0.5), (int)(G.orbs[i].y + 0.5), '*', 93);

    /* 弹幕 */
    for (i = 0; i < G.nBullets; i++)
        cell_put((int)(G.bullets[i].x + 0.5), (int)(G.bullets[i].y + 0.5),
                 (char)G.bullets[i].ch, G.bullets[i].col);

    /* 敌人 */
    {
        static const char ech[5] = {'g', 's', 'r', 'B', 'Y'};
        static const int ecol[5] = {92, 91, 95, 31, 91};
        for (i = 0; i < G.nEnemies; i++) {
            Enemy *e = &G.enemies[i];
            int ex = (int)(e->x + 0.5), ey = (int)(e->y + 0.5);
            int col = e->hitFlash > 0 ? 97 : ecol[e->type];
            if (e->state == 1) col = 91;
            if (e->type == E_BOSS) {
                cell_put(ex - 1, ey - 1, '/', col);
                cell_put(ex, ey - 1, 'Y', col);
                cell_put(ex + 1, ey - 1, '\\', col);
                cell_put(ex - 1, ey, '|', col);
                cell_put(ex, ey, 'Y', col);
                cell_put(ex + 1, ey, '|', col);
            } else {
                cell_put(ex, ey, ech[e->type], col);
            }
            if (e->type == E_ELITE || e->type == E_BOSS)
                cell_put(ex, ey - 2, e->type == E_BOSS ? '^' : '-', 91);
        }
    }

    /* 玩家 */
    if (!(G.p.invuln > 0 && ((int)(G.time * 14) & 1)))
        cell_put((int)(G.p.x + 0.5), (int)(G.p.y + 0.5), '@',
                 G.p.dashTime > 0 ? 97 : 93);

    /* 飘字 */
    for (i = 0; i < G.nFx; i++) {
        Fx *f = &G.fx[i];
        int j, fx = (int)(f->x + 0.5), fy = (int)(f->y + 0.5);
        for (j = 0; f->txt[j]; j++)
            if (f->txt[j] != ' ') cell_put(fx + j, fy, f->txt[j], f->col);
    }

    for (y = 0; y < H; y++) {
        int cur = -2;
        fb_add("\033[40m\033[97m");
        for (x = 0; x < W; x++) {
            char base = gTile[y][x];
            char ch = gGlyph[y][x] ? gGlyph[y][x] : base;
            int col = gCol[y][x] >= 0 ? gCol[y][x] : tile_color(base);
            if (base == 'O' && !gGlyph[y][x]) col = ((int)(G.time * 3) & 1) ? 96 : 94;
            if (col != cur) { fb_add("\033[%dm", col); cur = col; }
            fb_add("%c", ch);
        }
        fb_line();
    }
}

static void render_play(void) {
    Player *p = &G.p;
    int i, filled, n;
    char buf[256];
    Enemy *boss = NULL;

    fb_start();
    for (i = 0; i < G.nEnemies; i++)
        if (G.enemies[i].type == E_BOSS) boss = &G.enemies[i];

    fb_add(" \033[93m大闹阴曹·归山\033[90m  |  \033[97m%s 第 %d/%d 殿\033[90m  |  阴魂 %d  |  第 %d/%d 波",
           STAGE_NAME[G.stage], G.chamber + 1, STAGE_CHAMBERS[G.stage],
           G.nEnemies, G.wave ? G.wave : 1, G.waveTotal);
    fb_line();

    if (boss) {
        int seg = (int)(boss->hp / boss->maxhp * 34);
        if (seg < 0) seg = 0;
        fb_add(" \033[91m阎罗王 \033[97m[");
        fb_add("\033[91m");
        for (i = 0; i < seg; i++) fb_add("#");
        fb_add("\033[90m");
        for (i = seg; i < 34; i++) fb_add(".");
        fb_add("\033[97m] %d/%d", (int)boss->hp, (int)boss->maxhp);
    } else {
        fb_add(" \033[90m元魂 %d  |  击杀 %d  |  赐福 %d  |  %s",
               G.save.yuanhun, G.kills, p->blessings, STAGE_SUB[G.stage]);
    }
    fb_line();

    draw_map_cells();

    /* 生命条 */
    filled = (int)(p->hp / p->maxhp * 26);
    if (filled < 0) filled = 0; if (filled > 26) filled = 26;
    fb_add(" \033[97m生命 \033[91m[");
    for (i = 0; i < filled; i++) fb_add("#");
    fb_add("\033[90m");
    for (i = filled; i < 26; i++) fb_add(".");
    fb_add("\033[97m] %d/%d", (int)p->hp, (int)p->maxhp);
    fb_line();

    /* 灵魂 / 药 / 冲刺 */
    {
        int dashSeg = (int)((1.0 - p->dashCd / 1.55) * 10);
        if (dashSeg < 0) dashSeg = 0; if (dashSeg > 10) dashSeg = 10;
        fb_add(" \033[93m灵魂 %d\033[97m   回魂丹 x%d [Q]   冲刺 \033[96m[", p->souls, p->potions);
        for (i = 0; i < dashSeg; i++) fb_add("=");
        fb_add("\033[90m");
        for (i = dashSeg; i < 10; i++) fb_add(".");
        fb_add("\033[97m]   %s Lv%d", WD[p->weapon].name, p->wlevel[p->weapon]);
        fb_line();
    }

    /* 兵器栏 */
    n = 0;
    for (i = 0; i < 4; i++) {
        if (p->unlocked[i])
            n += sprintf(buf + n, " %s%d %s Lv%d%s",
                         (i == p->weapon) ? "\033[93m[" : " ",
                         i + 1, WD[i].name, p->wlevel[i],
                         (i == p->weapon) ? "\033[97m]" : " ");
        else
            n += sprintf(buf + n, " \033[90m%d %s 解锁%d\033[97m ", i + 1, WD[i].name, WD[i].cost);
    }
    if (n >= (int)sizeof(buf)) n = (int)sizeof(buf) - 1;
    buf[n] = 0;
    fb_add(" \033[97m%s", buf);
    fb_line();

    /* 消息 */
    if (G.msgTimer > 0) fb_add(" \033[%dm> %s", G.msgCol, G.msg);
    else fb_add(" \033[90m> %s", G.roomClear ? "此殿已清，按 E 前往下一处。" : "杀出一条回花果山的路。");
    fb_line();

    fb_add(" \033[90mWASD移动 J/空格攻击 K冲刺 Q药 1-4换兵器 E通行 P暂停");
    fb_line();
    fb_add(" \033[90m本轮灵魂 %d  |  击杀 %d  |  当前兵器 %s  |  P 暂停查看完整说明",
           G.earnedSouls, G.kills, WD[p->weapon].name);
    fb_line();
    fb_finish();
}

/* --------------------------- 各界面 ------------------------------------- */
static void render_title(void) {
    char buf[256];
    fb_start();
    ui_blank();
    ui_center("大 闹 阴 曹  ·  归 山", 93);
    ui_center("A LITTLE MONKEY'S ESCAPE FROM DIYU", 90);
    ui_blank();
    ui_rule();
    ui_center("孙悟空大闹地府、勾销生死簿之后，轮回失序，鬼门洞开。", 97);
    ui_center("花果山一只小猴追着一缕毫毛跌入阴曹，十八层地狱尽数封路。", 97);
    ui_center("黄泉路 → 奈何桥 → 枉死城 → 阎罗殿，打碎轮回门，回水帘洞去。", 97);
    ui_rule();
    ui_blank();
    ui_center("1  踏入鬼门关（开始新轮回）", 93);
    ui_center("2  阴德殿（用元魂永久强化）", 97);
    ui_center("3  操作与玩法说明", 97);
    ui_center("4  退出", 90);
    ui_blank();
    snprintf(buf, sizeof(buf), "元魂 %d   |   已历轮回 %d 次   |   最远抵达第 %d 重地府",
             G.save.yuanhun, G.save.runs, G.save.best);
    ui_center(buf, 90);
    ui_center("灵魂用于本轮购买；元魂在轮回结束后留存，用于永久强化。", 90);
    fb_finish();
}

static void render_story(void) {
    fb_start();
    ui_blank();
    ui_center("—— 楔子 · 一缕毫毛 ——", 93);
    ui_blank();
    ui_rule();
    ui_center("那日齐天大圣闯进森罗殿，一笔勾去猴属生死，掀了判官案。", 97);
    ui_center("阴曹大乱，忘川倒流，轮回门被鬼差层层锁死。", 97);
    ui_center("花果山有只没名没姓的小猴，追着大圣落下的一缕毫毛，", 97);
    ui_center("一脚踩空，跌进了黄泉路。", 93);
    ui_blank();
    ui_center("手里只有一根齐眉棍。要回家，就得一路打上去。", 97);
    ui_rule();
    ui_blank();
    ui_center("按 Enter 踏入黄泉路", 93);
    fb_finish();
}

static void render_help(void) {
    fb_start();
    ui_blank();
    ui_center("—— 操作与玩法 ——", 93);
    ui_blank();
    ui_left("移动    WASD / 方向键        攻击    J 或 空格（按住可连击）", 97, 10);
    ui_left("冲刺    K / Shift（短暂无敌）  回魂丹  Q（回复 42 生命）", 97, 10);
    ui_left("换兵器  1 / 2 / 3 / 4        通行    E（清场后前往下一殿）", 97, 10);
    ui_left("暂停    P / Esc              放弃轮回  暂停界面按 X", 97, 10);
    ui_blank();
    ui_rule();
    ui_left("棍：横扫范围大，初始持有        枪：刺得远、带击退  [45 灵魂]", 93, 10);
    ui_left("刀：一击重创、范围极广 [100 灵魂]  剑：出手极快、适合连击 [70 灵魂]", 93, 10);
    ui_blank();
    ui_left("击杀鬼怪掉落灵魂，走近自动吸取。清场后可选一项判官赐福，", 97, 10);
    ui_left("再进孟婆亭用灵魂买药、解锁/淬炼兵器、淬体或回满生命。", 97, 10);
    ui_left("四重地府：黄泉路 3 殿 → 奈何桥 3 殿 → 枉死城 3 殿 → 阎罗殿 Boss。", 97, 10);
    ui_left("战败后本轮灵魂散尽，但会留存 15% 为元魂，可在阴德殿永久变强。", 97, 10);
    ui_blank();
    ui_center("按 Enter 返回", 90);
    fb_finish();
}

static void render_meta(void) {
    char buf[256];
    int i;
    static const char *nm[4] = {"金刚不坏", "天生神力", "招魂幡", "葫芦"};
    static const char *ds[4] = {"初始生命上限 +15", "初始伤害 +8%", "开局灵魂 +15", "开局回魂丹 +1"};
    fb_start();
    ui_blank();
    ui_center("—— 阴 德 殿 ——", 93);
    ui_blank();
    snprintf(buf, sizeof(buf), "可用元魂：%d     （元魂 = 每轮所得灵魂的 15%%，战败亦有）", G.save.yuanhun);
    ui_center(buf, 97);
    ui_blank();
    ui_rule();
    for (i = 0; i < 4; i++) {
        snprintf(buf, sizeof(buf), "%d  %s  Lv%d/%d   %s   花费 %d 元魂",
                 i + 1, nm[i], meta_level(i), meta_max(i), ds[i], meta_cost(i));
        ui_left(buf, meta_level(i) >= meta_max(i) ? 90 : 97, 10);
    }
    ui_rule();
    ui_blank();
    ui_center("5  返回鬼门关", 93);
    if (G.msgTimer > 0) ui_center(G.msg, G.msgCol);
    fb_finish();
}

static void render_boon(void) {
    char buf[256];
    int i;
    fb_start();
    ui_blank();
    ui_center("—— 判 官 赐 福 · 三 择 一 ——", 93);
    ui_center("此殿已清，轮回门松动了一分。", 90);
    ui_blank();
    ui_rule();
    for (i = 0; i < 3; i++) {
        int b = G.boonChoices[i];
        snprintf(buf, sizeof(buf), "%d   【%s】  %s", i + 1, BOON_NAME[b], BOON_DESC[b]);
        ui_center(buf, 97);
        ui_blank();
    }
    ui_rule();
    ui_center("按 1 / 2 / 3 选择一项赐福，随后进入孟婆亭", 93);
    snprintf(buf, sizeof(buf), "本轮已得赐福：%d 项", G.p.blessings);
    ui_center(buf, 90);
    fb_finish();
}

static void render_shop(void) {
    char buf[256];
    int i, cost;
    Player *p = &G.p;
    fb_start();
    ui_blank();
    ui_center("—— 孟 婆 亭 ——", 96);
    snprintf(buf, sizeof(buf), "手中灵魂：%d    生命 %d/%d    回魂丹 %d/%d",
             p->souls, (int)p->hp, (int)p->maxhp, p->potions, p->maxPotions);
    ui_center(buf, 97);
    ui_blank();
    ui_rule();
    if (G.shopMode == 0) {
        ui_left("1  回魂丹 ·························· 25 灵魂（回复 42 生命）", 97, 10);
        cost = 30 + 25 * (p->wlevel[p->weapon] - 1);
        snprintf(buf, sizeof(buf), "2  淬炼 %s ··················· %d 灵魂（当前 Lv%d，最高 Lv4）",
                 WD[p->weapon].name, cost, p->wlevel[p->weapon]);
        ui_left(buf, 97, 10);
        ui_left("3  解锁 / 切换兵器 ················ 进入兵器架", 97, 10);
        ui_left("4  阴司淬体 ························ 60 灵魂（生命上限 +20）", 97, 10);
        ui_left("5  孟婆汤 ·························· 40 灵魂（立即回满生命）", 97, 10);
        ui_left("6  离开孟婆亭 ······················ 回到殿中，按 E 继续", 93, 10);
    } else {
        ui_center("—— 兵 器 架 ——", 93);
        for (i = 0; i < 4; i++) {
            if (p->unlocked[i])
                snprintf(buf, sizeof(buf), "%d  %s  Lv%d  【已解锁 · 按此键装备】 %s",
                         i + 1, WD[i].name, p->wlevel[i], WD[i].desc);
            else
                snprintf(buf, sizeof(buf), "%d  %s  【%d 灵魂解锁】 %s",
                         i + 1, WD[i].name, WD[i].cost, WD[i].desc);
            ui_left(buf, p->unlocked[i] ? 93 : 97, 10);
        }
        ui_center("5  返回孟婆亭主柜台", 93);
    }
    ui_rule();
    ui_blank();
    if (G.msgTimer > 0) ui_left(G.msg, G.msgCol, 10);
    else ui_center("按数字键选择", 90);
    fb_finish();
}

static void render_pause(void) {
    fb_start();
    ui_blank();
    ui_center("—— 暂 停 ——", 93);
    ui_blank();
    ui_rule();
    ui_center("WASD 移动   J/空格 攻击   K 冲刺   Q 回魂丹   1-4 换兵器", 97);
    ui_center("阴魂挡路，唯有打过去。清场后按 E 前往下一殿。", 97);
    ui_center("黄泉路 → 奈何桥 → 枉死城 → 阎罗殿 → 花果山", 96);
    ui_rule();
    ui_blank();
    ui_center("P / Esc / Enter  继续", 93);
    ui_center("X  放弃本轮轮回", 90);
    fb_finish();
}

static void render_dead(void) {
    char buf[256];
    fb_start();
    ui_blank();
    ui_center("—— 魂 散 奈 何 桥 ——", 91);
    ui_blank();
    ui_rule();
    ui_center("小猴的影子被忘川水一冲，散了。", 97);
    ui_center("可花果山那边，桃花还在开。再来一次。", 93);
    ui_blank();
    snprintf(buf, sizeof(buf), "本轮击杀 %d   所得灵魂 %d   抵达 %s", G.kills, G.earnedSouls, STAGE_NAME[G.stage]);
    ui_center(buf, 97);
    snprintf(buf, sizeof(buf), "留存元魂 +%d  （现有 %d，可在阴德殿变强）", G.lastGain, G.save.yuanhun);
    ui_center(buf, 93);
    ui_rule();
    ui_blank();
    ui_center("按 Enter 回到鬼门关", 93);
    fb_finish();
}

static void render_win(void) {
    char buf[256];
    fb_start();
    ui_blank();
    ui_center("—— 归 山 ——", 93);
    ui_blank();
    ui_rule();
    ui_center("阎罗的笔断在案上，轮回门被一棍捅了个通透。", 97);
    ui_center("小猴从门里跌出来，落在一片桃花上。", 97);
    ui_center("水帘洞前，猴子们正分桃子，谁也不知道他去了多久。", 97);
    ui_center("他把棍子往肩上一扛：『回家了。』", 93);
    ui_blank();
    snprintf(buf, sizeof(buf), "本轮击杀 %d   所得灵魂 %d   赐福 %d   留存元魂 +%d",
             G.kills, G.earnedSouls, G.p.blessings, G.lastGain);
    ui_center(buf, 97);
    ui_rule();
    ui_blank();
    ui_center("按 Enter 回到鬼门关（可再战，试试别的兵器）", 93);
    fb_finish();
}

static void render(void) {
    switch (G.state) {
    case ST_TITLE: render_title(); break;
    case ST_STORY: render_story(); break;
    case ST_HELP:  render_help(); break;
    case ST_META:  render_meta(); break;
    case ST_BOON:  render_boon(); break;
    case ST_SHOP:  render_shop(); break;
    case ST_PAUSE: render_pause(); break;
    case ST_DEAD:  render_dead(); break;
    case ST_WIN:   render_win(); break;
    default:       render_play(); break;
    }
}

/* --------------------------- 自检机器人 --------------------------------- */
/* 简易 BFS：给出从 (sx,sy) 走向 (tx,ty) 的下一步格子，用于验证地图连通性 */
static int bfs_step(int sx, int sy, int tx, int ty, int *nx, int *ny) {
    static int q[W * H], from[W * H];
    static unsigned char seen[W * H];
    static const int ddx[4] = {1, -1, 0, 0}, ddy[4] = {0, 0, 1, -1};
    int head = 0, tail = 0, i, start;
    if (sx < 0 || sx >= W || sy < 0 || sy >= H) return 0;
    if (tx < 0 || tx >= W || ty < 0 || ty >= H) return 0;
    if (solid_tile(tx, ty)) return 0;
    memset(seen, 0, sizeof(seen));
    for (i = 0; i < W * H; i++) from[i] = -1;
    start = sy * W + sx;
    q[tail++] = start; seen[start] = 1;
    while (head < tail) {
        int cur = q[head++], cx = cur % W, cy = cur / W;
        for (i = 0; i < 4; i++) {
            int x = cx + ddx[i], y = cy + ddy[i], id;
            if (x < 0 || x >= W || y < 0 || y >= H) continue;
            id = y * W + x;
            if (seen[id] || solid_tile(x, y)) continue;
            seen[id] = 1; from[id] = cur; q[tail++] = id;
            if (x == tx && y == ty) {
                int node = id;
                while (from[node] != start && from[node] != -1) node = from[node];
                *nx = node % W; *ny = node / W;
                return 1;
            }
        }
    }
    return 0;
}

static void sim_bot(void) {
    static int shopFrame = 0, lastState = -1;
    int i;
    if (G.state != lastState) { shopFrame = 0; lastState = G.state; }
    memcpy(gPrev, gKey, sizeof(gKey));
    memset(gKey, 0, sizeof(gKey));
    switch (G.state) {
    case ST_TITLE:  gKey[K_1] = 1; break;
    case ST_STORY:  gKey[K_ENTER] = 1; break;
    case ST_HELP:
    case ST_META:
    case ST_DEAD:
    case ST_WIN:    gKey[K_ENTER] = 1; break;
    case ST_BOON:   gKey[K_1] = 1; break;
    case ST_SHOP: {
        int step = (shopFrame++) / 3;
        if (G.shopMode == 1) {
            gKey[(step % 5) == 0 ? K_2 : K_5] = 1;
        } else {
            switch (step % 8) {
            case 0: gKey[K_1] = 1; break;   /* 回魂丹 */
            case 1: gKey[K_2] = 1; break;   /* 淬炼兵器 */
            case 2: gKey[K_3] = 1; break;   /* 兵器架 */
            case 3: gKey[K_4] = 1; break;   /* 淬体 */
            case 4: gKey[K_5] = 1; break;   /* 孟婆汤 */
            case 5: gKey[K_1] = 1; break;
            case 6: gKey[K_2] = 1; break;
            default: gKey[K_6] = 1; break;  /* 离开 */
            }
        }
        break;
    }
    case ST_PAUSE:  gKey[K_P] = 1; break;
    case ST_PLAY: {
        double best = 1e9, bx = 0, by = 0;
        int found = 0, nx, ny, ok = 0;
        if (G.roomClear) { gKey[K_E] = 1; break; }
        for (i = 0; i < G.nEnemies; i++) {
            double d = dist2(G.enemies[i].x - G.p.x, G.enemies[i].y - G.p.y);
            if (d < best) { best = d; bx = G.enemies[i].x; by = G.enemies[i].y; found = 1; }
        }
        if (G.nOrbs > 0 && !found) { bx = G.orbs[0].x; by = G.orbs[0].y; found = 1; }
        if (found) {
            ok = bfs_step((int)(G.p.x + 0.5), (int)(G.p.y + 0.5),
                          (int)(bx + 0.5), (int)(by + 0.5), &nx, &ny);
            if (ok) {
                if (nx - G.p.x > 0.12) gKey[K_RIGHT] = 1;
                else if (nx - G.p.x < -0.12) gKey[K_LEFT] = 1;
                if (ny - G.p.y > 0.12) gKey[K_DOWN] = 1;
                else if (ny - G.p.y < -0.12) gKey[K_UP] = 1;
            }
            if (!gKey[K_RIGHT] && !gKey[K_LEFT] && !gKey[K_UP] && !gKey[K_DOWN]) {
                if (bx - G.p.x > 0.12) gKey[K_RIGHT] = 1;
                else if (bx - G.p.x < -0.12) gKey[K_LEFT] = 1;
                if (by - G.p.y > 0.12) gKey[K_DOWN] = 1;
                else if (by - G.p.y < -0.12) gKey[K_UP] = 1;
            }
            if (best < 3.2) gKey[K_ATK] = 1;
        } else {
            int w = (int)(G.time * 2) % 4;
            if (w == 0) gKey[K_RIGHT] = 1;
            else if (w == 1) gKey[K_DOWN] = 1;
            else if (w == 2) gKey[K_LEFT] = 1;
            else gKey[K_UP] = 1;
        }
        if (G.p.hp < G.p.maxhp * 0.35 && G.p.potions > 0) gKey[K_POT] = 1;
        if ((rand() % 300) == 0) gKey[K_DASH] = 1;
        break;
    }
    }
}

/* ------------------------------ 主函数 ---------------------------------- */
int main(int argc, char **argv) {
    int simTicks = 0, i, ticks = 0;
    double last = 0.0, t0 = 0.0;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--sim") && i + 1 < argc) simTicks = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--stage") && i + 1 < argc) gSimStage = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--trace")) gTrace = 1;
        else if (!strcmp(argv[i], "--god")) gGodMode = 1;
        else if (!strcmp(argv[i], "--render")) gSimRender = 1;
        else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            printf("大闹阴曹·归山\n用法: diyu.exe [--sim <帧数>] [--stage 0..3] [--trace] [--god] [--render]\n");
            return 0;
        }
    }

#ifdef _WIN32
    srand((unsigned int)time(NULL) ^ (unsigned int)GetCurrentProcessId());
#else
    srand((unsigned int)time(NULL) ^ (unsigned int)getpid());
#endif
    memset(&G, 0, sizeof(G));
    G.state = ST_TITLE;
    gSimMode = (simTicks > 0);
    save_load();
    if (!simTicks) plat_init();
    last = now_sec();

    for (;;) {
        double dt;
        t0 = now_sec();
        dt = t0 - last;
        last = t0;
        if (dt > 0.06) dt = 0.06;
        if (dt < 0.001) dt = 0.001;

        if (simTicks) sim_bot(); else plat_poll();
        game_update(simTicks ? TICK_DT : dt);
        if (gQuit) break;
        if (!simTicks || gSimRender) render();
        if (simTicks && gTrace && (ticks % 1500) == 0)
            printf("t=%d st=%d stage=%d ch=%d wave=%d/%d clr=%d en=%d hp=%.0f kill=%d soul=%d px=%.1f py=%.1f\n",
                   ticks, G.state, G.stage, G.chamber, G.wave, G.waveTotal,
                   G.roomClear, G.nEnemies, G.p.hp, G.kills, G.p.souls, G.p.x, G.p.y);

        if (simTicks && ++ticks >= simTicks) break;
        if (!simTicks) {
            int wait = (int)((TICK_DT - (now_sec() - t0)) * 1000.0);
            if (wait > 0) plat_sleep_ms(wait);
        }
    }

    if (simTicks) {
        printf("SELFTEST OK: ticks=%d state=%d stage=%d chamber=%d kills=%d souls=%d earned=%d yuanhun=%d wins=%d\n",
               ticks, G.state, G.stage, G.chamber, G.kills, G.p.souls,
               G.earnedSouls, G.save.yuanhun, gWinCount);
    } else plat_shutdown();
    return 0;
}
