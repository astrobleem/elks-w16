/*
 * mem.c - segments, far copies and logging, for ELKS and for the host test build.
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "w16.h"

int opt_verbose, opt_trace, opt_debugport;
u8 w16_scratch[W16_SCRATCH];

#ifdef W16_HOST
/* ---------------------------------------------------------------- host: simulated memory */
u8 host_mem[0x110000];
static u16 host_next = 0x1000;          /* first simulated paragraph handed out */
int host_fail_after = -1;               /* tests: make the Nth allocation fail */
int host_live;                          /* allocations not yet freed */

struct halloc { u16 seg; u32 bytes; u8 live; };
static struct halloc allocs[512];
static int nallocs;

static void violation(const char *what, u16 seg, u16 off, u16 n)
{
    fprintf(stderr, "HOST BOUNDS VIOLATION: %s at %04x:%04x (+%u)\n", what, seg, off, n);
    abort();
}

int seg_alloc(u16 paras, u16 *seg)
{
    if (host_fail_after == 0)
        return -1;
    if (host_fail_after > 0)
        host_fail_after--;
    if ((u32)host_next + paras + 1 > 0xA000 || nallocs == (int)(sizeof(allocs) / sizeof(allocs[0])))
        return -1;
    *seg = host_next;
    memset(host_mem + ((u32)host_next << 4), 0xCC, (u32)paras << 4);   /* poison: loader must clear */
    allocs[nallocs].seg = host_next;
    allocs[nallocs].bytes = (u32)paras << 4;
    allocs[nallocs].live = 1;
    nallocs++;
    host_next += paras + 1;                 /* one guard paragraph between blocks */
    host_live++;
    return 0;
}

void seg_free(u16 seg)
{
    int i;
    for (i = nallocs - 1; i >= 0 && allocs[i].seg != seg; i--)
        ;
    if (i < 0 || !allocs[i].live)
        violation(i < 0 ? "free of a segment that was never allocated" : "double free", seg, 0, 0);
    allocs[i].live = 0;
    memset(host_mem + ((u32)seg << 4), 0xDD, allocs[i].bytes);
    host_live--;
}

/* every guest access goes through here: it must lie inside the live allocation based at seg */
u8 *host_ptr(u16 seg, u16 off, u16 n)
{
    int i;
    for (i = nallocs - 1; i >= 0 && allocs[i].seg != seg; i--)
        ;
    if (i < 0)
        violation("access through a segment that is not an allocation", seg, off, n);
    if (!allocs[i].live)
        violation("access to a freed segment", seg, off, n);
    if ((u32)off + n > allocs[i].bytes)
        violation("access outside its allocation", seg, off, n);
    return &host_mem[LIN(seg, off)];
}

void fcopy_in(void *dst, u16 seg, u16 off, u16 n) { u16 i; for (i = 0; i < n; i++) ((u8 *)dst)[i] = rd8(seg, (u16)(off + i)); }
void fcopy_out(u16 seg, u16 off, const void *src, u16 n) { u16 i; for (i = 0; i < n; i++) wr8(seg, (u16)(off + i), ((const u8 *)src)[i]); }
void fzero(u16 seg, u16 off, u16 n) { u16 i; for (i = 0; i < n; i++) wr8(seg, (u16)(off + i), 0); }
void e9puts(const char *s) { (void)s; }
#else
/* ---------------------------------------------------------------- ELKS */
#include <malloc.h>
void w16_farcpy(u16 dseg, u16 doff, u16 sseg, u16 soff, u16 n);
void w16_farset(u16 seg, u16 off, u16 val, u16 n);
u16  w16_get_ds(void);
void w16_outb(u16 port, u16 val);

int seg_alloc(u16 paras, u16 *seg)
{
    return _fmemalloc(paras, seg) ? -1 : 0;
}

void seg_free(u16 seg)
{
    _fmemfree(seg);
}

void fcopy_in(void *dst, u16 seg, u16 off, u16 n) { if (n) w16_farcpy(w16_get_ds(), (u16)(unsigned)dst, seg, off, n); }
void fcopy_out(u16 seg, u16 off, const void *src, u16 n) { if (n) w16_farcpy(seg, off, w16_get_ds(), (u16)(unsigned)src, n); }
void fzero(u16 seg, u16 off, u16 n) { if (n) w16_farset(seg, off, 0, n); }

void e9puts(const char *s)
{
    while (*s)
        w16_outb(0xE9, (u8)*s++);
}
#endif

u16 fstrncpy_in(char *dst, u16 seg, u16 off, u16 max)
{
    u16 i = 0;
    if (!max)
        return 0;
    while (i + 1 < max) {
        char c = (char)rd8(seg, (u16)(off + i));
        if (!c)
            break;
        dst[i++] = c;
    }
    dst[i] = 0;
    return i;
}

/* resource name from the program: MAKEINTRESOURCE (segment 0) or a string, "#n" meaning ordinal n.
 * name must hold 34 bytes; on return either name[0] is set or *id is the ordinal. */
void res_name(u16 off, u16 seg, char *name, u16 *id)
{
    *id = 0;
    name[0] = 0;
    if (!seg) {
        *id = off;
        return;
    }
    fstrncpy_in(name, seg, off, 33);
    if (name[0] == '#') {
        *id = (u16)atoi(name + 1);
        name[0] = 0;
    }
}

static char logbuf[256];

#ifndef W16_HOST
extern int opt_metrics;
extern u32 w16_t0;
u32 now_ms(void);
#endif

static void emit(void)
{
#ifndef W16_HOST
    if (opt_metrics) {                  /* -m: every log line carries its time since w16 started */
        char t[12];
        sprintf(t, "+%lu ", (unsigned long)(now_ms() - w16_t0));
        write(2, t, strlen(t));
        if (opt_debugport)
            e9puts(t);
    }
#endif
    write(2, logbuf, strlen(logbuf));
    if (opt_debugport)
        e9puts(logbuf);
}

void wlog(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(logbuf, sizeof(logbuf), fmt, ap);
    va_end(ap);
    emit();
}

void vlog(const char *fmt, ...)
{
    va_list ap;
    if (!opt_verbose)
        return;
    va_start(ap, fmt);
    vsnprintf(logbuf, sizeof(logbuf), fmt, ap);
    va_end(ap);
    emit();
}
