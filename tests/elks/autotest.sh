# w16 emulator test, run from /etc/rc.sys when this file exists (image built by tests/mkimage.sh).
# Runs each Win16 program present in /root (hello16, dlgtest, mbtest, winver, clock, reversi) under the Nano-X
# colour server. Mouse mode (default): tests/elks/w16mouse drives Nano-X's serial-mouse driver through
# /dev/ptyp1. With /root/ptykbd it also feeds Nano-X's keyboard through /dev/ptyp2 (KEYBOARD=/dev/ttyp2),
# so menus are exercised by key as well. Keyboard mode (/root/kbdmode): no mouse; keys come from the
# emulated keyboard (e.g. DOSBox-X AUTOTYPE) and CLOCK quits itself after a while (w16 -q).
# Flags: /root/skipneg skips the malformed-file checks, /root/skip<prog> skips a program, /root/quiet
# measures CLOCK without log and frame dumps, /root/nocache sets W16_NOCACHE=1, /root/repeat runs every
# program a second time (open, one action, close) to check that each start and exit returns its memory.
# Everything is logged to the 0xE9 debug port; files are kept under /tmp for extraction.
export PATH=/bin:/root
export CONSOLE=/dev/tty1 MOUSE_TYPE=ms MOUSE_PORT=/dev/ttyp1
rm -f /tmp/w16.ready /tmp/*.bin /tmp/*.txt /tmp/*.log
e9 "@@BEGIN w16 autotest"
meminfo > /tmp/mem_before.txt 2>&1; e9 < /tmp/mem_before.txt
tvprobe info > /tmp/tv_before.txt 2>&1     # BIOS video mode and the kernel's video reservation

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
beat=
if test -f /root/beat; then e9 -beat 10 & beat=$!; fi    # diagnostic heartbeat
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
test -f /root/servertext && export W16_SERVERTEXT=1    # same binary, text measured by the server (before/after)
# CLOCK: switch to digital and back through the menu (4 times), open About (twice); snapshots after the
# cycles show any growth. With the keyboard pty: F10/Down/Enter, Alt+S, Esc, the system menu by Alt+Space
# and Left/Right between it and the menu bar, and Alt+F4 to close; without it a double-click on the box.
cyc="button 1000 sleep 5 button 2 sleep 8 button 1000 sleep 5 button 1 sleep 10"
about="button 1000 sleep 5 button 4 sleep 20 say about-shown button 51916 sleep 12"
clockkeys=""
clockend="move 26 22 sleep 2 dclick"
hellokeys=""
mbsteps="say hand-shown button 1 sleep 12 say question-shown button 7 sleep 12 say exclamation-shown button 2 sleep 12 say asterisk-shown button 999 sleep 10 say sysmenu-shown button 61536"
if test -n "$KBDPTY"; then
    clockkeys="say keys key f10 sleep 4 key down sleep 2 key enter sleep 10 say digital-by-key key alt-s sleep 4 key enter sleep 10 say analog-by-key key alt-s sleep 4 key down,down sleep 2 key enter sleep 20 say about-by-key key enter sleep 12 key f10 sleep 4 key esc sleep 4 say menu-closed-by-key key alt-space sleep 8 say sysmenu-by-key key right sleep 8 say bar-from-sysmenu key left sleep 8 say sysmenu-from-bar key esc sleep 6"
    clockend="key alt-f4"
    hellokeys="key alt-space sleep 8 say sysmenu-by-key key esc sleep 6 key f10 sleep 8 say sysmenu-by-f10 key esc sleep 6"
    mbsteps="say hand-shown key enter sleep 12 say question-shown key n sleep 12 say exclamation-shown key esc sleep 12 say asterisk-shown key alt-space sleep 8 say sysmenu-shown key c"
fi
# REVERSI: a click on a square that is not legal at the start (the game's message box, with its icon), a
# move by mouse at a square that is legal at the start, three moves by Hint (Backspace) and a click where
# the hint put the pointer, Hint and About from the menus, a Help item (no help viewer: w16's stand-in
# message box), the Skill popup closed with Esc, the Backspace and F1 accelerators, the system menu opened
# and closed by mouse, then Exit from the menu (with the keyboard pty: Close from the system menu by key)
# (ELKS sh drops script lines longer than about 600 characters: keep each line short)
rev1="move 87 85 sleep 1 click sleep 25 say illegal-move button 1 sleep 10 move 105 95 sleep 1 click sleep 25 say moved-by-mouse"
rev2="key bs sleep 8 click sleep 25 key bs sleep 8 click sleep 25 key bs sleep 8 click sleep 25 say moved-by-hints snap reversi2"
rev3="button 1000 sleep 5 button 50 sleep 15 say hint button 1002 sleep 5 button 36 sleep 20 say about snap reversi3 button 1 sleep 10"
rev3="$rev3 button 1002 sleep 5 button 30 sleep 20 say help button 1 sleep 10 button 1001 sleep 8 say skill-popup key esc sleep 5"
rev4="button 999 sleep 10 say sysmenu-by-mouse button 999 sleep 8 snap reversi4"
revkeys=""
revend="button 1000 sleep 5 button 52 sleep 5"
# second pass with the keyboard pty: Alt+F4 while a menu is open closes the program (CLOCK with its
# Settings popup open, REVERSI with its system menu open); without the pty, as in the first pass
clockend2="$clockend"
revend2="button 61536 sleep 5"
if test -n "$KBDPTY"; then
    revkeys="key bs sleep 12 say hint-by-key key f1 sleep 15 say help-by-key button 1 sleep 10"
    revend="key alt-space sleep 10 say sysmenu-by-key key c sleep 5"
    clockend2="key f10 sleep 6 say menu-open key alt-f4"
    revend2="key alt-f4 sleep 5"
fi
# second pass (/root/repeat): open, one action, close
passes=1
test -f /root/repeat && passes="1 2"
for pass in $passes; do
for prog in hello16 dlgtest mbtest winver clock reversi; do
    test -f /root/$prog.exe || continue
    test -f /root/skip$prog && continue
    rm -f /tmp/w16.ready
    tag=$prog
    test $pass = 2 && tag=${prog}_2
    # the driver waits for the ready file, takes the memory snapshot, then drives the mouse (not in keyboard mode)
    if test -f /root/kbdmode; then
        steps="snap $prog"
    elif test $pass = 2; then
        case $prog in
        hello16) steps="snap $tag home sleep 2 move 219 130 sleep 2 click sleep 12 say clicked-again move 26 22 sleep 2 dclick";;
        mbtest)  steps="snap $tag home sleep 3 $mbsteps";;
        clock)   steps="snap $tag home sleep 15 say ticking-again button 1000 sleep 5 button 2 sleep 10 button 1000 sleep 5 button 1 sleep 10 $clockend2";;
        reversi) steps="snap $tag home sleep 5 move 105 95 sleep 1 click sleep 25 say moved-again button 999 sleep 10 say sysmenu-again $revend2";;
        *)       steps="snap $tag home sleep 3 say dialog-again button 1";;
        esac
    else
        case $prog in
        hello16) steps="snap $prog home sleep 2 say pointer-homed move 219 130 sleep 2 click sleep 12 say clicked button 999 sleep 10 say sysmenu-by-mouse button 999 sleep 8 say sysmenu-closed $hellokeys move 26 22 sleep 2 dclick";;
        mbtest)  steps="snap $prog home sleep 3 $mbsteps";;
        clock)   steps="snap $prog home sleep 20 say ticking $cyc $cyc $cyc $cyc snap clock2 $about $about $clockkeys snap clock3 sleep 5 $clockend";;
        reversi) steps="snap $prog home sleep 5 say board $rev1 $rev2 $rev3 $revkeys $rev4 $revend";;
        *)       steps="snap $prog home sleep 3 say dialog-shown button 1";;
        esac
    fi
    echo "$steps" > /tmp/w16.steps
    e9 "@@RUN $tag"
    date > /tmp/a_$tag.txt
    opts="-v -e -r -d /tmp/f$tag"
    case $prog in clock|reversi) opts="$opts -m";; esac
    test $prog = clock && test -f /root/quiet && opts="-r -m"     # measurement without log and dump costs
    test $prog = clock && test -f /root/kbdmode && opts="$opts -q 150"   # no mouse to close it with
    w16 $opts /root/$prog.exe > /tmp/w_$tag.log 2>&1
    status=$?
    date > /tmp/b_$tag.txt
    if test $status -ge 100; then meminfo > /tmp/mfail_$tag.txt 2>&1; e9 < /tmp/mfail_$tag.txt; fi   # exec failed: memory map
    e9 "@@W16STATUS $tag $status"
    sync
    sleep 2
    e9 "--- w16 log for $tag"
    e9 < /tmp/w_$tag.log
    e9 "--- meminfo while $tag ran"
    test -f /tmp/m_$tag.txt && e9 < /tmp/m_$tag.txt
    meminfo > /tmp/mx_$tag.txt 2>&1      # after it exited: what it took must be back
done
done

meminfo > /tmp/mem_after_app.txt 2>&1
echo stop > /tmp/w16.stop
sleep 2
kill $driver 2>/dev/null
test -n "$beat" && kill $beat 2>/dev/null
kill $server 2>/dev/null
sleep 3
tvprobe info > /tmp/tv_server_exit.txt 2>&1   # the server's own exit restored text mode (03h)?
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
