#!/usr/bin/env python3
from pathlib import Path
import subprocess,tempfile,os
r=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as td:
 p=str(Path(td)/'oval')
 subprocess.run(['cc','-O1','-I'+str(r/'src'),'-fsanitize=address,undefined','-fno-pie','-no-pie',str(r/'tests/oval_test.c'),str(r/'src/oval.c'),'-o',p],check=True)
 subprocess.run([p],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
