/*
 * Host test of the ELKS BIOS-keyboard driver (elks/arch/i86/drivers/char/kbd-poll.c, used by the
 * expanded-PCjr kernel) with elks-graphics-keys.patch: the real kbd_timer() is compiled against stub
 * kernel headers and fed INT 16h results (scan code in AH, character in AL) and BIOS shift flags; what
 * it sends to the console and whether it switches consoles is checked with and without a graphics
 * program holding the console. Build: see wslscripts/kbdpoll.sh.
 */
#include <stdio.h>
#include <string.h>

static unsigned feed;
static unsigned char shiftflags;
static char out[16];
static int nout, vc_switched, graphics;

void Console_conin(unsigned char k) { if (nout < 15) out[nout++] = (char)k; out[nout] = 0; }
void Console_set_vc(int n) { if (!graphics) vc_switched = n; }   /* the real one refuses under the lock */
int Console_graphics(void) { return graphics; }
int conio_poll(void) { unsigned v = feed; feed = 0; return (int)v; }
void conio_init(void) { }
unsigned char peekb(unsigned off, unsigned seg) { return off == 0x17 && seg == 0x40 ? shiftflags : 0; }

#include KBD_C

static int failures, tests;

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

int main(void)
{
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
    printf("== kbd-poll: %d passed, %d failed\n", tests - failures, failures);
    return failures != 0;
}
