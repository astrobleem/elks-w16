/*
 * ne.c - Win16 New Executable (NE) parsing, loading and relocation.
 *
 * Portable C: the same code runs on ELKS (real-mode segments from fmemalloc)
 * and on the host test harness (simulated 1 MB memory). Every rejection names
 * the feature that is not supported instead of guessing.
 */
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include "w16.h"

/* NE header offsets */
#define NE_ENTOFF   0x04
#define NE_ENTLEN   0x06
#define NE_FLAGS    0x0C
#define NE_AUTODATA 0x0E
#define NE_HEAP     0x10
#define NE_STACK    0x12
#define NE_IP       0x14
#define NE_CS       0x16
#define NE_SP       0x18
#define NE_SS       0x1A
#define NE_NSEG     0x1C
#define NE_NMOD     0x1E
#define NE_SEGTAB   0x22
#define NE_RSRCTAB  0x24
#define NE_RESNTAB  0x26
#define NE_MODREF   0x28
#define NE_IMPNAMES 0x2A
#define NE_NMOV     0x30
#define NE_ALIGN    0x32
#define NE_NRES     0x34
#define NE_OS       0x36
#define NE_FLAGS2   0x37
#define NE_EXPVER   0x3E

/* program flags */
#define NEF_SINGLEDATA  0x0001
#define NEF_MULTIDATA   0x0002
#define NEF_PROTMODE    0x0008
#define NEF_I286        0x0020
#define NEF_I386        0x0040
#define NEF_I87         0x0080
#define NEF_SELFLOAD    0x0800
#define NEF_LINKERR     0x2000
#define NEF_LIBRARY     0x8000

/* segment flags */
#define NES_DATA        0x0001
#define NES_ITERATED    0x0008
#define NES_RELOC       0x0100

/* relocation source types and target flags */
#define RS_LOBYTE   0
#define RS_SEGMENT  2
#define RS_FARADDR  3
#define RS_OFFSET   5
#define RT_INTERNAL 0
#define RT_IMPORDINAL 1
#define RT_IMPNAME  2
#define RT_OSFIXUP  3
#define RF_ADDITIVE 4

static u16 g16(const u8 *p) { return (u16)(p[0] | (p[1] << 8)); }
static u32 g32(const u8 *p) { return (u32)g16(p) | ((u32)g16(p + 2) << 16); }

const char *ne_errstr(int e)
{
    switch (e) {
    case W16_OK:      return "ok";
    case W16_EIO:     return "unreadable or truncated file";
    case W16_EFORMAT: return "not a valid NE executable";
    case W16_EUNSUPP: return "unsupported executable feature";
    case W16_EIMPORT: return "unsupported imports";
    case W16_ENOMEM:  return "out of memory";
    case W16_ERELOC:  return "bad relocation";
    }
    return "error";
}

static int rd_at(struct ne_module *m, u32 off, void *buf, u16 n, const char *what)
{
    if (off > m->filesize || (u32)n > m->filesize - off) {
        wlog("w16: %s: %s at file offset %lu (+%u) lies beyond the end of the file (%lu bytes)\n",
             m->path, what, (unsigned long)off, n, (unsigned long)m->filesize);
        return W16_EIO;
    }
    if (lseek(m->fd, (long)off, SEEK_SET) != (long)off || read(m->fd, buf, n) != (int)n) {
        wlog("w16: %s: read error in %s\n", m->path, what);
        return W16_EIO;
    }
    return 0;
}

/* read a Pascal string from the NE header area; returns length or -1 */
static int rd_pstr(struct ne_module *m, u32 off, char *out, int max, const char *what)
{
    u8 len;
    if (rd_at(m, off, &len, 1, what))
        return -1;
    if (len > max) {
        wlog("w16: %s: %s of %u characters (limit %d)\n", m->path, what, len, max);
        return -1;
    }
    if (len && rd_at(m, off + 1, out, len, what))
        return -1;
    out[len] = 0;
    return len;
}

