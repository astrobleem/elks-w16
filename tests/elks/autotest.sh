# w16 emulator test, run from /etc/rc.sys when this file exists (image built by tests/mkimage.sh).
# Runs each Win16 program present in /root (hello16, dlgtest, winver) under the Nano-X colour server.
# Mouse mode (default): tests/elks/w16mouse drives Nano-X's serial-mouse driver through /dev/ptyp1.
# Keyboard mode (/root/kbdmode): no mouse; keys come from the emulated keyboard (e.g. DOSBox-X AUTOTYPE).
# Flags: /root/skipneg skips the malformed-file checks, /root/skip<prog> skips a program.
# Everything is logged to the 0xE9 debug port; files are kept under /tmp for extraction.
export PATH=/bin:/root
export CONSOLE=/dev/tty1 MOUSE_TYPE=ms MOUSE_PORT=/dev/ttyp1
rm -f /tmp/w16.ready /tmp/*.bin /tmp/*.txt /tmp/*.log
e9 "@@BEGIN w16 autotest"
meminfo > /tmp/mem_before.txt 2>&1; e9 < /tmp/mem_before.txt

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
# one test driver holds the pty master for the whole run (ELKS refuses a master open once the slave is open)
rm -f /tmp/w16.stop /tmp/w16.steps
w16mouse /dev/ptyp1 -serve /tmp/w16.ready /tmp/w16.steps /tmp/w16.stop &
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

export W16INI=/tmp/win.ini    # CLOCK keeps its settings in WIN.INI
test -f /root/nocache && export W16_NOCACHE=1    # same binary, GC state cache off (before/after)
for prog in hello16 dlgtest winver clock; do
    test -f /root/$prog.exe || continue
    test -f /root/skip$prog && continue
    test $prog = clock && test -f /root/kbdmode && continue     # its menus need F10/Alt, which the tty keyboard driver drops
    rm -f /tmp/w16.ready
    # the driver waits for the ready file, takes the memory snapshot, then drives the mouse (not in keyboard mode)
    if test -f /root/kbdmode; then
        steps="snap $prog"
    else
        case $prog in
        hello16) steps="snap $prog home sleep 2 say pointer-homed move 219 130 sleep 2 click sleep 12 say clicked move 26 22 sleep 2 dclick";;
        # CLOCK: let it tick, switch to digital and back through the menu (4 times), open About (twice),
        # then close it from the system menu box; snapshots after the cycles show any growth
        clock)   steps="snap $prog home sleep 20 say ticking button 1000 sleep 5 button 2 sleep 8 button 1000 sleep 5 button 1 sleep 10 button 1000 sleep 5 button 2 sleep 8 button 1000 sleep 5 button 1 sleep 10 button 1000 sleep 5 button 2 sleep 8 button 1000 sleep 5 button 1 sleep 10 button 1000 sleep 5 button 2 sleep 8 button 1000 sleep 5 button 1 sleep 10 snap clock2 button 1000 sleep 5 button 4 sleep 20 say about-shown button 51916 sleep 12 button 1000 sleep 5 button 4 sleep 20 say about-shown button 51916 sleep 12 snap clock3 sleep 5 move 26 22 sleep 2 dclick";;
        *)       steps="snap $prog home sleep 3 say dialog-shown button 1";;
        esac
    fi
    echo "$steps" > /tmp/w16.steps
    e9 "@@RUN $prog"
    date > /tmp/a_$prog.txt
    opts="-v -e -r -d /tmp/f$prog"
    test $prog = clock && opts="$opts -m"
    test $prog = clock && test -f /root/quiet && opts="-r -m"     # measurement without log and dump costs
    w16 $opts /root/$prog.exe > /tmp/w_$prog.log 2>&1
    status=$?
    date > /tmp/b_$prog.txt
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
