<div align="center">

# w16 for ELKS

**Win16 applications on 8086/8088 ELKS: native CGA, experimental VGA, and target-specific Tandy/PCjr graphics.**

Run selected Windows programs on an 8086-class ELKS guest without running Windows. Choose the display build that matches the machine.

<a href="docs/screenshots/paintbrush-elks-logo-qemu-vga.png"><img src="docs/screenshots/paintbrush-elks-logo-qemu-vga.png" width="640" alt="Windows 3.0 Paintbrush on ELKS with a dark-blue ELKS logo drawn on its white canvas"></a>

**Windows 3.0 Paintbrush on ELKS · 640×480 VGA · QEMU**

The logo was drawn with Paintbrush’s line tool using emulated mouse input. This is an emulator capture, not a physical-hardware test. Paintbrush support is experimental: this drawing does not establish that every tool or file operation works.

[CGA setup](docs/build.md#cga-on-stock-elks) · [Experimental VGA setup](docs/build.md#experimental-vga-solitaire) · [Tandy/PCjr setup](docs/build.md#tandypcjr-user-space-build) · [Screenshots](docs/gallery.md) · [Technical guide](docs/technical.md) · [MIT license](LICENSE)

</div>

<table>
  <tr>
    <td width="50%"><a href="docs/screenshots/reversi-board-after-moves-martypc-8088-framebuffer.png"><img src="docs/screenshots/reversi-board-after-moves-martypc-8088-framebuffer.png" alt="Windows 3.0 Reversi running under w16 on ELKS after several moves, captured in MartyPC 8088 emulation" width="480"></a></td>
    <td width="50%"><a href="docs/screenshots/clock-martypc-8088-analog-framebuffer.png"><img src="docs/screenshots/clock-martypc-8088-analog-framebuffer.png" alt="Windows 3.0 Clock in analog mode under w16 on ELKS, captured in MartyPC 8088 emulation" width="480"></a></td>
  </tr>
  <tr>
    <td align="center"><b>Reversi</b> · MartyPC, emulated 8088</td>
    <td align="center"><b>Clock</b> · MartyPC, emulated 8088</td>
  </tr>
</table>

`w16` loads a Windows NE executable, binds its supported Win16 imports to native handlers, and runs its code on ELKS. The Windows 3.0 examples use unchanged application executables; Windows 3.1 Minesweeper needs a converted private copy. Nano-X supplies display and input. No Microsoft executables or system files are included.

**Current Program Manager milestone:** stock Windows 3.0 Program Manager opens its Games group and launches and closes stock Solitaire on an emulated **4.77 MHz 8088, 640 KiB, stock scancode ELKS and 640×200 monochrome CGA**. The first full deal, child cleanup, recovered RAM and a responsive parent were observed using a lean shell-free boot and `w16 -T`. This is an emulator result. Full Program Manager on physical hardware is unverified; the open DLGTEST example has passed one Escape back to a working shell on a real Tandy 1000 EX in its separate hardware setup.

### Program Manager on CGA

Captured in the same 8088 emulator run on stock ELKS. Click either screenshot for the full-size capture.

<table>
<tr>
<td align="center"><a href="docs/screenshots/progman-games-stock-elks-8088-cga.png"><img src="docs/screenshots/progman-games-stock-elks-8088-cga.png" width="440" alt="Windows 3.0 Program Manager with Games selected in its Window menu"></a><br>Program Manager’s Games menu</td>
<td align="center"><a href="docs/screenshots/progman-solitaire-stock-elks-8088-cga.png"><img src="docs/screenshots/progman-solitaire-stock-elks-8088-cga.png" width="440" alt="Windows 3.0 Solitaire launched from Program Manager with all seven columns dealt"></a><br>Solitaire after the first deal</td>
</tr>
</table>

The supported programs and display configurations are listed below. This is not a general-purpose Windows compatibility environment.

## What runs

| Program | Working features | Where to get it |
| :--- | :--- | :--- |
| **HELLO16** | Drawing, colours, text, mouse and keyboard input | [Source included](test/hello16) |
| **DLGTEST** | Modal dialogs, resources, buttons, menus and string tables | [Source included](test/dlgtest) |
| **MBTEST** | Message boxes, icons and button results | [Source included](test/mbtest) |
| **Windows 3.1 Minesweeper** | Reveal cells, flag them and change board size on emulated 8086 | Bring your own supported WINMINE.EXE; convert a private copy as below |
| **Windows 3.0 Paintbrush** | Line drawing in colour; opens indexed BMPs on the experimental VGA configuration | Bring your own EXE and companion DLL |
| **Windows 3.0 WINVER** | Version dialog | Bring your own copy |
| **Windows 3.0 CLOCK** | Analog/digital display, timer, Settings menu, About and WIN.INI preferences | Bring your own copy |
| **Windows 3.0 REVERSI** | Play against the computer, menus, hints, cursors, accelerators and About | Bring your own copy |
| **Windows 3.0 Program Manager** | Groups and launch/close of Solitaire with `-T` on emulated 8088 CGA; full hardware support unverified | Bring your own EXE, configuration and group files |
| **Windows 3.0 SOLITAIRE** | First deal and close on emulated CGA; basic moves and Options on experimental VGA | Bring your own copy |

### Win16 API binding coverage

| Module | Bindings / named exports | Coverage |
| :--- | ---: | ---: |
| KERNEL | 73 / 192 | 38.0% |
| USER | 157 / 387 | 40.6% |
| GDI | 72 / 235 | 30.6% |
| Total | 302 / 814 | 37.1% |

This is a static snapshot against one Windows 3.0 real-mode reference set. The denominator counts unique named export ordinals (aliases count once); it excludes ordinal-only exports and other DLLs. The numerator counts entries W16 resolves by ordinal in its default configuration, including exported constants and raw entries, but not diagnostic traps. A binding does not prove correct behavior or that an application runs. See the app results above for what has actually been exercised.

### Display configurations

| Display build | Resolution | Emulator coverage |
| :--- | :--- | :--- |
| **CGA** | 640×200 monochrome or 320×200 four colours | IBM CGA with interpreted 4.77 MHz 8088; stock-scancode mono launch/close checks |
| **Experimental VGA** | 640×480, 16 colours | Paintbrush and Solitaire in QEMU; converted Minesweeper in QEMU and DOSBox-X 8086 mode |
| **Tandy / expanded PCjr** | 320×200, 16 colours | Tandy in 86Box and MartyPC; expanded PCjr in DOSBox-X |

A result on one display is not a claim about the others. Physical Tandy coverage is limited to the open dialog test; VGA and full Program Manager hardware behavior remain unverified. See the [technical guide](docs/technical.md#patch-internals-and-emulator-measurements) for the test configurations.


<table>
  <tr>
    <td width="50%"><a href="docs/screenshots/hello16-colors-text-dosboxx-pcjr.png"><img src="docs/screenshots/hello16-colors-text-dosboxx-pcjr.png" alt="The source-built HELLO16 example showing coloured shapes, text and lines in DOSBox-X PCjr emulation" width="480"></a></td>
    <td width="50%"><a href="docs/screenshots/messagebox-question-icon-86box-8088.png"><img src="docs/screenshots/messagebox-question-icon-86box-8088.png" alt="The source-built MBTEST example showing a question icon and Yes and No buttons in 86Box 8088 emulation" width="480"></a></td>
  </tr>
  <tr>
    <td align="center"><b>HELLO16</b> · DOSBox-X, emulated PCjr</td>
    <td align="center"><b>MBTEST</b> · 86Box, emulated 8088</td>
  </tr>
</table>

[See the gallery →](docs/gallery.md) for VGA Solitaire, Clock and Reversi, and the Tandy/PCjr examples.

## Running the Windows 3.1 Minesweeper on an 8086-class guest

Unlike the Windows 3.0 examples, this game's original code contains instructions introduced after the 8088. On the build host, install NASM's `ndisasm` on your PATH and run:

```sh
python3 tools/winmine8088.py /path/to/original/WINMINE.EXE /tmp/mine8086.exe
```

The output path must not already exist. Copy that private output into your ELKS guest, start the patched VGA Nano-X server and run `w16 /path/to/mine8086.exe`. Use a writable working directory if you want settings saved to `winmine.ini`. No Microsoft executable is distributed here. The transformer targets a specific two-segment layout, not arbitrary Windows programs.

The converted game has run on ELKS in DOSBox-X's 8086 CPU mode. Mouse input revealed numbered squares and placed flags; the Game menu switched to an Intermediate board. This does not establish physical 8088 compatibility, original clock speed, or complete coverage of every game path. The legacy sound service reports no voices. The compact build retains API call tracing (`-t`) and error messages but omits detailed verbose logging and the allocation census to fit its two code segments.

## Solitaire on experimental VGA

[![Windows 3.0 Solitaire on ELKS after dealing three cards, captured in QEMU at 640×480 VGA](docs/screenshots/solitaire-stock-deal-qemu-vga.png)](docs/gallery.md#solitaire-on-experimental-vga)

The separately supplied Windows 3.0 executable runs in QEMU. Stock dealing, basic tableau moves, and Options have been tested; a full game and real hardware have not. See the [VGA setup and limitations](docs/build.md#experimental-vga-solitaire).

## Build it from source

Start with the **[build and run guide](docs/build.md)**. You will need:

- A built **ELKS 0.9.2** tree and its **ia16** cross toolchain. The normal runtime is a native ELKS **NE** executable and needs **`CONFIG_EXEC_OS2=y` and `CONFIG_EXEC_MMODEL=y`**. Ordinary CGA uses unmodified upstream ELKS; the stock keyboard route additionally needs **`CONFIG_KEYBOARD_SCANCODE=y`**. See [kernel setup](docs/build.md#2-prepare-elks).
- The pinned **Microwindows/Nano-X** checkout and three user-space baseline patches from the build guide. Keep each display server paired with the medium-model client archive used to link W16.
- **Open Watcom v2** for the included open Win16 examples, and a Linux host with Make, GCC, Python 3, Git and `patch`.

After preparing those trees, build the current CGA path from this repository's root:

```bash
export TOPDIR=/path/to/built-stock-elks
export MWDIR=/path/to/patched-microwindows
. "$TOPDIR/env.sh"
export CGA_OUT="$PWD/build/nanox-cga"
ELKS_KBD=scancode sh scripts/build-nanox-cga.sh
make -C src clean
make TOPDIR="$TOPDIR" MWDIR="$CGA_OUT"
```

Install `$CGA_OUT/nano-x` as `/bin/nano-x` and `src/w16` as `/bin/w16`. **`src/w16` is the default native NE output**, with three code groups, private task data and shared read-only constants. The old two-group a.out runtime no longer fits and is not a supported build. No kernel patch or Tandy framebuffer reservation is required for ordinary CGA. `CGA_MODE=color` selects 320×200 four-colour CGA; panning remains opt-in and consumes extra conventional memory.

Build the open dialog with `WATCOM=/path/to/open-watcom make samples`, copy `test/dlgtest/DLGTEST.EXE` to the guest, and run `w16 dlgtest.exe`. Let W16 autostart its nonpersistent server for the Escape-to-shell check. The scancode backend is US set-1 and must match the running kernel; BIOS-polling input cannot preserve all Alt shortcuts.

**Experimental VGA (640×480):**

```bash
export VGA_OUT="$PWD/build/nanox-vga"
ELKS_KBD=tty sh scripts/build-nanox-vga.sh
make -C src clean
make TOPDIR="$TOPDIR" MWDIR="$VGA_OUT"
```

Install that server and its matching `src/w16`. Earlier VGA application results came from the documented emulator configurations. They do **not** verify the current VGA driver on the exact stock-scancode setup: its console-lock integration for the raw backend is still missing. The `tty` recipe builds user space; it does not establish working stock-kernel Alt shortcuts or physical VGA support. See [VGA evidence and limitations](docs/build.md#experimental-vga-solitaire).

The target-specific [Tandy/PCjr fork builds](docs/build.md#tandypcjr-user-space-build) remain separate from ordinary CGA/VGA and require their own kernel setup.

With your own Windows 3.0 programs on the ELKS disk:

```sh
W16INI=/root/win.ini w16 clock.exe
w16 reversi.exe
```

For the experimental VGA Paintbrush setup shown above, put your own Windows 3.0 `pbrush.exe` and `pbrush.dll` in the working directory:

```sh
w16 -u -l PBRUSH=pbrush.dll pbrush.exe
```

`-l` binds the companion DLL explicitly. `-u` permits startup with unresolved imports; calling one stops the program rather than pretending the operation succeeded.

`-p` intentionally keeps Nano-X running after the application closes. For an owned nonpersistent server, W16 waits for server exit before returning the console, with a bounded timeout. `-T` enables the tested isolated Win16 child-task path; 640 KiB co-launches remain sensitive to boot processes and application memory needs. See the [build guide](docs/build.md) for keyboard and display limits.

## How it fits together

1. **Load the NE file.** Validate its segments and imports, allocate real-mode memory, and apply relocations.
2. **Bridge the Win16 calls.** 8086 assembly thunks switch between the application's stack and native ELKS handlers, including callbacks into the application.
3. **Draw through Nano-X.** USER handles windows, messages, menus and dialogs; GDI maps drawing to the Nano-X display, including 320×200 Tandy/PCjr and 640×480 VGA configurations.

The implementation includes local memory and resource handling, timers, accelerators, cursors, WIN.INI access and a subset of KERNEL/USER/GDI. See [how it works](docs/technical.md#how-it-works) and the [API list](docs/technical.md#what-is-supported) for the details.

## Know the limits

- **Real mode, 8086/8088 code only.** Protected-mode-only and 80286-flagged executables, 64 KB segments and general-purpose DLL loading are unsupported. The Paintbrush example uses an explicitly bound companion DLL.
- **A small Win16 API subset.** Many applications require APIs that are not implemented. By default, unsupported imports are reported before the application starts.
- **Basic UI and graphics.** Bitmap transfers, indexed image formats, editable text and file-dialog lists cover the exercised cases, not the full Windows APIs. Modeless dialogs and full window management remain unsupported. The system menu implements Close only.
- **Conventional memory matters.** The entire application must fit; memory is not discarded or moved. Only one Win16 program can run at a time.
- **Emulation is not hardware validation.** In particular, the DOSBox-X PCjr runs use a fast fixed-cycle configuration and do not establish real-PCjr performance.

The full [limitations list](docs/technical.md#what-is-not-yet) is the best place to check before trying another program. `tools/w16census.py` can inspect an NE executable's imports, resources and memory needs.

## Explore the repository

| Path | Contents |
| :--- | :--- |
| [`src/`](src) | NE loader, Win16 API implementations and 8086 call thunks |
| [`test/`](test) | Source-built Win16 example applications |
| [`tests/`](tests) | Host loader checks, malformed inputs, keyboard checks and ELKS emulator drivers |
| [`tools/`](tools) | NE inspection, disassembly and import-coverage tools |
| [`dependencies/`](dependencies) · [`patches/`](patches) | Nano-X base patch and license; experimental VGA capture and pixel extensions |
| [`docs/`](docs) | Build guide, implementation details and real emulator captures |

For the host loader checks:

```sh
WATCOM=/path/to/open-watcom make test
```

This builds the examples and checks the host-side loader and resources, **not execution of 16-bit code**. Optional checks need an ELKS or Microwindows tree; the [build guide](docs/build.md#host-side-checks) explains the prerequisites and skipped sections.

## License

The `w16` implementation and source examples are under the [MIT license](LICENSE). Patches to ELKS and Microwindows retain their [upstream licenses](dependencies/README.md). Open Watcom also has its own license.

The screenshots show prior emulator runs with the source examples and separately supplied Windows 3.0 applications. Microsoft Windows binaries are **not distributed** in this repository.
