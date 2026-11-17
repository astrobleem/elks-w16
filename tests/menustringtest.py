#!/usr/bin/env python3
import os,pathlib,re,subprocess,tempfile
root=pathlib.Path(__file__).resolve().parents[1]
source=(root/'src/menu.c').read_text()
def func(name):
    m=re.search(r'^.*\b'+name+r'\([^;]*?\)\n\{',source,re.M);assert m
    start=source.index('{',m.start());end=start+1;depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[m.start():end]
pre=r"""
#include "w16.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#define MAXMENU 2
#define SYSMENU 2
#define HMENU_BASE 0x2000
#define HMENU_STRIDE 0x200
"""
pre+=re.search(r'struct menu \{.*?\n\};',source,re.S)[0]
pre+=r"""
static void measure(struct menu *m,int x){(void)m;(void)x;}
static void menu_changed(struct menu *m){(void)m;}
static struct menu *menus[3];
#define USED(i) (menus[i]&&menus[i]->used)
static u8 ram[65536];
u8 *host_ptr(u16 s,u16 o,u16 n){assert(s==0x1000&&(u32)o+n<=65536);return ram+o;}
"""
test=r"""
static void args(u16 h,u16 id,u16 count,u16 flags){
wr16(0x1000,0,flags);wr16(0x1000,2,count);wr16(0x1000,4,200);wr16(0x1000,6,0x1000);wr16(0x1000,8,id);wr16(0x1000,10,h);
}
int main(void){
static struct menu m;static struct menu_item items[3];static i16 metrics[3];
menus[0]=&m;m.used=1;m.t.n=3;m.t.it=items;m.x0=metrics;
m.t.it[0].parent=255;m.t.it[0].flags=MF_POPUP;strcpy(m.t.it[0].text,"&File");
m.t.it[1].parent=0;m.t.it[1].id=301;strcpy(m.t.it[1].text,"&Open\tCtrl+O");
m.t.it[2].parent=0;m.t.it[2].flags=MF_SEPARATOR;
args(HMENU_BASE,301,28,0);assert(u_GetMenuString(0x1000,0)==strlen(m.t.it[1].text));
assert(!strcmp((char*)ram+200,m.t.it[1].text));
args(HMENU_BASE,0,28,MF_BYPOSITION);assert(u_GetMenuString(0x1000,0)==5);
args(HMENU_BASE+2,0,3,MF_BYPOSITION);assert(u_GetMenuString(0x1000,0)==2&&!strcmp((char*)ram+200,"&O"));
args(HMENU_BASE,999,28,0);assert(!u_GetMenuString(0x1000,0)&&!ram[200]);
args(HMENU_BASE+1,301,28,0);assert(!u_GetMenuString(0x1000,0)&&!ram[200]);
args(HMENU_BASE,301,1,0);assert(!u_GetMenuString(0x1000,0)&&!ram[200]);
args(HMENU_BASE,301,28,0);wr16(0x1000,4,65530);ram[65530]=99;
assert(!u_GetMenuString(0x1000,0)&&ram[65530]==99);
strcpy((char*)ram+400,"System");
wr16(0x1000,0,400);wr16(0x1000,2,0x1000);wr16(0x1000,4,333);
wr16(0x1000,6,MF_CHECKED);wr16(0x1000,8,301);wr16(0x1000,10,HMENU_BASE);
assert(u_ModifyMenu(0x1000,0));assert(m.t.it[1].id==333&&m.t.it[1].flags==MF_CHECKED);
assert(!strcmp(m.t.it[1].text,"System")&&m.t.it[1].parent==0);
memset(ram+400,'x',30);wr16(0x1000,8,333);
assert(!u_ModifyMenu(0x1000,0)&&!strcmp(m.t.it[1].text,"System"));
wr16(0x1000,6,MF_POPUP);assert(!u_ModifyMenu(0x1000,0));
wr16(0x1000,0,0);wr16(0x1000,2,333);wr16(0x1000,4,HMENU_BASE);
assert(u_GetMenuState(0x1000,0)==MF_CHECKED);
wr16(0x1000,2,999);assert(u_GetMenuState(0x1000,0)==0xffff);
wr16(0x1000,0,MF_BYPOSITION);wr16(0x1000,2,0);
assert(u_GetMenuState(0x1000,0)==(MF_POPUP | 0x200));
puts("Raw menu strings, id/position lookup, truncation, terminators and far bounds passed");return 0;
}
"""
with tempfile.TemporaryDirectory() as td:
    p=pathlib.Path(td);(p/'test.c').write_text(pre+''.join(func(n) for n in ['menu_of','find_item','u_GetMenuString','u_ModifyMenu','u_GetMenuState'])+test)
    subprocess.run(['cc','-DW16_HOST','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-no-pie','-I'+str(root/'src'),str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
