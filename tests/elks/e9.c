/* e9 - copy text to I/O port 0xE9 (the emulator debug console; MartyPC terminal_port).
 *   e9 words...      writes the words and a newline
 *   e9               copies standard input
 * Test helper only: on real hardware port 0xE9 is normally unused. */
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static void out(const char *s, int n)
{
    while (n-- > 0) {
        unsigned char c = (unsigned char)*s++;
        __asm__ volatile ("outb %%al, %%dx" : : "a"(c), "d"(0xE9));
    }
}

int main(int argc, char **argv)
{
    char buf[128];
    int i, n;
    if (argc == 3 && !strcmp(argv[1], "-beat")) {     /* heartbeat: shows whether sleeping tasks still wake */
        unsigned long k = 0;
        for (;;) {
            n = sprintf(buf, "@@BEAT %lu %ld\n", ++k, (long)time(NULL));
            out(buf, n);
            sleep(atoi(argv[2]));
        }
    }
    if (argc > 1) {
        for (i = 1; i < argc; i++) {
            if (i > 1)
                out(" ", 1);
            out(argv[i], strlen(argv[i]));
        }
        out("\n", 1);
        return 0;
    }
    while ((n = read(0, buf, sizeof(buf))) > 0)
        out(buf, n);
    return 0;
}
