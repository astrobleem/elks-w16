/*
 * Host test of the ELKS scancode keyboard driver (elks/arch/i86/drivers/char/kbd-scancode.c) with
 * elks-graphics-keys.patch: the real keyboard_irq() is compiled here against stub kernel headers and
 * fed make/break scancode sequences, and what it sends to the console (Console_conin) and whether it
 * switches consoles (Console_set_vc) is checked, with and without a graphics program holding the
 * console. Build: cc -I STUBDIR -include kbdscan_stubs.h -DKBD_C='"<path>/kbd-scancode.c"' kbdscan_test.c
 */
#include <stdio.h>
#include <string.h>

static unsigned char feed[16];
static int nfeed, pfeed;
static char out[64];
static int nout, vc_switched = -1, graphics;

int kraw;
void Console_conin(unsigned char k) { if (nout < 63) out[nout++] = (char)k; out[nout] = 0; }
void Console_set_vc(int n) { if (!graphics) vc_switched = n; }   /* the real one refuses under the lock */
int Console_graphics(void) { return graphics; }

#include KBD_C

/* the stubs' port read returns the next scancode */
int stub_inb(int port) { return port == KBD_IO && pfeed < nfeed ? feed[pfeed++] : 0; }

static int failures, tests;

static void press(int g, const char *what, const unsigned char *codes, int n, const char *want, int want_vc)
{
    int i;
    graphics = g;
    nout = 0; out[0] = 0; vc_switched = -1;
    memcpy(feed, codes, n); nfeed = n; pfeed = 0;
    for (i = 0; i < n; i++)
        keyboard_irq(1, 0);
    tests++;
    if (nout != (int)strlen(want) || memcmp(out, want, nout) || vc_switched != want_vc) {
        failures++;
        printf("FAIL %-34s graphics=%d: sent %d bytes", what, g, nout);
        for (i = 0; i < nout; i++)
            printf(" %02x", (unsigned char)out[i]);
        printf(", console switch %d (want %d)\n", vc_switched, want_vc);
    } else
        printf("ok   %-34s graphics=%d\n", what, g);
}

int main(void)
{
    static const unsigned char alt_f4[] = { 0x38, 0x3E, 0xBE, 0xB8 };
    static const unsigned char alt_f2[] = { 0x38, 0x3C, 0xBC, 0xB8 };
    static const unsigned char alt_f10[] = { 0x38, 0x44, 0xC4, 0xB8 };
    static const unsigned char alt_sp[] = { 0x38, 0x39, 0xB9, 0xB8 };
    static const unsigned char alt_s[] = { 0x38, 0x1F, 0x9F, 0xB8 };
    static const unsigned char alt_2[] = { 0x38, 0x03, 0x83, 0xB8 };
    static const unsigned char f4[] = { 0x3E, 0xBE };
    static const unsigned char f10[] = { 0x44, 0xC4 };
    static const unsigned char sp[] = { 0x39, 0xB9 };
    static const unsigned char a[] = { 0x1E, 0x9E };

    /* text mode: unchanged behaviour */
    press(0, "Alt+F4 switches to console 4", alt_f4, 4, "", 3);
    press(0, "Alt+F2 switches to console 2", alt_f2, 4, "", 1);
    press(0, "Alt+2 switches to console 2", alt_2, 4, "", 1);
    press(0, "Alt+F10 is ESC j", alt_f10, 4, "\033j", -1);
    press(0, "Alt+Space is 0xA0", alt_sp, 4, "\240", -1);
    press(0, "Alt+S is ESC S", alt_s, 4, "\033S", -1);
    press(0, "F4 is ESC d", f4, 2, "\033d", -1);
    press(0, "space", sp, 2, " ", -1);
    /* a graphics program holds the console */
    press(1, "Alt+F4 is ESC p", alt_f4, 4, "\033p", -1);
    press(1, "Alt+F2 is ESC n", alt_f2, 4, "\033n", -1);
    press(1, "Alt+F10 is ESC v", alt_f10, 4, "\033v", -1);
    press(1, "Alt+Space is ESC space", alt_sp, 4, "\033 ", -1);
    press(1, "Alt+S is ESC S", alt_s, 4, "\033S", -1);
    press(1, "Alt+2 is refused (no switch, no key)", alt_2, 4, "", -1);
    press(1, "F4 is ESC d", f4, 2, "\033d", -1);
    press(1, "F10 is ESC j", f10, 2, "\033j", -1);
    press(1, "a", a, 2, "a", -1);
    press(1, "space", sp, 2, " ", -1);
    printf("== kbd-scancode: %d passed, %d failed\n", tests - failures, failures);
    return failures != 0;
}
