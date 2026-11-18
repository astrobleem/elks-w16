#!/usr/bin/env python3
"""Clipboard ownership against the actual bitmap and palette backing stores."""
from pathlib import Path
import os
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
# Reuse the bitmap suite's bounded far-memory/Nano-X fixture, not its main.
fixture=root/'tests/bitmaptest.py'
ns={'__file__':str(fixture)}
exec(fixture.read_text().split('with tempfile.TemporaryDirectory() as td:')[0],ns)
code=ns['code'].split('int main(int argc,char**argv)')[0]
def replace_function(s,marker,replacement):
    start=s.index(marker); opening=s.index('{',start); end=opening+1; depth=1
    while depth:
        depth+=(s[end]=='{')-(s[end]=='}');end+=1
    return s[:start]+replacement+s[end:]
code=replace_function(code,'static int palette_object(', 'int palette_object(u16,i16,u16,u16);')
code=replace_function(code,'static int palette_entry(', 'int palette_entry(u16,u16,u8*);')
code+='\nstatic void fcopy_in(void*p,u16 s,u16 o,u16 n){u16 i;for(i=0;i<n;i++)((u8*)p)[i]=rd8(s,o+i); }\n'
code+='\n'+(root/'src/palette.c').read_text().replace('#include "w16.h"','')
code+=r'''
#define HSTOCK 0x0300
#define HOBJ_BASE 0x0400
#define MAXDC 2
static struct {int used;u16 palette;} dcs[MAXDC];
static u16 display_saved;
static int saved_object(u16 h){return h&&display_saved==h;}
struct wnd {int used;};static struct wnd window;
struct wnd *win_get(u16 h){return h==0x100?&window:0;}
static int global_clipboard_take(u16 h){(void)h;return 0;}
static void global_clipboard_release(u16 h){(void)h;assert(!"unexpected global handle");}
int gdi_clipboard(u16,u16,int);
'''
code+=(root/'src/clipboard.c').read_text().replace('#include "w16.h"','')
gdi=(root/'src/gdi.c').read_text()
code+=gdi[gdi.index('int gdi_clipboard('):gdi.index('/* DeleteObject(hObject) */')]
# Exercise the actual owned-handle guard and bitmap/palette DeleteObject paths.
delete=gdi[gdi.index('u32 g_DeleteObject('):gdi.index('/* SelectObject(hdc, hObject) */')]
code+=delete[:delete.index('    if (!o)')]+'    return 0;\n}\n'
code+=r'''
static u16 setclip(u16 f,u16 h){args[0]=h;args[1]=f;return u_SetClipboardData(0,0);}
static u16 getclip(u16 f){args[0]=f;return u_GetClipboardData(0,0);}
static u16 del(u16 h){args[0]=h;return g_DeleteObject(0,0);}
static u16 newpalette(void){
    wr16(99,0,0x300);wr16(99,2,2);
    memset(output+4,0,8);output[4]=27;output[9]=94;
    args[0]=0;args[1]=99;return g_CreatePalette(0,0);
}
int main(void){
    u16 b,b2,p,p2,dc;u8 entry[4];int live;native_palette.count=16;
    args[0]=0x100;assert(u_OpenClipboard(0,0)&&u_EmptyClipboard(0,0));
    b=bitmap_create(16,16,1,4,0,0);b2=bitmap_create(16,16,1,4,0,0);
    p=newpalette();p2=newpalette();assert(b&&b2&&p&&p2);live=seglive;
    {u8 pixel=0xa5;bitmap_write(&bitmaps[bi(b)],0,&pixel,1);}
    assert(!setclip(2,p)&&!setclip(9,b)&&!setclip(2,DEFAULT_BITMAP)&&!setclip(9,DEFAULT_PALETTE));
    args[0]=0;dc=g_CreateCompatibleDC(0,0);assert(dc);
    assert(bitmap_select(dc,b)==DEFAULT_BITMAP);
    assert(!setclip(2,b)&&!clipboard_owns_gdi(b));
    assert(bitmap_select(dc,DEFAULT_BITMAP)==b);
    bitmap_object(dc,5,p);assert(!setclip(9,p));bitmap_object(dc,5,DEFAULT_PALETTE);
    assert(setclip(2,b)==b&&setclip(9,p)==p);
    assert(setclip(2,b)==b&&setclip(9,p)==p);
    assert(getclip(2)==b&&getclip(9)==p&&!del(b)&&!del(p));
    assert(bitmap_clipboard(b,0)&&palette_entry(p,0,entry)&&entry[0]==27);
    assert(bitmap_index(&bitmaps[bi(getclip(2))],0,0)==10);
    assert(bitmap_index(&bitmaps[bi(getclip(2))],1,0)==5);
    /* Paste can select/read the payload. Empty and replacement must wait. */
    bitmap_select(dc,b);assert(bitmap_savedc(dc));bitmap_select(dc,DEFAULT_BITMAP);
    assert(!u_EmptyClipboard(0,0)&&getclip(2)==b&&getclip(9)==p);
    assert(!setclip(2,b2)&&!clipboard_owns_gdi(b2)&&seglive==live);
    assert(bitmap_restoredc(dc,-1));bitmap_select(dc,DEFAULT_BITMAP);
    bitmap_object(dc,5,p);assert(bitmap_savedc(dc));bitmap_object(dc,5,DEFAULT_PALETTE);
    assert(!u_EmptyClipboard(0,0)&&!setclip(9,p2));
    assert(bitmap_restoredc(dc,-1));bitmap_object(dc,5,DEFAULT_PALETTE);
    dcs[0].used=1;dcs[0].palette=p;
    assert(!u_EmptyClipboard(0,0)&&!setclip(9,p2));dcs[0].used=0;
    display_saved=p;assert(!u_EmptyClipboard(0,0));display_saved=0;
    assert(setclip(2,b2)==b2&&bi(b)<0&&seglive==live-1);
    assert(setclip(9,p2)==p2&&!palette_valid(p));
    assert(u_EmptyClipboard(0,0)&&bi(b2)<0&&!palette_valid(p2)&&!seglive);
    assert(!getclip(2)&&!getclip(9));
    b=bitmap_create(16,16,1,4,0,0);p=newpalette();
    assert(setclip(2,b)==b&&setclip(9,p)==p);
    bitmap_select(dc,b);bitmap_object(dc,5,p);
    /* Match actual process teardown: GDI first, then clipboard/global cleanup. */
    bitmap_shutdown();palette_shutdown();clipboard_shutdown();clipboard_shutdown();
    assert(!seglive&&!clipboard_owns_gdi(b)&&!clipboard_owns_gdi(p));
    puts("Clipboard bitmap/palette backing ownership, selected/save references, atomic replacement and teardown passed");
    return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
    p=Path(tmp);(p/'test.c').write_text(code)
    subprocess.run(['cc','-I'+str(root/'src'),'-fsanitize=address,undefined','-fno-pie','-no-pie',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
