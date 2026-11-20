#!/usr/bin/env python3
"""Focused raw-console decoder and ownership checks; no emulator required."""
import os,pathlib,subprocess,tempfile
root=pathlib.Path(__file__).resolve().parents[1]
mw=os.environ.get('MWDIR')
if not mw:raise SystemExit('set MWDIR to the Microwindows checkout')
source=(root/'drivers/kbd_elks_raw.c').read_text()
pre=r"""
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include "device.h"
#define DCSET_KRAW 0x4403
#define DCREL_KRAW 0x4404
static int fail,sets,closes,rawsets,rawrels,pending;
static char order[32];static int op;
static void note(char c){order[op++]=c;order[op]=0;}
static int fake_open(const char *n,int f){assert(!strcmp(n,"/dev/tty1"));assert(f==(O_RDONLY|O_NONBLOCK));note('o');return fail==1?-1:3;}
static int fake_close(int f){assert(f==3);closes++;note('c');return 0;}
static int fake_get(int f,struct termios*t){assert(f==3);note('g');memset(t,0,sizeof(*t));t->c_lflag=ECHO|ICANON|ISIG|IEXTEN;return fail==2?-1:0;}
static int fake_set(int f,int a,const struct termios*t){assert(f==3&&a==TCSAFLUSH);sets++;if(t->c_lflag&ISIG)assert(!pending);note((t->c_lflag&ISIG)?'t':'s');return fail==3&&sets==1?-1:0;}
static int fake_ioctl(int f,int cmd,int v){assert(f==3&&v==0);if(cmd==DCSET_KRAW){rawsets++;note('k');return fail==4?-1:0;}assert(cmd==DCREL_KRAW);rawrels++;note('r');return 1;}
static ssize_t fake_read(int f,void*b,size_t n){assert(f==3);if(n==128){assert(rawrels==1||!rawsets);if(pending){*(unsigned char*)b=pending==1?0xb8:'x';pending=0;note('d');return 1;}return 0;}assert(n==1);errno=EAGAIN;return -1;}
int GdError(const char*f,...){(void)f;return 0;}
#define open fake_open
#define close fake_close
#define tcgetattr fake_get
#define tcsetattr fake_set
#define ioctl fake_ioctl
#define read fake_read
"""
body='\n'.join(l for l in source.splitlines() if not l.startswith('#include') or l=='#include "elks_set1.h"')
main=r"""
static MWKEY k;static MWKEYMOD m;static MWSCANCODE scan;static struct elks_keys s;
static int step(unsigned c){return elks_decode(&s,c,&k,&m,&scan);}
int main(void){int n;MWKEYMOD possible,current;
 unsetenv("CONSOLE");unsetenv("KEYBOARD");
 assert(step(0x39)==1&&k==' '&&!m);assert(step(0xb9)==2&&k==' ');
 step(0x38);assert(step(0x39)==1&&k==' '&&(m&MWKMOD_ALT));step(0xb9);
 assert(step(0x3e)==1&&k==MWKEY_F4&&(m&MWKMOD_ALT));assert(step(0xbe)==2&&k==MWKEY_F4);
 assert(step(0x11)==1&&k=='w'&&(m&MWKMOD_ALT));step(0x91);step(0xb8);
 assert(step(0x39)==1&&k==' '&&!m);step(0xb9);
 step(0x2a);assert(step(0x1e)==1&&k=='A');step(0xaa);assert(step(0x9e)==2&&k=='A'&&!m);
 step(0x3a);step(0x3a);step(0xba);assert(step(0x1e)==1&&k=='A');step(0x9e);
 step(0x2a);assert(step(0x1e)==1&&k=='a');step(0x9e);step(0xaa);
 step(0x1d);assert(step(0x2e)==1&&k==3&&(m&MWKMOD_CTRL));step(0xae);step(0x9d);
 assert(step(0xe0)==0);assert(step(0x1d)==1&&(m&MWKMOD_RCTRL));step(0xe0);step(0x9d);
 step(0xe0);assert(step(0x4b)==1&&k==MWKEY_LEFT&&scan==0x14b);step(0xe0);assert(step(0xcb)==2&&k==MWKEY_LEFT);
 step(0xe0);assert(!step(0x2a)&&!(s.mods&MWKMOD_SHIFT));
 step(0x45);step(0xc5);assert(step(0x47)==1&&k=='7');step(0xc7);
 step(0x2a);assert(step(0x47)==1&&k==MWKEY_HOME);step(0xc7);step(0xaa);
 for(n=0;n<5;n++){
  fail=n;sets=closes=rawsets=rawrels=op=0;memset(order,0,sizeof(order));
  if(!n){pending=2;assert(raw_open(0)==3);raw_modifiers(&possible,&current);assert(possible&MWKMOD_ALT);assert(!current);assert(raw_read(&k,&m,&scan)==0);pending=1;raw_close();assert(!strcmp(order,"ogsdkrdtc"));assert(rawsets==1&&rawrels==1);}
  else{assert(raw_open(0)==DRIVER_FAIL);assert(fd==-1&&!raw);assert(!rawrels);if(n>=3)assert(order[op-2]=='t');}
  assert(closes==(n==1?0:1));raw_close();assert(closes==(n==1?0:1));
 }
 puts("ELKS set-1 modifiers, make/break, locks, keypad, extended keys and raw/termios ownership failure unwind passed");return 0;
}
"""
with tempfile.TemporaryDirectory() as td:
 p=pathlib.Path(td);(p/'test.c').write_text(pre+body+main)
 subprocess.run(['cc','-Wall','-Wextra','-fsanitize=address,undefined','-fno-pie','-no-pie','-I'+mw+'/src/include','-I'+str(root/'drivers'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
