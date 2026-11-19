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
 *     -m  measure: startup time, input-to-screen latency, timer message cost, resource census
 *     -q  test hook: close the program's main window SECS seconds after start (as the system menu would)
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
const char *const usage_host =
    "usage: w16host [-v] [-f N] [-o DIR] [-R TYPE NAME|#ID]... [-D NAME|#ID] [-M NAME|#ID] [-S ID]... [-B]\n"
    "               PROGRAM.EXE\n"
    "       w16host -P INIFILE SECTION KEY [VALUE|-]   (get; set; - deletes)\n"
    "  -B  self-test: write one byte past a host allocation, which must abort\n"
    "  -R  look up a resource (TYPE is a number or a type name; NAME by name, NAMETABLE or #ordinal)\n"
    "  -D  parse a DIALOG resource and print its template\n"
    "  -M  parse a MENU resource and print its items\n"
    "  -S  print string ID from the STRINGTABLE\n";

static void name_arg(const char *a, const char **name, u16 *id)
{
    *id = 0;
    *name = a;
    if (a[0] == '#') {
        *id = (u16)atoi(a + 1);
        *name = 0;
    }
}

static int dump_dialog(struct ne_module *m, const char *arg)
{
    static struct dlg_tmpl t;
    static u8 buf[4096];
    const char *name;
    u16 id, len, k;
    u32 off;
    name_arg(arg, &name, &id);
    if (ne_res_find(m, RT_DIALOG, name, id, &off, &len)) {
        printf("dialog %s not found\n", arg);
        return 1;
    }
    if (len > sizeof(buf))
        len = sizeof(buf);
    if (ne_read(m, off, buf, len) || dlg_parse(buf, len, &t)) {
        printf("dialog %s malformed\n", arg);
        return 1;
    }
    printf("dialog style %08lx at %d,%d %dx%d menu '%s' class '%s' caption '%s' font %u '%s', %u items\n",
           (unsigned long)t.style, t.x, t.y, t.cx, t.cy, t.menu, t.clsname, t.caption, t.pointsize, t.face, t.nitems);
    for (k = 0; k < t.nitems; k++)
        printf("item id %u class %02x '%s' style %08lx at %d,%d %dx%d text '%s'\n", t.item[k].id, t.item[k].cls,
               t.item[k].clsname, (unsigned long)t.item[k].style, t.item[k].x, t.item[k].y, t.item[k].cx, t.item[k].cy,
               t.item[k].text);
    return 0;
}

static int dump_menu(struct ne_module *m, const char *arg)
{
    static struct menu_tmpl t;
    static u8 buf[2048];
    const char *name;
    u16 id, len, k;
    u32 off;
    name_arg(arg, &name, &id);
    if (ne_res_find(m, RT_MENU, name, id, &off, &len)) {
        printf("menu %s not found\n", arg);
        return 1;
    }
    if (len > sizeof(buf))
        len = sizeof(buf);
    if (ne_read(m, off, buf, len) || menu_parse(buf, len, &t)) {
        printf("menu %s malformed\n", arg);
        return 1;
    }
    printf("menu %s: %u items\n", arg, t.n);
    for (k = 0; k < t.n; k++)
        printf("item %u parent %d flags %04x id %u text '%s'\n", k, t.it[k].parent == 0xFF ? -1 : t.it[k].parent,
               t.it[k].flags, t.it[k].id, t.it[k].text);
    return 0;
}

static int profile_cmd(int argc, char **argv)
{
    char v[128];
    int n;
    if (argc == 5) {
        n = prof_get(argv[2], argv[3], argv[4], v, sizeof(v));
        if (n < 0)
            printf("absent\n");
        else
            printf("value '%s' length %d\n", v, n);
        return 0;
    }
    if (argc == 6) {
        n = prof_put(argv[2], argv[3], strcmp(argv[4], "-") ? argv[4] : 0, strcmp(argv[5], "-") ? argv[5] : 0);
        printf("put %s\n", n ? "failed" : "ok");
        return n ? 1 : 0;
    }
    fputs(usage_host, stderr);
    return 2;
}

