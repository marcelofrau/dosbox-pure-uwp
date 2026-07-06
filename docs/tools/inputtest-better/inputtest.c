#include <conio.h>
#include <graph.h>
#include <bios.h>
#include <i86.h>
#include <stdio.h>
#include <string.h>

#define KW 38
#define KH 27
#define GAP 4
#define STEP (KW + GAP)
#define X0 14
#define Y0 50
#define HL_FRAMES 180
#define REL_FRAMES 30

#define KS_UP 0
#define KS_GRN 1
#define KS_YLW 2
#define KS_RED 3

#define C_PRE  10
#define C_HPRE 2
#define C_HLD  14
#define C_HHLD 12
#define C_REL  12
#define C_HREL 4
#define C_DFL  8
#define C_HDFL 7
#define C_PRV  7
#define C_HPRV 15

#define LOG_Y 244
#define LOG_N 11
#define MS_X 406

typedef struct {
    short x, y, w, h;
    const char *label;
    short sc;
} Key;

static Key keys[80];
static short nkeys;
static unsigned char hl[128];
static unsigned char rel[128];
static unsigned char kstate[128];
static unsigned char prev_ks[128];
static unsigned char pressed[128];

static short last_ascii, last_scan;
static char last_str[48], prev_stat_str[48];
static short prev_mod_state[7];
static short has_mouse;
static short prev_mx, prev_my, prev_mbtn;
static short done;

static short last_dir; /* 0=idle, 1=DN, 2=HLD, 3=UP */
static char log_buf[LOG_N][44];
static short log_idx, log_cnt;

static short col_of(short col) { return X0 + col * STEP; }
static short row_of(short r) { return Y0 + r * (KH + GAP); }

static short add_key(short r, short c, short w, const char *l, short s) {
    short i = nkeys++;
    keys[i].x = col_of(c);
    keys[i].y = row_of(r);
    keys[i].w = w * STEP - GAP;
    keys[i].h = KH;
    keys[i].label = l;
    keys[i].sc = s;
    return i;
}

static void init_keys(void) {
    short i;
    for (i = 0; i < 128; i++) kstate[i] = KS_UP;
    nkeys = 0;

    add_key(0, 0, 1, "`",  0x29);
    add_key(0, 1, 1, "1",  0x02);
    add_key(0, 2, 1, "2",  0x03);
    add_key(0, 3, 1, "3",  0x04);
    add_key(0, 4, 1, "4",  0x05);
    add_key(0, 5, 1, "5",  0x06);
    add_key(0, 6, 1, "6",  0x07);
    add_key(0, 7, 1, "7",  0x08);
    add_key(0, 8, 1, "8",  0x09);
    add_key(0, 9, 1, "9",  0x0A);
    add_key(0,10, 1, "0",  0x0B);
    add_key(0,11, 1, "-",  0x0C);
    add_key(0,12, 1, "=",  0x0D);
    add_key(0,13, 2, "BkSp", 0x0E);

    add_key(1, 0, 1, "Tab", 0x0F);
    add_key(1, 1, 1, "Q",   0x10);
    add_key(1, 2, 1, "W",   0x11);
    add_key(1, 3, 1, "E",   0x12);
    add_key(1, 4, 1, "R",   0x13);
    add_key(1, 5, 1, "T",   0x14);
    add_key(1, 6, 1, "Y",   0x15);
    add_key(1, 7, 1, "U",   0x16);
    add_key(1, 8, 1, "I",   0x17);
    add_key(1, 9, 1, "O",   0x18);
    add_key(1,10, 1, "P",   0x19);
    add_key(1,11, 1, "[",   0x1A);
    add_key(1,12, 1, "]",   0x1B);
    add_key(1,13, 1, "\\",  0x2B);

    add_key(2, 0, 2, "Caps", 0x3A);
    add_key(2, 2, 1, "A",   0x1E);
    add_key(2, 3, 1, "S",   0x1F);
    add_key(2, 4, 1, "D",   0x20);
    add_key(2, 5, 1, "F",   0x21);
    add_key(2, 6, 1, "G",   0x22);
    add_key(2, 7, 1, "H",   0x23);
    add_key(2, 8, 1, "J",   0x24);
    add_key(2, 9, 1, "K",   0x25);
    add_key(2,10, 1, "L",   0x26);
    add_key(2,11, 1, ";",   0x27);
    add_key(2,12, 1, "'",   0x28);
    add_key(2,13, 2, "Ent", 0x1C);

    add_key(3, 0, 2, "LSh", 0x2A);
    add_key(3, 2, 1, "Z",   0x2C);
    add_key(3, 3, 1, "X",   0x2D);
    add_key(3, 4, 1, "C",   0x2E);
    add_key(3, 5, 1, "V",   0x2F);
    add_key(3, 6, 1, "B",   0x30);
    add_key(3, 7, 1, "N",   0x31);
    add_key(3, 8, 1, "M",   0x32);
    add_key(3, 9, 1, ",",   0x33);
    add_key(3,10, 1, ".",   0x34);
    add_key(3,11, 1, "/",   0x35);
    add_key(3,12, 2, "RSh", 0x36);

    add_key(4, 0, 1, "LCt", 0x1D);
    add_key(4, 1, 1, "   ",  -1);
    add_key(4, 2, 1, "LAl", 0x38);
    add_key(4, 3, 6, "Space", 0x39);
    add_key(4, 9, 1, "RAl", 99);
    add_key(4,10, 1, "   ",  -1);
    add_key(4,11, 1, "Mnu",  -1);
    add_key(4,12, 1, "RCt", 100);

    add_key(5, 0, 1, "F1",  0x3B);
    add_key(5, 1, 1, "F2",  0x3C);
    add_key(5, 2, 1, "F3",  0x3D);
    add_key(5, 3, 1, "F4",  0x3E);
    add_key(5, 4, 1, "F5",  0x3F);
    add_key(5, 5, 1, "F6",  0x40);
    add_key(5, 6, 1, "F7",  0x41);
    add_key(5, 7, 1, "F8",  0x42);
    add_key(5, 8, 1, "F9",  0x43);
    add_key(5, 9, 1, "F10", 0x44);
    add_key(5,10, 1, "F11", 0x57);
    add_key(5,11, 1, "F12", 0x58);
    add_key(5,12, 1, "Esc", 0x01);
    add_key(5,13, 1, "Pau", -1);
}

