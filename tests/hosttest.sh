#!/bin/bash
# Host-only tests of the w16 NE loader (no emulator, no 16-bit code executed):
#  1. HELLO16.EXE loads; every segment matches an independent Python model of the expected image
#  2. malformed/unsupported variants are rejected with the expected code and message, nothing leaks
#  3. allocation failure at every step is reported and leaves no live allocations
#  4. random mutations never crash the loader (AddressSanitizer/UBSan build)
#  5-6. resource lookup and dialog templates match an independent Python decoder; mutations are safe
#  7. menus, string tables, resource types given by name, and the WIN.INI reader/writer
#  8. mutated menu templates are safe
#  9. (ELKS_TOPDIR) the ELKS keyboard drivers with the patch, and a headless-console kernel build; 10. (MWDIR) Alt+F4 with a menu open;
#  11. with those settings unset, their sections are skipped and the script still succeeds
# Usage: [ELKS_TOPDIR=DIR] [MWDIR=DIR] hosttest.sh   (run from the win16 directory after "make -C src host");
#        W16_PRIVATE=DIR adds checks of Windows 3.0 WINVER.EXE and CLOCK.EXE kept in DIR (never shipped)
set -u
cd "$(dirname "$0")/.."
H=./src/w16host
EXE=test/hello16/HELLO16.EXE
T=$(mktemp -d)
pass=0; fail=0
ok()  { pass=$((pass+1)); echo "PASS $*"; }
bad() { fail=$((fail+1)); echo "FAIL $*"; }

if [ "${HOSTTEST_ONLY:-}" != optional ]; then   # HOSTTEST_ONLY=optional: sections 9-10 only (used by 11)

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

echo "== 2b. host bounds checker self-test"
$H -B $EXE > $T/out.txt 2>&1; rc=$?
if [ $rc -ne 0 ] && [ $rc -ne 4 ] && grep -q "HOST BOUNDS VIOLATION: access outside its allocation" $T/out.txt; then ok "a write one byte past an allocation aborts the host build (rc $rc)"; else bad "bounds checker did not fire (rc $rc)"; cat $T/out.txt; fi

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

echo "== 5. resources and dialog templates"
for exe in test/dlgtest/DLGTEST.EXE test/hello16/HELLO16.EXE ${W16_PRIVATE:+$W16_PRIVATE/WINVER.EXE}; do
    [ -f "$exe" ] || continue
    if python3 tests/verify_dialogs.py $H $exe > $T/out.txt 2>&1; then ok "$(tail -1 $T/out.txt)"; else bad "$exe resources"; cat $T/out.txt; fi
done

echo "== 6. resource and dialog fuzz (ASan/UBSan)"
python3 - test/dlgtest/DLGTEST.EXE "$T/rfuzz" <<'PY'
import random, struct, sys, pathlib
good = bytearray(pathlib.Path(sys.argv[1]).read_bytes()); out = pathlib.Path(sys.argv[2]); out.mkdir()
ne = struct.unpack_from('<I', good, 0x3C)[0]
rs = ne + struct.unpack_from('<H', good, ne + 0x24)[0]; rn = ne + struct.unpack_from('<H', good, ne + 0x26)[0]
shift = struct.unpack_from('<H', good, rs)[0]; p = rs + 2; dlg = dlen = 0
while struct.unpack_from('<H', good, p)[0]:          # locate the DIALOG resource data
    t, cnt = struct.unpack_from('<HH', good, p); p += 8
    for k in range(cnt):
        off, ln = struct.unpack_from('<HH', good, p); p += 12
        if t == 0x8005:
            dlg, dlen = off << shift, min(ln << shift, 202)
rng = random.Random(87)
for i in range(800):
    b = bytearray(good)
    for _ in range(rng.randint(1, 4)):
        k = rng.randrange(rs, rn) if rng.random() < 0.5 else rng.randrange(dlg, dlg + dlen)
        b[k] = rng.randrange(256)
    (out / ('r%04d.exe' % i)).write_bytes(b)
