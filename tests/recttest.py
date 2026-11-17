#!/usr/bin/env python3
import os, pathlib, subprocess, tempfile
root=pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as td:
    exe=str(pathlib.Path(td)/'recttest')
    subprocess.run(['cc','-DW16_HOST','-std=c99','-Wall','-Wextra','-Werror',
                    '-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie',
                    '-I'+str(root/'src'),str(root/'src/rect.c'),
                    str(root/'tests/rect_test.c'),'-o',exe],check=True)
    subprocess.run([exe],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
