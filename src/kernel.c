/*
 * kernel.c - KERNEL entry points, task setup and the INT 21h (DOS) services a
 * Windows program in real mode calls directly.
 */
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include "w16.h"

struct ne_module *w16_mod;
u16 w16_exit_code;
int opt_strict_dos;
static u16 dos_vectors[256 * 2 / 16];   /* virtual vectors set by the program: [n] off,seg for a few */
static struct { u8 vec; u16 off, seg; } vtab[8];
static int nvtab;
static char modfile[64];                /* DOS-style module file name */

#define CMDLINE_OFF 0x81

/* ---------------------------------------------------------------- task setup */
int task_setup(struct ne_module *m, const char *path, int argc, char **argv)
{
    const char *base = strrchr(path, '/');
    u16 n = 0, i;
    char *d;

    base = base ? base + 1 : path;
    strcpy(modfile, "C:\\");
    d = modfile + 3;
    for (i = 0; base[i] && i < 12; i++)
        *d++ = (base[i] >= 'a' && base[i] <= 'z') ? base[i] - 32 : base[i];
    *d = 0;

    /* PSP: INT 20h, top of memory, environment segment, command tail */
    if (seg_alloc(16, &m->psp) || seg_alloc(4, &m->envseg)) {
        wlog("w16: no memory for the program segment prefix\n");
        return W16_ENOMEM;
    }
    m->total_bytes += 16 * 16 + 4 * 16;
    fzero(m->psp, 0, 256);
    wr8(m->psp, 0, 0xCD); wr8(m->psp, 1, 0x20);
    wr16(m->psp, 2, 0xA000);
    wr16(m->psp, 0x2C, m->envseg);
    for (i = 0; i < (u16)argc && n < 120; i++) {
        const char *a = argv[i];
        if (i)
            wr8(m->psp, CMDLINE_OFF + n++, ' ');
        while (*a && n < 120)
            wr8(m->psp, CMDLINE_OFF + n++, *a++);
    }
    wr8(m->psp, 0x80, (u8)n);
    wr8(m->psp, CMDLINE_OFF + n, 0);        /* Windows hands the tail over NUL-terminated */

    /* environment: one variable, then the program name (DOS 3+ layout) */
    fzero(m->envseg, 0, 64);
    fcopy_out(m->envseg, 0, "W16=ELKS\0\0\1\0", 12);
    fcopy_out(m->envseg, 12, modfile, strlen(modfile) + 1);

    /* instance data words at DGROUP:0006..000F */
    wr16(m->dgroup, 0x06, 0);                          /* pLocalHeap: no local heap manager */
    wr16(m->dgroup, 0x0A, m->seg[m->autodata - 1].minalloc);   /* pStackTop (lowest) */
    wr16(m->dgroup, 0x0C, m->init_sp);                 /* pStackMin */
    wr16(m->dgroup, 0x0E, m->init_sp);                 /* pStackBottom (highest) */
    return W16_OK;
}

/* ---------------------------------------------------------------- KERNEL */
u32 k_GetVersion(u16 ss, u16 ap)
{
    (void)ss; (void)ap;
    return 0x031E0003ul;        /* Windows 3.0 (AL=3 AH=0), DOS 3.30 in DX */
}

u32 k_WaitEvent(u16 ss, u16 ap)
{
    (void)ss; (void)ap;
    return 0;
}

/* GetModuleFileName(hModule, lpFilename, nSize) */
u32 k_GetModuleFileName(u16 ss, u16 ap)
{
    u16 size = AW(0), off = AW(2), seg = AW(4), n = strlen(modfile);
    if (!size)
        return 0;
    if (n > size - 1)
        n = size - 1;
    fcopy_out(seg, off, modfile, n);
    wr8(seg, off + n, 0);
    return n;
}

/* InitTask(): AX=1, BX=command line offset, CX=stack size, DX=nCmdShow, ES=PSP, SI=hPrev, DI=hInstance */
u32 k_InitTask(u16 ss, u16 ap)
{
    struct ne_module *m = w16_mod;
    u16 frame = ap - 12;
    wr16(ss, frame + 0, m->dgroup);     /* DI */
    wr16(ss, frame + 2, 0);             /* SI */
    w16_ret_bx = CMDLINE_OFF;
    w16_ret_cx = m->stack;
    w16_ret_es = m->psp;
    vlog("w16: InitTask: hInstance %04x, PSP %04x, command line \"%s\"\n", m->dgroup, m->psp, "");
    return 0x00010001ul;                /* DX = SW_SHOWNORMAL, AX = 1 */
}

