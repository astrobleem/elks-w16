# w16 emulator test, run from /etc/rc.sys when this file exists (image built by mkimage.sh).
# Everything is logged to the 0xE9 debug port; files are kept under /tmp for extraction.
export PATH=/bin:/root
export CONSOLE=/dev/tty1 MOUSE_TYPE=ms MOUSE_PORT=/dev/ttyp1
rm -f /tmp/w16.ready /tmp/fb*.bin /tmp/*.txt /tmp/w16.log
e9 "@@BEGIN w16 autotest"
e9 "--- meminfo before (no Nano-X, no w16)"
meminfo > /tmp/mem_before.txt 2>&1; e9 < /tmp/mem_before.txt

e9 "--- host-free checks in the guest: w16 -n on good and malformed files"
for f in /root/hello16.exe /root/bad/*; do
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
meminfo > /tmp/mem_noserver.txt 2>&1

if test -f /root/kbdmode; then
    # keyboard run: no mouse; keys arrive from the emulated keyboard (DOSBox-X AUTOTYPE)
    export MOUSE_PORT=none
    e9 "--- keyboard mode: starting nano-color without a mouse"
    mouse=""
else
    e9 "--- starting the pty mouse helper and nano-color"
    w16mouse /dev/ptyp1 /tmp/w16.ready home sleep 2 say pointer-homed move 219 130 sleep 2 click sleep 12 say clicked move 26 22 sleep 2 dclick &
    mouse=$!
    sleep 2
fi
nano-color > /tmp/nx.log 2>&1 &
server=$!
n=0
while ! ls /tmp/nxsock > /dev/null 2>&1; do
    sleep 1
    n=`expr $n + 1`
    if test $n -gt 60; then e9 "@@FAIL nano-color socket never appeared"; break; fi
done
if test -f /root/martyfix; then sleep 3; tvprobe martyfix; fi
e9 "--- nano-color pid $server up after $n s; running w16"
( while test ! -f /tmp/w16.ready; do sleep 1; done; meminfo > /tmp/mem_during.txt 2>&1; ps > /tmp/ps_during.txt 2>&1; sync; e9 < /tmp/nx.log; tvprobe info; sleep 2; tvprobe band 9800; sleep 4; tvprobe band 9800; sleep 20; meminfo > /tmp/mem_during2.txt 2>&1; sync ) &
date > /tmp/t0.txt
w16 -v -e -r -d /tmp/fb /root/hello16.exe > /tmp/w16.log 2>&1
status=$?
date > /tmp/t1.txt
e9 "@@W16STATUS $status"
sync
meminfo > /tmp/mem_after_app.txt 2>&1
kill $server
sleep 3
test -n "$mouse" && kill $mouse 2>/dev/null
vidreset > /dev/null 2>&1
meminfo > /tmp/mem_after.txt 2>&1
e9 "--- w16 log"
e9 < /tmp/w16.log
e9 "--- nano-color log"
e9 < /tmp/nx.log
e9 "--- meminfo while HELLO16 ran"
e9 < /tmp/mem_during.txt
e9 "--- meminfo later while HELLO16 ran"
e9 < /tmp/mem_during2.txt
e9 "--- ps while HELLO16 ran"
e9 < /tmp/ps_during.txt
e9 "--- meminfo after w16 exited (server still running)"
e9 < /tmp/mem_after_app.txt
e9 "--- meminfo after the server stopped"
e9 < /tmp/mem_after.txt
e9 "--- times"
e9 < /tmp/t0.txt
e9 < /tmp/t1.txt
sync
echo "w16 autotest finished: HELLO16 exit status $status"
e9 "@@END w16 autotest status $status"
