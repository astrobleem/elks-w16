# w16: a small Win16 compatibility layer for ELKS

`w16` loads an **unchanged** Windows 3.x NE executable on ELKS, binds its KERNEL, USER and GDI
imports to native implementations, and runs the program's own 8086 code on the 8088, with
Nano-X (the Tandy/PCjr colour build in `packages/nanox-video`) as the display.
It is Wine-inspired in approach, not in size: one ELKS a.out of about 92 KB (two code segments and
one data segment), no Windows files.

It runs the included test programs and, unchanged, Windows 3.0's own WINVER.EXE, CLOCK.EXE (analog
and digital modes, its menu, About box, timer and WIN.INI settings) and REVERSI.EXE (a game against
the computer with its menus, cursors, accelerators, hints and About box). It does not run
arbitrary Windows applications. See "What is supported" and "What is not" below.

## How it works

* **Loader** (`src/ne.c`): reads the MZ and NE headers, validates them, allocates one real-mode
  segment per NE segment with the ELKS `fmemalloc` system call, reads the segment data, and
  applies the relocation records: internal segment/offset/far-address fixups (including chains
  and additive records), imports by ordinal or by name, and Windows' prologue patch for exported
  callbacks (`push ds; pop ax; nop` becomes three NOPs). Every import is checked before anything
  is allocated, and everything unsupported is reported at once.
* **Thunks** (`src/thunk.S`, 8086 only): each provided entry point has an 8-byte stub in the
  loader's near code segment; the program's far calls land there. The common path saves the
  program's SS:SP, switches to the loader's own stack and data segment, far-calls the C handler,
  pops the Pascal arguments and returns with `RETF`. Calls back into the program (window
  procedures) switch to the program's stack with AX = DS = the instance data segment, as USER
  does. Both directions nest.
* **Memory model**: w16 is built in the medium model (`-mcmodel=medium`): C functions are far and
  the linker fills a near and a far code segment (about 30 KB and 39 KB), which the ELKS kernel
  loads as one allocation (`CONFIG_EXEC_MMODEL`, on in the Tandy kernel). The 64 KB small-model
  limit was reached with CLOCK; nothing was dropped to make room. Data stays near: one 48 KB data
  segment with a 5 KB heap and a 6 KB stack.
* **Task start**: the program starts at its NE entry point exactly as a Windows task does
  (DS = SS = automatic data segment, PSP in ES, BX/CX stack and heap sizes). `InitTask` returns its
  six registers. The C runtime's own `INT 21h` calls (DOS version, exit) are served by a handler
  installed only while the program runs.
* **USER** (`src/user.c`): window classes, top-level windows drawn with a Windows 3.0 style frame,
  caption and system-menu box on an undecorated Nano-X window, with a child Nano-X window as the
  client area so GDI output is clipped to it. A message queue translates Nano-X events into
  `WM_KEYDOWN`/`WM_CHAR`/`WM_KEYUP`, `WM_LBUTTONDOWN/UP`, `WM_PAINT`, `WM_CLOSE`. Double-clicking
  the system-menu box sends `WM_SYSCOMMAND SC_CLOSE` as on Windows 3.0. Timers (`SetTimer`,
  with or without a callback) produce `WM_TIMER` after paints, as on Windows; while waiting, w16
  sleeps in the Nano-X client library until the next input or timer.
* **Menus** (`src/menu.c`): a class menu (a MENU resource) becomes a menu bar drawn in the frame
  under the caption, with mnemonics underlined. Clicking an item drops its popup down as a Nano-X
  window of its own, with check marks, grayed items, separators and accelerator text; choosing an
  item posts `WM_COMMAND`, after `WM_INITMENU`/`WM_INITMENUPOPUP`. Up/Down/Left/Right, Enter, Esc
  and mnemonics work in an open menu; F10 and Alt+letter open one (with the Nano-X keyboard fix
  in `nanox-w16.patch`; the stock driver drops them).
* **Input** (`src/user.c`, `src/cursor.c`): `WM_MOUSEMOVE` from Nano-X's compressed position
  events (only the latest one waits in the queue), `SetCapture`, `GetKeyState` from the latest
  event's modifiers and buttons, `PeekMessage` (removing or not), `PostMessage`, accelerator
  tables (`LoadAccelerators`/`TranslateAccelerator`, virtual keys with Shift/Ctrl/Alt and ASCII
  keys), and cursors: a program's CURSOR resource (AND and XOR masks) becomes a Nano-X cursor,
  halved from 32x32 to 16x16 to suit 320x200; the wait and cross cursors are drawn here; the
  arrow is Nano-X's own; `ShowCursor` hides with a blank cursor.
