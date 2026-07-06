#include <conio.h>
#include <graph.h>
#include <bios.h>
#include <i86.h>
#include <stdio.h>
#include <string.h>

#define HL_FRAMES 2
#define REL_FRAMES 2
#define LOG_N 20
#define LOG_SHOWN 16

#define KS_UP 0
#define KS_GRN 1
#define KS_YLW 2
#define KS_RED 3

#define NGROUPS_MAX 80

static unsigned char hl[128], rel[128], kstate[128], pressed[128], prev_gs[NGROUPS_MAX], prev_log_ks[128], prev_kc[128];

static short last_ascii, last_scan;
static char last_str[64], prev_stat_str[64];
static short prev_mod_state[7];
static short has_mouse, prev_mx, prev_my, prev_mbtn, done;
static unsigned char last_phy_scan;
static char log_buf[LOG_N][56];
static unsigned long g_frame;
static short log_idx, log_cnt;

/* ── Keyboard layout: 6 rows, key groups ── */

static const char * const kb_rows[6] = {
    "[esc] [f1|2|3|4][5|6|7|8][9|10|11|12]    [del]     ",
    "[`][1][2][3][4][5][6][7][8][9][0][-][+] [Bksp]     |",
    "[Tab][q][w][e][r][t][y][u][i][o][p]  [{][}][|]   \xDA\xC4\xC4\xC4\xBF",
    "[Caps][a][s][d][f][g][h][j][k][l][;]['][Enter]   \xB3   \xB3",
    "[LShift][z][x][c][v][b][n][m][,][.][/][RShift]   \xB3   \xB3",
    "[LCtrl][LAlt][Space_____________][RAlt][RCtrl]   \xC0\xC4\xC4\xC4\xD9",
};

typedef struct {
    short row, col, len;
    short nsc;
    short scs[4];
} KbdGroup;

static short ngroups;
static KbdGroup groups[NGROUPS_MAX];

typedef struct { short row, col, len; } KeyPos;
static KeyPos keypos[128];

static void add_group(short r, short c, short len, short nsc, const short *scs) {
    short i = ngroups++;
    groups[i].row = r;
    groups[i].col = c;
    groups[i].len = len;
    groups[i].nsc = nsc;
    memcpy(groups[i].scs, scs, nsc * sizeof(short));
}

static const short _1[] = {0x29};
static const short _2[] = {0x02};
static const short _3[] = {0x03};
static const short _4[] = {0x04};
static const short _5[] = {0x05};
static const short _6[] = {0x06};
static const short _7[] = {0x07};
static const short _8[] = {0x08};
static const short _9[] = {0x09};
static const short _10[] = {0x0A};
static const short _11[] = {0x0B};
static const short _12[] = {0x0C};
static const short _13[] = {0x0D};
static const short sc_esc[] = {0x01};
static const short sc_f14[] = {0x3B, 0x3C, 0x3D, 0x3E};
static const short sc_f58[] = {0x3F, 0x40, 0x41, 0x42};
static const short sc_f912[] = {0x43, 0x44, 0x57, 0x58};
static const short sc_del[] = {0x53};
static const short sc_bksp[] = {0x0E};
static const short sc_tab[] = {0x0F};
static const short sc_q[] = {0x10};
static const short sc_w[] = {0x11};
static const short sc_e[] = {0x12};
static const short sc_r[] = {0x13};
static const short sc_t[] = {0x14};
static const short sc_y[] = {0x15};
static const short sc_u[] = {0x16};
static const short sc_i[] = {0x17};
static const short sc_o[] = {0x18};
static const short sc_p[] = {0x19};
static const short sc_obr[] = {0x1A};
static const short sc_cbr[] = {0x1B};
static const short sc_pipe[] = {0x2B};
static const short sc_capa[] = {0x3A};
static const short sc_a[] = {0x1E};
static const short sc_s[] = {0x1F};
static const short sc_d[] = {0x20};
static const short sc_f[] = {0x21};
static const short sc_g[] = {0x22};
static const short sc_h[] = {0x23};
static const short sc_j[] = {0x24};
static const short sc_k[] = {0x25};
static const short sc_l[] = {0x26};
static const short sc_semi[] = {0x27};
static const short sc_quot[] = {0x28};
static const short sc_enter[] = {0x1C};
static const short sc_lshift[] = {0x2A};
static const short sc_z[] = {0x2C};
static const short sc_x[] = {0x2D};
static const short sc_c[] = {0x2E};
static const short sc_v[] = {0x2F};
static const short sc_b[] = {0x30};
static const short sc_n[] = {0x31};
static const short sc_m[] = {0x32};
static const short sc_comma[] = {0x33};
static const short sc_dot[] = {0x34};
static const short sc_slash[] = {0x35};
static const short sc_rshift[] = {0x36};
static const short sc_rctrl[] = {100};
static const short sc_ralt[] = {99};
static const short sc_space[] = {0x39};
static const short sc_lalt[] = {0x38};
static const short sc_lctrl[] = {0x1D};