int main(int argc, char **argv)
{
    struct ne_module m;
    const char *outdir = 0, *dlg = 0, *mnu = 0, *rtype[8], *rname[8];
    int i, e, nres = 0, k, nstr = 0;
    u16 strid[8];

    if (argc > 1 && !strcmp(argv[1], "-P"))
        return profile_cmd(argc, argv);

    for (i = 1; i < argc && argv[i][0] == '-'; i++) {
        if (!strcmp(argv[i], "-v")) opt_verbose = 1;
        else if (!strcmp(argv[i], "-f") && i + 1 < argc) host_fail_after = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-o") && i + 1 < argc) outdir = argv[++i];
        else if (!strcmp(argv[i], "-R") && i + 2 < argc && nres < 8) { rtype[nres] = argv[++i]; rname[nres++] = argv[++i]; }
        else if (!strcmp(argv[i], "-D") && i + 1 < argc) dlg = argv[++i];
        else if (!strcmp(argv[i], "-M") && i + 1 < argc) mnu = argv[++i];
        else if (!strcmp(argv[i], "-S") && i + 1 < argc && nstr < 8) strid[nstr++] = (u16)atoi(argv[++i]);
        else if (!strcmp(argv[i], "-B")) {     /* self-test of the host bounds checker: must abort */
            u16 s;
            if (seg_alloc(1, &s)) return 3;
            wr8(s, 15, 0);                      /* last byte of the 16-byte block: allowed */
            wr8(s, 16, 0);                      /* one past the end: aborts */
            return 4;
        }
        else { fputs(usage_host, stderr); return 2; }
    }
    if (i >= argc) { fputs(usage_host, stderr); return 2; }
    e = ne_open(&m, argv[i]);
    if (!e && (nres || dlg || mnu || nstr)) {
        for (k = 0; k < nres; k++) {
            const char *name, *tname = rtype[k][0] >= '0' && rtype[k][0] <= '9' ? 0 : rtype[k];
            u16 id, len;
            u32 off;
            name_arg(rname[k], &name, &id);
            if (ne_res_find_t(&m, tname ? 0 : (u16)atoi(rtype[k]), tname, name, id, &off, &len))
                printf("resource %s %s not found\n", rtype[k], rname[k]);
            else
                printf("resource %s %s at %lu length %u\n", rtype[k], rname[k], (unsigned long)off, len);
        }
        if (dlg)
            dump_dialog(&m, dlg);
        if (mnu)
            dump_menu(&m, mnu);
        for (k = 0; k < nstr; k++) {
            char s[256];
            int n = ne_load_string(&m, strid[k], s, sizeof(s));
            if (n < 0)
                printf("string %u not found\n", strid[k]);
            else
                printf("string %u '%s' length %d\n", strid[k], s, n);
        }
        ne_free(&m);
        printf("live allocations after free %d\n", host_live);
        return 0;
    }
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
#include "nano-X.h"
#include "int21.h"
#include "launch.h"
#include "taskimage.h"
extern int opt_strict_dos, opt_metrics, opt_quit_s;
extern u32 w16_t0;
u32  now_ms(void);
void kernel_shutdown(void);
int  task_setup(struct ne_module *m, const char *path, int argc, char **argv);
u16  W16_CDECL w16_start16(const u16 *regs);
void W16_CDECL w16_thunk_done(void);

static void usage(void)
{
    fputs("usage: w16 [-u] [-v] [-t] [-e] [-n] [-s] [-m] [-T] [-d PREFIX] [-r] [-q SECS] [-l MODULE=DLL] PROGRAM.EXE [ARGS...]\n", stderr);
    exit(2);
}

int main(int argc, char **argv)
{
    const char *dllpath=0;
    char dllname[9];
    int i,e,checkonly=0,tasks=0;
    const char *path;

    /* Refuse a legacy stack-call Nano-X archive at link time. */
    { extern void w16_nanox_regparm_abi(void); w16_nanox_regparm_abi(); }
    w16_t0 = now_ms();
    for (i = 1; i < argc && argv[i][0] == '-'; i++) {
        char *o = argv[i] + 1;
        if (!strcmp(o, "d") && i + 1 < argc) { strncpy(opt_dump, argv[++i], sizeof(opt_dump) - 1); continue; }
        if (!strcmp(o, "q") && i + 1 < argc) { opt_quit_s = atoi(argv[++i]); continue; }
        if (!strcmp(o,"l") && i+1<argc) {
            const char *arg=argv[++i], *eq=strchr(arg,'=');
            unsigned j,n=eq?(unsigned)(eq-arg):0;
            if(dllpath || !eq || !n || n>8 || !eq[1]) usage();
            for(j=0;j<n;j++) dllname[j]=arg[j]>='a'&&arg[j]<='z'?arg[j]-32:arg[j];
            dllname[n]=0; dllpath=eq+1; continue;
        }
        for (; *o; o++)
            switch (*o) {
            case 'v': opt_verbose = 1;
#ifdef W16_COMPACT_LOG
                wlog("w16: compact build: use -t for API calls; detailed verbose logs omitted\n");
#endif
                break;
            case 't': opt_trace = 1; break;
            case 'u': opt_missing = 1; break;
            case 'e': opt_debugport = 1; break;
            case 'n': checkonly = 1; break;
            case 's': opt_strict_dos = 1; break;
            case 'r': opt_ready = 1; break;
            case 'm': opt_metrics = 1; break;
            case 'T': tasks=1; break;
            default: usage();
            }
    }
    if (i >= argc)
        usage();
    path = argv[i];

    if(tasks&&!checkonly) {
        e=launch_prepare(task_image_sp());
        if(e){wlog("w16: %s rejected: %s\n",path,ne_errstr(e));launch_finish();exit(e);}
    }
    w16_run(path,argc-i-1,argv+i+1,dllpath,dllname,checkonly);
    return 1;
}

void w16_run(const char *path,int argc,char **argv,const char *dllpath,const char *dllname,int checkonly)
{
    static struct ne_module m,dll;
    int dllindex=-1,e,graphics_open=0;
    u16 regs[13],code;
    w16_t0=now_ms();
    {
        extern int W16_CDECL w16_cpu_is_nec(void);
        vlog("w16: CPU %s\n", w16_cpu_is_nec() ? "NEC V20/V30 (AAD ignores its base operand)" : "Intel 8086/8088 class");
    }
    if (w16_thunk_init()) {
        wlog("w16: another w16 process shares this program's code segment; run one Win16 program at a time\n");
        launch_reply(8);session_shutdown();launch_finish();exit(1);
    }
    dll.fd=-1;
    e = ne_open(&m, path);
#ifdef W16_STARTUP_TRACE
    wlog("w16: NE header %d; GrOpen next\n", e);
#endif
    /* GrOpen may fork/exec the server. Do that before resident DLL and EXE
     * segments consume conventional memory. Keep this connection open so
     * an automatically started, non-persistent server survives loading;
     * user_init's GrOpen reuses it. Check-only must remain graphics-free. */
    if (!e && !checkonly && GrOpen() < 0) {
        wlog("w16: cannot connect to the Nano-X server before loading the program\n");
        ne_free(&m);
        w16_thunk_done();
        /* Match check-only teardown: do not return through the ELKS CRT. */
        launch_reply(8);session_shutdown();launch_finish();exit(1);
    }
    if (!e && !checkonly) graphics_open = 1;
#ifdef W16_STARTUP_TRACE
    wlog("w16: GrOpen completed\n");
#endif
    if(!e && dllpath) {
        int k;
        for(k=0;k<m.nmod;k++) if(!strcmp(m.modname[k],dllname)) { dllindex=k; break; }
        if(dllindex<0) {
            wlog("w16: explicitly requested DLL %s is not imported by %s\n",dllname,path);
            e=W16_EIMPORT;
        }
        if(!e) e=ne_open_library(&dll,dllpath);
        /* Bounded spare room lets the DLL grow fixed local blocks without
         * changing its real-mode DS or any relocated exported prologue. */
        if(!e && dll.heap<8192 && (u32)dll.heap_start+8192<=0xfff0UL) e=ne_reserve_library_heap(&dll,8192);
        if(!e) e=ne_check_imports(&dll);
        if(!e) e=ne_load(&dll);
        if(!e) e=ne_bind_library(&m,dllindex,&dll);
    }
    if (!e) e = ne_check_imports(&m);
    if (!e) e = ne_load(&m);
    if (!e) e = task_setup(&m,path,argc,argv);
    if (e) {
        wlog("w16: %s rejected: %s\n", path, ne_errstr(e));
        ne_free(&m); ne_free(&dll);
        if (graphics_open) GrClose();
        w16_thunk_done();
        /* Never unwind through the ELKS medium-model CRT on rejection. */
        launch_reply(launch_error(e));session_shutdown();launch_finish();exit(e);
    }
    wlog("w16: %s loaded: %u segments, %lu bytes, %u fixups (%u import references), DGROUP %04x (%u bytes)\n",
         path, m.nseg, (unsigned long)m.total_bytes, m.nreloc, m.nimport, m.dgroup, m.dgroup_size);
    if (checkonly) {
        ne_free(&m); ne_free(&dll);
        w16_thunk_done();
        wlog("w16: check only (-n): all segments freed\n");
        /* Use explicit libc teardown, as on the run path. Returning from
         * this check-only path has crashed in ELKS medium-model guests. */
        session_shutdown();launch_finish();exit(0);
    }
    w16_mod = &m;
    if (user_init()) {
        ne_free(&m); ne_free(&dll);
        GrClose();
        w16_thunk_done();
        launch_reply(8);session_shutdown();launch_finish();exit(1);
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
    if(launch_attach(m.dgroup)) {
        wlog("w16: INT21 failed\n");launch_reply(8);code=1;goto finished;
    }
    vlog("w16: starting %s at %04x:%04x, SS:SP %04x:%04x\n", path, regs[1], regs[0], regs[2], regs[3]);
    if (opt_debugport)
        e9puts("@@START\n");

    if(dllpath) {
        u16 dr[13];
        memcpy(dr,regs,sizeof(dr));
        dr[0]=dll.ip; dr[1]=dll.seg[dll.cs-1].sel;
        dr[4]=dll.dgroup; dr[6]=0; dr[7]=0;
        dr[8]=dll.heap; dr[9]=0; dr[10]=0; dr[11]=dll.dgroup; dr[12]=0;
        vlog("w16: initializing %s at %04x:%04x DS=%04x heap=%u\n",
             dllpath,dr[1],dr[0],dr[4],dr[8]);
        code=w16_init16(dr);
        if(code!=1) {
            wlog("w16: DLL initializer %s failed or trapped: %u\n",dllpath,code);
            code=code?code:W16_EIMPORT;
            goto finished;
        }
    }
    launch_reply(m.dgroup);
    code = w16_start16(regs);
finished:

    int21_router_leave();
    user_shutdown();
    kernel_shutdown();
    session_shutdown();
    {
        u32 bytes = m.total_bytes + dll.total_bytes;
        u16 nseg = m.nseg + dll.nseg;
        ne_free(&m); ne_free(&dll);
        w16_thunk_done();
        wlog("w16: program exited with code %u; freed %u segments + PSP/environment (%lu bytes)\n", code, nseg,
             (unsigned long)bytes);
    }
    /* The native root owns the resident router until every attached task
     * has unregistered. Its guest/windows may already be gone. Never free
     * code or overwrite a vector after an ownership conflict. */
    while(int21_router_close()){launch_poll();usleep(50000);}
    launch_finish();
    if (opt_debugport) {
        char line[32];
        sprintf(line, "@@EXIT %u\n", code);
        e9puts(line);
    }
    /* The guest exits through a nonlocal assembly unwind. Complete libc
     * teardown directly rather than re-entering the startup return frame. */
    exit(code);
}
#endif
