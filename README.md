# w16: a small Win16 compatibility layer for ELKS

`w16` loads an **unchanged** Windows 3.x NE executable on ELKS, binds its KERNEL, USER and GDI
imports to native implementations, and runs the program's own 8086 code on the 8088, with
Nano-X (the Tandy/PCjr colour build in `packages/nanox-video`) as the display.
It is Wine-inspired in approach, not in size: one ELKS a.out of about 44 KB, no Windows files.

This is a first milestone. It runs the included test program; it does not yet run arbitrary
Windows applications. See "What is supported" and "What is not" below.

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
  the system-menu box sends `WM_SYSCOMMAND SC_CLOSE` as on Windows 3.0.
* **GDI** (`src/gdi.c`): stock and created pens and brushes, device contexts on Nano-X graphics
  contexts, `Rectangle`, `Ellipse`, `MoveTo/LineTo`, `TextOut`, colours mapped to the nearest of
  the 16 Tandy colours by the Nano-X server.

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

The test program needs only Open Watcom v2 (open source; no Windows SDK, DDK or media):

    WATCOM=/path/to/open-watcom sh test/hello16/build.sh         # -> HELLO16.EXE

## Run

On ELKS with the Tandy kernel booted with `tandyvideo=1`:

    nano-color &                 # the colour Nano-X server
    w16 -v hello16.exe           # Esc or Q closes; the exit code is the number of keys typed

Options: `-v` log, `-t` trace every API/INT 21h call, `-n` load and check only, `-s` stop at the
first unsupported INT 21h function, `-e` copy the log to port 0xE9 (emulators), `-d PREFIX` dump
the frame buffer after each paint, `-r` create `/tmp/w16.ready` after the first paint.
Exit status: the program's own exit code, or 10 unreadable/truncated, 11 not a valid NE,
12 unsupported executable feature, 13 unsupported imports, 14 out of memory, 15 bad relocation.

## Tests

* `tests/hosttest.sh`: host-only. Loads HELLO16 into simulated memory and compares every segment
  with an independent Python model (`tests/verify_load.py`); rejects 25 malformed or unsupported
  variants made by `tests/mkbad.py` with the expected code and message; injects allocation failures;
  runs 1500 mutated files under AddressSanitizer/UBSan. Checks that nothing leaks in every case.
* `tests/elks/autotest.sh`: runs inside ELKS at boot (hooked from `/etc/rc.sys` in a test image),
  logging to port 0xE9. `tests/elks/w16mouse.c` drives Nano-X's serial-mouse driver through a
  pseudo-tty (Microsoft mouse packets), so mouse input reaches the program through the normal
  Nano-X input path; it is an injection at the serial byte level, not mouse hardware.
* `tools/checkords.py` checks every ordinal against an export list of the Windows 3.0 modules;
  `tools/w16census.py` lists, for any NE file, which imports w16 lacks, its resources and
  memory needs, and its 80186+ instruction sites.

## What is supported

KERNEL: InitTask, WaitEvent, GetVersion, GetModuleFileName, DOS3Call and the selector constants
(`__AHSHIFT`, `__AHINCR`, `__WINFLAGS`, `__A000H` ...). USER: InitApp, RegisterClass,
CreateWindow (top-level), ShowWindow, UpdateWindow, DestroyWindow, DefWindowProc, GetMessage,
TranslateMessage, DispatchMessage, PostQuitMessage, BeginPaint, EndPaint, InvalidateRect,
GetClientRect, LoadCursor/LoadIcon (system resources only), wsprintf. GDI: GetStockObject,
CreatePen, CreateSolidBrush, SelectObject, DeleteObject, SetTextColor, SetBkColor, SetBkMode,
Rectangle, Ellipse, MoveTo, LineTo, TextOut. INT 21h: 00h, 19h, 25h/35h (virtual vectors),
2Ah, 2Ch, 30h, 40h to stdout/stderr, 4Ch.

## What is not (yet)

DLLs and `LoadLibrary`; resources (icons, menus, dialogs, strings, bitmaps); dialog boxes and
the system control classes (BUTTON, EDIT, STATIC, LISTBOX ...); child windows; the local and
global heap APIs; timers; fonts other than the Nano-X system font; bitmaps and `BitBlt`;
mapping modes; clipping to the update region; file I/O through INT 21h; 8087 OS fixups;
self-loading, protected-mode-only and 80286-flagged executables; 64 KB segments; more than one
Win16 program at a time (the loader keeps its data segment in its own code segment).
Memory is never discarded or moved, so the whole program must fit in conventional memory.
