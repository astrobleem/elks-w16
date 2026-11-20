/* Host test of menu.c's keyboard handling: Alt+F4 while a menu bar popup or the system menu is open
 * must close the popup and send the owner WM_SYSCOMMAND SC_CLOSE (the key event belongs to the popup,
 * so the normal Alt+F4 path, which looks the window up by the event's window, cannot see it).
 * Nano-X and the rest of USER are stubbed; menu.c and dlgtmpl.c (menu_parse) are the real ones.
 * Build: gcc -DW16_HOST -I$MWDIR/src/include -Isrc tests/menukey_test.c src/dlgtmpl.c */
#include <stdlib.h>
/* Simulate the small free heap left after CLOCK/About interactions. */
static size_t allocation_limit=512,last_allocation;
static void *bounded_calloc(size_t n,size_t size){last_allocation=n*size;return last_allocation>allocation_limit?0:calloc(n,size);}
#define calloc bounded_calloc
#include "../src/menu.c"
#undef calloc

/* ---- stubs: Nano-X */
static int next_wid = 100, destroyed, focus;
GR_WINDOW_ID GrNewWindowEx(GR_WM_PROPS p, const char *t, GR_WINDOW_ID par, GR_COORD x, GR_COORD y,
                           GR_SIZE w, GR_SIZE h, GR_COLOR bg) { return ++next_wid; }
void GrMoveWindow(GR_WINDOW_ID w,GR_COORD x,GR_COORD y) {}
void GrResizeWindow(GR_WINDOW_ID w,GR_SIZE x,GR_SIZE y) {}
void GrClearArea(GR_WINDOW_ID w,GR_COORD x,GR_COORD y,GR_SIZE width,GR_SIZE height,int expose) {}
void GrDestroyWindow(GR_WINDOW_ID wid) { destroyed = wid; }
void GrMapWindow(GR_WINDOW_ID wid) {}
void GrSelectEvents(GR_WINDOW_ID wid, GR_EVENT_MASK m) {}
void GrSetFocus(GR_WINDOW_ID wid) { focus = wid; }
void GrGetScreenInfo(GR_SCREEN_INFO *si) { memset(si, 0, sizeof(*si)); si->cols = 320; si->rows = 200; }
void GrLine(GR_DRAW_ID d, GR_GC_ID gc, GR_COORD x1, GR_COORD y1, GR_COORD x2, GR_COORD y2) {}
void GrRect(GR_DRAW_ID d, GR_GC_ID gc, GR_COORD x, GR_COORD y, GR_SIZE w, GR_SIZE h) {}
void GrText(GR_DRAW_ID d, GR_GC_ID gc, GR_COORD x, GR_COORD y, void *s, GR_COUNT n, GR_TEXTFLAGS f) {}

