#!/usr/bin/env python3
"""Exercise the font packer and actual C decoder, without needing Nano-X installed."""
import os
import re
import subprocess
import tempfile
from pathlib import Path
root = Path(__file__).resolve().parents[1]

def validate(source, directory, name):
    original = re.sub(r'/\*.*?\*/', '', source, flags=re.S)
    def values(symbol):
        return [int(v.strip(), 0) for v in re.search(symbol+r'\[\]\s*=\s*\{([^}]+)\}', original)[1].split(',') if v.strip()]
    rows = values('winFreeSansSerif11x13_bits')
    widths = values('winFreeSansSerif11x13_width')
    path = directory/'font.c'
    path.write_text(source, encoding='latin1')
    packed = subprocess.check_output(['python3', str(root/'scripts/pack-rasterfont.py'), str(path)], text=True)
    (directory/'rasterfont-data.h').write_text(packed)
    code = '#define W16_RO_FAR\n#include <stdint.h>\n#include <assert.h>\ntypedef uint8_t u8;typedef uint16_t u16;typedef uint32_t u32;\n'
    code += (root/'src/rasterfont.c').read_text().replace('#include "wapi.h"', '')
    code += '\nstatic u16 expected[]={' + ','.join(map(str,rows)) + '};\n'
    code += 'static u8 widths[]={' + ','.join(map(str,widths)) + '};\n'
    code += '''int main(void){int c,y;for(c=0;c<256;c++){
      int i=c<32?0:c-32;assert(rasterfont_width(c)==widths[i]);
      for(y=0;y<13;y++)assert((rasterfont_row(c,y)&(0xffffu<<(16-widths[i])))==expected[i*13+y]);
    }return 0;}'''
    (directory/'test.c').write_text(code)
    subprocess.run(['cc','-fsanitize=address,undefined','-fno-pie','-no-pie','-I'+str(directory),str(directory/'test.c'),'-o',str(directory/'test')],check=True)
    subprocess.run([str(directory/'test')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
    print(name + ': all 224 glyphs / 2912 rows and control-character remapping passed')

with tempfile.TemporaryDirectory() as tmp:
    directory=Path(tmp)
    widths=[1+(i%16) for i in range(224)]
    rows=[((ch*13579+y*7919)^0xa55a)&(0xffff<<(16-widths[ch]))&0xffff for ch in range(224) for y in range(13)]
    source='static unsigned short winFreeSansSerif11x13_bits[]={'+','.join(map(str,rows))+'};\nstatic unsigned char winFreeSansSerif11x13_width[]={'+','.join(map(str,widths))+'};\n'
    validate(source,directory,'Packed font variable-width fixture')
    invalid=source.replace('bits[]={'+str(rows[0])+',','bits[]={'+str(rows[0]|1)+',',1)
    for bad in [source.replace('width[]={1,','width[]={0,',1),invalid,'no font arrays']:
        (directory/'bad.c').write_text(bad)
        result=subprocess.run(['python3',str(root/'scripts/pack-rasterfont.py'),str(directory/'bad.c')],capture_output=True)
        assert result.returncode != 0
    if os.environ.get('MWDIR'):
        actual=Path(os.environ['MWDIR'])/'src/fonts/compiled/winFreeSansSerif11x13.c'
        validate(actual.read_text(encoding='latin1'),directory,'Original Nano-X font')
