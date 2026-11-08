/*
 * mem.c - segments, far copies and logging, for ELKS and for the host test build.
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "w16.h"

int opt_verbose, opt_trace, opt_debugport;

#ifdef W16_HOST
/* ---------------------------------------------------------------- host: simulated memory */
u8 host_mem[0x110000];
static u16 host_next = 0x1000;          /* first simulated paragraph handed out */
int host_fail_after = -1;               /* tests: make the Nth allocation fail */
int host_live;                          /* allocations not yet freed */

int seg_alloc(u16 paras, u16 *seg)
{
    if (host_fail_after == 0)
        return -1;
    if (host_fail_after > 0)
        host_fail_after--;
    if ((u32)host_next + paras > 0xA000)
        return -1;
    *seg = host_next;
    memset(host_mem + ((u32)host_next << 4), 0xCC, (u32)paras << 4);   /* poison: loader must clear */
    host_next += paras;
    host_live++;
    return 0;
}

void seg_free(u16 seg)
{
    (void)seg;
    host_live--;
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

static char logbuf[256];

static void emit(void)
{
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