/* ── Per-key position mapping (row, col, len) for each scancode ── */
static void init_keypos(void) {
    short i;
    for (i = 0; i < 128; i++) { keypos[i].row = -1; keypos[i].col = -1; }

    keypos[0x01].row = 1; keypos[0x01].col = 2;  keypos[0x01].len = 3;  /* Esc  → "esc" */
    keypos[0x3B].row = 1; keypos[0x3B].col = 8;  keypos[0x3B].len = 2;  /* F1   → "f1" */
    keypos[0x3C].row = 1; keypos[0x3C].col = 11; keypos[0x3C].len = 1;  /* F2   → "2" */
    keypos[0x3D].row = 1; keypos[0x3D].col = 13; keypos[0x3D].len = 1;  /* F3   → "3" */
    keypos[0x3E].row = 1; keypos[0x3E].col = 15; keypos[0x3E].len = 1;  /* F4   → "4" */
    keypos[0x3F].row = 1; keypos[0x3F].col = 18; keypos[0x3F].len = 1;  /* F5   → "5" */
    keypos[0x40].row = 1; keypos[0x40].col = 20; keypos[0x40].len = 1;  /* F6   → "6" */
    keypos[0x41].row = 1; keypos[0x41].col = 22; keypos[0x41].len = 1;  /* F7   → "7" */
    keypos[0x42].row = 1; keypos[0x42].col = 24; keypos[0x42].len = 1;  /* F8   → "8" */
    keypos[0x43].row = 1; keypos[0x43].col = 27; keypos[0x43].len = 1;  /* F9   → "9" */
    keypos[0x44].row = 1; keypos[0x44].col = 29; keypos[0x44].len = 2;  /* F10  → "10" */
    keypos[0x57].row = 1; keypos[0x57].col = 32; keypos[0x57].len = 2;  /* F11  → "11" */
    keypos[0x58].row = 1; keypos[0x58].col = 35; keypos[0x58].len = 2;  /* F12  → "12" */
    keypos[0x53].row = 1; keypos[0x53].col = 42; keypos[0x53].len = 3;  /* Del  → "del" */

    keypos[0x29].row = 2; keypos[0x29].col = 2;  keypos[0x29].len = 1;  /* ` */
    keypos[0x02].row = 2; keypos[0x02].col = 5;  keypos[0x02].len = 1;  /* 1 */
    keypos[0x03].row = 2; keypos[0x03].col = 8;  keypos[0x03].len = 1;  /* 2 */
    keypos[0x04].row = 2; keypos[0x04].col = 11; keypos[0x04].len = 1;  /* 3 */
    keypos[0x05].row = 2; keypos[0x05].col = 14; keypos[0x05].len = 1;  /* 4 */
    keypos[0x06].row = 2; keypos[0x06].col = 17; keypos[0x06].len = 1;  /* 5 */
    keypos[0x07].row = 2; keypos[0x07].col = 20; keypos[0x07].len = 1;  /* 6 */
    keypos[0x08].row = 2; keypos[0x08].col = 23; keypos[0x08].len = 1;  /* 7 */
    keypos[0x09].row = 2; keypos[0x09].col = 26; keypos[0x09].len = 1;  /* 8 */
    keypos[0x0A].row = 2; keypos[0x0A].col = 29; keypos[0x0A].len = 1;  /* 9 */
    keypos[0x0B].row = 2; keypos[0x0B].col = 32; keypos[0x0B].len = 1;  /* 0 */
    keypos[0x0C].row = 2; keypos[0x0C].col = 35; keypos[0x0C].len = 1;  /* - */
    keypos[0x0D].row = 2; keypos[0x0D].col = 38; keypos[0x0D].len = 1;  /* + */
    keypos[0x0E].row = 2; keypos[0x0E].col = 42; keypos[0x0E].len = 4;  /* Bksp */

    keypos[0x0F].row = 3; keypos[0x0F].col = 2;  keypos[0x0F].len = 3;  /* Tab */
    keypos[0x10].row = 3; keypos[0x10].col = 7;  keypos[0x10].len = 1;  /* q */
    keypos[0x11].row = 3; keypos[0x11].col = 10; keypos[0x11].len = 1;  /* w */
    keypos[0x12].row = 3; keypos[0x12].col = 13; keypos[0x12].len = 1;  /* e */
    keypos[0x13].row = 3; keypos[0x13].col = 16; keypos[0x13].len = 1;  /* r */
    keypos[0x14].row = 3; keypos[0x14].col = 19; keypos[0x14].len = 1;  /* t */
    keypos[0x15].row = 3; keypos[0x15].col = 22; keypos[0x15].len = 1;  /* y */
    keypos[0x16].row = 3; keypos[0x16].col = 25; keypos[0x16].len = 1;  /* u */
    keypos[0x17].row = 3; keypos[0x17].col = 28; keypos[0x17].len = 1;  /* i */
    keypos[0x18].row = 3; keypos[0x18].col = 31; keypos[0x18].len = 1;  /* o */
    keypos[0x19].row = 3; keypos[0x19].col = 34; keypos[0x19].len = 1;  /* p */
    keypos[0x1A].row = 3; keypos[0x1A].col = 39; keypos[0x1A].len = 1;  /* { */
    keypos[0x1B].row = 3; keypos[0x1B].col = 42; keypos[0x1B].len = 1;  /* } */
    keypos[0x2B].row = 3; keypos[0x2B].col = 45; keypos[0x2B].len = 1;  /* | */

    keypos[0x3A].row = 4; keypos[0x3A].col = 2;  keypos[0x3A].len = 4;  /* Caps */
    keypos[0x1E].row = 4; keypos[0x1E].col = 8;  keypos[0x1E].len = 1;  /* a */
    keypos[0x1F].row = 4; keypos[0x1F].col = 11; keypos[0x1F].len = 1;  /* s */
    keypos[0x20].row = 4; keypos[0x20].col = 14; keypos[0x20].len = 1;  /* d */
    keypos[0x21].row = 4; keypos[0x21].col = 17; keypos[0x21].len = 1;  /* f */
    keypos[0x22].row = 4; keypos[0x22].col = 20; keypos[0x22].len = 1;  /* g */
    keypos[0x23].row = 4; keypos[0x23].col = 23; keypos[0x23].len = 1;  /* h */
    keypos[0x24].row = 4; keypos[0x24].col = 26; keypos[0x24].len = 1;  /* j */
    keypos[0x25].row = 4; keypos[0x25].col = 29; keypos[0x25].len = 1;  /* k */
    keypos[0x26].row = 4; keypos[0x26].col = 32; keypos[0x26].len = 1;  /* l */
    keypos[0x27].row = 4; keypos[0x27].col = 35; keypos[0x27].len = 1;  /* ; */
    keypos[0x28].row = 4; keypos[0x28].col = 38; keypos[0x28].len = 1;  /* ' */
    keypos[0x1C].row = 4; keypos[0x1C].col = 41; keypos[0x1C].len = 5;  /* Enter */

    keypos[0x2A].row = 5; keypos[0x2A].col = 2;  keypos[0x2A].len = 6;  /* LShift */
    keypos[0x2C].row = 5; keypos[0x2C].col = 10; keypos[0x2C].len = 1;  /* z */
    keypos[0x2D].row = 5; keypos[0x2D].col = 13; keypos[0x2D].len = 1;  /* x */
    keypos[0x2E].row = 5; keypos[0x2E].col = 16; keypos[0x2E].len = 1;  /* c */
    keypos[0x2F].row = 5; keypos[0x2F].col = 19; keypos[0x2F].len = 1;  /* v */
    keypos[0x30].row = 5; keypos[0x30].col = 22; keypos[0x30].len = 1;  /* b */
    keypos[0x31].row = 5; keypos[0x31].col = 25; keypos[0x31].len = 1;  /* n */
    keypos[0x32].row = 5; keypos[0x32].col = 28; keypos[0x32].len = 1;  /* m */
    keypos[0x33].row = 5; keypos[0x33].col = 31; keypos[0x33].len = 1;  /* , */
    keypos[0x34].row = 5; keypos[0x34].col = 34; keypos[0x34].len = 1;  /* . */
    keypos[0x35].row = 5; keypos[0x35].col = 37; keypos[0x35].len = 1;  /* / */
    keypos[0x36].row = 5; keypos[0x36].col = 40; keypos[0x36].len = 6;  /* RShift */

    keypos[0x1D].row = 6; keypos[0x1D].col = 2;  keypos[0x1D].len = 5;  /* LCtrl */
    keypos[0x38].row = 6; keypos[0x38].col = 9;  keypos[0x38].len = 4;  /* LAlt */
    keypos[0x39].row = 6; keypos[0x39].col = 15; keypos[0x39].len = 18; /* Space */
    keypos[99].row   = 6; keypos[99].col   = 35; keypos[99].len   = 4;  /* RAlt */
    keypos[100].row  = 6; keypos[100].col  = 41; keypos[100].len  = 5;  /* RCtrl */
}

