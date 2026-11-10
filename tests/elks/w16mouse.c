/* w16mouse - test driver: feeds Nano-X's serial mouse driver through an ELKS pseudo-tty.
 *
 *   w16mouse /dev/ptypN READYFILE STEP...                       one program
 *   w16mouse /dev/ptypN -serve READYFILE STEPFILE STOPFILE [/dev/ptypK]   several programs in a row;
 *       the optional second pty is Nano-X's keyboard (start the server with CONSOLE=/dev/ttypK)
 *
 * The pty master must be opened before the Nano-X server opens the slave /dev/ttypN (MOUSE_PORT,
 * MOUSE_TYPE=ms): ELKS refuses a master open while the slave is open, so one process keeps it
 * for the whole test. In -serve mode it repeatedly waits for READYFILE (written by w16 -r after the
 * program's first paint), runs the steps in STEPFILE, then waits for READYFILE to be removed;
 * it ends when STOPFILE exists. Steps:
 *   home            move to (0,0) with many small left/up moves
 *   move X Y        move to absolute screen position X,Y (homes first: the program may have moved the
 *                   pointer itself, as REVERSI's hint does with SetCursorPos)
 *   click / dclick  left click / double click
 *   button ID       click the centre of target ID (dialog push button, menu bar item 1000+n, or menu
 *                   item), as listed by w16 -r in READYFILE; waits up to 30 s for it to be listed
 *   snap NAME       save meminfo and ps output to /tmp/m_NAME.txt and /tmp/p_NAME.txt
 *   sleep N         wait N seconds
 *   say TEXT        log "@@SAY TEXT"
 *   key K[,K...]    type keys into the keyboard pty as the ELKS console would send them: a character,
 *                   enter, esc, tab, bs, space, up, down, left, right, f1..f12, alt-X (ESC X), and
 *                   alt-space, alt-f1..alt-f10 (ESC space, ESC m..v: what the patched console sends
 *                   for them while Nano-X holds it)
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

static int fd, kfd = -1, curx, cury, buttons;
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

/* back to (0,0), where the server clamps the pointer: the one position known for certain, since the
 * program can move the pointer itself (SetCursorPos) and no position report comes back this way */
static void rehome(void)
{
    int k;
    for (k = 0; k < 90; k++)
        packet(-4, -4);
    curx = cury = 0;
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
    char msg[80];
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
        st = -1;
        if (pid > 0)
            waitpid(pid, &st, 0);
        sprintf(msg, "@@MOUSE snap %s %s pid %d status %d\n", name, cmd[k] + 5, pid, st);
        e9(msg);
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
    rehome();
    moveby(x, y);
    progress("at button");
    click();
    progress("clicked button");
}

/* one key as the ELKS console delivers it: F1..F12 are ESC a..l, Alt+letter is ESC + the capital,
 * cursor keys ESC [ A..D; the bytes of a key go out in one write so the reader sees them together */
static void key(const char *name)
{
    unsigned char b[4];
    int n = 0, esc = 0;
    if (kfd < 0) {
        e9("@@SAY w16mouse: no keyboard pty for a key step\n");
        return;
    }
    if (!strcmp(name, "enter")) b[n++] = '\r';
    else if (!strcmp(name, "esc")) { b[n++] = 27; esc = 1; }
    else if (!strcmp(name, "tab")) b[n++] = 9;
    else if (!strcmp(name, "bs")) b[n++] = 8;
    else if (!strcmp(name, "space")) b[n++] = ' ';
    else if (!strcmp(name, "up")) { b[n++] = 27; b[n++] = '['; b[n++] = 'A'; }
    else if (!strcmp(name, "down")) { b[n++] = 27; b[n++] = '['; b[n++] = 'B'; }
    else if (!strcmp(name, "right")) { b[n++] = 27; b[n++] = '['; b[n++] = 'C'; }
    else if (!strcmp(name, "left")) { b[n++] = 27; b[n++] = '['; b[n++] = 'D'; }
    else if (name[0] == 'f' && name[1] >= '1' && name[1] <= '9') { b[n++] = 27; b[n++] = (unsigned char)('a' + atoi(name + 1) - 1); }
    else if (!strcmp(name, "alt-space")) { b[n++] = 27; b[n++] = ' '; }
    else if (!strncmp(name, "alt-f", 5) && name[5] >= '1' && name[5] <= '9') { b[n++] = 27; b[n++] = (unsigned char)('m' + atoi(name + 5) - 1); }
    else if (!strncmp(name, "alt-", 4) && name[4]) { b[n++] = 27; b[n++] = (unsigned char)(name[4] >= 'a' && name[4] <= 'z' ? name[4] - 32 : name[4]); }
    else b[n++] = (unsigned char)name[0];
    write(kfd, b, n);
    sprintf(line, "@@KEY %s\n", name);
    e9(line);
    usleep(esc ? 1500000UL : 500000UL);         /* a lone ESC is one that nothing follows at once */
}

static void run_steps(int n, char **v, const char *ready)
{
    int i;
    for (i = 0; i < n; i++) {
        if (!strcmp(v[i], "home")) {
            rehome();
            progress("homed");
        } else if (!strcmp(v[i], "move") && i + 2 < n) {
            rehome();
            moveby(atoi(v[i + 1]), atoi(v[i + 2]));
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
        } else if (!strcmp(v[i], "key") && i + 1 < n) {
            char *k, *rest = v[++i];
            while ((k = strtok(rest, ",")) != 0) {
                rest = 0;
                key(k);
            }
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

/* the steps file's text; autotest.sh removes the ready file before it writes the next program's steps */
static int read_steps(const char *name, char *buf, int size)
{
    int f = open(name, O_RDONLY), len = f >= 0 ? read(f, buf, size - 1) : 0;
    if (f >= 0)
        close(f);
    buf[len > 0 ? len : 0] = 0;
    return len > 0 ? len : 0;
}

/* After a program's steps: wait for the next program's steps (or the stop file). Waiting for the ready
 * file to disappear instead misses the gap when the next program paints within the one-second poll. */
static int wait_new_steps(const char *steps, const char *stop, const char *last, char *buf, int size)
{
    int n;
    for (n = 0; n < 1800; n++) {
        if (access(stop, F_OK) == 0)
            return 1;
        read_steps(steps, buf, size);
        if (strcmp(buf, last))
            return 0;
        sleep(1);
    }
    return -1;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: w16mouse /dev/ptypN READYFILE STEP...\n"
                        "       w16mouse /dev/ptypN -serve READYFILE STEPFILE STOPFILE [/dev/ptypK]\n");
        return 2;
    }
    fd = open(argv[1], O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        perror(argv[1]);
        return 1;
    }
    if (!strcmp(argv[2], "-serve") && argc >= 6) {
        static char buf[1200], last[1200];
        static char *tok[240];
        if (argc >= 7) {
            kfd = open(argv[6], O_RDWR | O_NONBLOCK);
            if (kfd < 0)
                perror(argv[6]);
        }
        for (;;) {
            int r = wait_for(argv[3], argv[5], 1), n = 0;
            char *p;
            if (r)
                break;
            e9("@@MOUSE ready file seen\n");
            read_steps(argv[4], buf, sizeof(buf));
            strcpy(last, buf);
            for (p = strtok(buf, " \t\r\n"); p && n < 240; p = strtok(0, " \t\r\n"))
                tok[n++] = p;
            run_steps(n, tok, argv[3]);
            e9("@@MOUSE steps done\n");
            if (wait_new_steps(argv[4], argv[5], last, buf, sizeof(buf)))
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
