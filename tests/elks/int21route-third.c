/* Keep one real compiled call in the optional third code group. */
#include "w16.h"
u16 W16_CDECL w16_get_ds(void);
u16 probe_third(void){return w16_get_ds();}