static void init_groups(void) {
    short i;
    ngroups = 0;
    for (i = 0; i < 128; i++) kstate[i] = KS_UP;
    for (i = 0; i < NGROUPS_MAX; i++) prev_gs[i] = KS_UP;
    for (i = 0; i < 128; i++) prev_kc[i] = KS_UP;

    /* Row 1 */
    add_group(1, 1,  5,  1, sc_esc);
    add_group(1, 7,  10, 4, sc_f14);
    add_group(1, 17, 9,  4, sc_f58);
    add_group(1, 26, 11, 4, sc_f912);
    add_group(1, 41, 5,  1, sc_del);

    /* Row 2 - compact, no spaces between number keys */
    add_group(2, 1,  3,  1, _1);     /* ` */
    add_group(2, 4,  3,  1, _2);     /* 1 */
    add_group(2, 7,  3,  1, _3);     /* 2 */
    add_group(2, 10, 3,  1, _4);     /* 3 */
    add_group(2, 13, 3,  1, _5);     /* 4 */
    add_group(2, 16, 3,  1, _6);     /* 5 */
    add_group(2, 19, 3,  1, _7);     /* 6 */
    add_group(2, 22, 3,  1, _8);     /* 7 */
    add_group(2, 25, 3,  1, _9);     /* 8 */
    add_group(2, 28, 3,  1, _10);    /* 9 */
    add_group(2, 31, 3,  1, _11);    /* 0 */
    add_group(2, 34, 3,  1, _12);    /* - */
    add_group(2, 37, 3,  1, _13);    /* + */
    add_group(2, 41, 6,  1, sc_bksp);

    /* Row 3 */
    add_group(3, 1,  5,  1, sc_tab);
    add_group(3, 6,  3,  1, sc_q);
    add_group(3, 9,  3,  1, sc_w);
    add_group(3, 12, 3,  1, sc_e);
    add_group(3, 15, 3,  1, sc_r);
    add_group(3, 18, 3,  1, sc_t);
    add_group(3, 21, 3,  1, sc_y);
    add_group(3, 24, 3,  1, sc_u);
    add_group(3, 27, 3,  1, sc_i);
    add_group(3, 30, 3,  1, sc_o);
    add_group(3, 33, 3,  1, sc_p);
    add_group(3, 38, 3,  1, sc_obr);
    add_group(3, 41, 3,  1, sc_cbr);
    add_group(3, 44, 3,  1, sc_pipe);

    /* Row 4 */
    add_group(4, 1,  6,  1, sc_capa);
    add_group(4, 7,  3,  1, sc_a);
    add_group(4, 10, 3,  1, sc_s);
    add_group(4, 13, 3,  1, sc_d);
    add_group(4, 16, 3,  1, sc_f);
    add_group(4, 19, 3,  1, sc_g);
    add_group(4, 22, 3,  1, sc_h);
    add_group(4, 25, 3,  1, sc_j);
    add_group(4, 28, 3,  1, sc_k);
    add_group(4, 31, 3,  1, sc_l);
    add_group(4, 34, 3,  1, sc_semi);
    add_group(4, 37, 3,  1, sc_quot);
    add_group(4, 40, 7,  1, sc_enter);

    /* Row 5 */
    add_group(5, 1,  8,  1, sc_lshift);
    add_group(5, 9,  3,  1, sc_z);
    add_group(5, 12, 3,  1, sc_x);
    add_group(5, 15, 3,  1, sc_c);
    add_group(5, 18, 3,  1, sc_v);
    add_group(5, 21, 3,  1, sc_b);
    add_group(5, 24, 3,  1, sc_n);
    add_group(5, 27, 3,  1, sc_m);
    add_group(5, 30, 3,  1, sc_comma);
    add_group(5, 33, 3,  1, sc_dot);
    add_group(5, 36, 3,  1, sc_slash);
    add_group(5, 39, 8,  1, sc_rshift);

    /* Row 6 */
    add_group(6, 1,  7,  1, sc_lctrl);
    add_group(6, 8,  6,  1, sc_lalt);
    add_group(6, 14, 20, 1, sc_space);
    add_group(6, 34, 6,  1, sc_ralt);
    add_group(6, 40, 7,  1, sc_rctrl);
}

