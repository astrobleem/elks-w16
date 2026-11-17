#!/usr/bin/env python3
import pathlib,subprocess,tempfile,os
root=pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as td:
 out=pathlib.Path(td)/'test'
 subprocess.run(['cc','-DW16_HOST','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie','-I'+str(root/'src'),str(root/'tests/palette_test.c'),str(root/'src/palette.c'),str(root/'src/mem.c'),'-o',str(out)],check=True)
 subprocess.run([str(out)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