PY
crash=0
for f in $T/rfuzz/*.exe; do
    $H -R 5 TestDlg -R 14 DlgIcon -R 3 "#1" -D TestDlg $f > $T/out.txt 2>&1; rc=$?
    if [ $rc -gt 15 ] || grep -q "Sanitizer\|runtime error" $T/out.txt || ! grep -q "live allocations after free 0" $T/out.txt; then
        crash=$((crash+1)); [ $crash -le 3 ] && { echo "--- $f rc $rc"; tail -5 $T/out.txt; }
    fi
done
[ $crash -eq 0 ] && ok "800 resource/dialog mutations: no crash, sanitizer report or leak" || bad "$crash resource fuzz failures"

echo "== 7. menus, string tables, named resource types, the NAMETABLE and WIN.INI"
D=test/dlgtest/DLGTEST.EXE
$H -M DlgMenu -S 1 -S 2 -S 17 -S 3 -S 16 -R DATA DlgData -R data dlgdata -R DATA Nope \
   -R 5 NtDlg -R 5 ntdlg -R DATA NtData -R 15 "#1" -R 5 NtNope -D NtDlg $D > $T/out.txt 2>&1
for want in "menu DlgMenu: 8 items" "item 0 parent -1 flags 0010 id 0 text '&File'" "item 2 parent 0 flags 0800 id 0 text ''" \
            "item 3 parent 0 flags 0008 id 11 text 'E&xit'" "item 5 parent 4 flags 0001 id 20 text '&Big'" \
            "item 7 parent -1 flags 0000 id 30 text '&Help!'" "string 1 'first' length 5" "string 17 'block two' length 9" \
            "string 3 not found" "string 16 not found" "resource DATA DlgData at 2990 length 10" \
            "resource data dlgdata at 2990 length 10" "resource DATA Nope not found" \
            "resource 5 NtDlg at 2870 length 40" "resource 5 ntdlg at 2870 length 40" "resource DATA NtData at 3000 length 10" \
            "resource 15 #1 at 3010 length 34" "resource 5 NtNope not found" "caption 'NtDlg'" "live allocations after free 0"; do
    grep -qF "$want" $T/out.txt && ok "DLGTEST: $want" || { bad "DLGTEST: missing '$want'"; cat $T/out.txt; }
done
# malformed resource tables and NAMETABLEs are refused when the file is opened (so also by w16 -n on the target)
python3 tests/mkbadres.py $D $T/badres > /dev/null
while read name code text; do
    $H $T/badres/$name > $T/out.txt 2>&1; rc=$?
    if [ $rc -eq $code ] && grep -qF "$text" $T/out.txt && grep -q "live allocations after free 0" $T/out.txt; then
        ok "$name -> $rc: $text"
    else
        bad "$name expected $code '$text', got $rc"; cat $T/out.txt
    fi
done < $T/badres/expect.txt
if [ -n "${W16_PRIVATE:-}" ] && [ -f "$W16_PRIVATE/CLOCK.EXE" ]; then
    # Windows 3.0 RC: names live in a NAMETABLE, including the string type "DATA" (type F006h)
    $H -R DATA CLOCK -R 4 CLOCK -R 5 CLOCK -R 14 CCKK -M CLOCK -S 1 -S 2 -S 4 -S 23 $W16_PRIVATE/CLOCK.EXE > $T/out.txt 2>&1
    for want in "resource DATA CLOCK at 10896 length 240" "resource 4 CLOCK at 8784 length 80" \
                "resource 14 CCKK at 8736 length 48" "menu CLOCK: 5 items" "item 4 parent 0 flags 0000 id 4 text 'A&bout Clock...'" \
                "string 2 'data' length 4" "string 4 'digital.fon' length 11" "string 23 'iFormat' length 7"; do
        grep -qF "$want" $T/out.txt && ok "CLOCK: $want" || { bad "CLOCK: missing '$want'"; cat $T/out.txt; }
    done
fi
if [ -n "${W16_PRIVATE:-}" ] && [ -f "$W16_PRIVATE/REVERSI.EXE" ]; then
    # accelerators named through the NAMETABLE, a cursor group, the menu with three popups
    $H -M "#1" -R 9 MainAcc -R 9 mainacc -R 12 "#1" -R 1 "#4" -S 13 -D "#3" $W16_PRIVATE/REVERSI.EXE > $T/out.txt 2>&1
    for want in "resource 9 MainAcc at 14864 length 16" "resource 9 mainacc at 14864 length 16" "resource 12 #1 at 13664 length 32" \
                "resource 1 #4 at 13728 length 320" "menu #1: 20 items" "item 19 parent 11 flags 0000 id 36 text '&About Reversi...'" \
                "item 4 parent 0 flags 0800 id 0 text ''" "string 13 'You may only move to a space where the cursor is a cross.' length 57" \
                "caption 'About Reversi'" "live allocations after free 0"; do
        grep -qF "$want" $T/out.txt && ok "REVERSI: $want" || { bad "REVERSI: missing '$want'"; cat $T/out.txt; }
    done
fi
I=$T/w.ini
$H -P $I Clock iFormat > $T/out.txt; grep -q "^absent" $T/out.txt && ok "WIN.INI: no file reads as absent" || bad "WIN.INI absent"
$H -P $I Clock iFormat 1 > /dev/null; $H -P $I intl iTime 0 > /dev/null; $H -P $I Clock iFormat 2 > /dev/null
$H -P $I CLOCK IFORMAT > $T/out.txt; grep -q "value '2' length 1" $T/out.txt && ok "WIN.INI: set, replace, case-insensitive get" || bad "WIN.INI get"
[ "$(grep -c iFormat $I)" = 1 ] && [ "$(grep -c '^\[' $I)" = 2 ] && ok "WIN.INI: one key line, two sections" || { bad "WIN.INI layout"; cat $I; }
printf '; comment\r\n[intl]\r\n  sTime = :  \r\n[Clock]\r\niFormat=1\r\n' > $I
$H -P $I intl sTime > $T/out.txt; grep -q "value ':' length 1" $T/out.txt && ok "WIN.INI: DOS line ends and spaces trimmed" || { bad "WIN.INI trim"; cat $T/out.txt; }
$H -P $I Clock iFormat - > /dev/null; $H -P $I Clock iFormat > $T/out.txt
grep -q "^absent" $T/out.txt && grep -q "sTime" $I && ok "WIN.INI: delete one key keeps the rest" || { bad "WIN.INI delete"; cat $I; }
$H -P $I intl - - > /dev/null; ! grep -q "intl\|sTime" $I && grep -q "; comment" $I && ok "WIN.INI: delete a section keeps comments" || { bad "WIN.INI section delete"; cat $I; }
head -c 3000 /dev/zero | tr '\0' 'x' > $I; $H -P $I a b c > $T/out.txt 2>&1
grep -q "^put failed" $T/out.txt && [ "$(wc -c < $I)" = 3000 ] && ok "WIN.INI: an oversized file is refused, not truncated" || { bad "WIN.INI oversize"; cat $T/out.txt; }
printf '[a]\nb=1\n' > $I; mkdir -p $I.new; $H -P $I a b 2 > $T/out.txt 2>&1
grep -q "^put failed" $T/out.txt && [ "$(cat $I)" = "$(printf '[a]\nb=1')" ] && [ ! -e $I.bak ] && ok "WIN.INI: a replacement that cannot be written leaves the original" || { bad "WIN.INI tmp failure"; cat $T/out.txt; }
rmdir $I.new; $H -P $I a b 3 > /dev/null; [ ! -e $I.new ] && [ ! -e $I.bak ] && grep -q "b=3" $I && ok "WIN.INI: a successful rewrite leaves no .new or .bak" || { bad "WIN.INI leftovers"; ls $T; }
mkdir -p $T/inidir; $H -P $T/inidir a b c > $T/out.txt 2>&1
grep -q "^put failed" $T/out.txt && [ -d $T/inidir ] && grep -q "cannot read" $T/out.txt && ok "WIN.INI: a read error is reported and nothing is written" || { bad "WIN.INI read error"; cat $T/out.txt; }
$H -P $T/nonexistent/x.ini a b > $T/out.txt 2>&1; grep -q "^absent" $T/out.txt && ok "WIN.INI: a missing file reads as absent without complaint" || { bad "WIN.INI absent path"; cat $T/out.txt; }

echo "== 8. menu template fuzz (ASan/UBSan)"
python3 - $D "$T/mfuzz" <<'PY'
import random, struct, sys, pathlib
good = bytearray(pathlib.Path(sys.argv[1]).read_bytes()); out = pathlib.Path(sys.argv[2]); out.mkdir()
mo, ml = 2826, 80                                   # DlgMenu data (checked above)
rng = random.Random(4)
for i in range(600):
    b = bytearray(good)
    for _ in range(rng.randint(1, 4)):
        b[rng.randrange(mo, mo + ml)] = rng.choice([0, 0x10, 0x80, 0x90, 0xFF, rng.randrange(256)])
    (out / ('m%04d.exe' % i)).write_bytes(b)
PY
crash=0
for f in $T/mfuzz/*.exe; do
    $H -M DlgMenu -S 1 -S 17 $f > $T/out.txt 2>&1; rc=$?
    if [ $rc -gt 15 ] || grep -q "Sanitizer\|runtime error" $T/out.txt || ! grep -q "live allocations after free 0" $T/out.txt; then
        crash=$((crash+1)); [ $crash -le 3 ] && { echo "--- $f rc $rc"; tail -5 $T/out.txt; }
    fi
done
[ $crash -eq 0 ] && ok "600 menu mutations: no crash, sanitizer report or leak" || bad "$crash menu fuzz failures"

fi   # end of the sections that need only the host build

echo "== 9. ELKS keyboard drivers with elks-graphics-keys.patch (set ELKS_TOPDIR)"
if [ -n "${ELKS_TOPDIR:-}" ]; then
    if bash tests/kbdtest.sh > $T/kbd.txt 2>&1; then ok "$(grep '^==' $T/kbd.txt | tr '\n' ' ')"; else bad "keyboard drivers"; grep -v '^ok' $T/kbd.txt; fi
    if bash tests/headless_build.sh > $T/headless.txt 2>&1; then ok "$(sed 's/^ok *//' $T/headless.txt)"; else bad "headless build"; cat $T/headless.txt; fi