/* ── Group state: aggregate KS_* for a group ── */
static short group_state(short idx) {
    short i;
    unsigned char has_red = 0, has_grn = 0, has_ylw = 0, s;
    KbdGroup *g = &groups[idx];
    for (i = 0; i < g->nsc; i++) {
        short sc = g->scs[i];
        if (sc >= 0 && sc < 128) {
            s = kstate[sc];
            if (s == KS_RED) has_red = 1;
            else if (s == KS_GRN) has_grn = 1;
            else if (s == KS_YLW) has_ylw = 1;
        }
    }
    if (has_red) return KS_RED;
    if (has_grn) return KS_GRN;
    if (has_ylw) return KS_YLW;
    return KS_UP;
}

/* ── Draw one group with state-based fg/bg ── */
static void draw_group(short idx) {
    KbdGroup *g = &groups[idx];
    char buf[24];
    short st, fg, bg;

    st = group_state(idx);
    switch (st) {
        case KS_RED: fg = 15; bg = 4; break;
        case KS_GRN: fg = 15; bg = 2; break;
        case KS_YLW: fg = 0;  bg = 6; break;
        default:
            fg = pressed[g->scs[0]] ? 8 : 7;
            bg = 0;
            break;
    }

    _setbkcolor((long)bg);
    _settextcolor(fg);
    _settextposition(g->row, g->col);

    strncpy(buf, kb_rows[g->row - 1] + g->col - 1, g->len);
    buf[g->len] = 0;
    _outtext(buf);
}

