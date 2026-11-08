/* w16mouse - drive Nano-X's serial mouse driver through an ELKS pseudo-tty.
 *
 *   w16mouse /dev/ptypN READYFILE STEP...
 *
 * Opens the pty master first (the Nano-X server opens the slave /dev/ttypN as MOUSE_PORT
 * with MOUSE_TYPE=ms), waits until READYFILE exists, then runs the steps:
 *   home            move to (0,0) with many small left/up moves
 *   move X Y        move to absolute screen position X,Y (after home)
 *   click           left button down, then up
 *   dclick          two clicks in quick succession
 *   sleep N         wait N seconds
 *   say TEXT        write "@@SAY TEXT" to the 0xE9 debug port
 * Packets are 3-byte Microsoft serial mouse reports; each move is at most 4 pixels so the
 * driver's acceleration (threshold 5) never applies and positions stay exact.
 * Test helper: it injects input at the serial-port byte level, not through mouse hardware. */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int fd, curx, cury, buttons;

static void e9(const char *s)
{
    while (*s) {
        unsigned char c = (unsigned char)*s++;
        __asm__ volatile ("outb %%al, %%dx" : : "a"(c), "d"(0xE9));
    }
}

static long npackets, nfail;

static void packet(int dx, int dy)
{
    unsigned char p[3];
    p[0] = 0x40 | (buttons << 4) | (((dy >> 6) & 3) << 2) | ((dx >> 6) & 3);
    p[1] = dx & 0x3F;
    p[2] = dy & 0x3F;
    unsigned char junk[64];
    int done = 0, tries = 0, n;
    while (done < 3 && tries < 200) {
        n = write(fd, p + done, 3 - done);
        if (n > 0)
            done += n;
        else {
            tries++;
            usleep(20000UL);
        }
        while (read(fd, junk, sizeof(junk)) > 0)    /* drain anything the slave echoed */
            ;
    }
    if (done < 3)
        nfail++;
    npackets++;
    usleep(30000UL);
}

static void progress(const char *what)
{
    char line[80];
    sprintf(line, "@@MOUSE %s at %d,%d packets %ld failed %ld\n", what, curx, cury, npackets, nfail);
    e9(line);
}

static void moveby(int dx, int dy)
{
    while (dx || dy) {
        int sx = dx > 4 ? 4 : dx < -4 ? -4 : dx;
        int sy = dy > 4 ? 4 : dy < -4 ? -4 : dy;
        packet(sx, sy);
        dx -= sx; dy -= sy;
        curx += sx; cury += sy;
    }
}

static void click(void)
{
    buttons = 2; packet(0, 0);      /* MS protocol: left button is bit 5 (value 2 in bits 4-5) */
    usleep(80000UL);
    buttons = 0; packet(0, 0);
}

int main(int argc, char **argv)
{
    int i, n;
    char line[96];
    if (argc < 3) {
        fprintf(stderr, "usage: w16mouse /dev/ptypN READYFILE STEP...\n");
        return 2;
    }
    fd = open(argv[1], O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        perror(argv[1]);
        return 1;
    }
    for (n = 0; n < 900 && access(argv[2], F_OK) != 0; n++)
        sleep(1);
    if (n == 900) {
        e9("@@SAY w16mouse: ready file never appeared\n");
        return 1;
    }
    for (i = 3; i < argc; i++) {
        if (!strcmp(argv[i], "home")) {
            for (n = 0; n < 90; n++)
                packet(-4, -4);
            curx = cury = 0;
            progress("homed");
        } else if (!strcmp(argv[i], "move") && i + 2 < argc) {
            int x = atoi(argv[i + 1]), y = atoi(argv[i + 2]);
            moveby(x - curx, y - cury);
            i += 2;
            progress("moved");
        } else if (!strcmp(argv[i], "click")) {
            click();
            progress("clicked");
        } else if (!strcmp(argv[i], "dclick")) {
            click();
            usleep(120000UL);
            click();
            progress("double-clicked");
        } else if (!strcmp(argv[i], "sleep") && i + 1 < argc) {
            sleep(atoi(argv[++i]));
        } else if (!strcmp(argv[i], "say") && i + 1 < argc) {
            sprintf(line, "@@SAY %s\n", argv[++i]);
            e9(line);
        }
    }
    close(fd);
    return 0;
}
