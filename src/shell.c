/* Small native About dialog: application-supplied strings, not Windows branding.
 * Owner/icon handles are accepted but the stock modal dialog backend supplies
 * its own framing; no claim is made to reproduce Windows' system-information UI.
 */
#include <string.h>
#include "w16.h"
u16 message_box(const char *, const char *, u16);

static void about_string(char *out, u16 cap, u16 seg, u16 off)
{
    u16 i;
    for (i = 0; i + 1u < cap && seg && (u32)off + i <= 0xfffful; i++) {
        out[i] = rd8(seg, off+i);
        if (!out[i]) break;
    }
    out[i] = 0;
}

/* ShellAbout(hwnd, app, other, icon) */
u32 sh_ShellAbout(u16 ss, u16 ap)
{
    char app[80], other[128], caption[48], body[212], *split;
    u16 n;
    about_string(app, sizeof(app), AW(8), AW(6));
    about_string(other, sizeof(other), AW(4), AW(2));
    split = strchr(app, '#');
    if (split) {
        *split++ = 0;
        strncpy(caption, app, sizeof(caption)-1);
        caption[sizeof(caption)-1] = 0;
        strcpy(body, split);
    } else {
        strcpy(caption, "About ");
        strncat(caption, app, sizeof(caption)-strlen(caption)-1);
        strcpy(body, app);
    }
    n = strlen(body);
    if (other[0]) {
        if (n) { body[n++] = ' '; body[n++] = '-'; body[n++] = ' '; }
        strcpy(body+n, other);
    }
    return message_box(body, caption, 0) == 1;
}
