/* Legacy services needed by Win16 applications.
 *
 * There is no SOUND play device in this port. OpenSound reports zero available
 * voices (the unsupported-device behavior used by Modular Windows); callers
 * can disable audio. Never return playback success for discarded notes.
 */
#include "w16.h"

u32 s_OpenSound(u16 ss, u16 ap)
{
    (void)ss; (void)ap;
    vlog("w16: SOUND has no play device (zero voices)\n");
    return 0;
}
u32 s_CloseSound(u16 ss, u16 ap)
{
    (void)ss; (void)ap;
    return 0;                         /* void; no device to close */
}
u32 s_Unavailable(u16 ss, u16 ap)
{
    (void)ss; (void)ap;
    return 0xffffu;                    /* S_SERDVNA: device unavailable */
}

u32 k_FatalExit(u16 ss, u16 ap)
{
    u16 code = AW(0);
    wlog("w16: application called FatalExit(%u)\n", code);
    w16_exit16(code ? code : 1);
    return 0;                         /* w16_exit16 unwinds to the launcher */
}

u32 k_FatalAppExit(u16 ss, u16 ap)
{
    char message[128];
    u16 off = AW(0), seg = AW(2), i;
    for (i = 0; i < sizeof(message) - 1 && seg && (u32)off + i <= 0xfffful; i++) {
        message[i] = rd8(seg, off + i);
        if (!message[i])
            break;
    }
    message[i] = 0;
    wlog("w16: application called FatalAppExit: %s\n", message);
    w16_exit16(1);
    return 0;
}
