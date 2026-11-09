/* nxstress - stress the Nano-X server's request handling (the batched reader in nanox-w16.patch):
 *   1. partial requests: a request header split across writes with pauses, then the rest;
 *   2. disconnects in the middle of a request, then a whole request on a fresh connection;
 *   3. reconnect churn: 25 short-lived clients, each a process of its own, that draw and leave;
 *   4. two clients at once (two children) drawing in turn.
 * Afterwards a normal client must still be able to connect and draw. Reports to port 0xE9 and stdout.
 * Raw connections use the same address length as the client library. Every client is a fresh process,
 * since the client library is not made to be opened twice in one process.
 * Build with the Nano-X client library (small model); runs before the Win16 programs in the autotest. */
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include "nano-X.h"
#include "nxproto.h"

static void say(const char *s)
{
    fputs(s, stdout);
    fflush(stdout);
    while (*s) {
        unsigned char c = (unsigned char)*s++;
        __asm__ volatile ("outb %%al, %%dx" : : "a"(c), "d"(0xE9));
    }
}

/* connect like the client library does (address length to the end of the name), with its retries */
static int rawsock(void)
{
    struct sockaddr_un a;
    int s, tries, len;
    memset(&a, 0, sizeof(a));
    a.sun_family = AF_UNIX;
    strcpy(a.sun_path, GR_NAMED_SOCKET);
    len = offsetof(struct sockaddr_un, sun_path) + strlen(a.sun_path) + 1;
    for (tries = 0; tries < 10; tries++) {
        s = socket(AF_UNIX, SOCK_STREAM, 0);
        if (s < 0)
            return -1;
        if (connect(s, (struct sockaddr *)&a, len) == 0)
            return s;
        close(s);
        usleep(100000UL);
    }
    return -1;
}

/* a GrNewGC request (header only) in pieces; the reply is a 2-byte type then the GC id */
static int partial(int split, int pause_ms)
{
    nxReq req;
    unsigned char reply[8];
    int s = rawsock(), n, got = 0;
    if (s < 0)
        return -1;
    req.reqType = GrNumNewGC;
    req.hilength = 0;
    req.length = sizeof(req);
    write(s, &req, split);
    usleep((unsigned long)pause_ms * 1000);
    write(s, (char *)&req + split, sizeof(req) - split);
    while (got < 2 + (int)sizeof(GR_GC_ID) && (n = read(s, reply + got, sizeof(reply) - got)) > 0)
        got += n;
    close(s);
    return got >= 2 + (int)sizeof(GR_GC_ID) ? 0 : -1;
}

static int draw_once(int tag)
{
    GR_WINDOW_ID w;
    GR_GC_ID gc;
    GR_SCREEN_INFO si;
    if (GrOpen() < 0)
        return -1;
    w = GrNewWindowEx(GR_WM_PROPS_NODECORATE, "stress", GR_ROOT_WINDOW_ID, 200 + (tag & 7) * 8, 150, 40, 30, GR_RGB(0, 0, 0));
    gc = GrNewGC();
    GrMapWindow(w);
    GrSetGCForeground(gc, GR_RGB(tag & 1 ? 255 : 0, tag & 2 ? 255 : 0, tag & 4 ? 255 : 0));
    GrFillRect(w, gc, 2, 2, 36, 26);
    GrSetGCForeground(gc, GR_RGB(255, 255, 255));
    GrLine(w, gc, 0, 0, 39, 29);
    GrGetScreenInfo(&si);              /* round trip: everything above was handled */
    GrDestroyGC(gc);
    GrDestroyWindow(w);
    GrClose();
    return si.cols > 0 ? 0 : -1;
}

/* a client in a process of its own; returns its failure count */
static int client_process(int from, int to)
{
    int pid = fork(), st, i, f = 0;
    if (pid == 0) {
        for (i = from; i < to; i++)
            if (draw_once(i))
                f++;
        _exit(f);
    }
    if (pid < 0)
        return to - from;
    return waitpid(pid, &st, 0) == pid ? WEXITSTATUS(st) : to - from;
}

int main(void)
{
    char line[96];
    int i, fails = 0, f, s, pid, st;
    nxReq req;

    say("@@STRESS begin\n");
    for (i = 0, f = 0; i < 12; i++)             /* 1. partial requests */
        if (partial(1 + i % 3, 50 + 200 * (i % 4)))
            f++;
    fails += f;
    sprintf(line, "@@STRESS partial requests: %d failures of 12\n", f);
    say(line);

    for (i = 0, f = 0; i < 6; i++) {            /* 2. disconnect mid-request, then a whole one */
        s = rawsock();
        if (s < 0) { f++; continue; }
        req.reqType = GrNumNewGC; req.hilength = 0; req.length = sizeof(req);
        write(s, &req, 1 + i % 3);
        close(s);
    }
    if (partial(2, 10))
        f++;
    fails += f;
    sprintf(line, "@@STRESS mid-request disconnects: %s\n", f ? "FAILED" : "server fine afterwards");
    say(line);

    for (i = 0, f = 0; i < 25; i++)             /* 3. reconnect churn, one process per client */
        f += client_process(i, i + 1);
    fails += f;
    sprintf(line, "@@STRESS 25 connect/draw/disconnect cycles: %d failures\n", f);
    say(line);

    pid = fork();                               /* 4. two clients at once */
    f = 0;
    if (pid == 0)
        _exit(client_process(8, 16));
    f += client_process(0, 8);
    if (pid > 0 && waitpid(pid, &st, 0) == pid)
        f += WEXITSTATUS(st);
    fails += f;
    sprintf(line, "@@STRESS two clients at once: %d failures\n", f);
    say(line);

    sprintf(line, "@@STRESS end: %d failures\n", fails);
    say(line);
    return fails ? 1 : 0;
}