/* ── Draw one key by scancode with state-based fg/bg ── */
static void draw_key(short sc) {
    KeyPos *kp;
    char buf[24];
    short fg, bg;
    unsigned char ks;

    if (sc < 0 || sc >= 128) return;
    kp = &keypos[sc];
    if (kp->row < 0) return;
    ks = kstate[sc];

    switch (ks) {
        case KS_RED: fg = 15; bg = 4; break;
        case KS_GRN: fg = 15; bg = 2; break;
        case KS_YLW: fg = 0;  bg = 6; break;
        default:
            fg = pressed[sc] ? 8 : 7;
            bg = 0;
            break;
    }

    _setbkcolor((long)bg);
    _settextcolor(fg);
    _settextposition(kp->row, kp->col);

    strncpy(buf, kb_rows[kp->row - 1] + kp->col - 1, kp->len);
    buf[kp->len] = 0;
    _outtext(buf);
}

static void clear_screen(void);

/* ── Draw all keyboard rows (initial) ── */
static void draw_bg(void) {
    short i;
    clear_screen();

    for (i = 0; i < 6; i++) {
        _setbkcolor(0L);
        _settextcolor(7);
        _settextposition(i + 1, 1);
        _outtext(kb_rows[i]);
    }
}

static void clear_screen(void) {
    union REGS r;
    r.h.ah = 0x06;
    r.h.al = 0;
    r.h.bh = 0x07;
    r.w.cx = 0;
    r.h.dh = 24;
    r.h.dl = 79;
    int86(0x10, &r, &r);
}

