/*
 * main.c - w16 command line.
 *
 *   w16 [-v] [-t] [-e] [-n] [-s] [-d PREFIX] [-r] PROGRAM.EXE [ARGS...]
 *     -v  verbose loader and window-manager log
 *     -t  trace every API and INT 21h call
 *     -e  copy the log to the 0xE9 debug port (emulators) and mark paints
 *     -n  load, relocate and check only; do not run
 *     -s  strict: stop the program at the first unsupported INT 21h function
 *     -d  dump the 32 KB Tandy frame buffer to PREFIX<n>.bin after each paint
 *     -r  create /tmp/w16.ready after the first paint (test synchronisation)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "w16.h"

#ifdef W16_HOST
/* ---------------------------------------------------------------- host test driver */
extern int host_fail_after, host_live;
const char *const usage_host = "usage: w16host [-v] [-f N] [-o DIR] PROGRAM.EXE\n";

int main(int argc, char **argv)
{
    struct ne_module m;
    const char *outdir = 0;
    int i, e;

    for (i = 1; i < argc && argv[i][0] == '-'; i++) {
        if (!strcmp(argv[i], "-v")) opt_verbose = 1;
        else if (!strcmp(argv[i], "-f") && i + 1 < argc) host_fail_after = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-o") && i + 1 < argc) outdir = argv[++i];
        else { fputs(usage_host, stderr); return 2; }
    }
    if (i >= argc) { fputs(usage_host, stderr); return 2; }
    e = ne_open(&m, argv[i]);
    if (!e) e = ne_check_imports(&m);
    if (!e) e = ne_load(&m);
    if (!e && outdir) {
        int s;
        char path[512];
        FILE *f;
        for (s = 0; s < m.nseg; s++) {
            sprintf(path, "%s/seg%d.bin", outdir, s + 1);
            f = fopen(path, "wb");
            if (!f) { perror(path); return 3; }
            fwrite(host_mem + ((u32)m.seg[s].sel << 4), 1, (u32)m.seg[s].paras << 4, f);
            fclose(f);
        }
        sprintf(path, "%s/load.txt", outdir);
        f = fopen(path, "w");
        for (s = 0; s < m.nseg; s++)
            fprintf(f, "seg %d sel %04x paras %u\n", s + 1, m.seg[s].sel, m.seg[s].paras);
        fprintf(f, "dgroup %04x size %u init_sp %04x fixups %u imports %u patched %u total %lu\n", m.dgroup,
                m.dgroup_size, m.init_sp, m.nreloc, m.nimport, m.npatched, (unsigned long)m.total_bytes);
        fclose(f);
    }
    ne_free(&m);
    printf("result %d (%s), live allocations after free %d\n", e, ne_errstr(e), host_live);
    return e;
}

#else
/* ---------------------------------------------------------------- ELKS */
extern int opt_strict_dos;
int  task_setup(struct ne_module *m, const char *path, int argc, char **argv);
u16  w16_start16(const u16 *regs);
void w16_thunk_done(void);

static void usage(void)
{
    fputs("usage: w16 [-v] [-t] [-e] [-n] [-s] [-d PREFIX] [-r] PROGRAM.EXE [ARGS...]\n", stderr);
    exit(2);
}

int main(int argc, char **argv)
{
    static struct ne_module m;
    int i, e, checkonly = 0;
    u32 old21;
    u16 regs[13], code;
    const char *path;

    for (i = 1; i < argc && argv[i][0] == '-'; i++) {
        char *o = argv[i] + 1;
        if (!strcmp(o, "d") && i + 1 < argc) { strncpy(opt_dump, argv[++i], sizeof(opt_dump) - 1); continue; }
        for (; *o; o++)
            switch (*o) {
            case 'v': opt_verbose = 1; break;
            case 't': opt_trace = 1; break;
            case 'e': opt_debugport = 1; break;
            case 'n': checkonly = 1; break;
            case 's': opt_strict_dos = 1; break;
            case 'r': opt_ready = 1; break;
            default: usage();
            }
    }
    if (i >= argc)
        usage();
    path = argv[i];

    {
        extern int w16_cpu_is_nec(void);
        vlog("w16: CPU %s\n", w16_cpu_is_nec() ? "NEC V20/V30 (AAD ignores its base operand)" : "Intel 8086/8088 class");
    }
    if (w16_thunk_init()) {
        wlog("w16: another w16 process shares this program's code segment; run one Win16 program at a time\n");
        return 1;
    }
    e = ne_open(&m, path);
    if (!e) e = ne_check_imports(&m);
    if (!e) e = ne_load(&m);
    if (!e) e = task_setup(&m, path, argc - i - 1, argv + i + 1);
    if (e) {
        wlog("w16: %s rejected: %s\n", path, ne_errstr(e));
        ne_free(&m);
        w16_thunk_done();
        return e;
    }
    wlog("w16: %s loaded: %u segments, %lu bytes, %u fixups (%u import references), DGROUP %04x (%u bytes)\n",
         path, m.nseg, (unsigned long)m.total_bytes, m.nreloc, m.nimport, m.dgroup, m.dgroup_size);
    if (checkonly) {
        ne_free(&m);
        w16_thunk_done();
        wlog("w16: check only (-n): all segments freed\n");
        return 0;
    }
    w16_mod = &m;
    if (user_init()) {
        ne_free(&m);
        w16_thunk_done();
        return 1;
    }
    dos_init();

    /* a Windows task starts with DS = SS = its data segment, BX = stack, CX = heap, DI = hInstance, ES = PSP */
    regs[0] = m.ip;                          regs[1] = m.seg[m.cs - 1].sel;
    regs[2] = m.dgroup;                      regs[3] = m.init_sp;
    regs[4] = m.dgroup;                      regs[5] = m.psp;
    regs[6] = 0;                             regs[7] = m.stack;
    regs[8] = m.heap;                        regs[9] = 0;
    regs[10] = 0;                            regs[11] = m.dgroup;
    regs[12] = 0;

    signal(SIGINT, SIG_IGN);                 /* a handler cannot run while the program owns SS/DS */
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    old21 = w16_getvect(0x21);
    w16_setvect(0x21, (u16)(unsigned)w16_int21, w16_get_cs());
    vlog("w16: starting %s at %04x:%04x, SS:SP %04x:%04x\n", path, regs[1], regs[0], regs[2], regs[3]);
    if (opt_debugport)
        e9puts("@@START\n");

    code = w16_start16(regs);

    w16_setvect(0x21, (u16)old21, (u16)(old21 >> 16));
    user_shutdown();
    {
        u32 bytes = m.total_bytes;
        u16 nseg = m.nseg;
        ne_free(&m);
        w16_thunk_done();
        wlog("w16: program exited with code %u; freed %u segments + PSP/environment (%lu bytes)\n", code, nseg,
             (unsigned long)bytes);
    }
    if (opt_debugport) {
        char line[32];
        sprintf(line, "@@EXIT %u\n", code);
        e9puts(line);
    }
    return code;
}
#endif
