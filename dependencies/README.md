# Display and kernel dependencies

W16's own code is under the [MIT license](../LICENSE). The patches to other
projects retain those projects' licensing; the root license does not replace it.

- **Microwindows / Nano-X:** [upstream source](https://github.com/ghaerr/microwindows/tree/ac1063aec73d341db04d5f08aa8eb601215a69bc),
  MPL 1.1 with the GPLv2 alternative described in [Microwindows-LICENSE](Microwindows-LICENSE).
  Apply `microwindows-tandy-pcjr.patch`, then the root `nanox-w16.patch` and
  `nanox-tandyfast.patch` to that exact upstream commit. These provide the
  Tandy/PCjr display, W16 input/server changes and faster raster drawing.
- **ELKS kernel:** [Tandy branch](https://github.com/astrobleem/elks/tree/tandy-video-v0.9.2)
  or [expanded-PCjr branch](https://github.com/astrobleem/elks/tree/pcjr-video-v0.9.2),
  [GNU GPL version 2](ELKS-COPYING). The root `elks-graphics-keys.patch` and
  `elks-pcjr.patch` modify ELKS kernel code and are distributed under GPLv2.
  Existing source copyright notices remain in the upstream files.

Only source patches are included here, not upstream binaries, firmware, Windows
files or operating-system disk images. See [the build guide](../docs/build.md)
for the complete patch order and configuration.