/* ── Log ── */
static void log_add(const char *msg) {
    strncpy(log_buf[log_idx], msg, 54);
    log_buf[log_idx][54] = 0;
    log_idx = (log_idx + 1) % LOG_N;
    if (log_cnt < LOG_N) log_cnt++;
}

static void draw_log(void) {
    short i, n;
    n = log_cnt < LOG_SHOWN ? log_cnt : LOG_SHOWN;
    for (i = 0; i < LOG_SHOWN; i++) {
        _setbkcolor(0L);
        _settextcolor(7);
        _settextposition(10 + i, 2);
        _outtext("                                                              ");
    }
    for (i = 0; i < n; i++) {
        short idx = (log_idx - n + i + LOG_N) % LOG_N;
        _settextposition(10 + i, 2);
        _outtext(log_buf[idx]);
    }
}

static void log_event(short scan, short ascii, short dir) {
    char buf[56];
    const char *dirs[4] = {"", "Down", "Hold", "Up"};
    if (ascii >= 32 && ascii <= 126) {
        sprintf(buf, "'%c' asc=%d sc=0x%02X %s [#%lu]", (char)ascii, ascii, scan, dirs[dir], g_frame);
    } else {
        static const char *fn[14] = {"","F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","","",""};
        const char *ext = "";
        if (ascii == 0 && scan >= 0x3B && scan <= 0x44)
            ext = fn[scan - 0x3A];
        else if (ascii == 0 && (scan == 0x57 || scan == 0x85))
            ext = "F11";
        else if (ascii == 0 && (scan == 0x58 || scan == 0x86))
            ext = "F12";
        else if (ascii == 13) ext = "Enter";
        else if (ascii == 8)  ext = "BkSp";
        else if (ascii == 9)  ext = "Tab";
        else if (ascii == 27) ext = "Esc";
        else if (scan == 0x2A) ext = "LShift";
        else if (scan == 0x36) ext = "RShift";
        else if (scan == 0x1D) ext = "LCtrl";
        else if (scan == 100)  ext = "RCtrl";
        else if (scan == 0x38) ext = "LAlt";
        else if (scan == 99)   ext = "RAlt";
        else if (scan == 0x3A) ext = "Caps";
        if (ext[0])
            sprintf(buf, "%s sc=0x%02X %s [#%lu]", ext, scan, dirs[dir], g_frame);
        else
            sprintf(buf, "sc=0x%02X %s [#%lu]", scan, dirs[dir], g_frame);
    }
    log_add(buf);
    draw_log();
}

/* ── Mouse ── */
static void init_mouse(void) {
    union REGS r;
    r.w.ax = 0;
    int86(0x33, &r, &r);
    has_mouse = (r.w.ax == 0xFFFF);
    if (has_mouse) {
        r.w.ax = 1;
        int86(0x33, &r, &r);
    }
}

static void draw_mouse_status(short x, short y, short btns) {
    char buf[48];

    _setbkcolor(0L);
    _settextcolor(7);
    _settextposition(8, 2);
    sprintf(buf, "X=%-4d Y=%-4d", x, y);
    _outtext(buf);

    _settextposition(8, 20);
    _settextcolor(btns & 1 ? 12 : 8);
    sprintf(buf, "L=%-1d", btns & 1 ? 1 : 0);
    _outtext(buf);
    _settextposition(8, 25);
    _settextcolor(btns & 4 ? 12 : 8);
    sprintf(buf, "M=%-1d", btns & 4 ? 1 : 0);
    _outtext(buf);
    _settextposition(8, 30);
    _settextcolor(btns & 2 ? 12 : 8);
    sprintf(buf, "R=%-1d", btns & 2 ? 1 : 0);
    _outtext(buf);
}

