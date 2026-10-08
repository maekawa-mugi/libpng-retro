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
| Sub, 3 bytes per pixel | Three packed byte lanes with EE MMI PADDB |
| Sub, 1/2 bytes per pixel | Four-byte prefix scan via EE PADDB, bytewise tails |
| Sub, 6/8 bytes per pixel | Two packed PADDB groups (4+2 or 4+4) |
| Sub, other byte-per-pixel sizes | Generic libpng C |
| Average, 3 bytes per pixel | Exact three-lane packed average and EE MMI PADDB |
| Paeth, 1/2/3/4 bytes per pixel | Optional packed MMI PADDB after exact scalar predictor |
| Average, 1/2 bytes per pixel | Experimental packed ADD, opt-in only |
| Average, 6/8 bytes per pixel | Two packed PADDB groups, opt-in |
| Paeth, 6/8 bytes per pixel | Exact bytewise predictor + packed PADDB, opt-in |
| Average for other pixel sizes / Paeth otherwise | Generic libpng C |

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
alignments for Up, Sub4, Average4, Sub3 and Average3, plus Paeth3/4 if enabled, checking against independent scalar
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

Define `PNG_PS2_EE_MMI_PAETH` to explicitly register experimental Paeth1,
Paeth2, Paeth3 and Paeth4 filters. Predictors use scalar comparisons and respect PNG
Paeth tie priority; the final modulo-256 addition uses EE PADDB. This is
not enabled by default because it may be slower than generic C.

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

## RGB8 Sub3 and Average3 regression tests

The production `filter_rgb3.c` contains the RGB8 read-filter loops and the
EE `PADDB` helper.  The standalone host test compiles **the same filter loops**
with only the single MMI instruction replaced by the equivalent scalar
three-byte-lane operation. It checks byte-exact output, previous-row
preservation, and 16 sentinel bytes beyond each row for 0..1024-byte rows,
all 16 alignments, and repeated pseudorandom inputs:

```sh
make -f ps2/Makefile.host test
```

This checks RGB3 logic and buffer safety on the host, but **does not verify
R5900 instruction encoding, scheduling, or speed**. The RGB3 packed path may
or may not outperform the generic C code; benchmark on EE before calling
it a performance improvement.

## Experimental packed Paeth3/4

The `filter_paeth_mmi.c` backend is registered only with
`-DPNG_PS2_EE_MMI_PAETH`. To test the real EE MMI instructions in the
standalone harness, rebuild with:

```sh
make -C ps2 clean
make -C ps2 EE_OPTFLAGS="-O2 -DPNG_PS2_EE_MMI_PAETH"
```

The host-side `test_paeth_host.c` compiles the same predictor and row
logic with a portable stand-in for the final PADDB instruction. It verifies
Paeth3 and Paeth4 against an independent PNG predictor implementation,
including truncated rows, all 16 alignments and post-row canaries. Both
GCC with ASan/UBSan and Clang pass this test on the development host.

No speedup is claimed for Paeth until EE hardware measurement.

## Sub1/Sub2 and experimental Average1/Average2

`filter_gray_mmi.c` uses EE `PADDB` to reconstruct four Sub1 bytes or
two Sub2 pixels per iteration with a packed-byte prefix scan. This is enabled
by default for PNG rows with one or two bytes per pixel. It never loads or
stores past the row; it handles arbitrary alignment and scalar tails.

Define `PNG_PS2_EE_MMI_GRAY_AVG` to register experimental Average1 and
Average2 filters. Their left-neighbor dependency limits SIMD parallelism,
so the generic C implementations remain the default until benchmarks on EE
prove these alternatives worthwhile.

The `test_gray_host.c` regression compiles the actual grayscale C loops
with only the `PADDB` helper replaced by its exact bytewise equivalent.
It tests Sub1/Sub2/Average1/Average2 against independent scalar references,
including 0..1024 byte rows, all 16 address alignments, and 16-byte
post-row canaries. Run with `make -f ps2/Makefile.host test`.

## Paeth1/Paeth2 and standalone EE harness coverage

The existing packed Paeth implementation now includes bpp=1 and bpp=2
wrappers. They remain under `PNG_PS2_EE_MMI_PAETH` because the exact
scalar predictor may dominate execution time; `PADDB` handles only the
per-pixel modulo-256 addition. The host Paeth test checks 1, 2, 3 and
4-byte pixels. The EE harness exercises seven default filters, two optional
gray Average filters and four optional Paeth filters. Enable both opt-ins:

```sh
make -C ps2 clean
make -C ps2 EE_OPTFLAGS="-O2 -DPNG_PS2_EE_MMI_GRAY_AVG -DPNG_PS2_EE_MMI_PAETH"
```

Use actual hardware or an EE emulator to test the instruction paths.

## Aligned-pair Up SIMD prologue

`filter_up_mmi.c` implements the Up filter with a scalar alignment
prologue before the 16-byte `LQ/PADDB/SQ` loop. Even if both row
pointers initially have nonzero alignment, the vector loop can now run
when the pointers have identical alignment modulo 16. If their
alignments differ, the row uses scalar code to avoid unaligned LQ/SQ.
No partial quadword loads/stores are performed at row edges.

The `test_up_host.c` regression compiles this source with a portable
16-lane PADDB equivalent. It verifies all 256 combinations of row and
previous-row alignment and row lengths 0..1024 with canary checks.
This does not validate R5900 instruction timing or assembly semantics.

## Host C translation-unit checks

The `test_ee_syntax.c` unit includes the MMI-only filter sources and
checks their C declarations and optional feature combinations without
requiring a PS2 cross-toolchain. The normal host test target also compiles
this file with portable equivalents of the `PADDB` helper on GCC/Clang.
Use `make -f ps2/Makefile.host syntax-ee-gcc` for an additional GCC
syntax-only pass over the R5900 inline-assembly strings. Neither mode
validates PS2 instruction assembly or runtime behavior; use PS2SDK and
an EE target for that.

## 16-bit RGB/RGBA: bytewise Sub6/Sub8, Average6/8 and Paeth6/8

`filter_wide_mmi.c` handles 16-bit PNG RGB (six bytes per pixel) and RGBA
(eight bytes per pixel). PNG filters operate independently on the **encoded
bytes**, not 16-bit sample arithmetic. Each pixel is divided into 4+2 or
4+4 byte groups and processed using EE `PADDB`. No unaligned word or
quadword loads are used, so odd alignments and short rows are safe.

Sub6/Sub8 are registered by default. Average6/Average8 require
`PNG_PS2_EE_MMI_WIDE_AVG` and Paeth6/Paeth8 require the existing
`PNG_PS2_EE_MMI_PAETH` opt-in. Both are experimental until cycle-level
benchmarking on real EE hardware.

`test_wide_host.c` compiles the production implementation with a portable
replacement for `PADDB`; it compares each byte against independent
Sub, Average and Paeth scalar references across lengths 0..1024 and
16 row alignments, with input-preservation and out-of-row canaries.

Build and run all host tests:

```sh
make -f ps2/Makefile.host test
```

Exercise the actual EE MMI instruction paths, including every opt-in,
in the standalone PS2SDK harness:

```sh
make -C ps2 clean
make -C ps2 EE_OPTFLAGS="-O2 -DPNG_PS2_EE_MMI_GRAY_AVG -DPNG_PS2_EE_MMI_WIDE_AVG -DPNG_PS2_EE_MMI_PAETH"
```

Cross-compilation and real EE hardware execution are still required
before this backend can be considered hardware-validated.
