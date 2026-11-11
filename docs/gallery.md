# Screenshot gallery

[← Project overview](../README.md) · [Build and run](build.md) · [Technical guide](technical.md)

Actual captures from prior emulator runs of `w16` on ELKS. Images are kept as supplied, with no invented UI or composited application windows. Click an image to view the full capture.

The display target is 320×200 with 16 colours; these captures are displayed at 640×400, or include emulator window controls. All machines shown below are **emulated**. These images do not establish physical-hardware compatibility or performance.

## Windows 3.0 applications

### Reversi after several moves

[![Reversi Practice Game after several moves, showing the game board and menu bar](screenshots/reversi-board-after-moves-martypc-8088-framebuffer.png)](screenshots/reversi-board-after-moves-martypc-8088-framebuffer.png)

**MartyPC · emulated 8088 · framebuffer capture.** The original Windows 3.0 application runs its own game logic, including the computer opponent and hints.

### Clock, analog and digital

<table>
  <tr>
    <td width="50%"><a href="screenshots/clock-martypc-8088-analog-framebuffer.png"><img src="screenshots/clock-martypc-8088-analog-framebuffer.png" alt="Windows 3.0 Clock in analog mode, MartyPC 8088 framebuffer capture" width="480"></a></td>
    <td width="50%"><a href="screenshots/clock-martypc-8088-digital-framebuffer.png"><img src="screenshots/clock-martypc-8088-digital-framebuffer.png" alt="Windows 3.0 Clock in digital mode, MartyPC 8088 framebuffer capture" width="480"></a></td>
  </tr>
  <tr><td align="center">Analog</td><td align="center">Digital</td></tr>
</table>

**MartyPC · emulated 8088 · framebuffer captures.** Clock uses timers, its Settings menu and WIN.INI preferences. All requested fonts currently use the Nano-X system font.

### The system menu

[![Clock with its Close system menu open after Alt+Space](screenshots/clock-system-menu-by-alt-space-86box-8088.png)](screenshots/clock-system-menu-by-alt-space-86box-8088.png)

**86Box · emulated 8088.** Alt+Space opens the system menu with the supplied keyboard patches. Close is implemented; moving, resizing, minimizing and maximizing are not.

### Reversi dialogs

<table>
  <tr>
    <td width="50%"><a href="screenshots/reversi-illegal-move-with-icon-86box-8088.png"><img src="screenshots/reversi-illegal-move-with-icon-86box-8088.png" alt="Reversi message explaining that a move must be made where the cursor is a cross, in 86Box" width="480"></a></td>
    <td width="50%"><a href="screenshots/reversi-help-86box-8088-fixed-server.png"><img src="screenshots/reversi-help-86box-8088-fixed-server.png" alt="Reversi's Help command displaying Help is not available under w16, in 86Box with the corrected Nano-X server" width="480"></a></td>
  </tr>
  <tr><td align="center">Move guidance</td><td align="center">Help fallback</td></tr>
</table>

**86Box · emulated 8088.** Message boxes are modal. There is no Windows Help viewer: Help requests display a short explanation instead. The Help capture uses the corrected Nano-X server, without the earlier cursor-redraw defect.

### WINVER

[![Windows 3.0 WINVER showing its Real Mode Windows Version 3.00 dialog under w16 in 86Box](screenshots/winver-86box-8088.png)](screenshots/winver-86box-8088.png)

**86Box · emulated 8088 · capture includes emulator controls.** This is the Windows application's own version dialog running under `w16`; it does not mean a Windows operating system is running underneath.

## Source-built examples

### HELLO16 on the PCjr configuration

[![HELLO16 showing a blue rectangle, red ellipse, text and green lines on the PCjr display](screenshots/hello16-colors-text-dosboxx-pcjr.png)](screenshots/hello16-colors-text-dosboxx-pcjr.png)

**DOSBox-X · emulated PCjr.** The example's [complete source](../test/hello16) is included and builds with Open Watcom v2, without Windows media.

### MBTEST message box

[![MBTEST question message box with a question icon and Yes and No buttons](screenshots/messagebox-question-icon-86box-8088.png)](screenshots/messagebox-question-icon-86box-8088.png)

**86Box · emulated 8088.** The [MBTEST source](../test/mbtest) exercises message boxes, drawn icons and button results.

## Expanded-PCjr display

[![Windows 3.0 Reversi at its initial board position in DOSBox-X PCjr emulation](screenshots/reversi-board-dosboxx-pcjr.png)](screenshots/reversi-board-dosboxx-pcjr.png)

**DOSBox-X · `machine=pcjr` · 640 KB.** The prior runs used an 8086 core at a fixed 8000 cycles with DOSBox-X's own PCjr BIOS. This is much faster than an original PCjr and is not a real-PCjr performance result. See the [technical notes](technical.md#patch-internals-and-emulator-measurements).

---

Windows applications shown here were supplied separately. This repository contains their screenshots, not their executable files. Use your own copies to reproduce those runs, or start with the included source examples.
