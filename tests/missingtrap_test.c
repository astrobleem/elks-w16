#include <assert.h>
#include "w16.h"
int main(void) {
 int i;u16 s,o;
 assert(!opt_missing);
 assert(api_find_ord(MOD_GDI,83)<0);
 assert(api_find_ord(MOD_USER,175)>=0);
 assert(api_table[api_find_ord(MOD_USER,175)].argbytes==6);
 assert(api_table[api_find_ord(MOD_GDI,34)].argbytes==20);
 assert(api_table[api_find_ord(MOD_GDI,82)].argbytes==8);
 assert(api_table[api_find_ord(MOD_USER,74)].argbytes==8);
 assert(api_table[api_find_ord(MOD_USER,76)].argbytes==8);
 assert(api_table[api_find_ord(MOD_USER,79)].argbytes==12);
 opt_missing=1;i=api_find_ord(MOD_GDI,83);
 assert(i>=0 && (api_table[i].flags&API_MISSING));
 assert(api_table[i].fn==0);
 assert(!api_target(i,&s,&o));assert(o==8*i);
 opt_missing=0;assert(api_find_ord(MOD_GDI,83)<0);
 assert(api_find_ord(MOD_GDI,65535)<0);
 assert(api_target(api_count,&s,&o)<0);
 return 0;
}
