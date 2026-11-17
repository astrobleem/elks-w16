#!/usr/bin/env python3
import os,pathlib,subprocess,tempfile
root=pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as d:
 exe=str(pathlib.Path(d)/"missingtrap")
 subprocess.run(["gcc","-DW16_HOST","-I"+str(root/"src"),"-Wall","-Wextra","-Werror","-Wno-missing-field-initializers","-fsanitize=address,undefined","-fno-pie","-no-pie",str(root/"tests/missingtrap_test.c"),str(root/"src/api.c"),"-o",exe],check=True)
 subprocess.run([exe],check=True,env={**os.environ,"ASAN_OPTIONS":"detect_leaks=0"})
print("Missing-import default rejection, thunk bounds and API ABI checks passed")
