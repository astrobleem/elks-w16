/*
 * profile.c - WIN.INI for GetProfileString, GetProfileInt and WriteProfileString:
 * a small text file of [section] headers and key=value lines, read whole and
 * rewritten through a temporary file. Portable: also built into the host tests.
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "w16.h"

#define PROF_MAX 2048                   /* larger files are refused, not truncated */
#define pbuf ((char *)w16_scratch)     /* PROF_MAX + 1 bytes: the shared scratch (see w16.h) */

const char *prof_path(void)
{
    const char *p = getenv("W16INI");
    return p && *p ? p : "win.ini";
}

/* bytes read (0 when there is no file), or -1 when the file cannot be read or is too large;
 * a missing file is normal, anything else is reported, and the writer then leaves the file alone */
static int load(const char *path)
{
    int fd = open(path, O_RDONLY), n = 0, r;
    char c;
    pbuf[0] = 0;
    if (fd < 0) {
        if (errno == ENOENT)
            return 0;
        wlog("w16: cannot open %s (errno %d); profile functions leave it alone\n", path, errno);
        return -1;
    }
    while (n < PROF_MAX && (r = read(fd, pbuf + n, PROF_MAX - n)) > 0)
        n += r;
    if (r < 0) {
        close(fd);
        wlog("w16: cannot read %s (errno %d); profile functions leave it alone\n", path, errno);
        return -1;
    }
    if (n == PROF_MAX && read(fd, &c, 1) > 0) {
        close(fd);
        wlog("w16: %s is larger than %d bytes; profile functions ignore it\n", path, PROF_MAX);
        return -1;
    }
    close(fd);
    pbuf[n] = 0;
    return n;
}

static int ieqn(const char *a, int n, const char *b)    /* a[0..n) equals b, ignoring case */
{
    int i;
    for (i = 0; i < n; i++) {
        char x = a[i], y = b[i];
        if (!y)
            return 0;
        if (x >= 'a' && x <= 'z') x -= 32;
        if (y >= 'a' && y <= 'z') y -= 32;
        if (x != y)
            return 0;
    }
    return b[n] == 0;
}

/* 1: [section] (name in a/an); 2: key=value (key in a/an, value in v/vn); 0: blank, comment or other */
static int parse_line(const char *s, int len, const char **a, int *an, const char **v, int *vn)
{
    const char *eq;
    int kn;
    while (len && (s[len - 1] == '\r' || s[len - 1] == ' ' || s[len - 1] == '\t'))
        len--;
    while (len && (*s == ' ' || *s == '\t')) {
        s++;
        len--;
    }
    if (!len || *s == ';')
        return 0;
    if (*s == '[') {
        const char *e = memchr(s, ']', len);
        if (!e)
            return 0;
        *a = s + 1;
        *an = (int)(e - s - 1);
        return 1;
    }
    eq = memchr(s, '=', len);
    if (!eq)
        return 0;
    kn = (int)(eq - s);
    while (kn && (s[kn - 1] == ' ' || s[kn - 1] == '\t'))
        kn--;
    *a = s;
    *an = kn;
    eq++;
    *vn = len - (int)(eq - s);
    while (*vn && (*eq == ' ' || *eq == '\t')) {
        eq++;
        (*vn)--;
    }
    *v = eq;
    return 2;
}

int prof_get(const char *path, const char *app, const char *key, char *out, int size)
{
    const char *p, *a = 0, *v = 0;
    int an = 0, vn = 0, insec = 0;
    if (size <= 0 || load(path) <= 0)
        return -1;
    for (p = pbuf; *p; ) {
        const char *e = strchr(p, '\n');
        int len = e ? (int)(e - p) : (int)strlen(p), k = parse_line(p, len, &a, &an, &v, &vn);
        if (k == 1)
            insec = ieqn(a, an, app);
        else if (k == 2 && insec && ieqn(a, an, key)) {
            if (vn > size - 1)
                vn = size - 1;
            memcpy(out, v, vn);
            out[vn] = 0;
            return vn;
        }
        p += len + (e ? 1 : 0);
    }
    return -1;
}

static int put(int fd, const char *s, int n)
{
    return write(fd, s, n) == n ? 0 : -1;
}

static int put_key(int fd, const char *key, const char *val)
{
    return put(fd, key, (int)strlen(key)) | put(fd, "=", 1) | put(fd, val, (int)strlen(val)) | put(fd, "\n", 1);
}

/* Set key in [app] to val; val 0 deletes the key, key 0 deletes the whole section.
 * The new contents go to PATH.new; the old file is kept as PATH.bak until the new one is in
 * place, so a failure at any step leaves either the original or a restorable copy. (ELKS
 * rename() is link + unlink and refuses an existing target, so the old file must move first.) */
int prof_put(const char *path, const char *app, const char *key, const char *val)
{
    char tmp[64], bak[64];
    const char *p, *a = 0, *v = 0;
    int an = 0, vn = 0, insec = 0, done = 0, seen = 0, err = 0, fd, had;
    had = load(path);
    if (strlen(path) + 5 > sizeof(tmp) || had < 0)
        return -1;
    sprintf(tmp, "%s.new", path);
    sprintf(bak, "%s.bak", path);
    fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
        return -1;
    for (p = pbuf; *p; ) {
        const char *e = strchr(p, '\n');
        int len = e ? (int)(e - p) : (int)strlen(p), k = parse_line(p, len, &a, &an, &v, &vn), skip = 0;
        if (k == 1) {
            if (insec && key && val && !done) {         /* leaving [app] without the key: add it at its end */
                err |= put_key(fd, key, val);
                done = 1;
            }
            insec = ieqn(a, an, app);
            seen |= insec;
            skip = insec && !key;
        } else if (insec && !key)
            skip = 1;
        else if (k == 2 && insec && ieqn(a, an, key)) {
            if (val && !done)
                err |= put_key(fd, key, val);
            done = skip = 1;
        }
        if (!skip)
            err |= put(fd, p, len) | put(fd, "\n", 1);
        p += len + (e ? 1 : 0);
    }
    if (key && val && !done) {
        if (!seen)
            err |= put(fd, "[", 1) | put(fd, app, (int)strlen(app)) | put(fd, "]\n", 2);
        err |= put_key(fd, key, val);
    }
    if (close(fd) || err) {
        unlink(tmp);
        return -1;
    }
    if (had > 0 || access(path, F_OK) == 0) {       /* an existing file steps aside first */
        unlink(bak);
        if (rename(path, bak)) {
            unlink(tmp);
            return -1;
        }
    }
    if (rename(tmp, path)) {
        rename(bak, path);                          /* put the original back */
        unlink(tmp);
        return -1;
    }
    unlink(bak);
    return 0;
}
