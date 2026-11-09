# w16 emulator test, run from /etc/rc.sys when this file exists (image built by tests/mkimage.sh).
# Runs each Win16 program present in /root (hello16, dlgtest, winver, clock, reversi) under the Nano-X
# colour server. Mouse mode (default): tests/elks/w16mouse drives Nano-X's serial-mouse driver through
# /dev/ptyp1. With /root/ptykbd it also feeds Nano-X's keyboard through /dev/ptyp2 (KEYBOARD=/dev/ttyp2),
# so menus are exercised by key as well. Keyboard mode (/root/kbdmode): no mouse; keys come from the
# emulated keyboard (e.g. DOSBox-X AUTOTYPE) and CLOCK quits itself after a while (w16 -q).
# Flags: /root/skipneg skips the malformed-file checks, /root/skip<prog> skips a program, /root/quiet
# measures CLOCK without log and frame dumps, /root/nocache sets W16_NOCACHE=1.
# Everything is logged to the 0xE9 debug port; files are kept under /tmp for extraction.
export PATH=/bin:/root
export CONSOLE=/dev/tty1 MOUSE_TYPE=ms MOUSE_PORT=/dev/ttyp1
rm -f /tmp/w16.ready /tmp/*.bin /tmp/*.txt /tmp/*.log
e9 "@@BEGIN w16 autotest"
meminfo > /tmp/mem_before.txt 2>&1; e9 < /tmp/mem_before.txt

if test -f /root/diag; then
    e9 "--- diag: medium-model hello and w16 -n"
    /root/mhello > /tmp/diag.txt 2>&1; echo "mhello exit $?" >> /tmp/diag.txt
    w16 -n /root/hello16.exe >> /tmp/diag.txt 2>&1; echo "w16 -n exit $?" >> /tmp/diag.txt
    meminfo >> /tmp/diag.txt 2>&1
    e9 < /tmp/diag.txt
fi
if test ! -f /root/skipneg; then
    e9 "--- w16 -n on good and malformed files"
    for f in /root/*.exe /root/bad/*; do
        w16 -n $f > /tmp/n.txt 2>&1
        s=$?
        e9 "## $f"
        e9 < /tmp/n.txt
        e9 "## exit status $s"
    done
    e9 "--- w16 without a Nano-X server"
    w16 /root/hello16.exe > /tmp/n.txt 2>&1
    e9 "## exit status $? (expect 1)"
    e9 < /tmp/n.txt
fi

if test -f /root/kbdmode; then
    export MOUSE_PORT=none
    e9 "--- keyboard mode: nano-color without a mouse"
fi
KBDPTY=""
if test -f /root/ptykbd; then
    KBDPTY=/dev/ptyp2
    export KEYBOARD=/dev/ttyp2       # the patched Nano-X keyboard driver reads this; CONSOLE stays the screen
    e9 "--- keyboard through /dev/ptyp2"
fi
# one test driver holds the pty masters for the whole run (ELKS refuses a master open once the slave is open)
rm -f /tmp/w16.stop /tmp/w16.steps
w16mouse /dev/ptyp1 -serve /tmp/w16.ready /tmp/w16.steps /tmp/w16.stop $KBDPTY &
driver=$!
sleep 2
nano-color -p > /tmp/nx.log 2>&1 &     # -p: keep running between programs
server=$!
n=0
while ! ls /tmp/nxsock > /dev/null 2>&1; do
    sleep 1
    n=`expr $n + 1`
    if test $n -gt 60; then e9 "@@FAIL nano-color socket never appeared"; break; fi
done
if test -f /root/martyfix; then sleep 3; tvprobe martyfix; fi
e9 "--- nano-color pid $server up after $n s"
if test -f /root/diag; then
    e9 "--- diag: with the server up: memory, mhello, w16 -n, a short HELLO16 run"
    meminfo > /tmp/diag2.txt 2>&1
    /root/mhello >> /tmp/diag2.txt 2>&1; echo "mhello exit $?" >> /tmp/diag2.txt
    w16 -n /root/hello16.exe >> /tmp/diag2.txt 2>&1; echo "w16 -n exit $?" >> /tmp/diag2.txt
    w16 -v -q 25 /root/hello16.exe >> /tmp/diag2.txt 2>&1; echo "w16 hello16 exit $?" >> /tmp/diag2.txt
    e9 < /tmp/diag2.txt
fi
if test -x /bin/nxstress && test ! -f /root/kbdmode; then    # not in keyboard mode: typed keys are timed from boot
    e9 "--- Nano-X server stress: partial requests, disconnects, reconnects, two clients"
    nxstress > /tmp/stress.txt 2>&1
    e9 "@@STRESS exit status $?"
fi

export W16INI=/tmp/win.ini    # CLOCK keeps its settings in WIN.INI
test -f /root/nocache && export W16_NOCACHE=1    # same binary, GC state cache off (before/after)
# CLOCK: switch to digital and back through the menu (4 times), open About (twice); snapshots after the
# cycles show any growth. With the keyboard pty: F10/Down/Enter, Alt+S, Esc as well.
cyc="button 1000 sleep 5 button 2 sleep 8 button 1000 sleep 5 button 1 sleep 10"
about="button 1000 sleep 5 button 4 sleep 20 say about-shown button 51916 sleep 12"
clockkeys=""
if test -n "$KBDPTY"; then
    clockkeys="say keys key f10 sleep 4 key down sleep 2 key enter sleep 10 say digital-by-key key alt-s sleep 4 key enter sleep 10 say analog-by-key key alt-s sleep 4 key down,down sleep 2 key enter sleep 20 say about-by-key key enter sleep 12 key f10 sleep 4 key esc sleep 4 say menu-closed-by-key"
fi
# REVERSI: a move by mouse at a square that is legal at the start, three moves by Hint (Backspace) and a
# click where the hint put the pointer, Hint and About from the menus, a Help item (no help viewer: a
# message box), the Skill popup closed with Esc, the Backspace and F1 accelerators, then Exit from the menu
revkeys=""
if test -n "$KBDPTY"; then
    revkeys="key bs sleep 12 say hint-by-key key f1 sleep 15 say help-by-key button 1 sleep 10"
fi
for prog in hello16 dlgtest winver clock reversi; do
    test -f /root/$prog.exe || continue
    test -f /root/skip$prog && continue
    rm -f /tmp/w16.ready
    # the driver waits for the ready file, takes the memory snapshot, then drives the mouse (not in keyboard mode)
    if test -f /root/kbdmode; then
        steps="snap $prog"
    else
        case $prog in
        hello16) steps="snap $prog home sleep 2 say pointer-homed move 219 130 sleep 2 click sleep 12 say clicked move 26 22 sleep 2 dclick";;
        clock)   steps="snap $prog home sleep 20 say ticking $cyc $cyc $cyc $cyc snap clock2 $about $about $clockkeys snap clock3 sleep 5 move 26 22 sleep 2 dclick";;
        reversi) steps="snap $prog home sleep 5 say board move 105 95 sleep 1 click sleep 25 say moved-by-mouse key bs sleep 8 click sleep 25 key bs sleep 8 click sleep 25 key bs sleep 8 click sleep 25 say moved-by-hints snap reversi2 button 1000 sleep 5 button 50 sleep 15 say hint button 1002 sleep 5 button 36 sleep 20 say about snap reversi3 button 1 sleep 10 button 1002 sleep 5 button 30 sleep 20 say help button 1 sleep 10 button 1001 sleep 8 say skill-popup key esc sleep 5 $revkeys snap reversi4 button 1000 sleep 5 button 52 sleep 5";;
        *)       steps="snap $prog home sleep 3 say dialog-shown button 1";;
        esac
    fi
    echo "$steps" > /tmp/w16.steps
    e9 "@@RUN $prog"
    date > /tmp/a_$prog.txt
    opts="-v -e -r -d /tmp/f$prog"
    case $prog in clock|reversi) opts="$opts -m";; esac
    test $prog = clock && test -f /root/quiet && opts="-r -m"     # measurement without log and dump costs
    test $prog = clock && test -f /root/kbdmode && opts="$opts -q 150"   # no mouse to close it with
    w16 $opts /root/$prog.exe > /tmp/w_$prog.log 2>&1
    status=$?
    date > /tmp/b_$prog.txt
    if test $status -ge 100; then meminfo > /tmp/mfail_$prog.txt 2>&1; e9 < /tmp/mfail_$prog.txt; fi   # exec failed: memory map
    e9 "@@W16STATUS $prog $status"
    sync
    sleep 2
    e9 "--- w16 log for $prog"
    e9 < /tmp/w_$prog.log
    e9 "--- meminfo while $prog ran"
    e9 < /tmp/m_$prog.txt
done

meminfo > /tmp/mem_after_app.txt 2>&1
echo stop > /tmp/w16.stop
sleep 2
kill $driver 2>/dev/null
kill $server 2>/dev/null
sleep 3
vidreset > /dev/null 2>&1
meminfo > /tmp/mem_after.txt 2>&1
e9 "--- nano-color log"
e9 < /tmp/nx.log
e9 "--- meminfo after the programs exited (server still running)"
e9 < /tmp/mem_after_app.txt
e9 "--- meminfo after the server stopped"
e9 < /tmp/mem_after.txt
sync
echo "w16 autotest finished"
e9 "@@END w16 autotest"
