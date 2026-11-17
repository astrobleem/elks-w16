#!/usr/bin/env python3
import pathlib,subprocess,tempfile,os
root=pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as td:
    d=pathlib.Path(td)
    subprocess.run(['cc','-DW16_HOST','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',
        '-I'+str(root/'src'),str(root/'tests/files_test.c'),str(root/'src/files.c'),str(root/'src/mem.c'),'-o',str(d/'test')],check=True)
    subprocess.run([str(d/'test')],cwd=d,check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
