#!/usr/bin/env python3
from pathlib import Path
import subprocess,tempfile,os
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as td:
 exe=str(Path(td)/'polygon')
 subprocess.run(['cc','-Wall','-Wextra','-fsanitize=address,undefined','-fno-pie','-no-pie','-I'+str(root/'src'),str(root/'tests/polygon_test.c'),str(root/'src/polygon.c'),'-o',exe],check=True)
 subprocess.run([exe],check=True,env=dict(os.environ,ASAN_OPTIONS="detect_leaks=0"))