static int modindex(const char *name)
{
    int i;
    for (i = 0; i < NMODS; i++)
        if (!strcmp(name, w16_modnames[i]))
            return i;
    return -1;
}

int ne_open(struct ne_module *m, const char *path)
{
    u8 h[64];
    u16 i, lfarlc;
    long size;
    int e;

    memset(m, 0, sizeof(*m));
    m->path = path;
    m->fd = open(path, O_RDONLY);
    if (m->fd < 0) {
        wlog("w16: cannot open %s\n", path);
        return W16_EIO;
    }
    size = lseek(m->fd, 0L, SEEK_END);
    if (size < 0) {
        wlog("w16: %s: cannot determine file size\n", path);
        return W16_EIO;
    }
    m->filesize = (u32)size;
    if (m->filesize < 64) {
        wlog("w16: %s: %lu bytes is too small for an MZ header\n", path, (unsigned long)m->filesize);
        return W16_EFORMAT;
    }
    if ((e = rd_at(m, 0, h, 64, "MZ header")))
        return e;
    if (h[0] != 'M' || h[1] != 'Z') {
        wlog("w16: %s: not an MZ executable (starts %02x %02x)\n", path, h[0], h[1]);
        return W16_EFORMAT;
    }
    lfarlc = g16(h + 0x18);
    m->ne_off = g32(h + 0x3C);
    if (lfarlc < 0x40) {
        wlog("w16: %s: plain DOS program (no new-executable header)\n", path);
        return W16_EFORMAT;
    }
    if (m->ne_off < 0x40 || m->ne_off > m->filesize - 64) {
        wlog("w16: %s: new-executable header offset %lu is outside the file\n", path, (unsigned long)m->ne_off);
        return W16_EFORMAT;
    }
    if ((e = rd_at(m, m->ne_off, h, 64, "NE header")))
        return e;
    if (h[0] == 'P' && h[1] == 'E') {
        wlog("w16: %s: 32-bit PE executable; only 16-bit NE (Windows 3.x) is supported\n", path);
        return W16_EUNSUPP;
    }
    if (h[0] == 'L' && (h[1] == 'E' || h[1] == 'X')) {
        wlog("w16: %s: %c%c executable (VxD/OS2 2.x); only 16-bit NE is supported\n", path, h[0], h[1]);
        return W16_EUNSUPP;
    }
    if (h[0] != 'N' || h[1] != 'E') {
        wlog("w16: %s: no NE signature at offset %lu\n", path, (unsigned long)m->ne_off);
        return W16_EFORMAT;
    }
    m->ent_off = g16(h + NE_ENTOFF);  m->ent_len = g16(h + NE_ENTLEN);
    m->flags = g16(h + NE_FLAGS);     m->autodata = g16(h + NE_AUTODATA);
    m->heap = g16(h + NE_HEAP);       m->stack = g16(h + NE_STACK);
    m->ip = g16(h + NE_IP);           m->cs = g16(h + NE_CS);
    m->sp = g16(h + NE_SP);           m->ss = g16(h + NE_SS);
    m->nseg = g16(h + NE_NSEG);       m->nmod = g16(h + NE_NMOD);
    m->segtab_off = g16(h + NE_SEGTAB); m->rsrc_off = g16(h + NE_RSRCTAB);
    m->resn_off = g16(h + NE_RESNTAB);  m->modref_off = g16(h + NE_MODREF);
    m->impn_off = g16(h + NE_IMPNAMES); m->nmov = g16(h + NE_NMOV);
    m->align = g16(h + NE_ALIGN);     m->nres = g16(h + NE_NRES);
    m->target_os = h[NE_OS];          m->flags2 = h[NE_FLAGS2];
    m->expver = g16(h + NE_EXPVER);

    vlog("w16: %s: NE linker %u.%u, target OS %u, flags %04x, expected Windows %u.%02u\n", path,
          h[2], h[3], m->target_os, m->flags, m->expver >> 8, m->expver & 0xFF);

    /* what kind of NE is this? */
    if (m->target_os == 1) {
        wlog("w16: %s: OS/2 1.x executable, not Windows (ELKS can exec some OS/2 programs itself)\n", path);
        return W16_EUNSUPP;
    }
    if (m->target_os != 0 && m->target_os != 2) {
        wlog("w16: %s: target OS %u is not Windows\n", path, m->target_os);
        return W16_EUNSUPP;
    }
    if (m->flags & NEF_LIBRARY) {
        wlog("w16: %s: is a library (DLL), not an application\n", path);
        return W16_EUNSUPP;
    }
    if (m->flags & NEF_LINKERR) {
        wlog("w16: %s: the linker marked this image as containing errors\n", path);
        return W16_EFORMAT;
    }
    if (m->flags & NEF_SELFLOAD) {
        wlog("w16: %s: self-loading application (custom segment loader) is not supported\n", path);
        return W16_EUNSUPP;
    }
    if (m->flags & NEF_PROTMODE) {
        wlog("w16: %s: marked protected-mode only; an 8088 runs real mode\n", path);
        return W16_EUNSUPP;
    }
    if (m->flags & (NEF_I286 | NEF_I386)) {
        wlog("w16: %s: linker flags say it needs an 80%s86 CPU; refusing on an 8088 target\n", path,
             (m->flags & NEF_I386) ? "3" : "2");
        return W16_EUNSUPP;
    }
    if ((m->flags & 3) != NEF_MULTIDATA) {
        wlog("w16: %s: data model %s; applications use one automatic data segment per instance\n", path,
             (m->flags & 3) == 0 ? "NOAUTODATA" : (m->flags & 3) == 1 ? "SINGLEDATA (library style)" : "invalid");
        return W16_EUNSUPP;
    }
    if (m->nseg == 0 || m->nseg > MAXSEG) {
        wlog("w16: %s: %u segments (this layer handles 1..%d)\n", path, m->nseg, MAXSEG);
        return m->nseg ? W16_EUNSUPP : W16_EFORMAT;
    }
    if (!m->autodata || m->autodata > m->nseg || !m->cs || m->cs > m->nseg || !m->ss || m->ss > m->nseg) {
        wlog("w16: %s: segment numbers out of range (autodata %u, CS %u, SS %u, %u segments)\n", path,
             m->autodata, m->cs, m->ss, m->nseg);
        return W16_EFORMAT;
    }
    if (m->ss != m->autodata) {
        wlog("w16: %s: stack segment %u is not the automatic data segment %u\n", path, m->ss, m->autodata);
        return W16_EUNSUPP;
    }
    if (m->nmod > MAXMOD) {
        wlog("w16: %s: imports from %u modules (limit %d)\n", path, m->nmod, MAXMOD);
        return W16_EUNSUPP;
    }
    if (m->align == 0)
        m->align = 9;
    if (m->align > 15) {
        wlog("w16: %s: sector alignment shift %u is invalid\n", path, m->align);
        return W16_EFORMAT;
    }

    /* segment table */
    for (i = 0; i < m->nseg; i++) {
        u8 s[8];
        struct ne_seg *sp = &m->seg[i];
        if ((e = rd_at(m, m->ne_off + m->segtab_off + 8u * i, s, 8, "segment table")))
            return e;
        sp->fileoff = g16(s); sp->size = g16(s + 2); sp->flags = g16(s + 4); sp->minalloc = g16(s + 6);
        if (sp->flags & NES_ITERATED) {
            wlog("w16: %s: segment %u uses iterated (compressed) data, not supported\n", path, i + 1);
            return W16_EUNSUPP;
        }
        if ((sp->fileoff && sp->size == 0) || sp->minalloc == 0) {
            wlog("w16: %s: segment %u is 64 KB; not supported by this layer\n", path, i + 1);
            return W16_EUNSUPP;
        }
        if (sp->fileoff) {
            u32 start = (u32)sp->fileoff << m->align;
            if (start > m->filesize || sp->size > m->filesize - start) {
                wlog("w16: %s: segment %u data (%u bytes at %lu) runs past the end of the file\n", path, i + 1,
                     sp->size, (unsigned long)start);
                return W16_EIO;
            }
        } else if (sp->flags & NES_RELOC) {
            wlog("w16: %s: segment %u has relocations but no data\n", path, i + 1);
            return W16_EFORMAT;
        }
        if (sp->size > sp->minalloc)
            sp->minalloc = sp->size;
        if ((i + 1 == m->cs) && (sp->flags & NES_DATA)) {
            wlog("w16: %s: entry segment %u is a data segment\n", path, i + 1);
            return W16_EFORMAT;
        }
    }
    if (!(m->seg[m->autodata - 1].flags & NES_DATA)) {
        wlog("w16: %s: automatic data segment %u is a code segment\n", path, m->autodata);
        return W16_EFORMAT;
    }

    /* imported module names */
    for (i = 0; i < m->nmod; i++) {
        u8 w[2];
        int k;
        if ((e = rd_at(m, m->ne_off + m->modref_off + 2u * i, w, 2, "module reference table")))
            return e;
        if (rd_pstr(m, m->ne_off + m->impn_off + g16(w), m->modname[i], 8, "imported module name") < 0)
            return W16_EFORMAT;
        for (k = 0; m->modname[i][k]; k++)
            if (m->modname[i][k] >= 'a' && m->modname[i][k] <= 'z')
                m->modname[i][k] -= 32;
        m->modid[i] = modindex(m->modname[i]);
    }

    /* entry table */
    if (m->ent_len > 4096) {
        wlog("w16: %s: entry table of %u bytes (limit 4096)\n", path, m->ent_len);
        return W16_EUNSUPP;
    }
    m->enttab = malloc(m->ent_len + 1);
    if (!m->enttab)
        return W16_ENOMEM;
    if (m->ent_len && (e = rd_at(m, m->ne_off + m->ent_off, m->enttab, m->ent_len, "entry table")))
        return e;
    m->enttab[m->ent_len] = 0;

    /* module name (first resident name) */
    if (rd_pstr(m, m->ne_off + m->resn_off, m->name, 8, "module name") < 0)
        strcpy(m->name, "?");

    if (m->rsrc_off != m->resn_off)
        vlog("w16: %s: has a resource table (resources are not loaded by this layer)\n", path);
    vlog("w16: %s: module %s, %u segments, entry %u:%04x, autodata %u (heap %u, stack %u), imports from %u modules\n",
          path, m->name, m->nseg, m->cs, m->ip, m->autodata, m->heap, m->stack, m->nmod);
    return W16_OK;
}

