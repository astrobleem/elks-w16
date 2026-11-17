#!/usr/bin/env python3
"""No-device SOUND behavior and actual fatal-exit handlers, with checked memory."""
import os
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as d:
    exe=str(Path(d)/'legacy')
    subprocess.run(['gcc','-DW16_HOST','-I'+str(root/'src'),'-Wall','-Wextra',
                    '-Werror','-Wno-missing-field-initializers','-fsanitize=address,undefined',
                    '-fno-pie','-no-pie',str(root/'tests/legacy_test.c'),
                    str(root/'src/legacy.c'),str(root/'src/api.c'),str(root/'src/mem.c'),
                    '-o',exe],check=True)
    subprocess.run([exe],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
print('Legacy SOUND unavailable-device contract, import ABI and fatal exits passed')
