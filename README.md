# w16: a small Win16 compatibility layer for ELKS

`w16` loads an **unchanged** Windows 3.x NE executable on ELKS, binds its KERNEL, USER and GDI
imports to native implementations, and runs the program's own 8086 code on the 8088, with
Nano-X (the Tandy/PCjr colour build in `packages/nanox-video`) as the display.
It is Wine-inspired in approach, not in size: one ELKS a.out of about 76 KB, no Windows files.

It runs the included test programs and, unchanged, Windows 3.0's own WINVER.EXE and CLOCK.EXE
(analog and digital modes, its menu, About box, timer and WIN.INI settings). It does not run
arbitrary Windows applications. See "What is supported" and "What is not" below.

## How it works

* **Loader** (`src/ne.c`): reads the MZ and NE headers, validates them, allocates one real-mode
  segment per NE segment with the ELKS `fmemalloc` system call, reads the segment data, and
  applies the relocation records: internal segment/offset/far-address fixups (including chains
  and additive records), imports by ordinal or by name, and Windows' prologue patch for exported
  callbacks (`push ds; pop ax; nop` becomes three NOPs). Every import is checked before anything
  is allocated, and everything unsupported is reported at once.
* **Thunks** (`src/thunk.S`, 8086 only): each provided entry point has an 8-byte stub in the
  loader's code segment; the program's far calls land there. The common path saves the program's
  SS:SP, switches to the loader's own stack and data segment, calls the C handler, pops the Pascal
  arguments and returns with `RETF`. Calls back into the program (window procedures) switch to the
  program's stack with AX = DS = the instance data segment, as USER does. Both directions nest.
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
  and mnemonics work in an open menu; F10 and Alt+letter open one when the keyboard driver
  delivers them (the ELKS tty keyboard driver of Nano-X does not: see below).
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
  4:3 pixels, `GetTextMetrics`, colours mapped to the nearest of the 16 Tandy colours. Icons (Windows 3.x DIB images up to 32x32 at 1 or
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

Optional, recommended: `nanox-batch.patch` changes the Nano-X server (not w16) so that on ELKS it
reads each client's requests in blocks and handles every request it already has before the next
`select()`. The stock server pays a `select()` and two `read()` calls per request, which on an 8088
is most of the cost of drawing; with the patch CLOCK repaints its analog face about 1.7 times
faster. Apply it to the tree patched with `packages/nanox-video`, then rebuild the server:

    cd /path/to/microwindows && patch -p1 < /path/to/packages/win16/nanox-batch.patch
    cd src && make -f Makefile.elks clean && make -f Makefile.elks SCREEN_TANDY=y

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
buttons and menu items (test synchronisation), `-m` measure: time-stamped log, startup time,
input-to-screen latency, timer message cost, and a count of windows, DCs, GDI objects, timers,
menus, local heap and loaded resources after each menu command and dialog (leak checks).

Windows 3.0 CLOCK.EXE (your own copy): `W16INI=/root/win.ini w16 clock.exe`. Click Settings to
switch between Analog and Digital or open About; double-click the system-menu box to close.
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
  table and a resource of a named type, checked against expected values, and 600 mutated menu
  templates run under the sanitizers; the WIN.INI code is checked for set, replace, delete, DOS
  line ends and oversized files. Set `W16_PRIVATE` to a folder holding your own WINVER.EXE and
  CLOCK.EXE to include their NAMETABLEs, menus and string tables in the checks.
* `tests/elks/autotest.sh`: runs inside ELKS at boot (hooked from `/etc/rc.sys` in a test image),
  logging to port 0xE9. `tests/elks/w16mouse.c` drives Nano-X's serial-mouse driver through a
  pseudo-tty (Microsoft mouse packets), so mouse input reaches the program through the normal
  Nano-X input path; it is an injection at the serial byte level, not mouse hardware.
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
GetMenu, GetSubMenu, CheckMenuItem, EnableMenuItem, DrawMenuBar, LoadCursor/LoadIcon, wsprintf.
GDI: GetStockObject, CreatePen, CreateSolidBrush, CreateFontIndirect, SelectObject, DeleteObject,
SetTextColor, SetBkColor, SetBkMode, SetROP2, SetTextAlign, Rectangle, Ellipse, MoveTo, LineTo,
TextOut, ExtTextOut, GetTextMetrics, GetNearestColor, CreateDC/DeleteDC (the display),
GetDeviceCaps, AddFontResource/RemoveFontResource (accepted, fonts not loaded). INT 21h: 00h,
19h, 25h/35h (virtual vectors), 2Ah, 2Ch, 30h, 40h to stdout/stderr, 4Ch.

## What is not (yet)

DLLs and `LoadLibrary`; bitmaps and cursors from resources (`LoadIcon`/`LoadCursor` accept the
program's own but show the standard ones); menus from `LoadMenu`/`SetMenu` (class menus only),
nested popups, the system menu itself; modeless dialogs (`CreateDialog`) and `DialogBoxParam`;
check boxes, radio buttons, group boxes, EDIT, LISTBOX, COMBOBOX and SCROLLBAR controls (drawn
as empty boxes); keyboard focus moves between controls; child windows outside dialogs; the
global heap (`GlobalAlloc`); font files and fonts other than the Nano-X system font (every
`CreateFontIndirect` font draws with it); bitmaps and `BitBlt`; mapping modes; clipping to the
update region and `ETO_CLIPPED`; file I/O through INT 21h; 8087 OS fixups;
self-loading, protected-mode-only and 80286-flagged executables; 64 KB segments; more than one
Win16 program at a time (the loader keeps its data segment in its own code segment).
Keyboard: the Nano-X tty keyboard driver used on ELKS decodes only three-byte `ESC [` sequences,
so the ELKS console's `ESC a`..`ESC j` (F1-F10) and `ESC A`..`ESC Z` (Alt+letter) never arrive as
keys; menus are therefore reached with the mouse.
Memory is never discarded or moved, so the whole program must fit in conventional memory.