/* find an entry point by ordinal; returns 0 if found */
int ne_entry(struct ne_module *m, u16 ord, u16 *seg, u16 *off)
{
    u16 p = 0, n = 1;
    while (p + 2 <= m->ent_len) {
        u8 cnt = m->enttab[p], type = m->enttab[p + 1];
        u16 size = type == 0 ? 0 : type == 0xFF ? 6 : 3;
        p += 2;
        if (cnt == 0)
            break;
        if (ord < n + cnt) {
            u16 q = p + (ord - n) * size;
            if (type == 0 || q + size > m->ent_len)
                return -1;
            if (type == 0xFF) { *seg = m->enttab[q + 3]; *off = g16(m->enttab + q + 4); }
            else { *seg = type; *off = g16(m->enttab + q + 1); }
            return (*seg >= 1 && *seg <= m->nseg) ? 0 : -1;
        }
        n += cnt;
        p += cnt * size;
    }
    return -1;
}

/* ------------------------------------------------------------------ relocation records */
struct reloc { u8 st, fl; u16 src, t1, t2; };

typedef int (*reloc_cb)(struct ne_module *m, int segi, struct reloc *r, void *ctx);

static int each_reloc(struct ne_module *m, int segi, reloc_cb cb, void *ctx)
{
    struct ne_seg *sp = &m->seg[segi];
    u8 buf[8 * 32], cnt[2];
    u32 pos;
    u16 n, i, k;
    int e;

    if (!(sp->flags & NES_RELOC))
        return 0;
    pos = ((u32)sp->fileoff << m->align) + sp->size;
    if ((e = rd_at(m, pos, cnt, 2, "relocation count")))
        return e;
    n = g16(cnt);
    pos += 2;
    for (i = 0; i < n; i += k) {
        k = n - i > 32 ? 32 : n - i;
        if ((e = rd_at(m, pos + 8ul * i, buf, 8 * k, "relocation records")))
            return e;
        {
            u16 j;
            for (j = 0; j < k; j++) {
                struct reloc r;
                u8 *q = buf + 8 * j;
                r.st = q[0]; r.fl = q[1]; r.src = g16(q + 2); r.t1 = g16(q + 4); r.t2 = g16(q + 6);
                if ((e = cb(m, segi, &r, ctx)))
                    return e;
            }
        }
    }
    return 0;
}