static void draw_mouse(void) {
    short x = 0, y = 0, btns = 0;

    if (has_mouse) {
        union REGS r;
        r.w.ax = 3;
        int86(0x33, &r, &r);
        x = r.w.cx;
        y = r.w.dx;
        btns = r.w.bx & 7;
    }

    if (x == prev_mx && y == prev_my && btns == prev_mbtn)
        return;
    prev_mx = x; prev_my = y; prev_mbtn = btns;

    draw_mouse_status(x, y, btns);

    /* Redraw mouse body with button indicators */
    _setbkcolor(0L);
    _settextposition(4, 50);
    _outtext("\xB3");
    _settextcolor(btns & 1 ? 12 : 8);
    _outtext("\xDB");
    _settextcolor(btns & 4 ? 12 : 8);
    _outtext("\xDB");
    _settextcolor(btns & 2 ? 12 : 8);
    _outtext("\xDB");
    _settextcolor(7);
    _outtext("\xB3");
}

/* ── Keyboard event handling ── */
static void format_last(unsigned char ascii, unsigned char scan, const char *evt) {
    char b[2];
    if (ascii >= 32 && ascii <= 126) {
        b[0] = (char)ascii; b[1] = 0;
        sprintf(last_str, "'%s' asc=%d sc=0x%02X %s [F#%lu]", b, ascii, scan, evt, g_frame);
    } else if (ascii == 13) {
        sprintf(last_str, "Enter asc=%d sc=0x%02X %s [F#%lu]", ascii, scan, evt, g_frame);
    } else if (ascii == 8) {
        sprintf(last_str, "BkSp asc=%d sc=0x%02X %s [F#%lu]", ascii, scan, evt, g_frame);
    } else if (ascii == 9) {
        sprintf(last_str, "Tab asc=%d sc=0x%02X %s [F#%lu]", ascii, scan, evt, g_frame);
    } else if (ascii == 27) {
        sprintf(last_str, "Esc asc=%d sc=0x%02X %s [F#%lu]", ascii, scan, evt, g_frame);
    } else if (ascii == 0) {
        static const char *fn[14] = {"","F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","","",""};
        const char *ext = "";
        if (scan >= 0x3B && scan <= 0x44)
            ext = fn[scan - 0x3A];
        else if (scan == 0x57 || scan == 0x85)
            ext = "F11";
        else if (scan == 0x58 || scan == 0x86)
            ext = "F12";
        sprintf(last_str, "%s sc=0x%02X %s [F#%lu]", ext, scan, evt, g_frame);
    } else {
        sprintf(last_str, "asc=%d sc=0x%02X %s [F#%lu]", ascii, scan, evt, g_frame);
    }
}

static void handle_key(unsigned k) {
    unsigned char ascii, scan;
    ascii = k & 0xFF;
    scan = k >> 8;

    format_last(ascii, scan, "DN");

    if (scan == 0x44 || scan == 0x01)
        done = 1;

    if (scan < 128) {
        pressed[scan] = 1;

        if (scan == last_phy_scan && kstate[scan] != KS_UP) {
            hl[scan] = HL_FRAMES;
            rel[scan] = 0;
        } else {
            hl[scan] = HL_FRAMES;
            rel[scan] = 0;
            last_ascii = ascii;
            last_scan = scan;
            kstate[scan] = KS_GRN;
            log_event(scan, ascii, 1);
        }
        last_phy_scan = scan;
    }
}

/* ── Modifier polling ── */
static void poll_mods(void) {
    short i;
    union REGS r;
    static const char *ml[7] = {"RSh","LSh","LCt","LAl","RCt","RAl","Cap"};
    static short scs[7] = {0x36, 0x2A, 0x1D, 0x38, 100, 99, 0x3A};
    r.w.ax = 0x1200;
    r.w.bx = 0;
    r.w.cx = 0;
    r.w.dx = 0;
    int86(0x16, &r, &r);
    for (i = 0; i < 7; i++) {
        short st, old = prev_mod_state[i];
        if (i < 2)      st = (r.h.al >> i) & 1;
        else if (i < 4) st = (r.h.ah >> (i - 2)) & 1;
        else if (i < 6) st = (r.h.ah >> (i - 2)) & 1;
        else            st = (r.h.al & 64) ? 1 : 0;

        if (st != old) {
            prev_mod_state[i] = st;
            if (st) {
                hl[scs[i]] = HL_FRAMES;
                kstate[scs[i]] = KS_GRN;
                sprintf(last_str, "%s sc=0x%02X DN", ml[i], scs[i]);
                log_event(scs[i], 0, 1);
            } else {
                hl[scs[i]] = 0;
                kstate[scs[i]] = KS_RED;
                rel[scs[i]] = REL_FRAMES;
                sprintf(last_str, "%s sc=0x%02X UP", ml[i], scs[i]);
                log_event(scs[i], 0, 3);
            }
        } else if (st) {
            if (hl[scs[i]] < HL_FRAMES - 1) hl[scs[i]] = HL_FRAMES - 1;
        }
    }
}

