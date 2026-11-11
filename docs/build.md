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

No Windows SDK, DDK, ROM image or Windows installation media is needed to build the samples. To run WINVER, CLOCK or REVERSI, supply your own **Windows 3.0** copies; these are not downloaded or included by the build.

## 2. Prepare ELKS

Use **one** of the target branches below. Keep Tandy and PCjr builds in separate source trees, and do not reuse kernel objects between them.

### Tandy

```bash
git clone --branch tandy-video-v0.9.2 --single-branch \
  https://github.com/astrobleem/elks.git ../elks-tandy
export TOPDIR="$(cd ../elks-tandy && pwd)"
patch -d "$TOPDIR" -p1 < "$W16DIR/elks-graphics-keys.patch"
```

Follow [`BUILD.md` on the Tandy branch](https://github.com/astrobleem/elks/blob/tandy-video-v0.9.2/BUILD.md) to build the ia16 toolchain, configure ELKS and build the system. Enable **`CONFIG_EXEC_MMODEL=y`**: `w16` uses the medium memory model.

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

### Expanded PCjr

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

### What the keyboard patch adds

`elks-graphics-keys.patch` lets Alt+F4 and Alt+Space reach graphical applications, and fixes the relevant BIOS-keyboard sequences for the PCjr. It changes graphics-mode handling while retaining text-mode console switching. Without it, mouse-based closing is still available, but some keyboard shortcuts do not work. Rebuild the kernel after applying it.

## 3. Build Nano-X and w16

Apply the **three Microwindows patches in this order**, once each, to the revision from step 1:

```bash
patch -d "$MWDIR" -p1 < "$W16DIR/dependencies/microwindows-tandy-pcjr.patch"
patch -d "$MWDIR" -p1 < "$W16DIR/nanox-w16.patch"
patch -d "$MWDIR" -p1 < "$W16DIR/nanox-tandyfast.patch"

cd "$W16DIR"
. "$TOPDIR/env.sh"
sh scripts/build-nanox.sh
make -C src TOPDIR="$TOPDIR" MWDIR="$MWDIR"
```

The [build helper](../scripts/build-nanox.sh) assumes the source trees are already present and patched. It does not download dependencies or install files into an ELKS disk.

It builds:

| Output | Purpose |
| :--- | :--- |
| `$MWDIR/src/bin/nano-x` | The Tandy/PCjr colour Nano-X server |
| `$MWDIR/src/lib/libnano-X.a` | Small-model library for Nano-X demos and test drivers |
| `$MWDIR/src/lib/libnano-X-medium.a` | Medium-model library linked by `w16` |
| `$MWDIR/nxcolor`, `$MWDIR/vidinfo`, `$MWDIR/vidreset` | Display utilities from the base patch |
| `src/w16` | The native ELKS Win16 loader and API layer |

The two client archives are intentionally separate. **Do not overwrite the small-model archive with medium-model objects.** The helper cleans and rebuilds the Nano-X tree first, then builds the medium-model objects in this repository's `build/nanox-medium/` directory. It also supplies `MEM_GETPCJR=15` when compiling against Tandy headers that lack that ioctl definition.

`nanox-w16.patch` supplies server request batching, keyboard decoding, line-endpoint behaviour and the cursor-redraw correction. `nanox-tandyfast.patch` accelerates the colour driver's line, fill, copy and glyph paths. The base display patch and license notices live in [`dependencies/`](../dependencies).

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
| `$MWDIR/src/bin/nano-x` | `/bin/nano-color` |
| `$MWDIR/vidreset` | `/bin/vidreset` |
| `$W16DIR/src/w16` | `/bin/w16` |
| `$W16DIR/test/hello16/HELLO16.EXE` | `/root/hello16.exe` |
| `$W16DIR/test/dlgtest/DLGTEST.EXE` | `/root/dlgtest.exe` |
| `$W16DIR/test/mbtest/MBTEST.EXE` | `/root/mbtest.exe` |

`nano-color` is simply the installed name for the server built as `nano-x`. Keep its build matched to the client library used for `w16`.

For a **MINIX filesystem image** already compatible with the selected kernel, ELKS's `mfs` utility can copy files into the image. Work on an unmounted image copy, never the active emulator disk. For example:

```bash
# Set this to your own working copy of a compatible image.
export IMAGE=/path/to/working-copy.img
mfs "$IMAGE" cp "$MWDIR/src/bin/nano-x" /bin/nano-color
mfs "$IMAGE" cp "$MWDIR/vidreset" /bin/vidreset
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