/* resolve an import record to an api_table index (or <0) */
static int resolve_import(struct ne_module *m, struct reloc *r, char *desc, int dlen)
{
    int mi = r->t1 - 1, idx;
    char nm[33];
    if (mi < 0 || mi >= m->nmod) {
        strcpy(desc, "import module index out of range");
        return -2;
    }
    if ((r->fl & 3) == RT_IMPORDINAL) {
        const char *known = m->modid[mi] >= 0 ? api_known_name(m->modid[mi], r->t2) : 0;
        idx = m->modid[mi] >= 0 ? api_find_ord(m->modid[mi], r->t2) : -1;
        if (idx < 0) {
            if (strlen(m->modname[mi]) + 30 < (unsigned)dlen) {
                strcpy(desc, m->modname[mi]); strcat(desc, ".");
                {   char num[8]; int v = r->t2, j = 7; num[j] = 0;
                    do { num[--j] = '0' + v % 10; v /= 10; } while (v && j);
                    strcat(desc, num + j); }
                if (known && strlen(desc) + strlen(known) + 4 < (unsigned)dlen) {
                    strcat(desc, " ("); strcat(desc, known); strcat(desc, ")");
                }
            }
        }
        return idx;
    }
    if (rd_pstr(m, m->ne_off + m->impn_off + r->t2, nm, 32, "imported entry name") < 0) {
        strcpy(desc, "unreadable imported name");
        return -2;
    }
    idx = m->modid[mi] >= 0 ? api_find_name(m->modid[mi], nm) : -1;
    if (idx < 0 && strlen(m->modname[mi]) + strlen(nm) + 2 < (unsigned)dlen) {
        strcpy(desc, m->modname[mi]); strcat(desc, "."); strcat(desc, nm);
    }
    return idx;
}

