#ifndef W16_FILELIST_H
#define W16_FILELIST_H
#include "w16.h"
#define FILELIST_MAX 64
struct filelist { char *text[FILELIST_MAX]; u16 count,top; i16 selected; };
void filelist_clear(struct filelist *l);
int filelist_add(struct filelist *l,const char *text,int sorted);
int filelist_scan(struct filelist *l,const char *directory,const char *pattern,u16 flags);
#endif
