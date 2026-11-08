#!/bin/bash
# Host-only tests of the w16 NE loader (no emulator, no 16-bit code executed):
#  1. HELLO16.EXE loads; every segment matches an independent Python model of the expected image
#  2. malformed/unsupported variants are rejected with the expected code and message, nothing leaks
#  3. allocation failure at every step is reported and leaves no live allocations
#  4. random mutations never crash the loader (AddressSanitizer/UBSan build)
# Usage: hosttest.sh   (run from the win16 directory after "make -C src host")
set -u
cd "$(dirname "$0")/.."
H=./src/w16host
EXE=test/hello16/HELLO16.EXE
T=$(mktemp -d)
pass=0; fail=0
ok()  { pass=$((pass+1)); echo "PASS $*"; }
bad() { fail=$((fail+1)); echo "FAIL $*"; }

echo "== 1. load HELLO16 and compare with the model"
mkdir -p $T/load
if $H -o $T/load $EXE > $T/load/out.txt 2>&1 && python3 tests/verify_load.py $EXE $T/load src/api.c; then ok "HELLO16 load matches model"; else bad "HELLO16 load"; cat $T/load/out.txt; fi
grep -q "live allocations after free 0" $T/load/out.txt && ok "HELLO16 frees all segments" || bad "HELLO16 leak"

echo "== 2. malformed and unsupported executables"
python3 tests/mkbad.py $EXE $T/bad > /dev/null
while read name code text; do
    $H $T/bad/$name > $T/out.txt 2>&1; rc=$?
    if [ $rc -eq $code ] && grep -qF "$text" $T/out.txt && grep -q "live allocations after free 0" $T/out.txt; then
        ok "$name -> $rc: $(grep -m1 -F "$text" $T/out.txt | sed 's/^w16: [^:]*: //')"
    else
        bad "$name expected $code '$text', got $rc"; cat $T/out.txt
    fi
done < $T/bad/expect.txt

echo "== 3. allocation failure at each step"
for n in 0 1 2 3; do
    $H -f $n $EXE > $T/out.txt 2>&1; rc=$?
    if [ $rc -eq 14 ] && grep -q "live allocations after free 0" $T/out.txt; then ok "fail allocation $n -> ENOMEM, no leak"; else bad "alloc fail $n rc $rc"; cat $T/out.txt; fi
done

echo "== 4. mutation fuzz (ASan/UBSan)"
python3 - "$EXE" "$T/fuzz" <<'PY'
import random, sys, pathlib
good = bytearray(pathlib.Path(sys.argv[1]).read_bytes()); out = pathlib.Path(sys.argv[2]); out.mkdir()
rng = random.Random(1609)
for i in range(1500):
    b = bytearray(good)
    for _ in range(rng.randint(1, 6)):
        k = rng.randrange(len(b)) if rng.random() < 0.5 else rng.randrange(0x80, 0x140)
        b[k] = rng.randrange(256)
    if rng.random() < 0.1:
        b = b[:rng.randrange(len(b))]
    (out / ('f%04d.exe' % i)).write_bytes(b)
PY
crash=0; codes=""
for f in $T/fuzz/*.exe; do
    $H $f > $T/out.txt 2>&1; rc=$?
    if [ $rc -gt 15 ] || grep -q "Sanitizer\|runtime error" $T/out.txt || ! grep -q "live allocations after free 0" $T/out.txt; then
        crash=$((crash+1)); [ $crash -le 3 ] && { echo "--- $f rc $rc"; tail -5 $T/out.txt; }
    fi
    codes="$codes $rc"
done
echo "fuzz result codes:"; echo $codes | tr ' ' '\n' | sort -n | uniq -c
[ $crash -eq 0 ] && ok "1500 mutated files: no crash, sanitizer report or leak" || bad "$crash fuzz failures"

echo "== host tests: $pass passed, $fail failed"
rm -rf $T
[ $fail -eq 0 ]
