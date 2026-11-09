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
static void lheap_init(struct ne_module *m);

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
    lheap_init(m);
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

/* GetWinFlags(): real mode on an 8086-class CPU, no EMS frame */
u32 k_GetWinFlags(u16 ss, u16 ap)
{
    (void)ss; (void)ap;
    return 0x0040ul;            /* WF_CPU086 */
}

/* GetModuleUsage(hModule): one instance of everything */
u32 k_GetModuleUsage(u16 ss, u16 ap)
{
    (void)ss; (void)ap;
    return 1;
}

/* LockSegment(wSegment) / UnlockSegment(wSegment): segments never move here; -1 means the caller's DS */
u32 k_LockSegment(u16 ss, u16 ap)
{
    u16 s = AW(0);
    return s == 0xFFFF ? rd16(ss, (u16)(ap - 12 + 4)) : s;
}

u32 k_UnlockSegment(u16 ss, u16 ap)
{
    return k_LockSegment(ss, ap);
}

/* MakeProcInstance(lpProc, hInstance): an instance thunk "mov ax,hInstance; jmp far lpProc",
 * the same code Windows builds in real mode. 32 thunks of 8 bytes in one loader-owned segment. */
#define NTHUNK 32
static u8 thunk_used[NTHUNK];

u32 k_MakeProcInstance(u16 ss, u16 ap)
{
    struct ne_module *m = w16_mod;
    u16 hinst = AW(0), off = AW(2), seg = AW(4), i, t;
    if (!seg && !off)
        return 0;
    if (!m->thunkseg) {
        if (seg_alloc(NTHUNK * 8 / 16, &m->thunkseg)) {
            wlog("w16: MakeProcInstance: no memory for thunks\n");
            return 0;
        }
        m->total_bytes += NTHUNK * 8;
        memset(thunk_used, 0, sizeof(thunk_used));
    }
    for (i = 0; i < NTHUNK && thunk_used[i]; i++)
        ;
    if (i == NTHUNK) {
        wlog("w16: MakeProcInstance: thunk table full (%d)\n", NTHUNK);
        return 0;
    }
    thunk_used[i] = 1;
    t = i * 8;
    wr8(m->thunkseg, t, 0xB8);                  /* mov ax, hInstance */
    wr16(m->thunkseg, t + 1, hinst ? hinst : m->dgroup);
    wr8(m->thunkseg, t + 3, 0xEA);              /* jmp far seg:off */
    wr16(m->thunkseg, t + 4, off);
    wr16(m->thunkseg, t + 6, seg);
    vlog("w16: MakeProcInstance(%04x:%04x, %04x) -> %04x:%04x\n", seg, off, hinst, m->thunkseg, t);
    return ((u32)m->thunkseg << 16) | t;
}

/* FreeProcInstance(lpProc) */
u32 k_FreeProcInstance(u16 ss, u16 ap)
{
    struct ne_module *m = w16_mod;
    u16 off = AW(0), seg = AW(2);
    if (m->thunkseg && seg == m->thunkseg && !(off & 7) && off / 8 < NTHUNK)
        thunk_used[off / 8] = 0;
    return 0;
}

/* ---------------------------------------------------------------- local heap */
/* First-fit blocks in DGROUP between the stack and the end of the segment (the NE heap size).
 * Each block starts with a WORD holding its size in bytes (even, header included) | 1 when in use;
 * a handle is the address of the data, so LMEM_MOVEABLE blocks simply never move. */
static u16 lh_start, lh_end;

static void lheap_init(struct ne_module *m)
{
    lh_start = m->heap_start;
    lh_end = m->dgroup_size & ~1u;
    if (!m->heap || lh_end < lh_start + 4) {
        lh_start = lh_end = 0;
        return;
    }
    wr16(m->dgroup, lh_start, lh_end - lh_start);
}

/* the header of the block in use whose data starts at h, or 0; with h 0, the totals of blocks in use */
static u16 lh_walk(u16 h, u16 *used, u16 *blocks)
{
    u16 p = lh_start, dg = w16_mod->dgroup;
    if (used) *used = 0;
    if (blocks) *blocks = 0;
    while (lh_start && p + 2 <= lh_end) {
        u16 hd = rd16(dg, p), sz = hd & ~1u;
        if (sz < 2 || (u32)p + sz > lh_end) {
            wlog("w16: local heap damaged at DGROUP:%04x (header %04x)\n", p, hd);
            return 0;
        }
        if (h && p + 2 == h)
            return (hd & 1) ? p : 0;
        if ((hd & 1) && used) {
            *used += sz;
            (*blocks)++;
        }
        p += sz;
    }
    return 0;
}

