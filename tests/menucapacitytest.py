#!/usr/bin/env python3
import os,pathlib,subprocess,tempfile
root=pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as td:
    exe=str(pathlib.Path(td)/'menucapacity')
    subprocess.run(['cc','-DW16_HOST','-Isrc','-Wall','-Wextra','-Werror',
        '-fsanitize=address,undefined','-fno-pie','-no-pie',
        'tests/menucapacity_test.c','src/dlgtmpl.c','-o',exe],cwd=root,check=True)
    subprocess.run([exe],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