else
    echo "skipped: ELKS_TOPDIR is not set"
fi

echo "== 10. menus: Alt+F4 with a menu bar popup or the system menu open (set MWDIR)"
if [ -n "${MWDIR:-}" ]; then
    if gcc -DW16_HOST -Wall -Wno-unused-parameter -fsanitize=address,undefined -I"$MWDIR/src/include" -Isrc \
           -o $T/menukey tests/menukey_test.c src/dlgtmpl.c > $T/menukey.txt 2>&1 && $T/menukey >> $T/menukey.txt 2>&1; then
        ok "$(grep '^==' $T/menukey.txt)"
    else
        bad "menus"; grep -v '^ok' $T/menukey.txt
    fi
else
    echo "skipped: MWDIR is not set"
fi

if [ "${HOSTTEST_ONLY:-}" != optional ]; then
echo "== 11. optional settings unset: the sections above are skipped, not failed"
if env -u ELKS_TOPDIR -u MWDIR -u W16_PRIVATE HOSTTEST_ONLY=optional bash tests/hosttest.sh > $T/unset.txt 2>&1 &&
   grep -q "skipped: ELKS_TOPDIR is not set" $T/unset.txt && grep -q "skipped: MWDIR is not set" $T/unset.txt &&
   grep -q "== host tests: 0 passed, 0 failed" $T/unset.txt; then
    ok "hosttest.sh with ELKS_TOPDIR, MWDIR and W16_PRIVATE unset skips their sections and exits 0"
else
    bad "hosttest.sh with the optional settings unset"; cat $T/unset.txt
fi
for s in kbdtest.sh headless_build.sh; do
    env -u ELKS_TOPDIR bash tests/$s > $T/unset.txt 2>&1; rc=$?
    if [ $rc -eq 2 ] && grep -q "usage: ELKS_TOPDIR=" $T/unset.txt; then
        ok "$s without ELKS_TOPDIR prints its usage and exits 2"
    else
        bad "$s without ELKS_TOPDIR: rc $rc"; cat $T/unset.txt
    fi
done
fi

echo "== host tests: $pass passed, $fail failed"
rm -rf $T
[ $fail -eq 0 ]
