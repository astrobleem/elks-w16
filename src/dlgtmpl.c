/*
 * dlgtmpl.c - parse a Windows 3.x dialog template (RT_DIALOG resource). Portable and bounds-checked:
 * used by the ELKS dialog manager and by the host tests.
 *
 * DLGTEMPLATE: DWORD style, BYTE count, WORD x, y, cx, cy, menu, class, caption strings,
 *              [WORD pointsize, face string if DS_SETFONT]
 * DLGITEMTEMPLATE: WORD x, y, cx, cy, id, DWORD style, class (BYTE 0x80.. or string), text, BYTE extra, extra bytes
 * A string is 0xFF + WORD ordinal (kept as "#n") or NUL-terminated text.
 */
#include <string.h>
#include "w16.h"

#define DS_SETFONT 0x40

static u16 g16(const u8 *p) { return (u16)(p[0] | (p[1] << 8)); }

static int get_str(const u8 *d, u16 len, u16 *p, char *out, u16 max)
{
    u16 i = 0;
    if (*p >= len)
        return -1;
    if (d[*p] == 0xFF) {
        u16 v, k = 0;
        char tmp[8];
        if (*p + 3 > len)
            return -1;
        v = g16(d + *p + 1);
        *p += 3;
        do { tmp[k++] = (char)('0' + v % 10); v /= 10; } while (v && k < 6);
        out[i++] = '#';
        while (k && i + 1 < max)
            out[i++] = tmp[--k];
        out[i] = 0;
        return 0;
    }
    while (*p < len && d[*p]) {
        if (i + 1 < max)
            out[i++] = (char)d[*p];
        (*p)++;
    }
    if (*p >= len)
        return -1;              /* unterminated */
    (*p)++;
    out[i] = 0;
    return 0;
}

int dlg_parse(const u8 *d, u16 len, struct dlg_tmpl *t)
{
    u16 p = 13, k;
    memset(t, 0, sizeof(*t));
    if (len < 13)
        return -1;
    t->style = (u32)g16(d) | ((u32)g16(d + 2) << 16);
    t->nitems = d[4];
    t->x = (i16)g16(d + 5); t->y = (i16)g16(d + 7); t->cx = (i16)g16(d + 9); t->cy = (i16)g16(d + 11);
    if (t->nitems > DLG_MAXITEMS)
        return -1;
    if (get_str(d, len, &p, t->menu, sizeof(t->menu)) || get_str(d, len, &p, t->clsname, sizeof(t->clsname)) ||
        get_str(d, len, &p, t->caption, sizeof(t->caption)))
        return -1;
    if (t->style & DS_SETFONT) {
        if (p + 2 > len)
            return -1;
        t->pointsize = g16(d + p);
        p += 2;
        if (get_str(d, len, &p, t->face, sizeof(t->face)))
            return -1;
    }
    for (k = 0; k < t->nitems; k++) {
        struct dlg_item *it = &t->item[k];
        if (p + 14 > len)
            return -1;
        it->x = (i16)g16(d + p); it->y = (i16)g16(d + p + 2); it->cx = (i16)g16(d + p + 4); it->cy = (i16)g16(d + p + 6);
        it->id = g16(d + p + 8);
        it->style = (u32)g16(d + p + 10) | ((u32)g16(d + p + 12) << 16);
        p += 14;
        if (p >= len)
            return -1;
        if (d[p] & 0x80) {
            it->cls = d[p++];
        } else if (get_str(d, len, &p, it->clsname, sizeof(it->clsname)))
            return -1;
        if (get_str(d, len, &p, it->text, sizeof(it->text)))
            return -1;
        if (p >= len)
            return -1;
        p += 1 + d[p];          /* creation data */
        if (p > len)
            return -1;
    }
    return 0;
}

/* MENUITEMTEMPLATEHEADER {WORD version, WORD offset}, then items {WORD flags, [WORD id unless MF_POPUP], text\0};
 * a popup's items follow it; MF_END marks the last item at each level. Popups nest at most two deep. */
static int menu_items(const u8 *d, u16 len, u16 *p, struct menu_tmpl *t, u8 parent, int depth)
{
    for (;;) {
        struct menu_item *it;
        u16 fl, id = 0, k = 0;
        if (*p + 2 > len || t->n >= MENU_MAXITEMS)
            return -1;
        fl = g16(d + *p);
        *p += 2;
        if (!(fl & MF_POPUP)) {
            if (*p + 2 > len)
                return -1;
            id = g16(d + *p);
            *p += 2;
        }
        it = &t->it[t->n++];
        it->flags = fl & (u16)~MF_END;
        it->id = id;
        it->parent = parent;
        while (*p < len && d[*p]) {
            if (k + 1u < sizeof(it->text))
                it->text[k++] = (char)d[*p];
            (*p)++;
        }
        if (*p >= len)
            return -1;
        (*p)++;
        it->text[k] = 0;
        if (!k && !id && !(fl & MF_POPUP))
            it->flags |= MF_SEPARATOR;
        if (fl & MF_POPUP) {
            if (depth >= 2 || menu_items(d, len, p, t, (u8)(t->n - 1), depth + 1))
                return -1;
        }
        if (fl & MF_END)
            return 0;
    }
}

int menu_parse(const u8 *d, u16 len, struct menu_tmpl *t)
{
    u16 p;
    memset(t, 0, sizeof(*t));
    if (len < 6 || g16(d) != 0)
        return -1;
    p = (u16)(4 + g16(d + 2));
    if (p >= len)
        return -1;
    return menu_items(d, len, &p, t, 0xFF, 0);
}