static void draw_rect(short x, short y, short w, short h, short fill, short bor) {
    _setcolor(fill);
    _rectangle(_GFILLINTERIOR, x, y, x + w - 1, y + h - 1);
    _setcolor(bor);
    _rectangle(_GBORDER, x, y, x + w - 1, y + h - 1);
}

static void log_add(const char *msg) {
    strncpy(log_buf[log_idx], msg, 43);
    log_buf[log_idx][43] = 0;
    log_idx = (log_idx + 1) % LOG_N;
    if (log_cnt < LOG_N) log_cnt++;
}

static void draw_log(void) {
    short i, r;
    _setcolor(0);
    _rectangle(_GFILLINTERIOR, 8, LOG_Y - 4, MS_X - 4, LOG_Y + LOG_N * 16 + 4);
    for (i = 0; i < log_cnt; i++) {
        short idx = (log_idx - 1 - i + LOG_N) % LOG_N;
        r = LOG_Y / 16 + 1 + (log_cnt - 1 - i);
        _setbkcolor(0L);
        _settextcolor(7);
        _settextposition(r, 2);
        _outtext(log_buf[idx]);
    }
}

static void log_event(short scan, short ascii, short dir) {
    char buf[44];
    const char *dirs[4] = {"", "Down", "Hold", "Up"};
    if (ascii >= 32 && ascii <= 126)
        sprintf(buf, "'%c' asc=%d sc=0x%02X %s", (char)ascii, ascii, scan, dirs[dir]);
    else {
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
            sprintf(buf, "%s sc=0x%02X %s", ext, scan, dirs[dir]);
        else
            sprintf(buf, "sc=0x%02X %s", scan, dirs[dir]);
    }
    log_add(buf);
    draw_log();
}

static void draw_key(short i) {
    Key *k = &keys[i];
    short sc = k->sc;
    short fill, bor, txtc, bkc;

    if (sc < 0) { fill = C_DFL; bor = C_HDFL; txtc = 15; bkc = 8; }
    else if (kstate[sc] == KS_RED) { fill = C_REL; bor = C_HREL; txtc = 15; bkc = 12; }
    else if (kstate[sc] == KS_GRN) { fill = C_PRE; bor = C_HPRE; txtc = 0; bkc = 10; }
    else if (kstate[sc] == KS_YLW) { fill = C_HLD; bor = C_HHLD; txtc = 0; bkc = 14; }
    else if (pressed[sc]) { fill = C_PRV; bor = C_HPRV; txtc = 15; bkc = 8; }
    else { fill = C_DFL; bor = C_HDFL; txtc = 15; bkc = 8; }

    draw_rect(k->x, k->y, k->w, k->h, fill, bor);

    {
        short len = strlen(k->label);
        short row = (k->y + k->h / 2 - 8) / 16 + 1;
        short col = (k->x + k->w / 2 - len * 4) / 8 + 1;
        _setbkcolor((long)bkc);
        _settextposition(row, col);
        _settextcolor(txtc);
        _outtext(k->label);
    }
}