void lheap_census(u16 *used, u16 *blocks, u16 *size)
{
    lh_walk(0, used, blocks);
    *size = lh_end - lh_start;
}

/* LocalAlloc(wFlags, wBytes) */
u32 k_LocalAlloc(u16 ss, u16 ap)
{
    u16 bytes = AW(0), flags = AW(2), dg = w16_mod->dgroup, p = lh_start, need;
    if (bytes > 0xFFF0u)
        return 0;
    need = (u16)((bytes + 3u) & ~1u);
    if (need < 4)
        need = 4;
    while (lh_start && p + 2 <= lh_end) {
        u16 hd = rd16(dg, p), sz = hd & ~1u;
        if (sz < 2 || (u32)p + sz > lh_end)
            break;
        if (!(hd & 1)) {
            while ((u32)p + sz + 2 <= lh_end) {         /* merge the free blocks that follow */
                u16 nx = rd16(dg, p + sz);
                if ((nx & 1) || (nx & ~1u) < 2 || (u32)p + sz + (nx & ~1u) > lh_end)
                    break;
                sz += nx & ~1u;
            }
            wr16(dg, p, sz);
            if (sz >= need) {
                if (sz - need >= 4) {
                    wr16(dg, p + need, sz - need);
                    sz = need;
                }
                wr16(dg, p, sz | 1);
                if (flags & 0x40)                       /* LMEM_ZEROINIT */
                    fzero(dg, p + 2, sz - 2);
                return p + 2;
            }
        }
        p += sz;
    }
    wlog("w16: LocalAlloc(%04x, %u): the %u-byte local heap has no room\n", flags, bytes, lh_end - lh_start);
    return 0;
}

/* LocalFree(hMem): 0 on success, the handle on failure */
u32 k_LocalFree(u16 ss, u16 ap)
{
    u16 h = AW(0), b = lh_walk(h, 0, 0);
    if (!b) {
        wlog("w16: LocalFree(%04x): not a local heap block in use\n", h);
        return h;
    }
    wr16(w16_mod->dgroup, b, rd16(w16_mod->dgroup, b) & ~1u);
    return 0;
}

u32 k_LocalLock(u16 ss, u16 ap)   { return lh_walk(AW(0), 0, 0) ? AW(0) : 0; }
u32 k_LocalUnlock(u16 ss, u16 ap) { (void)ss; (void)ap; return 0; }

u32 k_LocalSize(u16 ss, u16 ap)
{
    u16 b = lh_walk(AW(0), 0, 0);
    return b ? (rd16(w16_mod->dgroup, b) & ~1u) - 2 : 0;
}

/* ---------------------------------------------------------------- resources */
/* FindResource returns an HRSRC naming a file range; LoadResource reads it into a segment of its own,
 * whose selector is the HGLOBAL, so GlobalLock/LockResource return seg:0. Programs may write to it. */
#define MAXRSRC    8
#define HRSRC_BASE 0x0700
static struct { u8 used; u32 off; u16 len, seg; } rsrc[MAXRSRC];

static int rsrc_by_seg(u16 seg)
{
    int i;
    for (i = 0; i < MAXRSRC; i++)
        if (rsrc[i].used && rsrc[i].seg && rsrc[i].seg == seg)
            return i;
    return -1;
}