/* pass 1: every relocation is of a supported kind and every import is provided */
struct chk { int missing, bad, nimp; char seen[16][64]; };

static int chk_one(struct ne_module *m, int segi, struct reloc *r, void *ctx)
{
    struct chk *c = ctx;
    char desc[64];
    int t = r->fl & 3, idx, k;

    desc[0] = 0;
    if (r->st != RS_LOBYTE && r->st != RS_SEGMENT && r->st != RS_FARADDR && r->st != RS_OFFSET) {
        wlog("w16: %s: segment %d: relocation source type %u (32-bit/48-bit fixup) not supported\n",
             m->path, segi + 1, r->st);
        c->bad++;
        return 0;
    }
    if (t == RT_OSFIXUP) {
        wlog("w16: %s: segment %d: OS fixup type %u (8087 emulation patching) not supported\n",
             m->path, segi + 1, r->t1);
        c->bad++;
        return 0;
    }
    if (t == RT_INTERNAL) {
        u16 s, o;
        if (r->t1 == 0xFF ? ne_entry(m, r->t2, &s, &o) : (r->t1 < 1 || r->t1 > m->nseg)) {
            wlog("w16: %s: segment %d: internal reference to %s %u is invalid\n", m->path, segi + 1,
                 r->t1 == 0xFF ? "entry" : "segment", r->t1 == 0xFF ? r->t2 : r->t1);
            c->bad++;
        }
        return 0;
    }
    c->nimp++;
    idx = resolve_import(m, r, desc, sizeof(desc));
    if (idx >= 0) {
        if ((api_table[idx].flags & API_CONST) && r->st == RS_FARADDR) {
            wlog("w16: %s: constant import %s used as a far address\n", m->path, api_table[idx].name);
            c->bad++;
        }
        return 0;
    }
    if (idx == -2) {
        wlog("w16: %s: segment %d: %s\n", m->path, segi + 1, desc);
        c->bad++;
        return 0;
    }
    for (k = 0; k < c->missing && k < 16; k++)
        if (!strcmp(c->seen[k], desc))
            return 0;
    if (c->missing < 16)
        strcpy(c->seen[c->missing], desc);
    c->missing++;
    return 0;
}