* **KERNEL memory and resources** (`src/kernel.c`): a first-fit local heap in the automatic data
  segment (`LocalAlloc` and friends); `FindResource`/`LoadResource` read a resource into a segment
  of its own, which `GlobalLock`/`LockResource` return; string tables (`LoadString`); resource
  types given by name, also through the Windows 3.0 NAMETABLE. Loaded resources the program never
  frees are freed when it exits, as Windows does for a task.
* **WIN.INI** (`src/profile.c`): `GetProfileString`, `GetProfileInt` and `WriteProfileString` read
  and rewrite a small text file, `$W16INI` or `win.ini` in the current directory.
* **GDI** (`src/gdi.c`): stock and created pens and brushes, device contexts on Nano-X graphics
  contexts (`BeginPaint`, `GetDC`, `CreateDC("DISPLAY")`), `Rectangle`, `Ellipse`, `MoveTo/LineTo`
  (end point excluded, as on Windows), `FillRect`, `TextOut`/`ExtTextOut` with `SetTextAlign`,
  all sixteen `SetROP2` modes on the Nano-X raster ops, `GetDeviceCaps` for a 320x200 screen with
  4:3 pixels, `GetTextMetrics`, `GetTextExtent`, colours mapped to the nearest of the 16 Tandy
  colours. `PatBlt` fills with the brush (PATCOPY, PATINVERT, DSTINVERT, BLACKNESS, WHITENESS);
  a pattern brush from a monochrome `CreateBitmap` of up to 16x16 is drawn as a tiled bitmap in
  the DC's text and background colours, aligned to the window origin as Windows does. Icons (Windows 3.x DIB images up to 32x32 at 1 or
  4 bits) are drawn with one Nano-X bitmap per colour.
* **Resources and dialogs**: resources are found by ordinal, by name, or through a Windows 3.0
  NAMETABLE, and read from the file when needed (`src/ne.c`). `DialogBox` parses the DIALOG
  template (`src/dlgtmpl.c`), converts dialog units from the Nano-X system font (scaled down to
  fit 320x200 when needed), and creates the dialog with built-in STATIC (left, centred, right
  text and icons) and push-button controls. Its modal loop gives Enter to the default button,
  Esc and the system-menu box to IDCANCEL, and clicks to the buttons, as the Windows dialog
  manager does. `MakeProcInstance` builds real `mov ax,hInstance; jmp far` instance thunks.

## Build

Requirements: an ELKS 0.9.2 tree built with its ia16 cross toolchain (the Tandy branch
`tandy-video-v0.9.2` for the colour display), and the Microwindows tree from
`packages/nanox-video` built with `make -f Makefile.elks SCREEN_TANDY=y` (pass
`LOCALFLAGS=-DMEM_GETPCJR=15` when building against the Tandy branch headers, which do not
declare the PCjr ioctl).

    export TOPDIR=/path/to/elks
    . $TOPDIR/env.sh
    make -C src TOPDIR=$TOPDIR MWDIR=/path/to/microwindows       # -> src/w16 (ELKS a.out)
    make -C src host                                             # -> src/w16host (host tests)

The Nano-X client library must be built for the medium model too (Makefile.elks builds the
small-model one for the demos). The four client files take w16's own flags:

    cd /path/to/microwindows/src
    CF="-mcmodel=medium -melks-libc -march=i8086 -Os -DELKS=1 -DUNIX=1 -DNDEBUG=1 -DNONETWORK=0
        -DMWCONFIG_FILE='\"mwconfig.elks\"' -DMWPIXEL_FORMAT=MWPF_PALETTE -DSCREEN_PIXTYPE=MWPF_PALETTE
        -DSCREEN_TANDY=1 -DMEM_GETPCJR=15 -Iinclude -Ielksdrivers"
    for f in nanox/client.c nanox/nxproto.c nanox/nxutil.c drivers/osdep.c; do ia16-elf-gcc $CF -c -o ${f%.c}.o $f; done
    ia16-elf-ar rc lib/libnano-X.a nanox/client.o nanox/nxproto.o nanox/nxutil.o drivers/osdep.o

