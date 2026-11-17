#include "w16.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned char data[2049];
static struct menu_tmpl menu;
static unsigned put(unsigned p,unsigned v) { data[p++]=v;data[p++]=v>>8;return p; }
static unsigned fixture(unsigned count) {
    unsigned p=4,i;memset(data,0,sizeof(data));
    for(i=0;i<count;i++) {
        p=put(p,i+1==count?MF_END:0);p=put(p,100+i);
        memcpy(data+p,"Example",8);p+=8;
    } return p;
}
int main(int argc,char **argv) {
    unsigned n=fixture(104),i;u8 count;struct menu_item *exact;
    assert(!menu_parse_into(data,n,0,MENU_MAXITEMS,&count)&&count==104);
    exact=malloc(count*sizeof(*exact));assert(exact);
    assert(!menu_parse_into(data,n,exact,count,&count)&&count==104&&exact[103].id==203);
    assert(menu_parse_into(data,n,exact,103,&count)<0);free(exact);
    assert(n>512 && !menu_parse(data,n,&menu)&&menu.n==104);
    assert(menu.it[103].id==203);
    n=fixture(128);assert(!menu_parse(data,n,&menu)&&menu.n==128);
    for(i=0;i<n;i++)assert(menu_parse(data,i,&menu)<0);
    n=fixture(129);assert(menu_parse(data,n,&menu)<0);
    /* An arbitrary header offset must not wrap to the first item. */
    n=fixture(1);data[2]=0xff;data[3]=0xff;assert(menu_parse(data,n,&menu)<0);
    if(argc==2) {
        FILE *f=fopen(argv[1],"rb");assert(f);n=fread(data,1,sizeof(data),f);fclose(f);
        assert(!menu_parse(data,n,&menu));
        printf("Private menu parsed: %u items, %u bytes\n",menu.n,n);
    }
    puts("Large menu capacity, truncation and overflow checks passed");
    return 0;
}