/* FindResource(hInstance, lpName, lpType) */
u32 k_FindResource(u16 ss, u16 ap)
{
    char tname[34], name[34];
    u16 tid, id, len;
    u32 off;
    int i;
    res_name(AW(0), AW(2), tname, &tid);
    res_name(AW(4), AW(6), name, &id);
    if (ne_res_find_t(w16_mod, tid, tname[0] ? tname : 0, name[0] ? name : 0, id, &off, &len)) {
        wlog("w16: FindResource: resource %s%s of type %s%s not found\n", name[0] ? name : "#",
             name[0] ? "" : "(ordinal)", tname[0] ? tname : "#", tname[0] ? "" : "(number)");
        return 0;
    }
    for (i = 0; i < MAXRSRC; i++)
        if (rsrc[i].used && rsrc[i].off == off)
            return HRSRC_BASE + 2 * i;
    for (i = 0; i < MAXRSRC && rsrc[i].used; i++)
        ;
    if (i == MAXRSRC) {
        wlog("w16: FindResource: resource table full (%d)\n", MAXRSRC);
        return 0;
    }
    rsrc[i].used = 1;
    rsrc[i].off = off;
    rsrc[i].len = len;
    rsrc[i].seg = 0;
    vlog("w16: FindResource %s type %s -> %u bytes at file offset %lu\n", name[0] ? name : "#", tname[0] ? tname : "#",
         len, (unsigned long)off);
    return HRSRC_BASE + 2 * i;
}

/* LoadResource(hInstance, hResInfo) */
u32 k_LoadResource(u16 ss, u16 ap)
{
    static u8 buf[128];
    u16 h = AW(0), i = (u16)(h - HRSRC_BASE) >> 1, paras, pos, n;
    if (h < HRSRC_BASE || (h & 1) || i >= MAXRSRC || !rsrc[i].used) {
        wlog("w16: LoadResource(%04x): not a resource handle\n", h);
        return 0;
    }
    if (rsrc[i].seg)
        return rsrc[i].seg;
    paras = (u16)(((u32)rsrc[i].len + 15) >> 4);
    if (seg_alloc(paras ? paras : 1, &rsrc[i].seg)) {
        rsrc[i].seg = 0;
        wlog("w16: LoadResource: no memory for %u bytes\n", rsrc[i].len);
        return 0;
    }
    for (pos = 0; pos < rsrc[i].len; pos += n) {
        n = rsrc[i].len - pos > sizeof(buf) ? sizeof(buf) : rsrc[i].len - pos;
        if (ne_read(w16_mod, rsrc[i].off + pos, buf, n)) {
            seg_free(rsrc[i].seg);
            rsrc[i].seg = 0;
            return 0;
        }
        fcopy_out(rsrc[i].seg, pos, buf, n);
    }
    vlog("w16: LoadResource -> segment %04x (%u bytes)\n", rsrc[i].seg, rsrc[i].len);
    return rsrc[i].seg;
}

/* GlobalLock(hMem) / LockResource(hResData): far pointer to a loaded resource */
u32 k_GlobalLock(u16 ss, u16 ap)
{
    u16 h = AW(0);
    if (rsrc_by_seg(h) < 0) {
        wlog("w16: GlobalLock(%04x): only loaded resources are global memory here\n", h);
        return 0;
    }
    return (u32)h << 16;
}

u32 k_GlobalUnlock(u16 ss, u16 ap) { (void)ss; (void)ap; return 0; }

/* FreeResource(hResData): 0 on success */
u32 k_FreeResource(u16 ss, u16 ap)
{
    int i = rsrc_by_seg(AW(0));
    if (i < 0)
        return 1;
    seg_free(rsrc[i].seg);
    rsrc[i].used = 0;
    rsrc[i].seg = 0;
    return 0;
}

void rsrc_census(u16 *n, u32 *bytes)
{
    int i;
    *n = 0;
    *bytes = 0;
    for (i = 0; i < MAXRSRC; i++)
        if (rsrc[i].used && rsrc[i].seg) {
            (*n)++;
            *bytes += ((u32)rsrc[i].len + 15) & ~15ul;
        }
}

/* resources the program never freed go with the task, as in Windows */
void kernel_shutdown(void)
{
    int i, n = 0;
    for (i = 0; i < MAXRSRC; i++) {
        if (rsrc[i].used && rsrc[i].seg) {
            seg_free(rsrc[i].seg);
            n++;
        }
        rsrc[i].used = 0;
        rsrc[i].seg = 0;
    }
    if (n)
        vlog("w16: freed %d loaded resource(s) at exit\n", n);
}

/* ---------------------------------------------------------------- strings, instance data */
static u16 fstrlen(u16 seg, u16 off)
{
    u16 n = 0;
    while (rd8(seg, (u16)(off + n)) && n < 0xFFF0u)
        n++;
    return n;
}

