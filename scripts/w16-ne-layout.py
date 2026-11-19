#!/usr/bin/env python3
"""Optional native NE layout: keep bitmap.o intact as a third code group."""
import sys
from pathlib import Path
s=Path(sys.argv[1]).read_text();needle='\n\t.data 0x30000'
if s.count(needle)!=1:raise SystemExit('unsupported medium linker layout')
head,tail=s.split(needle)
head=head.replace('*(', '*(EXCLUDE_FILE (bitmap.o) ')
print(head+'''
 .bitmap 0x30000 : {
   bitmap.o(".fartext.*!")
   bitmap.o(.fartext .fartext$* ".fartext.*[^&]")
   bitmap.o(".fartext.*&")
   . = ALIGN(0x10);
 }
'''+ '\n\t.data 0x40000'+tail)
print('ASSERT (SIZEOF (.bitmap) <= 0xfff0, "W16 bitmap code segment exceeds ELKS limit");')