/* ── Status line (key info on row 7) ── */
static void update_status(void) {
    char buf[80];
    short len;
    sprintf(buf, "%s", last_str);
    if (strcmp(buf, prev_stat_str) != 0) {
        strcpy(prev_stat_str, buf);
        len = strlen(buf);
        if (len > 70) len = 70;
        buf[len] = 0;

        _setbkcolor(0L);
        _settextcolor(15);
        _settextposition(7, 2);
        _outtext("                                                                      ");
        _settextposition(7, 2);
        _outtext(buf);
    }
}

/* ── Main ── */
int main(void) {
    short i;
    unsigned long tick, prev_tick = 0;
    union REGS tr;

    done = 0;
    log_idx = 0; log_cnt = 0;
    last_phy_scan = 0;
    last_ascii = 0; last_scan = 0;
    last_str[0] = 0; prev_stat_str[0] = 0;
    g_frame = 0;
    memset(hl, 0, sizeof(hl));
    memset(rel, 0, sizeof(rel));
    memset(pressed, 0, sizeof(pressed));
    memset(prev_mod_state, -1, sizeof(prev_mod_state));
    memset(prev_log_ks, 0, sizeof(prev_log_ks));

    _setvideomode(3);
    init_keypos();
    init_groups();
    draw_bg();
    init_mouse();

    /* Draw info area header */
    _setbkcolor(0L);
    _settextcolor(15);
    _settextposition(7, 1);
    _outtext(">");
    _settextposition(8, 1);
    _outtext(">");
    _settextposition(9, 1);
    _outtext("-Log-");

    while (!done) {
        g_frame++;

        /* Gate decrement on system tick (~18.2 Hz) for visual colors */
        tr.h.ah = 0;
        int86(0x1A, &tr, &tr);
        tick = ((unsigned long)tr.w.cx << 16) | tr.w.dx;
        if (tick != prev_tick) {
            prev_tick = tick;
            for (i = 0; i < 128; i++) {
                if (hl[i] > 0) {
                    hl[i]--;
                    if (kstate[i] == KS_GRN && hl[i] <= HL_FRAMES - 2)
                        kstate[i] = KS_YLW;
                    if (hl[i] == 0) {
                        kstate[i] = KS_RED;
                        rel[i] = REL_FRAMES;
                    }
                }
                if (rel[i] > 0) {
                    rel[i]--;
                    if (rel[i] == 0 && kstate[i] == KS_RED)
                        kstate[i] = KS_UP;
                }
            }
        }

        /* Log transitions immediately (outside tick gate) */
        for (i = 0; i < 128; i++) {
            unsigned char ks = kstate[i];
            if (ks != prev_log_ks[i]) {
                prev_log_ks[i] = ks;
                if (i == last_scan) {
                    if (ks == KS_YLW) {
                        format_last(last_ascii, i, "HLD");
                        log_event(i, last_ascii, 2);
                    } else if (ks == KS_RED) {
                        format_last(last_ascii, i, "UP");
                        log_event(i, last_ascii, 3);
                    }
                }
            }
        }

        draw_mouse();

        /* Poll keyboard */
        while (_bios_keybrd(_KEYBRD_READY)) {
            unsigned k = _bios_keybrd(_KEYBRD_READ);
            handle_key(k);
        }

        poll_mods();

        /* Redraw changed keys (per-key, not group) */
        for (i = 0; i < 128; i++) {
            if (kstate[i] != prev_kc[i]) {
                prev_kc[i] = kstate[i];
                draw_key(i);
            }
        }

        update_status();
    }

    _setvideomode(_DEFAULTMODE);
    return 0;
}
