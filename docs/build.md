# Build and run

[← Project overview](../README.md) · [Screenshot gallery](gallery.md) · [Technical guide](technical.md)

This is a source build for an experimental ELKS graphics stack. Start with the included example applications; they do not require Microsoft software. Commands below assume a **Bash shell on Linux** and use absolute paths so that changing directories does not change their meaning.

## 1. Get the sources and tools

You need:

- Git, Make, GCC, Bash, Python 3 and `patch` on the host.
- An ELKS 0.9.2 tree with its ia16 cross toolchain and built libc. Follow that tree's `BUILD.md` for host packages and toolchain setup.
- The Microwindows checkout below. Use the specified revision because the patches are written against it.
- [Open Watcom v2](https://github.com/open-watcom/open-watcom-v2) for the Win16 samples. Set `WATCOM` to the installation root containing `binl64/` or `binl/`, `h/` and `lib286/`.

```bash
git clone https://github.com/astrobleem/elks-w16.git
cd elks-w16
export W16DIR="$PWD"

# Choose a sibling directory for the display-server source.
git clone https://github.com/ghaerr/microwindows.git ../microwindows
export MWDIR="$(cd ../microwindows && pwd)"
git -C "$MWDIR" checkout ac1063aec73d341db04d5f08aa8eb601215a69bc
```

No Windows SDK, DDK, ROM image or Windows installation media is needed to build the samples. To run WINVER, CLOCK, REVERSI or SOLITAIRE, supply your own **Windows 3.0** copies; these are not downloaded or included by the build.

## 2. Prepare ELKS

Choose the kernel for the machine. Ordinary CGA uses upstream ELKS without source patches; Tandy/PCjr framebuffer reservations are separate historical fork configurations. Do not mix their kernel objects.

### CGA on stock ELKS

```bash
git clone https://github.com/ghaerr/elks.git ../elks-stock
git -C ../elks-stock checkout v0.9.2
export TOPDIR="$(cd ../elks-stock && pwd)"
```

Follow that tree's `BUILD.md` to build the ia16 toolchain, libc and system. Enable **`CONFIG_EXEC_OS2=y` and `CONFIG_EXEC_MMODEL=y`** for the native W16 NE executable. For the tested local keyboard route, use **`CONFIG_CONSOLE_DIRECT=y` and `CONFIG_KEYBOARD_SCANCODE=y`** on an IBM-PC target. No Tandy/PCjr reservation, UART, EMS or graphics-keyboard kernel patch is needed for this CGA route. Build and boot the same configuration whose headers are used below.

`ELKS_KBD=scancode` selects the user-space US set-1 decoder through stock raw-console ioctls. BIOS-polling and serial input use the existing `tty` backend; polling discards some Alt distinctions before user space can recover them. International layouts and physical VGA with the new raw backend are unverified.

The ordinary VGA user-space build is experimental. Earlier VGA emulator results do not prove the current stock-scancode combination: the VGA driver's raw-keyboard console-lock lifecycle has not been integrated. Do not apply a kernel patch merely to infer support for that combination.

### Tandy fork configuration

```bash
git clone --branch tandy-video-v0.9.2 --single-branch \
  https://github.com/astrobleem/elks.git ../elks-tandy
export TOPDIR="$(cd ../elks-tandy && pwd)"
patch -d "$TOPDIR" -p1 < "$W16DIR/elks-graphics-keys.patch"
```

Follow [`BUILD.md` on the Tandy branch](https://github.com/astrobleem/elks/blob/tandy-video-v0.9.2/BUILD.md) to build the ia16 toolchain, configure ELKS and build the system. Enable **`CONFIG_EXEC_OS2=y` and `CONFIG_EXEC_MMODEL=y`**: the current `w16` output is native NE with far code.

For a fresh tree, the normal ELKS sequence is:

```bash
cd "$TOPDIR"
mkdir -p cross
tools/build.sh
. ./env.sh
make menuconfig
make all
cd "$W16DIR"
```

The branch supplies the kernel-side video-memory reservation. On the **ELKS boot disk**, add `tandyvideo=1` to `/bootopts` to opt in. The driver checks that the kernel reserved a supported framebuffer before using it. This branch targets the original-Tandy memory layout, not every machine with a Tandy label. Read its [`Documentation/tandy-video.txt`](https://github.com/astrobleem/elks/blob/tandy-video-v0.9.2/Documentation/tandy-video.txt) before choosing a machine configuration.

### Expanded PCjr fork configuration

```bash
git clone --branch pcjr-video-v0.9.2 --single-branch \
  https://github.com/astrobleem/elks.git ../elks-pcjr
export TOPDIR="$(cd ../elks-pcjr && pwd)"
patch -d "$TOPDIR" -p1 < "$W16DIR/elks-graphics-keys.patch"
patch -d "$TOPDIR" -p1 < "$W16DIR/elks-pcjr.patch"
```

This target requires **640 KB** and the branch's `pcjr-640.config`. Stock 128 KB PCjr systems are unsupported. After building the toolchain using the branch's `BUILD.md`, configure and build from the clean PCjr tree:

```bash
cd "$TOPDIR"
. ./env.sh
cp pcjr-640.config .config
make kconfig
make include/autoconf.h
make all
cd "$W16DIR"
```

The PCjr kernel **and its matching MINIX bootloader must be installed together**. The load and relocation addresses differ from the default build; replacing only the kernel on a normal ELKS disk is not enough. Read the branch's PCjr documentation before constructing an image. PCjr support is selected by `CONFIG_HW_PCJR`; it is **not** enabled by the Tandy `tandyvideo=1` option.

`elks-pcjr.patch` makes reserved low memory available after boot staging and corrects the BIOS timer accounting. The prior full-suite PCjr runs need that additional memory. See [patch details](technical.md#patch-internals-and-emulator-measurements).

### Historical fork keyboard patch

The fork recipes above use `elks-graphics-keys.patch`; it is not required by the stock-scancode CGA route. It lets Alt+F4 and Alt+Space reach graphical applications, and fixes the relevant BIOS-keyboard sequences for the PCjr. It changes graphics-mode handling while retaining text-mode console switching. Without it, mouse-based closing is still available, but some keyboard shortcuts do not work. Rebuild the kernel after applying it.

## 3. Build Nano-X and w16

Apply these **user-space** baseline patches once, in order, to the pinned Microwindows revision from step 1. Their Tandy/PCjr filename does not require a kernel reservation when building CGA or VGA.

```bash
patch -d "$MWDIR" -p1 < "$W16DIR/dependencies/microwindows-tandy-pcjr.patch"
patch -d "$MWDIR" -p1 < "$W16DIR/nanox-w16.patch"
patch -d "$MWDIR" -p1 < "$W16DIR/nanox-tandyfast.patch"
cd "$W16DIR"
. "$TOPDIR/env.sh"
```

### CGA build

```bash
export CGA_OUT="$W16DIR/build/nanox-cga"
ELKS_KBD=scancode sh scripts/build-nanox-cga.sh
make -C src clean
make TOPDIR="$TOPDIR" MWDIR="$CGA_OUT"
export NANOX_SERVER="$CGA_OUT/nano-x"
```

The default mode is 640×200 monochrome. Set `CGA_MODE=color` on the helper command for 320×200 four colours; keep its output directory and matching W16 client separate. Panning is opt-in, requires a shadow framebuffer and has additional conventional-memory and performance costs. Do not enable it for a first test.

The default build produces **`src/w16`**, a native ELKS NE executable with three code groups, private task data, a shared read-only bank and lazy UI storage. `src/w16-ro.ne` is the same build artifact under its internal name. The old two-code-group a.out runtime exceeds its segment limits and is no longer a supported target. The kernel needs `CONFIG_EXEC_OS2=y` and `CONFIG_EXEC_MMODEL=y`; this is distinct from the Windows NE program supplied as input to W16.

Copy the matching server and runtime as described below. Run the open DLGTEST first and press Escape once; an owned nonpersistent server is awaited before returning to the shell. The wait is bounded at 30 seconds and reports failure if the server does not exit. An already-running server or one started with `NXPERSIST=1` is not awaited.

The CGA path has interpreted 4.77 MHz 8088/IBM CGA coverage, including stock Program Manager opening Games and launching/closing Solitaire on a lean 640 KiB stock-scancode setup. The separate physical Tandy result is limited to DLGTEST Escape-to-shell; full hardware Program Manager is unverified.

### Experimental VGA Solitaire

```bash
export VGA_OUT="$W16DIR/build/nanox-vga"
ELKS_KBD=tty sh scripts/build-nanox-vga.sh
make -C src clean
make TOPDIR="$TOPDIR" MWDIR="$VGA_OUT"
export NANOX_SERVER="$VGA_OUT/nano-x"
```

This builds the 640×480, 16-colour server, matching medium-model client and native NE runtime from the current source. The helper also exports the font source needed to build W16. **Do not infer stock-scancode VGA support from this build recipe.** The existing VGA driver's console-lock handling is not ready for the raw keyboard backend. The `tty` backend retains its existing input limitations on unpatched polling/scancode kernels.

Earlier QEMU VGA runs exercised stock Solitaire dealing, basic moves and Options, and Paintbrush drawing. Those results used their recorded emulator/kernel setups, not a verification of this exact stock-scancode configuration. VGA hardware, all game commands and a full game remain unverified. Supply your own Windows 3.0 files; no proprietary files are included. Historical permissive runs used `-u`, which binds missing imports to diagnostic traps, not implementations.

### Tandy/PCjr user-space build

For the fork kernel configurations in step 2:

```bash
sh scripts/build-nanox.sh
make -C src clean
make TOPDIR="$TOPDIR" MWDIR="$MWDIR"
export NANOX_SERVER="$MWDIR/src/bin/nano-x"
```

The helper also builds the small-model client archive and target display utilities. Keep small- and medium-model archives separate. Kernel reservations and the target-specific bootloader requirements belong to these fork configurations, not ordinary CGA/VGA.

All helpers keep each server paired with the client archive used by W16. Clean `src` when switching display builds. They build user space only and do not install or patch the guest kernel.

## 4. Build the sample applications

```bash
cd "$W16DIR"
export WATCOM=/path/to/open-watcom
make samples
```

This builds all three source examples:

- `test/hello16/HELLO16.EXE`
- `test/dlgtest/DLGTEST.EXE`
- `test/mbtest/MBTEST.EXE`

You can build one example directly with its `build.sh`, for example:

```bash
WATCOM=/path/to/open-watcom sh test/hello16/build.sh
```

The examples use 8086 instructions and the Win16 medium memory model. DLGTEST also uses Python 3 to generate its icon and named-resource table.

## 5. Put the files on an ELKS disk

Use a copy of a bootable disk image for the chosen target. The repository does not distribute a ready-made OS image or a physical-disk installer.

Copy the built files to the guest, using your usual ELKS file-transfer or image tools:

| Host file | Suggested ELKS path |
| :--- | :--- |
| `$NANOX_SERVER` from the chosen build | `/bin/nano-x` |
| `$W16DIR/src/w16` (native NE) | `/bin/w16` |
| `$W16DIR/test/hello16/HELLO16.EXE` | `/root/hello16.exe` |
| `$W16DIR/test/dlgtest/DLGTEST.EXE` | `/root/dlgtest.exe` |
| `$W16DIR/test/mbtest/MBTEST.EXE` | `/root/mbtest.exe` |

Let W16 autostart `/bin/nano-x` for the normal close-to-shell path. `NXSERVER` can select another exact server path. `NXPERSIST=1` deliberately keeps an autostarted server alive; it is not suitable for an Escape-to-text-console smoke. Historical Tandy automation installs the server as `nano-color` instead.

For a **MINIX filesystem image** already compatible with the selected kernel, ELKS's `mfs` utility can copy files into the image. Work on an unmounted image copy, never the active emulator disk. For example:

```bash
# Set this to your own working copy of a compatible image.
export IMAGE=/path/to/working-copy.img
mfs "$IMAGE" cp "$NANOX_SERVER" /bin/nano-x
mfs "$IMAGE" cp "$W16DIR/src/w16" /bin/w16
mfs "$IMAGE" cp "$W16DIR/test/hello16/HELLO16.EXE" /root/hello16.exe
mfs "$IMAGE" cp "$W16DIR/test/dlgtest/DLGTEST.EXE" /root/dlgtest.exe
mfs "$IMAGE" cp "$W16DIR/test/mbtest/MBTEST.EXE" /root/mbtest.exe
```

This copies applications only. It does **not** configure the kernel, bootloader, boot options or emulator devices for you.

### Optional Tandy regression image

[`tests/mkimage.sh`](../tests/mkimage.sh) constructs a 20 MB, MBR-wrapped MINIX image with boot-time automation. It is a development helper for the Tandy workflow, not a general PCjr installer. It needs the built ELKS system, Nano-X, all three sample programs and `w16`.

```bash
cd "$W16DIR"
ELKS_ROOT="$TOPDIR" MWDIR="$MWDIR" \
  bash tests/mkimage.sh w16-demo skipwinver skipclock skipreversi
```

It produces `w16-demo.img` and `w16-demo-mbr.img` and refuses to overwrite existing outputs. Build and output paths must not contain whitespace. The skip flags leave out the separately supplied Microsoft applications; to include your own files, pass whitespace-free paths in `W16_EXTRA`. See the script for flags and image geometry.

The image configuration, staging directory and intermediate image are isolated in a temporary directory. Upstream image generation can still rebuild the ELKS kernel and bootblocks, so use a disposable, fully built ELKS checkout and keep working boot media separate.

## 6. Run on ELKS

Boot the matching kernel and disk in the configured emulator. From the guest shell:

```sh
cd /root
nano-color -p &
w16 hello16.exe
w16 dlgtest.exe
w16 mbtest.exe
```

- `-p` keeps Nano-X alive between applications. Without it, the server exits when the client closes.
- Run **one Win16 application at a time**.
- HELLO16 closes with Esc or Q; its exit code is the number of keys typed.
- DLGTEST returns 1 for OK/Enter and 2 for Cancel/Esc. Those are application results, not loader errors.
- Use the system-menu Close command, or Alt+F4 with the keyboard patch, to close an application. Alt+Space opens the system menu.

With your own Windows 3.0 copies in `/root`:

```sh
w16 winver.exe
W16INI=/root/win.ini w16 clock.exe
w16 reversi.exe
```

Clock's Settings menu switches display mode and opens About. Reversi supports mouse play, Game → Hint, Backspace, Skill selection and About. Other Help commands display a message because there is no Help viewer.

## Host-side checks

The host build checks loading and resources using simulated memory. **It does not run the application's 16-bit instructions or validate an emulator display.**

```bash
cd "$W16DIR"
WATCOM=/path/to/open-watcom make test
```

Or build the samples once, then rerun just the loader suite:

```bash
make host
bash tests/hosttest.sh
```

The default suite covers NE loading and relocation, malformed files, allocation failures, resources, dialogs, menus and WIN.INI access, with AddressSanitizer/UndefinedBehaviorSanitizer mutation checks. Optional sections are skipped unless you provide their dependency trees:

```bash
ELKS_TOPDIR="$TOPDIR" MWDIR="$MWDIR" bash tests/hosttest.sh
```

Use a patched ELKS tree for the keyboard and headless-kernel checks. With `MWDIR`, the suite also checks menu-key handling against Nano-X headers. Without those variables, a successful result can still contain skipped sections; read the output rather than treating it as a full graphics-system pass. `W16_PRIVATE=/path/to/your/windows30` adds resource checks for your own uppercase `WINVER.EXE`, `CLOCK.EXE` and `REVERSI.EXE` files.

## If something fails

- **`ia16-elf-gcc` not found:** build the ELKS toolchain and source its `env.sh` from Bash. Check that `$TOPDIR/cross/bin` is on `PATH`.
- **Missing `libnano-X-medium.a`:** apply all three display patches and run `scripts/build-nanox.sh` before building `w16`.
- **Patch failure:** confirm the correct ELKS branch or Microwindows revision and check whether the patch is already applied. Do not blindly reapply it or accept partial hunks.
- **No colour display or refused framebuffer:** check the target kernel, memory layout and video reservation. The Tandy boot option and PCjr build selection are different mechanisms.
- **Alt shortcuts missing:** rebuild both the patched ELKS kernel and the patched Nano-X server.
- **Out of memory:** the application and graphics stack share conventional memory. On PCjr, the 640 KB configuration and `elks-pcjr.patch` are essential for the documented suite.
- **Unsupported import or executable:** compatibility is limited to the implemented API and real-mode NE format. Try `w16 -n program.exe` to check loading, or inspect it with `tools/w16census.py` on the host.

Use `w16 -v program.exe` for a log or `w16 -t program.exe` for API/INT 21h tracing. The [runtime reference](technical.md#runtime-reference) documents every option and loader exit code.
