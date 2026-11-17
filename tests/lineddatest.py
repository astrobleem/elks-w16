#!/usr/bin/env python3
import os,pathlib,subprocess,tempfile
root=pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as td:
 exe=str(pathlib.Path(td)/'lineddatest')
 subprocess.run(['cc','-DW16_HOST','-std=c99','-Wall','-Wextra','-Wno-misleading-indentation','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-I'+str(root/'src'),str(root/'src/linedda.c'),str(root/'tests/linedda_test.c'),'-o',exe],check=True)
 subprocess.run([exe],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
