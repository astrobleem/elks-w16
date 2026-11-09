/* w16mouse - test driver: feeds Nano-X's serial mouse driver through an ELKS pseudo-tty.
 *
 *   w16mouse /dev/ptypN READYFILE STEP...                       one program
 *   w16mouse /dev/ptypN -serve READYFILE STEPFILE STOPFILE      several programs in a row
 *
 * The pty master must be opened before the Nano-X server opens the slave /dev/ttypN (MOUSE_PORT,
 * MOUSE_TYPE=ms): ELKS refuses a master open while the slave is open, so one process keeps it
 * for the whole test. In -serve mode it repeatedly waits for READYFILE (written by w16 -r after the
 * program's first paint), runs the steps in STEPFILE, then waits for READYFILE to be removed;
 * it ends when STOPFILE exists. Steps:
 *   home            move to (0,0) with many small left/up moves
 *   move X Y        move to absolute screen position X,Y (after home)
 *   click / dclick  left click / double click
 *   button ID       click the centre of target ID (dialog push button, menu bar item 1000+n, or menu
 *                   item), as listed by w16 -r in READYFILE; waits up to 30 s for it to be listed
 *   snap NAME       save meminfo and ps output to /tmp/m_NAME.txt and /tmp/p_NAME.txt
 *   sleep N         wait N seconds
 *   say TEXT        log "@@SAY TEXT"
 * Progress goes to port 0xE9 (emulator debug console) and to /tmp/mouse.log.
 * Packets are 3-byte Microsoft serial mouse reports; each move is at most 4 pixels so the driver's
 * acceleration (threshold 5) never applies and positions stay exact. This injects input at the
 * serial byte level, not through mouse hardware. */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static int fd, curx, cury, buttons;
static long npackets, nfail;
static char line[100];

static void e9(const char *s)
{
    int f = open("/tmp/mouse.log", O_WRONLY | O_CREAT | O_APPEND, 0644);   /* emulators without port 0xE9 */
    if (f >= 0) {
        write(f, s, strlen(s));
        close(f);
        sync();
    }
    while (*s) {
        unsigned char c = (unsigned char)*s++;
        __asm__ volatile ("outb %%al, %%dx" : : "a"(c), "d"(0xE9));
    }
}

static void packet(int dx, int dy)
{
    unsigned char p[3], junk[64];
    int done = 0, tries = 0, n;
    p[0] = 0x40 | (buttons << 4) | (((dy >> 6) & 3) << 2) | ((dx >> 6) & 3);
    p[1] = dx & 0x3F;
    p[2] = dy & 0x3F;
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
    char msg[80];
    sprintf(msg, "@@MOUSE %s at %d,%d packets %ld failed %ld\n", what, curx, cury, npackets, nfail);
    e9(msg);
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

static void snap(const char *name)
{
    static const char *const cmd[2] = { "/bin/meminfo", "/bin/ps" };
    int k;
    for (k = 0; k < 2; k++) {
        int pid, st, o;
        sprintf(line, "/tmp/%c_%s.txt", k ? 'p' : 'm', name);
        pid = fork();
        if (pid == 0) {
            o = open(line, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (o >= 0) { dup2(o, 1); dup2(o, 2); }
            execl(cmd[k], cmd[k] + 5, (char *)0);
            _exit(127);
        }
        if (pid > 0)
            waitpid(pid, &st, 0);
    }
    sync();
}

static void button(const char *ready, int want)
{
    FILE *f;
    int id, x, y, found = 0, tries;
    for (tries = 0; !found && tries < 30; tries++) {   /* the target may still be on its way: wait up to 30 s */
        if (tries)
            sleep(1);
        f = fopen(ready, "r");
        while (f && fgets(line, sizeof(line), f))
            if (sscanf(line, "button %d %d %d", &id, &x, &y) == 3 && id == want) {
                found = 1;
                break;
            }
        if (f)
            fclose(f);
    }
    if (!found) {
        e9("@@SAY w16mouse: button not listed in the ready file, which holds:\n");
        f = fopen(ready, "r");
        while (f && fgets(line, sizeof(line), f))
            e9(line);
        if (f)
            fclose(f);
        return;
    }
    moveby(x - curx, y - cury);
    progress("at button");
    click();
    progress("clicked button");
}

static void run_steps(int n, char **v, const char *ready)
{
    int i, k;
    for (i = 0; i < n; i++) {
        if (!strcmp(v[i], "home")) {
            for (k = 0; k < 90; k++)
                packet(-4, -4);
            curx = cury = 0;
            progress("homed");
        } else if (!strcmp(v[i], "move") && i + 2 < n) {
            moveby(atoi(v[i + 1]) - curx, atoi(v[i + 2]) - cury);
            i += 2;
            progress("moved");
        } else if (!strcmp(v[i], "snap") && i + 1 < n) {
            snap(v[++i]);
        } else if (!strcmp(v[i], "button") && i + 1 < n) {
            button(ready, atoi(v[++i]));
        } else if (!strcmp(v[i], "click")) {
            click();
            progress("clicked");
        } else if (!strcmp(v[i], "dclick")) {
            click();
            usleep(120000UL);
            click();
            progress("double-clicked");
        } else if (!strcmp(v[i], "sleep") && i + 1 < n) {
            sleep(atoi(v[++i]));
        } else if (!strcmp(v[i], "say") && i + 1 < n) {
            sprintf(line, "@@SAY %s\n", v[++i]);
            e9(line);
        }
    }
}

static int wait_for(const char *a, const char *b, int present)
{
    int n;
    for (n = 0; n < 1800; n++) {                /* 30 minutes */
        if (b && access(b, F_OK) == 0)
            return 1;
        if ((access(a, F_OK) == 0) == present)
            return 0;
        sleep(1);
    }
    return -1;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: w16mouse /dev/ptypN READYFILE STEP...\n"
                        "       w16mouse /dev/ptypN -serve READYFILE STEPFILE STOPFILE\n");
        return 2;
    }
    fd = open(argv[1], O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        perror(argv[1]);
        return 1;
    }
    if (!strcmp(argv[2], "-serve") && argc >= 6) {
        static char buf[1200];
        static char *tok[240];
        for (;;) {
            int r = wait_for(argv[3], argv[5], 1), n = 0, f, len;
            char *p;
            if (r)
                break;
            f = open(argv[4], O_RDONLY);
            len = f >= 0 ? read(f, buf, sizeof(buf) - 1) : 0;
            if (f >= 0)
                close(f);
            buf[len > 0 ? len : 0] = 0;
            for (p = strtok(buf, " \t\r\n"); p && n < 240; p = strtok(0, " \t\r\n"))
                tok[n++] = p;
            run_steps(n, tok, argv[3]);
            e9("@@MOUSE steps done\n");
            if (wait_for(argv[3], argv[5], 0))
                break;
        }
    } else {
        if (wait_for(argv[2], 0, 1)) {
            e9("@@SAY w16mouse: ready file never appeared\n");
            return 1;
        }
        run_steps(argc - 3, argv + 3, argv[2]);
    }
    close(fd);
    return 0;
}