/* ---------------------------------------------------------------- INT 21h */
/* register frame at w16_app_ss:w16_app_sp: AX BX CX DX SI DI ES DS BP IP CS FLAGS */
#define R_AX 0
#define R_BX 2
#define R_CX 4
#define R_DX 6
#define R_SI 8
#define R_DI 10
#define R_ES 12
#define R_DS 14
#define R_IP 18
#define R_CS 20
#define R_FL 22

void dos_init(void)
{
    nvtab = 0;
    (void)dos_vectors;
}

void w16_int21_dispatch(void)
{
    u16 ss = w16_app_ss, f = w16_app_sp;
    u16 ax = rd16(ss, f + R_AX), ah = ax >> 8, al = ax & 0xFF;
    u16 fl = rd16(ss, f + R_FL) & ~1u;      /* clear carry */
    int i;

    if (opt_trace)
        wlog("w16: INT 21h AH=%02x AL=%02x\n", ah, al);
    switch (ah) {
    case 0x00:                  /* terminate */
        w16_exit_code = 0;
        w16_exit16(0);
        break;
    case 0x4C:                  /* terminate with return code */
        w16_exit_code = al;
        vlog("w16: program exit via INT 21h AH=4Ch, code %u\n", al);
        w16_exit16(al);
        break;
    case 0x30:                  /* DOS version: 3.30 */
        wr16(ss, f + R_AX, 0x1E03);
        wr16(ss, f + R_BX, 0);
        wr16(ss, f + R_CX, 0);
        break;
    case 0x19:                  /* current drive: C: */
        wr16(ss, f + R_AX, (ax & 0xFF00) | 2);
        break;
    case 0x25:                  /* set vector: recorded, never installed in the real IVT */
        for (i = 0; i < nvtab && vtab[i].vec != al; i++)
            ;
        if (i == nvtab && nvtab < 8)
            nvtab++;
        if (i < 8) {
            vtab[i].vec = (u8)al; vtab[i].off = rd16(ss, f + R_DX); vtab[i].seg = rd16(ss, f + R_DS);
        }
        vlog("w16: INT 21h set vector %02x to %04x:%04x (virtual, not installed)\n", al,
              rd16(ss, f + R_DS), rd16(ss, f + R_DX));
        break;
    case 0x35:                  /* get vector: virtual table, else 0:0 */
        for (i = 0; i < nvtab && vtab[i].vec != al; i++)
            ;
        wr16(ss, f + R_BX, i < nvtab ? vtab[i].off : 0);
        wr16(ss, f + R_ES, i < nvtab ? vtab[i].seg : 0);
        break;
    case 0x2A: case 0x2C: {     /* date / time */
        time_t t = time(0);
        struct tm *tm = localtime(&t);
        if (ah == 0x2A) {
            wr16(ss, f + R_CX, tm->tm_year + 1900);
            wr16(ss, f + R_DX, ((tm->tm_mon + 1) << 8) | tm->tm_mday);
            wr16(ss, f + R_AX, (ax & 0xFF00) | tm->tm_wday);
        } else {
            wr16(ss, f + R_CX, (tm->tm_hour << 8) | tm->tm_min);
            wr16(ss, f + R_DX, tm->tm_sec << 8);
        }
        break;
    }
    case 0x40:                  /* write: only stdout/stderr, forwarded to the log */
        if (rd16(ss, f + R_BX) == 1 || rd16(ss, f + R_BX) == 2) {
            char buf[81];
            u16 n = rd16(ss, f + R_CX), k = n > 80 ? 80 : n;
            fcopy_in(buf, rd16(ss, f + R_DS), rd16(ss, f + R_DX), k);
            buf[k] = 0;
            wlog("w16: program output: %s\n", buf);
            wr16(ss, f + R_AX, n);
            break;
        }
        /* fall through */
    default:
        wlog("w16: unsupported INT 21h function AH=%02x AL=%02x at %04x:%04x; returning error 1\n", ah, al,
             rd16(ss, f + R_CS), rd16(ss, f + R_IP));
        if (opt_strict_dos) {
            w16_exit_code = 255;
            w16_exit16(255);
        }
        wr16(ss, f + R_AX, 1);
        fl |= 1;
        break;
    }
    wr16(ss, f + R_FL, fl);
}
