#!/usr/bin/env python3
"""Exercise native ShellAbout guest arguments with a modal-dialog double."""
import os
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as d:
    exe=str(Path(d)/'legacy')
    subprocess.run(['gcc','-DW16_HOST','-I'+str(root/'src'),'-Wall','-Wextra',
                    '-Werror','-Wno-missing-field-initializers','-fsanitize=address,undefined',
                    '-fno-pie','-no-pie',str(root/'tests/shell_test.c'),
                    str(root/'src/shell.c'),str(root/'src/mem.c'),
                    '-o',exe],check=True)
    subprocess.run([exe],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
print('ShellAbout strings, title separator, guest bounds and modal result passed')