int ne_check_imports(struct ne_module *m)
{
    struct chk c;
    int i, e;

    memset(&c, 0, sizeof(c));
    for (i = 0; i < m->nmod; i++)
        if (m->modid[i] < 0)
            wlog("w16: %s: imports module %s, which this layer does not provide (KERNEL, USER and GDI only)\n",
                 m->path, m->modname[i]);
    for (i = 0; i < m->nseg; i++)
        if ((e = each_reloc(m, i, chk_one, &c)))
            return e;
    for (i = 0; i < c.missing && i < 16; i++)
        wlog("w16: %s: unsupported import %s\n", m->path, c.seen[i]);
    if (c.missing > 16)
        wlog("w16: %s: ... and %d more unsupported imports\n", m->path, c.missing - 16);
    m->nimport = c.nimp;
    if (c.bad)
        return c.bad && !c.missing ? W16_ERELOC : W16_EIMPORT;
    if (c.missing)
        return W16_EIMPORT;
    for (i = 0; i < m->nmod; i++)
        if (m->modid[i] < 0)
            return W16_EIMPORT;
    vlog("w16: %s: all %u import references resolve to provided entry points\n", m->path, c.nimp);
    return W16_OK;
}

/* pass 2: apply */
static int apply_one(struct ne_module *m, int segi, struct reloc *r, void *ctx)
{
    struct ne_seg *sp = &m->seg[segi];
    u16 sel = sp->sel, tseg = 0, toff = 0, src = r->src, width, guard = 0;
    int t = r->fl & 3, isconst = 0;
    char desc[64];

    (void)ctx;
    if (t == RT_INTERNAL) {
        if (r->t1 == 0xFF) {
            u16 s, o;
            if (ne_entry(m, r->t2, &s, &o))
                return W16_ERELOC;
            tseg = m->seg[s - 1].sel; toff = o;
        } else {
            if (r->t1 < 1 || r->t1 > m->nseg)
                return W16_ERELOC;
            tseg = m->seg[r->t1 - 1].sel; toff = r->t2;
        }
    } else if (t == RT_IMPORDINAL || t == RT_IMPNAME) {
        int idx = resolve_import(m, r, desc, sizeof(desc));
        if (idx < 0)
            return W16_EIMPORT;
        if (api_table[idx].flags & API_CONST) {
            isconst = 1; tseg = toff = api_table[idx].value;
            if (r->st == RS_FARADDR)
                return W16_ERELOC;
        } else if (api_target(idx, &tseg, &toff))
            return W16_EIMPORT;
    } else
        return W16_EUNSUPP;
    (void)isconst;

    width = r->st == RS_LOBYTE ? 1 : r->st == RS_FARADDR ? 4 : 2;
    if (r->fl & RF_ADDITIVE) {
        if ((u32)src + width > sp->size) {
            wlog("w16: %s: segment %d: additive fixup at %04x outside the segment data (%u bytes)\n",
                 m->path, segi + 1, src, sp->size);
            return W16_ERELOC;
        }
        switch (r->st) {
        case RS_LOBYTE:  wr8(sel, src, rd8(sel, src) + (toff & 0xFF)); break;
        case RS_SEGMENT: wr16(sel, src, rd16(sel, src) + tseg); break;
        case RS_OFFSET:  wr16(sel, src, rd16(sel, src) + toff); break;
        case RS_FARADDR: wr16(sel, src, rd16(sel, src) + toff); wr16(sel, src + 2, tseg); break;
        }
        m->nreloc++;
        return 0;
    }
    for (;;) {
        u16 next;
        if ((u32)src + width > sp->size || (width == 1 && (u32)src + 2 > sp->size)) {
            wlog("w16: %s: segment %d: fixup chain reaches %04x, outside the segment data (%u bytes)\n",
                 m->path, segi + 1, src, sp->size);
            return W16_ERELOC;
        }
        if (++guard > 8192) {
            wlog("w16: %s: segment %d: fixup chain does not terminate\n", m->path, segi + 1);
            return W16_ERELOC;
        }
        next = rd16(sel, src);
        if (next == src) {
            wlog("w16: %s: segment %d: fixup chain at %04x loops back to itself\n", m->path, segi + 1, src);
            return W16_ERELOC;
        }
        switch (r->st) {
        case RS_LOBYTE:  wr8(sel, src, toff & 0xFF); next = 0xFFFF; break;
        case RS_SEGMENT: wr16(sel, src, tseg); break;
        case RS_OFFSET:  wr16(sel, src, toff); break;
        case RS_FARADDR: wr16(sel, src, toff); wr16(sel, src + 2, tseg); break;
        }
        m->nreloc++;
        if (next == 0xFFFF)
            break;
        src = next;
    }
    return 0;
}

