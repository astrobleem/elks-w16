#include <assert.h>
#include "w16.h"
int main(void) {
 int i;u16 s,o;
 assert(!opt_missing);
 assert(api_find_ord(MOD_GDI,35)<0);
 assert(api_find_ord(MOD_USER,175)>=0);
 assert(api_table[api_find_ord(MOD_USER,175)].argbytes==6);
 assert(api_table[api_find_ord(MOD_GDI,34)].argbytes==20);
 assert(api_table[api_find_ord(MOD_GDI,82)].argbytes==8);
 assert(api_table[api_find_ord(MOD_GDI,83)].argbytes==6);
 assert(api_table[api_find_ord(MOD_GDI,103)].argbytes==6);
 assert(api_table[api_find_ord(MOD_GDI,100)].argbytes==16);
 assert(api_table[api_find_ord(MOD_GDI,79)].argbytes==2);
 assert(api_table[api_find_ord(MOD_GDI,31)].argbytes==10);
 assert(api_table[api_find_ord(MOD_GDI,148)].argbytes==6);
 assert(api_table[api_find_ord(MOD_USER,85)].argbytes==14);
 assert(api_table[api_find_ord(MOD_USER,74)].argbytes==8);
 assert(api_table[api_find_ord(MOD_USER,76)].argbytes==8);
 assert(api_table[api_find_ord(MOD_USER,79)].argbytes==12);
 assert(api_table[api_find_ord(MOD_USER,34)].argbytes==4);
 assert(api_table[api_find_ord(MOD_USER,96)].argbytes==8);
 assert(api_table[api_find_ord(MOD_USER,97)].argbytes==6);
 assert(api_table[api_find_ord(MOD_USER,98)].argbytes==4);
 assert(api_find_name(MOD_PBRUSH,"VBITBLT")<0);
 opt_missing=1;
 i=api_find_name(MOD_PBRUSH,"vBitBlt");assert(i>=0&&api_table[i].flags==API_MISSING);
 assert(api_find_ord(MOD_PBRUSH,5)==i && !api_target(i,&s,&o) && o==8*i);
 i=api_find_ord(MOD_USER,473);assert(i>255 && api_table[i].flags==API_MISSING);
 assert(!api_target(i,&s,&o) && o==8*i);
 i=api_find_ord(MOD_GDI,35);
 assert(i>=0 && (api_table[i].flags&API_MISSING));
 assert(api_table[i].fn==0);
 assert(!api_target(i,&s,&o));assert(o==8*i);
 opt_missing=0;assert(api_find_ord(MOD_GDI,35)<0);
 assert(api_find_ord(MOD_GDI,65535)<0);
 assert(api_target(api_count,&s,&o)<0);
 return 0;
}