/* lstrcpy(lpString1, lpString2) / lstrcat(lpString1, lpString2) / lstrlen(lpString) */
u32 k_lstrcpy(u16 ss, u16 ap)
{
    u16 so = AW(0), sg = AW(2), dof = AW(4), dg = AW(6), i = 0;
    u8 c;
    do {
        c = rd8(sg, (u16)(so + i));
        wr8(dg, (u16)(dof + i), c);
        i++;
    } while (c && i < 0xFFF0u);
    return ((u32)dg << 16) | dof;
}

u32 k_lstrcat(u16 ss, u16 ap)
{
    u16 dof = AW(4), dg = AW(6), n = fstrlen(dg, dof);
    wr16(ss, ap + 4, (u16)(dof + n));
    k_lstrcpy(ss, ap);
    wr16(ss, ap + 4, dof);
    return ((u32)dg << 16) | dof;
}

u32 k_lstrlen(u16 ss, u16 ap) { return fstrlen(AW(2), AW(0)); }

/* GetInstanceData(hInstance, pData, nCount): copy from a previous instance's data segment */
u32 k_GetInstanceData(u16 ss, u16 ap)
{
    u16 n = AW(0), off = AW(2), from = AW(4), to = w16_mod->dgroup, i;
    if (from != to)
        for (i = 0; i < n; i++)
            wr8(to, (u16)(off + i), rd8(from, (u16)(off + i)));
    return n;
}

/* ---------------------------------------------------------------- WIN.INI */
static void farstr(char *dst, u16 off, u16 seg, u16 max)
{
    if (seg || off)
        fstrncpy_in(dst, seg, off, max);
    else
        dst[0] = 0;
}

/* GetProfileInt(lpAppName, lpKeyName, nDefault) */
u32 k_GetProfileInt(u16 ss, u16 ap)
{
    char app[40], key[40], v[16];
    u16 r = 0, k;
    farstr(key, AW(2), AW(4), sizeof(key));
    farstr(app, AW(6), AW(8), sizeof(app));
    if (prof_get(prof_path(), app, key, v, sizeof(v)) < 0)
        return AW(0);
    for (k = 0; v[k] >= '0' && v[k] <= '9'; k++)
        r = (u16)(r * 10 + (v[k] - '0'));
    vlog("w16: GetProfileInt [%s] %s = %u\n", app, key, r);
    return r;
}

/* GetProfileString(lpAppName, lpKeyName, lpDefault, lpReturnedString, nSize) */
u32 k_GetProfileString(u16 ss, u16 ap)
{
    char app[40], key[40], v[80];
    u16 size = AW(0), ro = AW(2), rs = AW(4), n;
    int r;
    if (!size)
        return 0;
    farstr(key, AW(10), AW(12), sizeof(key));
    farstr(app, AW(14), AW(16), sizeof(app));
    if (!AW(10) && !AW(12)) {
        wlog("w16: GetProfileString [%s] with no key (list of keys) is not supported; returning the default\n", app);
        r = -1;
    } else
        r = prof_get(prof_path(), app, key, v, sizeof(v));
    if (r < 0)
        farstr(v, AW(6), AW(8), sizeof(v));
    n = (u16)strlen(v);
    if (n > size - 1)
        n = size - 1;
    fcopy_out(rs, ro, v, n);
    wr8(rs, (u16)(ro + n), 0);
    vlog("w16: GetProfileString [%s] %s = \"%s\"%s\n", app, key, v, r < 0 ? " (default)" : "");
    return n;
}

/* WriteProfileString(lpAppName, lpKeyName, lpString): NULL string deletes the key, NULL key the section */
u32 k_WriteProfileString(u16 ss, u16 ap)
{
    char app[40], key[40], v[80];
    int nokey = !AW(4) && !AW(6), noval = !AW(0) && !AW(2), r;
    farstr(v, AW(0), AW(2), sizeof(v));
    farstr(key, AW(4), AW(6), sizeof(key));
    farstr(app, AW(8), AW(10), sizeof(app));
    r = prof_put(prof_path(), app, nokey ? 0 : key, noval ? 0 : v);
    if (r)
        wlog("w16: WriteProfileString [%s] %s: cannot rewrite %s\n", app, key, prof_path());
    else
        vlog("w16: WriteProfileString [%s] %s = \"%s\"\n", app, nokey ? "(section)" : key, noval ? "(deleted)" : v);
    return r ? 0 : 1;
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