static int load_segment_data(struct ne_module *m, int i)
{
    struct ne_seg *sp = &m->seg[i];
    u8 buf[512];
    u16 done = 0, n;
    u32 pos = (u32)sp->fileoff << m->align;
    int e;

    fzero(sp->sel, 0, sp->paras >= 0x1000 ? 0 : (u16)(sp->paras << 4));
    if (sp->paras >= 0x1000) { fzero(sp->sel, 0, 0x8000); fzero(sp->sel, 0x8000, 0x8000); }
    if (!sp->fileoff)
        return 0;
    while (done < sp->size) {
        n = (u16)(sp->size - done) > (u16)sizeof(buf) ? (u16)sizeof(buf) : (u16)(sp->size - done);
        if ((e = rd_at(m, pos + done, buf, n, "segment data")))
            return e;
        fcopy_out(sp->sel, done, buf, n);
        done += n;
    }
    return 0;
}

/* Windows patches the prologue of exported functions in applications:
 * "push ds; pop ax; nop" (1E 58 90) or "mov ax,ds; nop" (8C D8 90) becomes three NOPs, so the
 * caller's AX (the instance data segment) is loaded into DS by the rest of the prologue. */
static void patch_prologues(struct ne_module *m)
{
    u16 p = 0, ord = 1;
    while (p + 2 <= m->ent_len) {
        u8 cnt = m->enttab[p], type = m->enttab[p + 1], k;
        u16 size = type == 0 ? 0 : type == 0xFF ? 6 : 3;
        p += 2;
        if (!cnt)
            break;
        for (k = 0; k < cnt && p + size <= m->ent_len; k++, ord++, p += size) {
            u16 s, o, sel;
            u8 b0, b1, b2;
            if (type == 0 || !(m->enttab[p] & 1))
                continue;
            if (ne_entry(m, ord, &s, &o) || (m->seg[s - 1].flags & NES_DATA) || (u32)o + 3 > m->seg[s - 1].size)
                continue;
            sel = m->seg[s - 1].sel;
            b0 = rd8(sel, o); b1 = rd8(sel, o + 1); b2 = rd8(sel, o + 2);
            if ((b0 == 0x1E && b1 == 0x58 && b2 == 0x90) || (b0 == 0x8C && b1 == 0xD8 && b2 == 0x90)) {
                wr8(sel, o, 0x90); wr8(sel, o + 1, 0x90);
                m->npatched++;
                vlog("w16: exported entry %u at %u:%04x: prologue patched for instance data in AX\n", ord, s, o);
            }
        }
    }
}