/* ---- stubs: the rest of w16 */
struct wnd wins[MAXWIN];
int font_h = 10;
GR_GC_ID gdi_gc;
char opt_dump[40];
u8 w16_scratch[W16_SCRATCH];
struct ne_module *w16_mod;
static u8 fake_mem[65536];
u8 *host_ptr(u16 seg, u16 off, u16 n) { return fake_mem + (u16)(seg * 16 + off); }   /* never reached */
static int nposts;
static u16 post_hwnd, post_msg, post_wp;
struct wnd *win_get(u16 hwnd)
{
    int i;
    for (i = 0; i < MAXWIN; i++)
        if (wins[i].used && wins[i].hwnd == hwnd)
            return &wins[i];
    return 0;
}
u32 win_send(struct wnd *w, u16 msg, u16 wp, u32 lp) { return 0; }
void w16_post(u16 hwnd, u16 msg, u16 wp, u32 lp) { nposts++; post_hwnd = hwnd; post_msg = msg; post_wp = wp; }
void win_invalidate_all(struct wnd *w,int erase) {}
void res_name(u16 off,u16 seg,char *name,u16 *id) {name[0]=0;*id=off;}
void nc_sysbox(struct wnd *w, int open) {}
void ready_write(void) {}
void lat_shown(const char *what, int synced) {}
int w16_dump_frame(const char *tag) { return 0; }
void gdi_fg(GR_COLOR c) {}
void gdi_fill(GR_WINDOW_ID wid, GR_COLOR c, int x, int y, int w, int h) {}
void gdi_usebg(int on) {}
void gdi_text_size(const char *s, int n, GR_SIZE *w, GR_SIZE *h, GR_SIZE *b) { *w = 8 * n; *h = 10; *b = 8; }
u32 gdi_sys_color(u16 idx) { return 0; }
void wlog(const char *f, ...) {}
void vlog(const char *f, ...) {}
/* a MENU resource: popup "&Settings" holding "&Analog" (1) and "&Digital" (2) */
static const u8 menu_res[] = { 0, 0, 0, 0,
    0x90, 0, '&', 'S', 'e', 't', 't', 'i', 'n', 'g', 's', 0,
    0x00, 0, 1, 0, '&', 'A', 'n', 'a', 'l', 'o', 'g', 0,
    0x80, 0, 2, 0, '&', 'D', 'i', 'g', 'i', 't', 'a', 'l', 0 };
static const u8 *active_res=menu_res;
static u16 active_len=sizeof(menu_res);
int ne_res_find(struct ne_module *m, u16 type, const char *name, u16 id, u32 *off, u16 *len)
{
    *off = 0; *len = active_len; return 0;
}
int ne_read(struct ne_module *m, u32 off, void *buf, u16 n) { memcpy(buf, active_res + off, n); return 0; }

