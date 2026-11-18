#!/usr/bin/env python3
"""Verify actual post-link loader-budget rejection, including its boundary."""
import struct
import subprocess
import tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
    path=Path(tmp)/'w16'
    def check(data,bss,heap,stack,expected,patch=None):
        header=bytearray(64);header[:4]=bytes((1,3,0x30,4));header[4]=64;header[6]=1
        struct.pack_into('<II',header,12,data,bss);struct.pack_into('<HH',header,24,heap,stack)
        if patch:patch(header)
        path.write_bytes(header)
        result=subprocess.run(['python3',str(root/'scripts/check-near-budget.py'),str(path)],capture_output=True)
        assert (result.returncode==0)==expected,(data,bss,heap,stack,result.stderr)
    check(21840,24080,0x2800,0x1800,True)
    check(25296,24080,0x2800,0x1800,False) # Original launch regression.
    check(0xfff0-1024-0x4000-20000,20000,0x2800,0x1800,True)
    check(0xfff0-1024-0x4000-20000+1,20000,0x2800,0x1800,False)
    check(1000,1000,0,0x1800,False)
    check(1000,1000,0x2800,0,False)
    check(1000,1000,0x2800,0x1800,False,lambda h:h.__setitem__(0,0))
    check(1000,1000,0x2800,0x1800,False,lambda h:h.__setitem__(6,0))
    path.write_bytes(b'bad')
    assert subprocess.run(['python3',str(root/'scripts/check-near-budget.py'),str(path)],capture_output=True).returncode
print('Near-data budget: launch regression, exact boundary, zero budgets and invalid headers passed')