int ne_load(struct ne_module *m)
{
    int i, e;

    for (i = 0; i < m->nseg; i++) {
        struct ne_seg *sp = &m->seg[i];
        u32 bytes = sp->minalloc;
        if (i + 1 == m->autodata) {
            /* automatic data segment: static data, then the stack, then the local heap */
            bytes += m->stack;
            m->heap_start = (u16)((bytes + 1) & ~1ul);
            bytes = m->heap_start + (u32)m->heap;
            if (bytes > 0xFFF0ul) {
                wlog("w16: %s: automatic data segment would be %lu bytes (data %u + stack %u + heap %u), over 64 KB\n",
                     m->path, (unsigned long)bytes, sp->minalloc, m->stack, m->heap);
                return W16_ENOMEM;
            }
            m->dgroup_size = (u16)bytes;
        }
        sp->paras = (u16)((bytes + 15) >> 4);
        if (seg_alloc(sp->paras, &sp->sel)) {
            wlog("w16: %s: no memory for segment %u (%u bytes)\n", m->path, i + 1, sp->paras << 4);
            sp->paras = 0;
            return W16_ENOMEM;
        }
        m->total_bytes += (u32)sp->paras << 4;
        if ((e = load_segment_data(m, i)))
            return e;
        vlog("w16: segment %u: %s %5u bytes in file, %5u allocated at %04x:0000%s\n", i + 1,
              (sp->flags & NES_DATA) ? "data" : "code", sp->size, sp->paras << 4, sp->sel,
              (sp->flags & NES_RELOC) ? ", has relocations" : "");
    }
    m->dgroup = m->seg[m->autodata - 1].sel;
    m->init_sp = m->sp ? m->sp : (u16)(m->seg[m->autodata - 1].minalloc + m->stack) & ~1;
    if (m->init_sp > m->dgroup_size) {
        wlog("w16: %s: initial SP %04x is outside the %u-byte data segment\n", m->path, m->init_sp, m->dgroup_size);
        return W16_EFORMAT;
    }
    for (i = 0; i < m->nseg; i++)
        if ((e = each_reloc(m, i, apply_one, 0)))
            return e;
    patch_prologues(m);
    vlog("w16: %s: %u fixups applied, %u prologues patched, %lu bytes of segments\n", m->path, m->nreloc,
          m->npatched, (unsigned long)m->total_bytes);
    return W16_OK;
}

void ne_free(struct ne_module *m)
{
    int i;
    for (i = 0; i < m->nseg && i < MAXSEG; i++)
        if (m->seg[i].paras) {
            seg_free(m->seg[i].sel);
            m->seg[i].paras = 0;
        }
    if (m->psp) { seg_free(m->psp); m->psp = 0; }
    if (m->envseg) { seg_free(m->envseg); m->envseg = 0; }
    if (m->enttab) { free(m->enttab); m->enttab = 0; }
    if (m->fd >= 0) { close(m->fd); m->fd = -1; }
}