(`-ffunction-sections` is not possible with far functions, and `signal()`'s return must be kept in
a function pointer, not a `void *`: the loop above compiles the files as they are except for that
one line in `GrClose`, which the patch below also fixes.)

Recommended: `nanox-w16.patch` changes the Nano-X server (not w16). On ELKS it reads each client's
requests in blocks and handles every request it already has before the next `select()` (the stock
server pays a `select()` and two `read()` calls per request, which on an 8088 was most of the cost
of drawing); `GrLine` can leave out its end point as Windows does (`GR_MODE_EXCLUDEENDPOINT`,
acknowledged to clients, so w16 knows); and the tty keyboard driver turns the ELKS console's
two-byte sequences ESC a..l and ESC A..Z into F1..F12 and Alt+letter instead of dropping them
together with the next key; and the engine no longer re-shows the software cursor between the
glyphs of one string when a driver has no glyph blitter (the rest of the string was drawn under the
visible cursor and wiped when it moved: seen as damaged menu-bar text after REVERSI's hint moved
the pointer). `nanox-tandyfast.patch` speeds up the Tandy/PCjr 320x200x16 screen driver: a
row-offset table replaces a multiply per pixel, horizontal lines and fills write whole bytes in
every raster mode, aligned copies move pixel pairs by byte, and a glyph blitter draws core-font
text two pixels per byte instead of a DrawPixel call per pixel; the pixel layout and the
Tandy/PCjr segment choice are unchanged. Apply them to the tree patched with `packages/nanox-video`,
then rebuild the server:

    cd /path/to/microwindows && patch -p1 < /path/to/packages/win16/nanox-w16.patch
    patch -p1 < /path/to/packages/win16/nanox-tandyfast.patch
    cd src && make -f Makefile.elks clean && make -f Makefile.elks SCREEN_TANDY=y

Measured (emulated, not real hardware: 86Box Tandy 1000 HX, 8088 at 7.16 MHz, `w16 -r -m` with
no logging, stock Tandy driver -> patched driver, medians): CLOCK start to first paint 4.26 -> 4.17 s,
Settings menu open 0.85 -> 0.79 s, switch to digital 1.56 -> 1.43 s, to analog 1.89 -> 1.73 s, About
box 3.87 -> 3.35 s, close 3.01 -> 2.58 s, timer tick 92 -> 91 ms, longest message 1020 -> 750 ms; on
a V20 start 3.70 -> 2.98 s and About 3.40 -> 2.94 s. REVERSI with logging and frame dumps on: first
paint 5.89 -> 5.57 s, click to repaint 3.1-3.6 -> 2.7-3.2 s. The frame dumps of both drivers are
byte-identical except for CLOCK frames (the time) and the one frame that shows the cursor defect
the engine fix removes. MartyPC's cycle-accurate 8088 runs the same images about 1.7x slower than
86Box (CLOCK start 7.6 s, click to repaint 3-4 s; REVERSI first paint 10.0 s, click to repaint
5.3-6.1 s, the same sequence passing), so the 86Box figures are the optimistic end for a real
Tandy 1000. Memory while REVERSI runs beside the server and the test driver: 404 of 448 KB
in use (w16 69,216 + 45,936 bytes; nano-color 50,544 + 38,432 with the patches), all freed at exit.

The test program needs only Open Watcom v2 (open source; no Windows SDK, DDK or media):

    WATCOM=/path/to/open-watcom sh test/hello16/build.sh         # -> HELLO16.EXE
    WATCOM=/path/to/open-watcom sh test/dlgtest/build.sh         # -> DLGTEST.EXE (needs python3 for its icon)

DLGTEST exercises the same calls as Windows 3.0's WINVER.EXE with source that can be shared.
To try WINVER itself, use your own copy (for example as `W16_EXTRA` for `tests/mkimage.sh`);
no Windows file is part of this package.

## Run

On ELKS with the Tandy kernel booted with `tandyvideo=1`:

    nano-color &                 # the colour Nano-X server (it exits when the program closes)
    w16 -v hello16.exe           # Esc or Q closes; the exit code is the number of keys typed

Start the server with `nano-color -p &` to keep it running for several programs in a row, for
example `w16 dlgtest.exe` (OK or Enter exits 1, Cancel or Esc exits 2) and `w16 winver.exe`.

