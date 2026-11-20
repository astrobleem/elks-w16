/* US PC/XT set-1 decoding for the explicit ELKS scancode backend.
 * No BIOS/port access; bytes come from the stock console raw-keyboard API. */
#include <string.h>
struct elks_keys {
 MWKEYMOD mods;
 MWKEY down[128];
 unsigned char held[16],extended,pause;
};
static MWKEY elks_key(unsigned char code,int extended)
{
 static const unsigned char normal[59]={
 0,27,'1','2','3','4','5','6','7','8','9','0','-','=',8,9,
 'q','w','e','r','t','y','u','i','o','p','[',']',13,0,'a','s',
 'd','f','g','h','j','k','l',';','\'','`',0,'\\','z','x','c','v',
 'b','n','m',',','.','/',0,'*',0,' ',0};
 if(extended){
  switch(code){
  case 0x1c:return MWKEY_KP_ENTER;case 0x1d:return MWKEY_RCTRL;
  case 0x35:return '/';case 0x37:return MWKEY_PRINT;case 0x38:return MWKEY_RALT;
  case 0x47:return MWKEY_HOME;case 0x48:return MWKEY_UP;case 0x49:return MWKEY_PAGEUP;
  case 0x4b:return MWKEY_LEFT;case 0x4d:return MWKEY_RIGHT;case 0x4f:return MWKEY_END;
  case 0x50:return MWKEY_DOWN;case 0x51:return MWKEY_PAGEDOWN;
  case 0x52:return MWKEY_INSERT;case 0x53:return MWKEY_DELETE;
  default:return 0; /* E0 fake shifts are not physical modifiers */
  }
 }
 if(code<59){
  switch(code){case 0x1d:return MWKEY_LCTRL;case 0x2a:return MWKEY_LSHIFT;
  case 0x36:return MWKEY_RSHIFT;case 0x38:return MWKEY_LALT;case 0x3a:return MWKEY_CAPSLOCK;}
  return normal[code];
 }
 if(code<=0x44)return MWKEY_F1+code-0x3b;
 switch(code){
 case 0x45:return MWKEY_NUMLOCK;case 0x46:return MWKEY_SCROLLOCK;
 case 0x47:return MWKEY_HOME;case 0x48:return MWKEY_UP;case 0x49:return MWKEY_PAGEUP;
 case 0x4a:return '-';case 0x4b:return MWKEY_LEFT;case 0x4c:return MWKEY_KP5;
 case 0x4d:return MWKEY_RIGHT;case 0x4e:return '+';case 0x4f:return MWKEY_END;
 case 0x50:return MWKEY_DOWN;case 0x51:return MWKEY_PAGEDOWN;
 case 0x52:return MWKEY_INSERT;case 0x53:return MWKEY_DELETE;
 case 0x57:return MWKEY_F11;case 0x58:return MWKEY_F12;default:return 0;
 }
}
static int elks_decode(struct elks_keys *s,unsigned char byte,MWKEY *out,MWKEYMOD *mods,MWSCANCODE *scan)
{
 unsigned code,bit;int ext,up,held;MWKEY key;MWKEYMOD modifier=0,lock=0;
 if(s->pause){s->pause--;return 0;}
 if(byte==0xe1){s->pause=5;return 0;} /* consume the six-byte Pause sequence */
 if(byte==0xe0){s->extended=1;return 0;}
 ext=s->extended;s->extended=0;code=byte&0x7f;up=(byte&0x80)!=0;
 key=elks_key((unsigned char)code,ext);if(!key)return 0;
 switch(key){
 case MWKEY_LSHIFT:modifier=MWKMOD_LSHIFT;break;case MWKEY_RSHIFT:modifier=MWKMOD_RSHIFT;break;
 case MWKEY_LCTRL:modifier=MWKMOD_LCTRL;break;case MWKEY_RCTRL:modifier=MWKMOD_RCTRL;break;
 case MWKEY_LALT:modifier=MWKMOD_LALT;break;case MWKEY_RALT:modifier=MWKMOD_RALT;break;
 case MWKEY_CAPSLOCK:lock=MWKMOD_CAPS;break;case MWKEY_NUMLOCK:lock=MWKMOD_NUM;break;
 case MWKEY_SCROLLOCK:lock=MWKMOD_SCR;break;
 }
 bit=1u<<(code&7);held=!ext&&(s->held[code>>3]&bit);
 if(!ext){if(up)s->held[code>>3]&=~bit;else s->held[code>>3]|=bit;}
 if(modifier){if(up)s->mods&=~modifier;else s->mods|=modifier;}
 if(lock&&!up&&!held)s->mods^=lock;
 if(up&&!ext&&s->down[code]){key=s->down[code];s->down[code]=0;}
 else if(!up){
  int shift=(s->mods&MWKMOD_SHIFT)!=0;
  if(!ext&&code>=0x47&&code<=0x53&&code!=0x4a&&code!=0x4e&&((s->mods&MWKMOD_NUM)!=0)!=shift){
   static const unsigned char digits[]="789-456+1230.";key=digits[code-0x47];
  }else if(key>='a'&&key<='z'){
   if(s->mods&MWKMOD_CTRL)key=key-'a'+1;
   else if(shift!=((s->mods&MWKMOD_CAPS)!=0))key-=32;
  }else if(shift&&key<128){
   static const char plain[]="1234567890-=[];'`,./\\";
   static const char upper[]="!@#$%^&*()_+{}:\"~<>?|";
   const char *p=strchr(plain,(char)key);if(p)key=upper[p-plain];
  }
  if(!ext)s->down[code]=key;
 }
 *out=key;*mods=s->mods;*scan=code|(ext?0x100:0);return up?KBD_KEYRELEASE:KBD_KEYPRESS;
}
