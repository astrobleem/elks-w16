#!/usr/bin/env python3
"""Host VRAM model, using actual Microwindows declarations from MWDIR.
This does not execute ELKS code, an 8088 instruction stream, or a real BIOS.
"""
import os,pathlib,subprocess,tempfile
root=pathlib.Path(__file__).resolve().parents[1]
mw=pathlib.Path(os.environ['MWDIR']).resolve()
for name in ['src/include/device.h','src/drivers/vgaplan4.h','src/drivers/fb.h']:
 if not (mw/name).is_file():raise SystemExit('Missing pinned Microwindows header: '+name)
with tempfile.TemporaryDirectory() as td:
 p=pathlib.Path(td);(p/'linuxmt').mkdir();(p/'linuxmt/ntty.h').write_text('#define DCGET_GRAPH ((\'D\'<<8)+1)\n#define DCREL_GRAPH ((\'D\'<<8)+2)\n');(p/'arch').mkdir();(p/'arch/io.h').write_text('/* Host model: no port I/O. */\n')
 for color in (0,1):
  exe=p/('cga-'+str(color))
  subprocess.run(['cc','-DCGA_COLOR='+str(color),'-Dopen=cga_test_open','-Dclose=cga_test_close','-Dioctl=cga_test_ioctl','-Dgetenv=cga_test_getenv','-DELKS=1','-D__far=','-DMWPIXEL_FORMAT=MWPF_PALETTE',
   '-DMWCONFIG_FILE="mwconfig.elks"','-I'+str(p),'-I'+str(mw/'src/include'),
   '-I'+str(mw/'src/drivers'),'-Wall','-Wextra','-Werror','-Wno-unused-parameter',
   '-Wno-missing-field-initializers','-fsanitize=address,undefined','-fno-pie','-no-pie',
   str(root/'tests/cgamonodriver_test.c'),'-o',str(exe)],check=True)
  subprocess.run([str(exe)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