Options: `-v` log, `-t` trace every API/INT 21h call, `-n` load and check only, `-s` stop at the
first unsupported INT 21h function, `-e` copy the log to port 0xE9 (emulators), `-d PREFIX` dump
the frame buffer after each paint, `-r` write `/tmp/w16.ready` with the screen positions of
buttons and menu items (test synchronisation), `-q SECS` close the main window after SECS seconds
as the system menu would (keyboard-only tests), `-m` measure: time-stamped log, startup time,
input-to-screen latency, timer message cost, and a count of windows, DCs, GDI objects, timers,
menus, local heap and loaded resources after each menu command and dialog (leak checks).

Windows 3.0 CLOCK.EXE (your own copy): `W16INI=/root/win.ini w16 clock.exe`. Click Settings to
switch between Analog and Digital or open About; double-click the system-menu box to close.
REVERSI.EXE: `w16 reversi.exe`; play with the mouse (the cursor is a cross on the squares you may
take), Game > Hint or Backspace (the game moves the pointer to the square it suggests, with
SetCursorPos, and retitles itself a practice game), Skill, Help > About; Help items other than
About show a message box, since there is no help viewer; Game > Exit ends it.
Exit status: the program's own exit code, or 10 unreadable/truncated, 11 not a valid NE,
12 unsupported executable feature, 13 unsupported imports, 14 out of memory, 15 bad relocation.

## Tests

