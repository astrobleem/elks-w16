/*
 * Host test of the ELKS BIOS-keyboard driver (elks/arch/i86/drivers/char/kbd-poll.c, used by the
 * expanded-PCjr kernel) with elks-graphics-keys.patch: the real kbd_timer() is compiled against stub
 * kernel headers and fed INT 16h results (scan code in AH, character in AL) and BIOS shift flags; what
 * it sends to the console and whether it switches consoles is checked with and without a graphics
 * program holding the console. The BIOS key buffer (BDA 41Eh-43Dh, head 41Ah, tail 41Ch) is emulated
 * too, so keys can wait in it while Alt changes: the driver must take Alt+Space's Alt state when the
 * key was first observed in graphics mode, not when it reads it. Graphics/text transitions and
 * complete ring wraps must not reuse that observation for a later key. Build: tests/kbdtest.sh.
 */
#include <stdio.h>
#include <string.h>

static unsigned feed;                   /* a key returned without the buffer (the plain cases) */
static unsigned char bda[256];          /* segment 40h: shift flags at 17h, the key buffer */
#define shiftflags bda[0x17]
static char out[16];
static int nout, vc_switched, graphics;

void Console_conin(unsigned char k) { if (nout < 15) out[nout++] = (char)k; out[nout] = 0; }
void Console_set_vc(int n) { if (!graphics) vc_switched = n; }   /* the real one refuses under the lock */
int Console_graphics(void) { return graphics; }
static unsigned bdaw(unsigned off) { return bda[off] | bda[off + 1] << 8; }
static void setw(unsigned off, unsigned v) { bda[off] = v & 0xFF; bda[off + 1] = v >> 8; }
/* INT 16h AH=01h then AH=00h: the key at the head, which then moves on */
int conio_poll(void)
{
    unsigned v = feed, h = bdaw(0x1A);
    if (v) {
        feed = 0;
        return (int)v;
    }
    if (h == bdaw(0x1C))
        return 0;
    v = bdaw(h);
    setw(0x1A, h + 2 >= 0x3E ? 0x1E : h + 2);
    return (int)v;
}
/* the BIOS keyboard interrupt puts a key in the buffer */
static void bios_key(unsigned ax)
{
    unsigned tl = bdaw(0x1C);
    setw(tl, ax);
    setw(0x1C, tl + 2 >= 0x3E ? 0x1E : tl + 2);
}
void conio_init(void) { }
unsigned char peekb(unsigned off, unsigned seg) { return seg == 0x40 && off < 256 ? bda[off] : 0; }
unsigned short peekw(unsigned short off, unsigned short seg) { return seg == 0x40 && off < 255 ? bdaw(off) : 0; }

#include KBD_C

static int failures, tests;

/* one timer run with the buffer: what it sent */
static void poll_tick(int g)
{
    graphics = g; nout = 0; out[0] = 0; vc_switched = -1;
    kbd_timer(0);
}

static int sent(const char *want)
{
    return nout == (int)strlen(want) && !memcmp(out, want, nout);
}

static void tick(int g, const char *what, const char *want)
{
    int i;
    poll_tick(g);
    tests++;
    if (!sent(want)) {
        failures++;
        printf("FAIL %-48s sent %d bytes", what, nout);
        for (i = 0; i < nout; i++)
            printf(" %02x", (unsigned char)out[i]);
        printf("\n");
    } else
        printf("ok   %s\n", what);
}
static void expect(int cond, const char *what)
{
    tests++;
    if (!cond)
        failures++;
    printf("%s %s\n", cond ? "ok  " : "FAIL", what);
}

static void key(int g, const char *what, unsigned ax, unsigned char flags, const char *want, int want_vc)
{
    int i;
    graphics = g; shiftflags = flags; feed = ax;
    nout = 0; out[0] = 0; vc_switched = -1;
    kbd_timer(0);
    tests++;
    if (nout != (int)strlen(want) || memcmp(out, want, nout) || vc_switched != want_vc) {
        failures++;
        printf("FAIL %-30s graphics=%d: sent %d bytes", what, g, nout);
        for (i = 0; i < nout; i++)
            printf(" %02x", (unsigned char)out[i]);
        printf(", console switch %d (want %d)\n", vc_switched, want_vc);
    } else
        printf("ok   %-30s graphics=%d\n", what, g);
}

/* Cache a queued Space, let text mode consume it, then reuse its slot after one or more
 * complete ring wraps. The new Space must use the new Alt state, in either direction.
 * Exercise every initial slot, partial/no wrap, and repeated full wraps without touching
 * the driver's private cache. Each setup step checks the driver's actual console output. */
