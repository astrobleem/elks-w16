#!/usr/bin/env python3
"""Native NE layout: keep bitmap.o and user.o intact as a third code group."""
import sys
from pathlib import Path
s=Path(sys.argv[1]).read_text();needle='\n\t.data 0x30000'
if s.count(needle)!=1:raise SystemExit('unsupported medium linker layout')
head,tail=s.split(needle)
head=head.replace('*(', '*(EXCLUDE_FILE (*bitmap.o *user.o) ')
print(head+'''
 .bitmap 0x30000 : {
   *bitmap.o(".fartext.*!")
   *user.o(".fartext.*!")
   *bitmap.o(.fartext .fartext$* ".fartext.*[^&]")
   *user.o(.fartext .fartext$* ".fartext.*[^&]")
   *bitmap.o(".fartext.*&")
   *user.o(".fartext.*&")
   . = ALIGN(0x10);
 }
 .sharedconst 0x50000 : {
   *(".farrodata!*" ".farrodata.*!")
   *(.farrodata .farrodata$* ".farrodata.*[^&]")
   *(".farrodata&*" ".farrodata.*&")
   . = ALIGN(0x10);
 }
'''+ '\n\t.data 0x40000'+tail)
print('ASSERT (SIZEOF (.sharedconst) <= 0xfff0, "W16 read-only bank exceeds ELKS limit");')
print('ASSERT (SIZEOF (.bitmap) <= 0xfff0, "W16 third code segment exceeds ELKS limit");')
