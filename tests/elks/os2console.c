/* Owned observation of stock ELKS console semantics needed by OS/2 VIO.
 * This is a native ELKS probe, not an implementation of VioSetAnsi. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

static int sample(int fd,const struct termios *original,int post)
{
    struct termios next=*original,actual;
    const char *text=post?"OPOST ON : \033[7mREVERSE\033[0m plain\r\n":
                          "OPOST OFF: \033[7mREVERSE\033[0m plain\r\n";
    if(post)next.c_oflag|=OPOST;else next.c_oflag&=~OPOST;
    if(tcsetattr(fd,TCSANOW,&next)<0)return 5;
    if(tcgetattr(fd,&actual)<0 || !!(actual.c_oflag&OPOST)!=!!post)return 7;
    fprintf(stderr,"OPOST=%d oflag=%u\n",post,(unsigned)actual.c_oflag);
    return write(fd,text,strlen(text))==(int)strlen(text)?0:6;
}
int main(void)
{
    const char *path=getenv("CONSOLE");
    struct termios original,restored;
    int fd,rc,r,error;
    fd=open(path?path:"/dev/tty1",O_WRONLY);
    if(fd<0)return 2;
    if(!isatty(fd)){close(fd);return 3;}
    if(tcgetattr(fd,&original)<0){close(fd);return 4;}
    r=tcflush(fd,TCIFLUSH);error=errno;
    fprintf(stderr,"TCIFLUSH rc=%d errno=%d\n",r,r<0?error:0);
    rc=sample(fd,&original,1);
    if(!rc)rc=sample(fd,&original,0);
    /* Always restore, including a failed sample/write. */
    if(tcsetattr(fd,TCSANOW,&original)<0)rc=8;
    else if(tcgetattr(fd,&restored)<0 || memcmp(&original,&restored,sizeof(original)))rc=9;
    fprintf(stderr,"CONSOLE_RESTORE=%s result=%d\n",rc?"FAILED":"exact",rc);
    close(fd);return rc;
}
