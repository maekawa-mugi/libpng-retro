# PS2 Emotion Engine MMI backend

This directory contains an experimental PNG read-filter backend for the
PlayStation 2 Emotion Engine (R5900).  It uses the **EE 128-bit integer MMI
instructions**, not MIPS MSA and not Loongson's unrelated MMI extension.

## Enable / disable

Compile **all libpng translation units** with an EE-targeted PS2 toolchain.
The backend is selected if `__R5900__` is defined, or if the build defines
`PNG_PS2_EE_MMI`.  If the compiler does not define `__R5900__`, add
`-DPNG_PS2_EE_MMI` to the compiler flags.  Do not enable this flag for
non-EE MIPS targets.

For a generic-C baseline on the same toolchain, add
`-DPNG_PS2_EE_MMI_DISABLE`.  This overrides both automatic detection and
the explicit enable flag.

The libpng configuration must enable `PNG_READ_SUPPORTED` and
`PNG_TARGET_SPECIFIC_CODE_SUPPORTED`; the supplied
`pnglibconf.h.prebuilt` has the latter enabled.  Builds disabling
target-specific code will not include this backend.

Build `pngsimd.c` as usual; **do not separately compile** `ee_init.c`
or `filter_mmi.c`, since `pngsimd.c` includes the backend through
`pngtarget.h`.  The standard libpng public API is unchanged.

## Coverage

| Reverse PNG filter | Backend |
| --- | --- |
| None | No operation |
| Up, all byte-per-pixel sizes | 16-byte EE MMI LQ / PADDB / SQ, scalar tail |
| Sub, 4 bytes per pixel | 4-byte packed EE MMI PADDB; optional 16-byte prefix scan |
| Average, 4 bytes per pixel | Exact packed 4-byte mean + EE MMI PADDB |
| Sub, other byte-per-pixel sizes | Generic libpng C |
| Average for other pixel sizes / Paeth | Generic libpng C |

Define `PNG_PS2_EE_MMI_SUB4_PREFIX` to try an experimental 128-bit
Sub4 prefix scan using QFSRV, PADDB, PEXTLW and PCPYLD. This is opt-in
until it has been tested on real EE hardware and benchmarked. The scan
preserves the EE SA register; the default still uses the simpler Sub4 loop.

The accelerated functions check pointer alignment; nonconforming input is
handled by a bytewise fallback without out-of-range loads.  The libpng
row allocator already supports a 16-byte-aligned pixel start when
`PNG_TARGET_ROW_ALIGNMENT` is 16.  The logic uses modulo-256 byte
addition, **not saturating addition**.

Both functions work on the *actual* row byte length, including shorter
Adam7 passes.  The default Sub4 loop is horizontal and intentionally
uses only the low four byte lanes of PADDB; an optimized full-width
prefix-sum implementation could replace it after profiling.

## Testing on PS2

`test_filter_mmi.c` is a standalone harness that includes
`filter_mmi.c` with minimal compatible type definitions.  Compile it
as an EE program with the PS2SDK toolchain and run it on hardware or in
an emulator.  It exercises lengths 0..1024 and 16 possible input
alignments for Up, Sub4 and Average4, checking against independent scalar
reference functions, checking input preservation and output bounds.

Also test with real PNGs (using libpng's `pngtest` or your application's
image-loading path):

- Noninterlaced RGB8, RGBA8, grayscale8 and 16-bit PNGs
- Adam7 interlace and small dimensions, including one-pixel rows
- Each PNG filter, mixed-filter images, truncated/bad input handling
- Compare output byte-for-byte with an EE generic-C build
- Benchmark decoding time, unfilter time and whole-program time separately

A portable mathematical model of the packed mean and prefix-sum scan was
tested on the development host. This does not verify EE assembler syntax,
pipeline hazards or speed.

The library must still be tested on real EE hardware.  The backend does
not accelerate zlib/DEFLATE, palette expansion, PNG writing, or VU0/VU1.

## Host-side algorithm checks

The deterministic, standalone `test_mmi_models.c` verifies the
byte-exact packed Average4 arithmetic and the 16-byte Sub4 prefix-scan
algorithm against independent scalar references. It does **not** execute
R5900 instructions:

```sh
cc -std=c99 -O2 -Wall -Wextra -Werror ps2/test_mmi_models.c -o test_mmi_models
./test_mmi_models
```

Run `test_filter_mmi.c` under an EE toolchain and on PS2 hardware to
exercise the actual MMI assembly and its timing.


## Build the EE filter harness with PS2SDK

From the repository root, with PS2SDK configured in your environment:

```sh
make -C ps2
```

This produces `ps2/test_filter_mmi.elf`. Execute the ELF on
PS2 hardware or an EE-compatible emulator. To test the experimental
128-bit Sub4 scan, rebuild the harness with:

```sh
make -C ps2 clean
make -C ps2 EE_OPTFLAGS="-O2 -DPNG_PS2_EE_MMI_SUB4_PREFIX"
```

The regular build covers Up, Sub4 and Average4; rebuilding with this
flag also exercises QFSRV and the SA register save/restore sequence.