static void draw_mouse_bg(void);

static void draw_bg(void) {
    short i;
    _setcolor(0);
    _rectangle(_GFILLINTERIOR, 0, 0, 639, 479);
    for (i = 0; i < nkeys; i++)
        draw_key(i);
    _setbkcolor(0L);
    _settextcolor(7);
    _settextposition(1, 1);
    _outtext("Last: ");
    draw_mouse_bg();
}

static void poll_mods(void) {
    short i;
    union REGS r;
    static const char *ml[7] = {"RSh","LSh","LCt","LAl","RCt","RAl","Caps"};
    static short scs[7] = {0x36, 0x2A, 0x1D, 0x38, 100, 99, 0x3A};
    r.w.ax = 0x1200;
    r.w.bx = 0;
    r.w.cx = 0;
    r.w.dx = 0;
    int86(0x16, &r, &r);
    for (i = 0; i < 7; i++) {
        short st, rc, old = prev_mod_state[i];
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
            if (hl[scs[i]] < 20) hl[scs[i]] = 20;
        }

        if (i < 2)      rc = st ? 15 : 7;
        else if (i < 4) rc = st ? 10 : 7;
        else if (i < 6) rc = st ? 14 : 7;
        else            rc = st ? 11 : 7;
        if (st != old) {
            _setbkcolor(0L);
            _settextcolor(rc);
            _settextposition(29, 2 + i * 6);
            _outtext("     ");
            _settextposition(29, 2 + i * 6);
            _outtext(ml[i]);
        }
    }
}

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
        _settextcolor(7);
        _settextposition(1, 1);
        _outtext("                                                                              ");
        _settextposition(1, 1);
        _outtext(buf);
    }
}

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

static void draw_mouse_bg(void) {
    short mx = MS_X + 30, my = LOG_Y + 85;

    _setbkcolor(0L);
    _settextcolor(15);
    _settextposition(LOG_Y/16+1, MS_X/8+1);
    _outtext("Mouse");

    _settextcolor(7);
    _settextposition(LOG_Y/16+3, MS_X/8+1);
    _outtext("X=   Y=   ");

    _settextposition(LOG_Y/16+5, MS_X/8+1);
    _outtext("L=0  M=0  R=0");

    _setcolor(7);
    _rectangle(_GFILLINTERIOR, mx, my, mx+49, my+59);
    _setcolor(15);
    _rectangle(_GBORDER, mx, my, mx+49, my+59);

    _setcolor(8);
    _rectangle(_GFILLINTERIOR, mx+2, my+2, mx+14, my+10);
    _setcolor(7);
    _rectangle(_GBORDER, mx+2, my+2, mx+14, my+10);

    _setcolor(8);
    _rectangle(_GFILLINTERIOR, mx+18, my+2, mx+30, my+10);
    _setcolor(7);
    _rectangle(_GBORDER, mx+18, my+2, mx+30, my+10);

    _setcolor(8);
    _rectangle(_GFILLINTERIOR, mx+35, my+2, mx+47, my+10);
    _setcolor(7);
    _rectangle(_GBORDER, mx+35, my+2, mx+47, my+10);
}

static void draw_mouse(void) {
    short x = 0, y = 0, btns = 0;
    char buf[32];
    short mx = MS_X + 30, my = LOG_Y + 85;

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

    _setbkcolor(0L);
    _settextcolor(7);
    sprintf(buf, "X=%-3d Y=%-3d", x, y);
    _settextposition(LOG_Y/16+3, MS_X/8+1);
    _outtext(buf);

    sprintf(buf, "L=%d  M=%d  R=%d", btns&1?1:0, btns&4?1:0, btns&2?1:0);
    _settextposition(LOG_Y/16+5, MS_X/8+1);
    _outtext(buf);

    _setcolor(btns & 1 ? 12 : 8);
    _rectangle(_GFILLINTERIOR, mx+2, my+2, mx+14, my+10);
    _setcolor(btns & 1 ? 15 : 7);
    _rectangle(_GBORDER, mx+2, my+2, mx+14, my+10);

    _setcolor(btns & 4 ? 12 : 8);
    _rectangle(_GFILLINTERIOR, mx+18, my+2, mx+30, my+10);
    _setcolor(btns & 4 ? 15 : 7);
    _rectangle(_GBORDER, mx+18, my+2, mx+30, my+10);

    _setcolor(btns & 2 ? 12 : 8);
    _rectangle(_GFILLINTERIOR, mx+35, my+2, mx+47, my+10);
    _setcolor(btns & 2 ? 15 : 7);
    _rectangle(_GBORDER, mx+35, my+2, mx+47, my+10);
}

