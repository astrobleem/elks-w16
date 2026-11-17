/* Bounded storage for real filesystem-backed Win16 file-dialog lists. */
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include "filelist.h"
static unsigned fold(unsigned c){return c>='a'&&c<='z'?c-32:c;}
static int matches(const char *p,const char *s)
{
    const char *star=0,*retry=0;
    if(!strcmp(p,"*.*"))p="*";
    while(*s){
        if(*p=='?'||fold(*p)==fold(*s)){p++;s++;}
        else if(*p=='*'){star=++p;retry=s;}
        else if(star){p=star;s=++retry;}
        else return 0;
    }
    while(*p=='*')p++;
    return !*p;
}
void filelist_clear(struct filelist *l)
{
    unsigned i;for(i=0;i<l->count;i++)free(l->text[i]);
    memset(l,0,sizeof(*l));l->selected=-1;
}
int filelist_add(struct filelist *l,const char *text,int sorted)
{
    unsigned i=l->count,n=strlen(text);char *p;
    if(i>=FILELIST_MAX||n>=124)return -2;
    p=malloc(n+1);if(!p)return -2;memcpy(p,text,n+1);
    if(sorted)while(i&&strcmp(l->text[i-1],p)>0){l->text[i]=l->text[i-1];i--;}
    l->text[i]=p;l->count++;return i;
}
int filelist_scan(struct filelist *l,const char *directory,const char *pattern,u16 flags)
{
    DIR *d;struct dirent *e;struct stat st;char path[256],label[128];int result=0,isdir;
    d=opendir(directory);if(!d)return -1;
    while((e=readdir(d))!=0){
        if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;
        if(strlen(directory)+strlen(e->d_name)+2>sizeof(path)||strlen(e->d_name)+3>sizeof(label))continue;
        strcpy(path,directory);strcat(path,"/");strcat(path,e->d_name);
        if(lstat(path,&st)||S_ISLNK(st.st_mode))continue;
        isdir=S_ISDIR(st.st_mode);
        if(isdir){if(!(flags&0x10))continue;}
        else if(!S_ISREG(st.st_mode)||(flags&0x8000)||!matches(pattern,e->d_name))continue;
        if(e->d_name[0]=='.'&&!(flags&2))continue;
        if(isdir){strcpy(label,"[");strcat(label,e->d_name);strcat(label,"]");}
        else strcpy(label,e->d_name);
        if(filelist_add(l,label,1)<0){result=-2;break;}
    }
    closedir(d);return result;
}
