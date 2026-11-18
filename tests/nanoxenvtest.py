#!/usr/bin/env python3
"""Exercise the shipped GrOpen autostart patch with intercepted process calls."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
patch = (root / 'patches/nanox-autostart-env.patch').read_text()
source = '\n'.join(line[1:] for line in patch.splitlines()
                   if line[:1] in ('+', ' ') and not line.startswith('+++'))
block = source[source.index('#if UNIX && AUTO_START_SERVER'):]
block = block[:block.index('#endif') + len('#endif')]
pre = r'''
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#define UNIX 1
#define AUTO_START_SERVER 1
#define F_OK 0
#define X_OK 1
#define BIN_NANOX "/bin/nano-x"
#define GR_NAMED_SOCKET "/tmp/nxsock"
#define EPRINTF(...) ((void)0)
static int exists, denied, forkret, execfail, accesses, forks, execs, nxSocket;
static char path[128], option[16];
static jmp_buf done;
static int access(const char *p,int mode) {
    ++accesses;
    if(mode==F_OK) {assert(!strcmp(p,GR_NAMED_SOCKET));return exists?0:-1;}
    assert(mode==X_OK);strcpy(path,p);return denied?-1:0;
}
static int fork(void) {++forks;return forkret;}
static int execl(const char *p,const char *arg,...) {
    const char *a;va_list ap;
    ++execs;assert(!strcmp(p,path)&&!strcmp(arg,p));
    va_start(ap,arg);a=va_arg(ap,const char*);
    if(a){strcpy(option,a);assert(va_arg(ap,const char*)==NULL);}
    va_end(ap);
    if(!execfail)longjmp(done,1);
    return -1;
}
static void _exit(int code){assert(code==1);longjmp(done,2);}
static int start(void) {
    if(nxSocket>=0)return nxSocket;
'''
post = r'''
    return 0;
}
static int run(const char *server,const char *persist,int live,int bad,int pid,int fail) {
    exists=live;denied=bad;forkret=pid;execfail=fail;
    accesses=forks=execs=0;path[0]=option[0]=0;nxSocket=-1;
    if(server)setenv("NXSERVER",server,1);else unsetenv("NXSERVER");
    if(persist)setenv("NXPERSIST",persist,1);else unsetenv("NXPERSIST");
    {int result=setjmp(done);if(result)return 100+result;}
    return start();
}
int main(void) {
    const char *off[]={NULL,"","0","true","01","1 ","2"};
    unsigned i;
    assert(run(NULL,NULL,0,0,0,0)==101 && !strcmp(path,BIN_NANOX) && !option[0]);
    assert(run("","1",0,0,0,0)==101 && !strcmp(path,BIN_NANOX) && !strcmp(option,"-p"));
    assert(run("/bin/nano-color","1",0,0,0,0)==101 && !strcmp(path,"/bin/nano-color") && !strcmp(option,"-p"));
    assert(run("./nano test;echo nope","1",0,0,0,0)==101 && !strcmp(path,"./nano test;echo nope"));
    for(i=0;i<sizeof(off)/sizeof(off[0]);++i)
        assert(run("nano-alt",off[i],0,0,0,0)==101 && !strcmp(path,"nano-alt") && !option[0]);
    assert(run("/missing","1",0,1,0,0)==-1 && forks==0 && execs==0);
    assert(run("/not-executable","1",0,1,0,0)==-1 && forks==0);
    assert(run(NULL,NULL,0,0,-1,0)==-1 && forks==1 && execs==0);
    assert(run(NULL,NULL,0,0,42,0)==0 && forks==1 && execs==0);
    assert(run(NULL,"1",0,0,0,1)==102 && execs==1);
    /* Existing path may be live or stale: neither is deleted or replaced. */
    assert(run("/missing","1",1,1,0,0)==0 && accesses==1 && !forks && !execs);
    nxSocket=7;accesses=0;assert(start()==7 && !accesses);
    puts("Nano-X autostart: 18 default/path/persistence/reuse/failure scenarios passed");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='nxenv-') as tmp:
    c = Path(tmp) / 'test.c'
    c.write_text(pre + block + post)
    exe = Path(tmp) / 'test'
    subprocess.run(['cc', '-std=gnu99', '-Wall', '-Wextra', '-Werror',
                    '-fno-builtin', '-fsanitize=address,undefined', '-no-pie',
                    str(c), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0'))
