/*
 * w16 - a small Wine-inspired Win16 compatibility layer for ELKS.
 *
 * Loads an unchanged Windows 3.x NE executable into real-mode segments owned by
 * this process, binds its KERNEL/USER/GDI imports to native implementations,
 * and runs it on the 8088 with Nano-X as the display.
 *
 * Builds two ways:
 *   ELKS target (ia16-elf-gcc, small model): the real loader.
 *   W16_HOST (host gcc): NE parsing, loading and relocation only, against a
 *   simulated 1 MB real-mode memory, for host-side tests.
 */
#ifndef W16_H
#define W16_H

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned long  u32;
typedef short          i16;
typedef long           i32;

#define MAXSEG   32          /* NE segments per module */
#define MAXMOD   8           /* imported module names */

/* ---------------------------------------------------------------- far memory */
#ifdef W16_HOST
extern u8 host_mem[0x110000];
#define LIN(s, o)   ((((u32)(u16)(s)) << 4) + (u16)(o))
#define rd8(s, o)       (host_mem[LIN(s, o)])
#define wr8(s, o, v)    (host_mem[LIN(s, o)] = (u8)(v))
#define rd16(s, o)      ((u16)(rd8(s, o) | (rd8(s, (u16)((o) + 1)) << 8)))
#define wr16(s, o, v)   do { u16 _v = (v); wr8(s, o, _v); wr8(s, (u16)((o) + 1), _v >> 8); } while (0)
#else
#define FP(s, o)    ((void __far *)((((unsigned long)(u16)(s)) << 16) | (u16)(o)))
#define rd8(s, o)       (*(volatile u8 __far *)FP(s, o))
#define wr8(s, o, v)    (*(volatile u8 __far *)FP(s, o) = (u8)(v))
#define rd16(s, o)      (*(volatile u16 __far *)FP(s, o))
#define wr16(s, o, v)   (*(volatile u16 __far *)FP(s, o) = (u16)(v))
#endif
#define rd32(s, o)      ((u32)rd16(s, o) | ((u32)rd16(s, (u16)((o) + 2)) << 16))
#define wr32(s, o, v)   do { u32 _w = (v); wr16(s, o, (u16)_w); wr16(s, (u16)((o) + 2), (u16)(_w >> 16)); } while (0)

void fcopy_in(void *dst, u16 seg, u16 off, u16 n);         /* far -> near */
void fcopy_out(u16 seg, u16 off, const void *src, u16 n);  /* near -> far */
void fzero(u16 seg, u16 off, u16 n);
u16  fstrncpy_in(char *dst, u16 seg, u16 off, u16 max);    /* NUL-terminated copy, returns length */

/* segment allocation: whole paragraphs, owned by this process */
int  seg_alloc(u16 paras, u16 *seg);
void seg_free(u16 seg);

/* ---------------------------------------------------------------- logging */
extern int opt_verbose, opt_trace, opt_debugport;
void wlog(const char *fmt, ...);        /* stderr (and port 0xE9 with -e) */
void vlog(const char *fmt, ...);       /* only with -v */
void e9puts(const char *s);             /* debug port only */

/* ---------------------------------------------------------------- NE module */
struct ne_seg {
    u16 fileoff;        /* sector offset (<< align) */
    u16 size;           /* bytes in file (0 = 64K, rejected) */
    u16 flags;
    u16 minalloc;       /* bytes to allocate (0 = 64K, rejected) */
    u16 sel;            /* real-mode segment once loaded */
    u16 paras;          /* allocated paragraphs */
};