static void transition_wrap(unsigned start, int old_alt, int text_keys)
{
    int i, setup = 1;
    char what[96];

    setw(0x1A, start); setw(0x1C, start);
    feed = 0;
    poll_tick(0); setup &= sent("");
    poll_tick(1); setup &= sent("");   /* first graphics sample: empty buffer */
    shiftflags = old_alt;
    bios_key(0x2D78); bios_key(0x3920);
    poll_tick(1); setup &= sent("x"); /* queued Space now has a cached Alt observation */
    shiftflags = old_alt ^ 0x08;
    poll_tick(0); setup &= sent(" "); /* text consumes Space, regardless of Alt */
    for (i = 0; i < text_keys; i++) {
        bios_key(0x1E61);
        poll_tick(0); setup &= sent("a");
    }
    bios_key(0x3920);
    poll_tick(1);
    sprintf(what, "transition at %02x, %d text keys, Alt %d -> %d", start, text_keys,
            old_alt != 0, shiftflags != 0);
    expect(setup && sent(shiftflags ? "\033 " : " "), what);
}

int main(void)
{
    unsigned start;
    int old_alt, n;
    static const int text_keys[] = { 0, 15, 31, 47 };
    setw(0x1A, 0x1E); setw(0x1C, 0x1E);    /* empty buffer */
    /* text mode: unchanged */
    key(0, "F1 switches to console 1", 0x3B00, 0, "", 0);
    key(0, "Alt+F2 switches to console 2", 0x6900, 0x08, "", 1);
    key(0, "F10 is ESC [ j (ANSI)", 0x4400, 0, "\033[j", -1);
    key(0, "Alt+S is dropped", 0x1F00, 0x08, "", -1);
    key(0, "Alt+Space is a space", 0x3920, 0x08, " ", -1);
    key(0, "a", 0x1E61, 0, "a", -1);
    key(0, "Up is ESC [ A", 0x4800, 0, "\033[A", -1);
    /* a graphics program holds the console */
    key(1, "F1 is ESC a", 0x3B00, 0, "\033a", -1);
    key(1, "F4 is ESC d", 0x3E00, 0, "\033d", -1);
    key(1, "F10 is ESC j", 0x4400, 0, "\033j", -1);
    key(1, "Alt+F4 is ESC p", 0x6B00, 0x08, "\033p", -1);
    key(1, "Alt+F10 is ESC v", 0x7100, 0x08, "\033v", -1);
    key(1, "Alt+S is ESC S", 0x1F00, 0x08, "\033S", -1);
    key(1, "Alt+G is ESC G", 0x2200, 0x08, "\033G", -1);
    key(1, "Alt+M is ESC M", 0x3200, 0x08, "\033M", -1);
    key(1, "Alt+Space is ESC space", 0x3920, 0x08, "\033 ", -1);
    key(1, "Space is a space", 0x3920, 0, " ", -1);
    key(1, "a", 0x1E61, 0, "a", -1);
    key(1, "Up is ESC [ A", 0x4800, 0, "\033[A", -1);
    key(1, "Enter", 0x1C0D, 0, "\r", -1);
    /* Alt+Space timing: keys wait in the BIOS buffer while Alt changes (graphics mode) */
    shiftflags = 0x08;                  /* Alt down: x, then Alt+Space, typed before the next run */
    bios_key(0x2D78); bios_key(0x3920);
    tick(1, "x read first (Alt still down)", "x");
    shiftflags = 0;                     /* Alt released before Alt+Space is read */
    tick(1, "Alt+Space read after Alt was released", "\033 ");
    tick(1, "nothing left", "");
    bios_key(0x2D78); bios_key(0x3920); /* x, then a plain Space, typed before the next run */
    tick(1, "x read first", "x");
    shiftflags = 0x08;                  /* Alt pressed before the Space is read */
    tick(1, "a Space read after Alt went down is a space", " ");
    shiftflags = 0;
    bios_key(0x3920);
    tick(1, "Space entered and read on one run, Alt up", " ");
    shiftflags = 0x08;
    bios_key(0x3920);
    tick(1, "Alt+Space entered and read on one run", "\033 ");
    /* text mode: Alt+Space stays a space whatever Alt did */
    bios_key(0x2D78); bios_key(0x3920); tick(0, "text mode: x", "x");
    shiftflags = 0; tick(0, "text mode: Alt+Space is a space", " ");
    /* Session boundaries must discard cached slot identities, even after an exact wrap. */
    for (start = 0x1E; start < 0x3E; start += 2)
        for (old_alt = 0; old_alt <= 8; old_alt += 8)
            for (n = 0; n < 4; n++)
                transition_wrap(start, old_alt, text_keys[n]);
    /* Empty transitions must not break normal sampling in the next graphics session. */
    tick(0, "empty text poll between graphics sessions", "");
    tick(1, "empty new graphics session", "");
    shiftflags = 0x08;
    bios_key(0x2D78); bios_key(0x3920);
    tick(1, "new session caches queued Alt+Space", "x");
    shiftflags = 0;
    tick(1, "new session keeps sampled Alt after release", "\033 ");
    /* the poll interval: every tick under graphics (to see keys as they arrive), 80 ms otherwise */
    jiffies = 1000;
    graphics = 1; kbd_timer(0); expect(timer_due == 1001, "graphics: the driver runs again on the next tick");
    graphics = 0; kbd_timer(0); expect(timer_due == 1008, "text mode: the driver runs again after 8 ticks");
    printf("== kbd-poll: %d passed, %d failed\n", tests - failures, failures);
    return failures != 0;
}
