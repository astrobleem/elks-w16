#!/usr/bin/env python3
"""Exercise the actual ELKS entry point with instrumented subsystem stubs."""
import os
import pathlib
import subprocess
import tempfile

root = pathlib.Path(__file__).resolve().parents[1]
source = (root / 'src/main.c').read_text()
source = source.split('/* ---------------------------------------------------------------- ELKS */', 1)[1]
source = source.rsplit('#endif', 1)[0].replace('#include "nano-X.h"', '')
stubs = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "w16.h"
#undef FOFF
#undef FSEG
#define FOFF(x) 0
#define FSEG(x) 0
int opt_verbose, opt_trace, opt_debugport, opt_missing, opt_ready;
int opt_strict_dos, opt_metrics, opt_quit_s;
char opt_dump[40];
u32 w16_t0;
struct ne_module *w16_mod;
static int connected;
static struct ne_seg segment;
static void event(const char *s) { puts(s); fflush(stdout); }
static int fail(const char *s) {
    const char *f = getenv("FAIL_STAGE");
    return f && !strcmp(f, s);
}
static int stage(const char *s) { event(s); return fail(s) ? 7 : 0; }
void wlog(const char *fmt, ...) { (void)fmt; }
void vlog(const char *fmt, ...) { (void)fmt; }
void e9puts(const char *s) { (void)s; }
u32 now_ms(void) { return 0; }
int w16_cpu_is_nec(void) { return 0; }
void w16_nanox_regparm_abi(void) {}
int w16_thunk_init(void) { return stage("thunk_init"); }
void w16_thunk_done(void) { event("thunk_done"); }
int GrOpen(void) {
    event(connected ? "reuse" : "connect");
    if (fail("connect")) return -1;
    connected = 1;
    return 3;
}
void GrClose(void) {
    assert(connected); event("close"); connected = 0;
}
static void module_init(struct ne_module *m) {
    memset(m, 0, sizeof(*m)); m->cs = 1; m->seg = &segment;
    m->nmod = 1; strcpy(m->modname[0], "PBRUSH");
}
int ne_open(struct ne_module *m, const char *p) {
    (void)p; module_init(m); return stage("metadata");
}
int ne_open_library(struct ne_module *m, const char *p) {
    (void)p; module_init(m); m->library = 1; return stage("dll_metadata");
}
int ne_reserve_library_heap(struct ne_module *m, u16 n) {
    (void)m; (void)n; return stage("dll_heap");
}
int ne_check_imports(struct ne_module *m) {
    return stage(m->library ? "dll_imports" : "exe_imports");
}
int ne_load(struct ne_module *m) {
    assert(connected || getenv("CHECK_ONLY"));
    m->loaded = 1; return stage(m->library ? "dll_load" : "exe_load");
}
int ne_bind_library(struct ne_module *m, u16 i, struct ne_module *d) {
    (void)m; (void)i; (void)d; return stage("bind");
}
int task_setup(struct ne_module *m, const char *p, int n, char **a) {
    (void)m; (void)p; (void)n; (void)a; return stage("task");
}
void ne_free(struct ne_module *m) {
    event(m->library ? "free_dll" : "free_exe"); m->loaded = 0;
}
const char *ne_errstr(int e) { (void)e; return "injected"; }
int user_init(void) { event("user_init"); assert(GrOpen() >= 0); return fail("user_init"); }
void user_shutdown(void) { event("user_shutdown"); GrClose(); }
void dos_init(void) { event("dos_init"); }
void kernel_shutdown(void) { event("kernel_shutdown"); }
u32 w16_getvect(u16 v) { (void)v; return 0; }
void w16_setvect(u16 v, u16 o, u16 s) { (void)v; (void)o; (void)s; }
void w16_int21(void) {}
u16 w16_init16(const u16 *r) { (void)r; event("dll_init"); return fail("dll_init") ? 7 : 1; }
u16 w16_start16(const u16 *r) { (void)r; event("run"); return 0; }
'''

with tempfile.TemporaryDirectory() as tmp:
    c = pathlib.Path(tmp) / 'startup.c'
    exe = pathlib.Path(tmp) / 'startup'
    c.write_text(stubs + source)
    subprocess.run(['gcc', '-DW16_HOST', '-I' + str(root / 'src'),
                    '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                    '-fno-pie', '-no-pie', str(c), '-o', str(exe)], check=True)

    def run(failure='', check=False, dll=True):
        args = [str(exe)] + (['-n'] if check else [])
        if dll:
            args += ['-l', 'PBRUSH=pbrush.dll']
        args += ['pbrush.exe']
        env = {**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0', 'FAIL_STAGE': failure}
        env.pop('CHECK_ONLY', None)
        if check:
            env['CHECK_ONLY'] = '1'
        result = subprocess.run(args, env=env, text=True, capture_output=True)
        events = result.stdout.splitlines()
        expected = 1 if failure in ('connect', 'thunk_init', 'user_init') else 7 if failure else 0
        assert result.returncode == expected, (failure, check, result.returncode, events, result.stderr)
        return events

    for dll in (False, True):
        events = run(dll=dll)
        assert events.index('metadata') < events.index('connect') < events.index('exe_load')
        if dll:
            assert events.index('connect') < events.index('dll_metadata') < events.index('dll_load')
        assert events.count('connect') == events.count('reuse') == events.count('close') == 1
        assert events.index('reuse') < events.index('run') < events.index('close')
        assert events[-1] == 'thunk_done'
        events = run(check=True, dll=dll)
        assert not set(events) & {'connect', 'reuse', 'close', 'user_init', 'run'}
        assert events[-1] == 'thunk_done'

    assert run('thunk_init') == ['thunk_init']
    events = run('metadata')
    assert not set(events) & {'connect', 'close', 'dll_metadata', 'exe_load'}
    assert events[-1] == 'thunk_done'
    events = run('connect')
    assert not set(events) & {'close', 'dll_metadata', 'exe_load', 'task'}
    assert events[-2:] == ['free_exe', 'thunk_done']
    for failure in ('dll_metadata', 'dll_heap', 'dll_imports', 'dll_load',
                    'bind', 'exe_imports', 'exe_load', 'task'):
        events = run(failure)
        assert events.count('close') == 1 and 'user_init' not in events
        assert events[-2:] == ['close', 'thunk_done']
        assert events.index('free_exe') < events.index('close')
        events = run(failure, check=True)
        assert not set(events) & {'connect', 'close', 'user_init'}
        assert events[-1] == 'thunk_done'
    events = run('user_init')
    assert events.count('close') == 1 and 'run' not in events and events[-1] == 'thunk_done'
    events = run('dll_init')
    assert 'run' not in events and events.count('close') == 1 and events[-1] == 'thunk_done'

print('ELKS startup connection ordering, check-only isolation and failure cleanup checks passed')