struct ne_module {
    const char *path;
    int  fd;
    u32  ne_off;        /* file offset of the NE header */
    u32  filesize;
    /* header fields */
    u16  ent_off, ent_len, flags, autodata, heap, stack, ip, cs, sp, ss;
    u16  nseg, nmod, segtab_off, rsrc_off, resn_off, modref_off, impn_off;
    u16  nmov, align, nres, expver;
    u8   target_os, flags2;
    struct ne_seg seg[MAXSEG];
    char modname[MAXMOD][9];
    int  modid[MAXMOD];         /* index into the provided-module table, -1 if unknown */
    u8  *enttab;                /* copy of the entry table */
    char name[9];               /* module name from the resident-name table */
    /* runtime */
    u16  dgroup;                /* automatic data segment = hInstance */
    u16  dgroup_size;           /* bytes, including stack and local heap */
    u16  init_sp;
    u16  heap_start;
    u16  psp, envseg;
    u16  nreloc, nimport, npatched;
    u32  total_bytes;           /* all memory allocated for the module */
};

/* error codes returned by the loader (also used as process exit status) */
enum {
    W16_OK = 0,
    W16_EIO = 10,       /* unreadable or truncated file */
    W16_EFORMAT,        /* not MZ/NE or inconsistent header */
    W16_EUNSUPP,        /* valid NE feature this layer does not implement */
    W16_EIMPORT,        /* imports a module or entry point this layer does not provide */
    W16_ENOMEM,         /* fmemalloc failed or segment over 64K */
    W16_ERELOC          /* relocation record out of range or malformed */
};

int  ne_open(struct ne_module *m, const char *path);   /* read and validate headers */
int  ne_check_imports(struct ne_module *m);            /* every import provided? */
int  ne_load(struct ne_module *m);                     /* allocate, read, relocate */
void ne_free(struct ne_module *m);
int  ne_entry(struct ne_module *m, u16 ord, u16 *seg, u16 *off);
const char *ne_errstr(int e);

/* ---------------------------------------------------------------- imports */
#define MOD_KERNEL 0
#define MOD_USER   1
#define MOD_GDI    2
#define NMODS      3
extern const char *const w16_modnames[NMODS];

#define API_CONST  0x01     /* value import (selector constant), not callable */
#define API_RAW    0x02     /* raw entry point in thunk.S, not via the dispatcher */
#define API_CDECL  0x04     /* caller pops (wsprintf) */

typedef u32 (*api_fn)(u16 ss, u16 ap);   /* ap: offset of the last-pushed argument */
struct api {
    u8   mod;
    u16  ord;
    const char *name;
    u8   argbytes;          /* Pascal: bytes the callee pops */
    u8   flags;
    api_fn fn;              /* or constant value for API_CONST */
    u16  value;
};
extern const struct api api_table[];
extern const int api_count;
int  api_find_ord(int mod, u16 ord);
int  api_find_name(int mod, const char *name);
const char *api_known_name(int mod, u16 ord);     /* diagnostic names of unimplemented entries */
int  api_target(int idx, u16 *seg, u16 *off);     /* far address an import binds to */

/* ---------------------------------------------------------------- runtime (ELKS) */
extern struct ne_module *w16_mod;
extern u16 w16_app_ss, w16_app_sp, w16_ldr_sp;
extern u16 w16_ret_bx, w16_ret_cx, w16_ret_es;
u32  w16_call16(u16 off, u16 seg, u16 ds, u16 nwords, const u16 *words);
void w16_exit16(u16 code);                  /* terminate the app, unwind to w16_run() */
u16  w16_get_cs(void);
void w16_setvect(u16 vec, u16 off, u16 seg);
u32  w16_getvect(u16 vec);
extern void w16_stubs(void);
extern void w16_int21(void);
extern void w16_dos3call(void);
int  w16_thunk_init(void);                  /* 0 ok, -1 another instance shares our text */

/* argument access inside API handlers */
#define AW(n)   rd16(ss, (u16)(ap + (n)))
#define AL(n)   rd32(ss, (u16)(ap + (n)))

/* subsystem entry points */
int  user_init(void);
void user_shutdown(void);
int  gdi_init(void);
void gdi_shutdown(void);
void dos_init(void);
extern u16 w16_exit_code;
int  w16_dump_frame(const char *tag);       /* frame buffer dump with -d */
extern char opt_dump[40];
extern int  opt_ready;

#endif