/* ---- the test */
static int fails, cases;
static void check(int ok, const char *what)
{
    cases++;
    if (!ok)
        fails++;
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
}
static int key(GR_KEY ch, int mods)
{
    GR_EVENT ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = GR_EVENT_TYPE_KEY_DOWN;
    ev.keystroke.wid = pop.open ? pop.wid : 0;
    ev.keystroke.ch = ch;
    ev.keystroke.modifiers = mods;
    return menu_event(&ev);
}
static struct wnd *mkwin(int i, u16 hwnd)
{
    struct wnd *w = &wins[i];
    memset(w, 0, sizeof(*w));
    w->used = w->visible = 1;
    w->hwnd = hwnd;
    w->style = WS_CAPTION | WS_SYSMENU;
    w->x = 10; w->y = 10; w->w = 200; w->h = 150;
    w->frame = 10 + i; w->client = 20 + i;
    return w;
}
int main(void)
{
    struct wnd *w = mkwin(0, 0x100), *box = mkwin(1, 0x102);
    w->hmenu = menu_load(w, 0, 1);
    check(w->hmenu != 0, "menu bar loaded from a MENU resource");
    check(last_allocation<=512&&menus[0]->capacity==3,"tiny menu fits fragmented 512-byte heap budget");

    {
      u16 second=menu_load(0,0,2),first=w->hmenu;
      check(second!=0 && menu_attach(w,second),"unattached menu replaces bar");
      check(menu_attach(box,first),"replaced menu can be reused");
      check(!menu_attach(w,first),"cannot share attached menu across windows");
      check(!menu_attach(w,second+2),"popup cannot become menu bar");
      check(menu_attach(box,0) && menu_attach(w,first),"detach and restore menu");
      menu_free(second);
    }
    /* menu bar popup, opened with F10 */
    nposts = 0;
    check(menu_key(w, MWKEY_F10, 0) && pop.open, "F10 opens the Settings popup");
    check(key(MWKEY_F4, MWKMOD_ALT) == 1, "Alt+F4 with the popup open is taken by the menu code");
    check(!pop.open && destroyed, "the popup is closed");
    check(nposts == 1 && post_hwnd == 0x100 && post_msg == WM_SYSCOMMAND && post_wp == SC_CLOSE,
          "the window gets WM_SYSCOMMAND SC_CLOSE");

    /* system menu, opened as Alt+Space does */
    nposts = 0;
    check(menu_sys_open(w, 1, 0) && pop.open && pop.mi == SYSMENU, "Alt+Space opens the system menu");
    check(last_allocation<=512&&menus[SYSMENU]->capacity==2,"system menu allocates only its two entries");
    check(key(MWKEY_F4, MWKMOD_ALT) == 1 && !pop.open, "Alt+F4 with the system menu open closes it");
    check(nposts == 1 && post_hwnd == 0x100 && post_msg == WM_SYSCOMMAND && post_wp == SC_CLOSE,
          "the window gets WM_SYSCOMMAND SC_CLOSE");

    /* the system menu of a second window (a message box): SC_CLOSE goes to that window */
    nposts = 0;
    menu_sys_open(box, 1, 0);
    key(MWKEY_F4, MWKMOD_ALT);
    check(nposts == 1 && post_hwnd == 0x102 && post_wp == SC_CLOSE, "a message box's system menu: SC_CLOSE to the box");

    /* controls: Esc and a plain F4 close the popup without closing anything */
    nposts = 0;
    menu_key(w, MWKEY_F10, 0);
    key(27, 0);
    check(!pop.open && nposts == 0, "Esc closes the popup and sends nothing");
    menu_sys_open(w, 1, 0);
    key(MWKEY_F4, 0);
    check(pop.open && nposts == 0, "F4 without Alt does not close the window");
    key(27, 0);
    check(key(MWKEY_F4, MWKMOD_ALT) == 0 && nposts == 0, "with no menu open Alt+F4 is left to the window");


    allocation_limit=65535;
    /* Large menu resources and later popup handles must not alias other menus. */
    { int sub, i; u16 h;
      menu_free(w->hmenu);
      { static u8 large[1600];unsigned p=4,j;
        for(j=0;j<104;j++) {
            large[p++]=j==103?MF_END:0;large[p++]=0;
            large[p++]=(u8)j;large[p++]=1;
            memcpy(large+p,"Example",8);p+=8;
        }
        active_res=large;active_len=p;
        check(menu_load(w,0,1)!=0 && menus[0]->t.n==104,
              "runtime loader reads a complete 1252-byte menu");
        menu_free(w->hmenu);
      }
      memset(&menus[0]->t,0,sizeof(menus[0]->t));
      check(slot(0,MENU_MAXITEMS)!=0,"full-capacity menu allocation remains supported");
      menus[0]->used=1;menus[0]->t.n=MENU_MAXITEMS;
      for(i=0;i<MENU_MAXITEMS;i++) {
          menus[0]->t.it[i].parent=0xff;
          menus[0]->t.it[i].flags=MF_POPUP;
      }
      for(i=0;i<MENU_MAXITEMS;i++) {
          h=HMENU_BASE+2*(i+1);
          check(menu_of(h,&sub)==menus[0]&&sub==i,"popup handle retains full item index");
      }
      check(menu_of(HMENU_BASE+1,&sub)==0,"odd handles rejected");

    }
    /* The real retained Window submenu, custom rows and real child ids. */
    {
      struct wnd *c=mkwin(2,0x104),*a=mkwin(3,0x108),*b=mkwin(4,0x10c);
      struct menu *m;int sub;u16 h=HMENU_BASE+2;
      menu_free(w->hmenu);m=slot(0,3);m->used=1;m->hwnd=w->hwnd;m->t.n=3;w->hmenu=HMENU_BASE;
      memset(m->t.it,0,3*sizeof(*m->t.it));
      m->t.it[0].flags=MF_POPUP;m->t.it[0].parent=255;strcpy(m->t.it[0].text,"&Window");
      m->t.it[1].parent=0;m->t.it[1].id=301;strcpy(m->t.it[1].text,"&Cascade");
      m->t.it[2].parent=0;m->t.it[2].id=302;strcpy(m->t.it[2].text,"Custom");
      measure(m,5);
      c->cls=CLASS_MDICLIENT;c->parent=w->hwnd;c->result=h;c->mdi_active=a->hwnd;
      a->parent=b->parent=c->hwnd;a->id=1000;b->id=1001;
      strcpy(a->title,"Alpha & Beta");strcpy(b->title,"Games");b->style|=WS_MINIMIZE;
      m=menu_of(h,&sub);
      check(m&&m->t.n==6&&m->mdi_count==3&&sub==0,"MDI creates separator and visible/minimized children in actual submenu");
      check(m->t.it[1].id==301&&m->t.it[2].id==302&&m->x0[0]==5,"custom entries, ids and bar origin preserved");
      check(!strcmp(m->t.it[4].text,"&1 Alpha && Beta")&&m->t.it[4].flags==MF_CHECKED&&m->t.it[5].id==1001,"escaped title, current checkmark and actual child id");
      check(menu_key(w,'w',MWKMOD_ALT)&&pop.open,"keyboard opens MDI Window menu");
      nposts=0;key('2',0);
      check(nposts==1&&post_msg==WM_COMMAND&&post_wp==1001&&post_hwnd==w->hwnd,"numbered child mnemonic posts existing restore/activation command");
      c->mdi_active=b->hwnd;strcpy(b->title,"Renamed");m=menu_of(h,&sub);
      check(!m->t.it[4].flags&&m->t.it[5].flags==MF_CHECKED&&!strcmp(m->t.it[5].text,"&2 Renamed"),"activation and title changes refresh without replacing custom rows");
      a->visible=0;m=menu_of(h,&sub);
      check(m->mdi_count==2&&m->t.it[4].id==1001&&!strcmp(m->t.it[4].text,"&1 Renamed"),"hidden child removed and remaining child numbered");
      menu_key(w,'w',MWKMOD_ALT);nposts=0;b->used=0;key('1',0);
      check(!pop.open&&!nposts,"lifecycle change closes stale popup before a command can target a dead child");
      m=menu_of(h,&sub);
      check(m->t.n==3&&!m->mdi_count&&m->t.it[2].id==302,"last child destruction removes generated separator and commands");
      a->visible=1;a->id=301;m=menu_of(h,&sub);
      check(m->t.n==3,"conflicting custom command id never duplicated");
      a->id=1000;b->used=1;allocation_limit=1;
      /* Force growth beyond the previous two-child allocation. */
      {struct wnd *d=mkwin(5,0x110);d->parent=c->hwnd;d->id=1002;strcpy(d->title,"Third");}
      m=menu_of(h,&sub);
      check(m->t.n==3&&!m->mdi_count&&m->t.it[1].id==301,"failed growth preserves custom rows and leaves no stale child ids");
      allocation_limit=65535;m=menu_of(h,&sub);
      check(m->mdi_count==4&&m->t.n==7,"retry after allocation failure populates live children");
      c->used=0;m=menu_of(h,&sub);
      check(m->t.n==3&&!m->mdi_count,"client destruction clears only generated rows");
      c->used=1;m=menu_of(h,&sub);check(m->capacity==7&&m->mdi_count==4,"reuse fixture has a generated tail");
      {static u8 reload[64];unsigned p=4,j;
       for(j=0;j<7;j++){reload[p++]=j==6?MF_END:0;reload[p++]=0;reload[p++]=20+j;reload[p++]=0;reload[p++]='X';reload[p++]=0;}
       menu_free(w->hmenu);active_res=reload;active_len=p;
       check(menu_load(w,0,1)!=0,"same-capacity menu slot reloads a different resource");
       m=menu_of(w->hmenu,&sub);
       check(m->t.n==7&&!m->mdi_count&&m->t.it[6].id==26,"resource reload cannot inherit the old generated-tail ownership");}

    }
    printf("== menus: %d passed, %d failed\n", cases - fails, fails);
    return fails != 0;
}