* `tests/hosttest.sh`: host-only. Loads HELLO16 into simulated memory and compares every segment
  with an independent Python model (`tests/verify_load.py`); rejects 30 malformed or unsupported
  variants made by `tests/mkbad.py` with the expected code and message; injects allocation failures;
  runs 1500 mutated files under AddressSanitizer/UBSan. Checks that nothing leaks in every case.
  `tests/verify_dialogs.py` checks every resource lookup (by ordinal, by name in any case, and
  through a NAMETABLE) and every DIALOG template against its own decoder, and 800 mutated
  resource tables and templates run under the sanitizers. DLGTEST also carries a menu, a string
  table, a resource of a named type and a Windows 3.0 style NAMETABLE (made by
  `test/dlgtest/mknametab.py`) naming a dialog and a DATA resource, so the NAMETABLE path is
  checked without a Microsoft-built file; 600 mutated menu templates run under the sanitizers;
  `tests/mkbadres.py` derives files with a wrapping, truncated or unterminated NAMETABLE and
  resource entries pointing outside the file, which the loader must refuse when it opens them (the
  same files go to `/root/bad` on the test disk, where `w16 -n` refuses them on the 8088 itself,
  so the 16-bit arithmetic is exercised, not only the host's). The WIN.INI code is checked for
  set, replace, delete, DOS line ends, oversized files, a read error (nothing is written), and a
  replacement that cannot be written (the original stays). Set `W16_PRIVATE` to a folder holding
  your own WINVER.EXE, CLOCK.EXE and REVERSI.EXE to include their NAMETABLEs, menus, cursors,
  accelerators and string tables in the checks.
* `tests/elks/autotest.sh`: runs inside ELKS at boot (hooked from `/etc/rc.sys` in a test image),
  logging to port 0xE9. `tests/elks/w16mouse.c` drives Nano-X's serial-mouse driver through a
  pseudo-tty (Microsoft mouse packets), so mouse input reaches the program through the normal
  Nano-X input path; it is an injection at the serial byte level, not mouse hardware. With the
  `ptykbd` image flag it also feeds Nano-X's keyboard through a second pseudo-tty (the patched
  keyboard driver takes the device from `KEYBOARD`, leaving `CONSOLE` to the screen) with the byte
  sequences the ELKS console produces (F10 is ESC j, Alt+S is ESC S, Down is ESC [ B), so menus
  are driven by key as well: this exercises the Nano-X keyboard driver and w16, not the kernel's
  scancode layer; a DOSBox-X run with typed keys covers that layer. The driver tracks the pointer
  from relative packets, so it homes the pointer to the top-left corner (where the server clamps it)
  before every move: a program can move the pointer itself, as REVERSI's hint does. The REVERSI
  steps play one move by mouse at a square that is legal at the start and three by Hint followed by
  a click where the hint put the pointer, then use the menus, the About box, a Help item, the Skill
  popup (closed with Esc), the Backspace and F1 accelerators and Game > Exit.
  `tests/elks/nxstress.c` stresses the Nano-X server before the programs run: request headers
  split across writes with pauses, disconnects in the middle of a request, 25 connect/draw/
  disconnect cycles and two clients drawing at once; the programs then have to work as before.
* `tools/checkords.py` checks every ordinal against an export list of the Windows 3.0 modules;
  `tools/w16census.py` lists, for any NE file, which imports w16 lacks, its resources and
  memory needs, and its 80186+ instruction sites.

## What is supported

KERNEL: InitTask, WaitEvent, GetVersion, GetWinFlags, GetModuleFileName, LockSegment,
UnlockSegment, MakeProcInstance, FreeProcInstance, LocalAlloc, LocalFree, LocalLock,
LocalUnlock, LocalSize, GlobalLock, GlobalUnlock, FindResource, LoadResource, LockResource,
FreeResource, GetInstanceData, GetProfileInt, GetProfileString, WriteProfileString, lstrcpy,
lstrcat, lstrlen, DOS3Call and the selector constants (`__AHSHIFT`, `__AHINCR`, `__WINFLAGS`,
`__A000H` ...). USER: InitApp, RegisterClass (with a class menu), CreateWindow (top-level),
ShowWindow, UpdateWindow, DestroyWindow, DefWindowProc, GetMessage, TranslateMessage,
DispatchMessage, PostQuitMessage, BeginPaint, EndPaint, InvalidateRect, GetClientRect, GetDC,
ReleaseDC, SetTimer, KillTimer, SetRect, OffsetRect, InflateRect, FillRect, GetSysColor,
LoadString, MessageBox, DialogBox, EndDialog, GetDlgItem, SetDlgItemText, GetSystemMetrics,
GetMenu, GetSubMenu, CheckMenuItem, EnableMenuItem, DrawMenuBar, LoadCursor (standard and the
program's own), LoadIcon, SetCursor, ShowCursor, SetCursorPos, SetCapture, ReleaseCapture,
SetFocus, ClientToScreen, IsIconic, SetWindowText, GetKeyState, PeekMessage, PostMessage,
LoadAccelerators, TranslateAccelerator, WinHelp (a message box), wsprintf. GDI: GetStockObject,
CreatePen, CreateSolidBrush, CreateFontIndirect, CreateBitmap (monochrome, up to 16x16),
CreatePatternBrush, SelectObject, DeleteObject, UnrealizeObject, SetTextColor, SetBkColor,
SetBkMode, SetROP2, SetTextAlign, Rectangle, Ellipse, MoveTo, LineTo, PatBlt, TextOut, ExtTextOut,
GetTextMetrics, GetTextExtent, GetNearestColor, CreateDC/DeleteDC (the display), GetDeviceCaps,
AddFontResource/RemoveFontResource (accepted, fonts not loaded). KERNEL also: GetModuleUsage.
INT 21h: 00h, 19h, 25h/35h (virtual vectors), 2Ah, 2Ch, 30h, 40h to stdout/stderr, 4Ch.

## What is not (yet)

DLLs and `LoadLibrary`; bitmaps from resources (`LoadBitmap`, `BitBlt`) and `LoadIcon` from the
program's own resources (the frame icon stands in); colour cursors; menus from `LoadMenu`/`SetMenu`
(class menus only), nested popups, the system menu itself; modeless dialogs (`CreateDialog`) and
`DialogBoxParam`; `SendMessage` and `PostMessage` to all windows;
check boxes, radio buttons, group boxes, EDIT, LISTBOX, COMBOBOX and SCROLLBAR controls (drawn
as empty boxes); keyboard focus moves between controls; child windows outside dialogs; the
global heap (`GlobalAlloc`); font files and fonts other than the Nano-X system font (every
`CreateFontIndirect` font draws with it); bitmaps and `BitBlt`; mapping modes; clipping to the
update region and `ETO_CLIPPED`; file I/O through INT 21h; 8087 OS fixups;
self-loading, protected-mode-only and 80286-flagged executables; 64 KB segments; more than one
Win16 program at a time (the loader keeps its data segment in its own code segment).
Keyboard: without `nanox-w16.patch` the Nano-X tty keyboard driver decodes only three-byte
`ESC [` sequences, so the ELKS console's F1-F12 and Alt+letter never arrive and menus need the
mouse. Alt+F1..F8 switch ELKS consoles in the kernel and never reach a program, so Alt+F4 does
not close a window here; the system-menu box (double-click) or the program's own Exit does.
Memory is never discarded or moved, so the whole program must fit in conventional memory.