static void format_last(unsigned char ascii, unsigned char scan, const char *evt) {
    char b[2];
    if (ascii >= 32 && ascii <= 126) {
        b[0] = (char)ascii; b[1] = 0;
        sprintf(last_str, "'%s' asc=%d sc=0x%02X %s", b, ascii, scan, evt);
    } else if (ascii == 13) {
        sprintf(last_str, "Enter asc=%d sc=0x%02X %s", ascii, scan, evt);
    } else if (ascii == 8) {
        sprintf(last_str, "BkSp asc=%d sc=0x%02X %s", ascii, scan, evt);
    } else if (ascii == 9) {
        sprintf(last_str, "Tab asc=%d sc=0x%02X %s", ascii, scan, evt);
    } else if (ascii == 27) {
        sprintf(last_str, "Esc asc=%d sc=0x%02X %s", ascii, scan, evt);
    } else if (ascii == 0) {
        static const char *fn[14] = {"","F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","","",""};
        const char *ext = "";
        if (scan >= 0x3B && scan <= 0x44)
            ext = fn[scan - 0x3A];
        else if (scan == 0x57 || scan == 0x85)
            ext = "F11";
        else if (scan == 0x58 || scan == 0x86)
            ext = "F12";
        sprintf(last_str, "%s sc=0x%02X %s", ext, scan, evt);
    } else {
        sprintf(last_str, "asc=%d sc=0x%02X %s", ascii, scan, evt);
    }
}

static void handle_key(unsigned k) {
    unsigned char ascii, scan;

    ascii = k & 0xFF;
    scan = k >> 8;

    last_ascii = ascii;
    last_scan = scan;

    format_last(ascii, scan, "DN");

    if (scan == 0x44 || scan == 0x01)
        done = 1;

    if (scan < 128) {
        hl[scan] = HL_FRAMES;
        rel[scan] = 0;
        pressed[scan] = 1;
        if (kstate[scan] == KS_YLW) {
            ; /* repeat — stays yellow */
        } else {
            if (kstate[scan] == KS_RED)
                kstate[scan] = KS_YLW;
            else
                kstate[scan] = KS_GRN;
            last_dir = 1;
            log_event(scan, ascii, 1);
        }
    }
}

int main(void) {
    short i;
    done = 0;
    last_dir = 0;
    log_idx = 0;
    log_cnt = 0;

    _setvideomode(_VRES16COLOR);
    memset(hl, 0, sizeof(hl));
    memset(rel, 0, sizeof(rel));
    memset(kstate, 0, sizeof(kstate));
    memset(prev_ks, 0, sizeof(prev_ks));
    memset(pressed, 0, sizeof(pressed));
    memset(prev_mod_state, -1, sizeof(prev_mod_state));
    prev_stat_str[0] = 0;
    last_ascii = 0;
    last_scan = 0;
    last_str[0] = 0;

    init_keys();
    draw_bg();
    init_mouse();

    while (!done) {
        draw_mouse();

        while (_bios_keybrd(_KEYBRD_READY)) {
            unsigned k = _bios_keybrd(_KEYBRD_READ);
            handle_key(k);
        }

        poll_mods();

        for (i = 0; i < nkeys; i++) {
            short sc = keys[i].sc;
            if (sc < 0 || sc >= 128) continue;
            if (kstate[sc] != prev_ks[sc]) {
                draw_key(i);
                prev_ks[sc] = kstate[sc];
            }
        }

        update_status();

        for (i = 0; i < 128; i++) {
            if (hl[i] > 0) {
                hl[i]--;
                if (kstate[i] == KS_GRN && hl[i] < HL_FRAMES - 40) {
                    kstate[i] = KS_YLW;
                    if (i == last_scan) {
                        format_last(last_ascii, i, "HLD");
                        last_dir = 2;
                        log_event(i, last_ascii, 2);
                    }
                }
                if (hl[i] == 0) {
                    kstate[i] = KS_RED;
                    rel[i] = REL_FRAMES;
                    if (i == last_scan) {
                        format_last(last_ascii, i, "UP");
                        last_dir = 3;
                        log_event(i, last_ascii, 3);
                    }
                }
            }
            if (rel[i] > 0) {
                rel[i]--;
                if (rel[i] == 0 && kstate[i] == KS_RED)
                    kstate[i] = KS_UP;
            }
        }

        ;
    }

    _setvideomode(_DEFAULTMODE);
    return 0;
}
