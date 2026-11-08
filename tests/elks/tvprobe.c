/* tvprobe - emulator diagnostics for the Tandy mode 09h frame buffer (test helper, not part of w16).
 *   tvprobe info        print BIOS video state and the kernel reservation to port 0xE9
 *   tvprobe band SEG    XOR the first 8 rows of a 32 KB mode-09h buffer at SEG:0000 (run twice to undo)
 * Only touch 9800h (the kernel-reserved video RAM) or B800h (the CPU video window). */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linuxmt/mem.h>

#define FP(s, o) ((unsigned char __far *)((((unsigned long)(s)) << 16) | (unsigned)(o)))

static void e9(const char *s)
{
    while (*s) {
        unsigned char c = (unsigned char)*s++;
        __asm__ volatile ("outb %%al, %%dx" : : "a"(c), "d"(0xE9));
    }
}

int main(int argc, char **argv)
{
    char line[120];
    if (argc > 1 && !strcmp(argv[1], "info")) {
        unsigned r = 0;
        int k = open("/dev/kmem", O_RDONLY);
        if (k >= 0) { if (ioctl(k, MEM_GETTANDY, &r) < 0) r = 0; close(k); }
        sprintf(line, "@@TV mode %02x cols %u pagesize %u pageoff %u bios_kb %u tandy_reserved %04x\n",
                *FP(0x40, 0x49), *(unsigned __far *)FP(0x40, 0x4A), *(unsigned __far *)FP(0x40, 0x4C),
                *(unsigned __far *)FP(0x40, 0x4E), *(unsigned __far *)FP(0x40, 0x13), r);
        e9(line);
        sprintf(line, "@@TV bda 8a..8f %02x %02x %02x %02x %02x %02x\n", *FP(0x40, 0x8A), *FP(0x40, 0x8B),
                *FP(0x40, 0x8C), *FP(0x40, 0x8D), *FP(0x40, 0x8E), *FP(0x40, 0x8F));
        e9(line);
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "martyfix")) {
        /* EMULATOR-ONLY display workaround: MartyPC 0.4.1 recognises Tandy mode 09h only when the
         * CGA mode-control register has the graphics bit clear; the HX ROM sets it. Rewrite the
         * register from the BIOS copy at 0040:0065 without bit 1. Never use on real hardware. */
        unsigned char v = *FP(0x40, 0x65), nv = v & ~0x02;
        __asm__ volatile ("outb %%al, %%dx" : : "a"(nv), "d"(0x3D8));
        sprintf(line, "@@TV martyfix mode control %02x -> %02x (emulator display workaround)\n", v, nv);
        e9(line);
        return 0;
    }
    if (argc > 2 && !strcmp(argv[1], "band")) {
        unsigned seg = (unsigned)strtoul(argv[2], 0, 16), y, x;
        if (seg != 0x9800 && seg != 0xB800) {
            fprintf(stderr, "tvprobe: only 9800 or B800\n");
            return 2;
        }
        for (y = 0; y < 8; y++)
            for (x = 0; x < 160; x++)
                *FP(seg, ((y & 3) << 13) + (y >> 2) * 160 + x) ^= 0xFF;
        sprintf(line, "@@SAY band-%04x\n", seg);
        e9(line);
        return 0;
    }
    fprintf(stderr, "usage: tvprobe info | tvprobe band 9800|B800\n");
    return 2;
}
