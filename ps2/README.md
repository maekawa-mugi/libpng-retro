# PS2 Emotion Engine MMI backend

## EE MMI winner-first display (single screen)

The **`auto-fastest.elf` scoreboard shows all 71 filter/bpp contests on
one 80x25 PS2 screen** (four columns by eighteen rows). Each cell contains
only a group number, winning plan ID (or `S` for scalar), `WIN`, and
speed ratio, such as `01:57 WIN 43.3x` or `09:S WIN 1.5x`.
`-- N/A` means the timer cannot establish a valid win, and `= TIE`
means equal recorded net time. These are **1024-byte aligned** contests,
not global recommendations for all image sizes.

Groups update **live** as each benchmark reaches the 1024-byte shape.
The bottom line changes to **`DONE: PASS`** or **`DONE: FAIL`** when all
correctness checks and benchmarks finish. There is no rotating page
carousel, no `AOBO` correctness wall, and no artificial halt message.
The correctness checks still run; only the screen presentation changed.

`GROUP_MAP,<group>,<filter>,<bpp>` and
`PLAN_MAP,<id>,<group>,<filter>,<bpp>,<name>` preserve the
full ID-to-implementation mapping in stdout. The original
`RESULT`, `BENCH`, `FASTEST`, and `AUTO_WIN` CSV lines also retain
all exact timings and names. All debug output goes to stdout, not
the coordinate-based framebuffer screen. Each 18-character scoreboard
cell stops before the final text columns to avoid line-wrap/white-box
artifacts. `test_compact_layout_host` checks the 72-cell geometry.

## Integrated PR #1–#7 on the mmi branch

The `mmi` branch combines all seven MMI PRs and the local per-item A/B
result improvements in one commit based on `libpng18`. Build the integrated
93-candidate automatic-selection harness with `bash ps2/build-pcsx2.sh`;
the output is `build-ps2-mmi/auto-fastest.elf`. The historical build and
verification notes below describe the earlier 88-candidate integration.
The combined sources pass 22 host regression executables, two portable
syntax configurations and eight Python log-parser tests on Windows.

Run `bash ps2/build-all-pr-msys.sh` from the repository root. It uses
`/usr/local/ps2dev` and its `ps2sdk` subdirectory by default; existing
PS2DEV/PS2SDK values override these paths. GCC 15.2.0 successfully built:

- `build-ps2-mmi/all-pr/all-in-one.elf`: every experimental option, the
  unified sampled correctness checks and the 88-variant benchmark lab. Diagnostics
  go to both the PS2 screen and stdout; `TEST: OK! code=0` appears only after
  the fused sweep finishes. The final screen remains visible.
  The screen follows `openssl-retro/test/ps2/main.c`: each candidate has
  its own A (generic C) and B (MMI candidate) columns, changing from white
  `WAIT` to yellow `RUNNING` to green `O (N ms)` or red `X (N ms)`.
  A validates the scalar reference's bounds and previous-row preservation;
  B validates output equivalence and previous-row preservation. A reference
  validation failure also invalidates the corresponding B comparison.
  Timing is this candidate's matched workload, copy overhead subtracted,
  not the accumulated time across all 88 candidates or whole-PNG decoding.
  Representative autotuning shapes use three measurements per batch;
  other shapes use one. Zero timing does not change test status.
  The current winner-first screen has 71 contests in one fixed table,
  with only winning plan numbers, Scalar wins and ratios. No page
  rotation or A/B candidate list. The full per-item status/timing
  remains in stdout CSV. The final footer is DONE: PASS/FAIL.
  The fused checks continue after a mismatch to finish each item's matrix
  and preserve distinct A/B results. All screen rendering is outside timing.
  CSV and per-item `RESULT` records go to stdout without drawing each log row.
- `build-ps2-mmi/all-pr/libpng.a` and `include/`: full PS2 libpng static
  library with all production read-filter options enabled.
- `build-ps2-mmi/all-pr/pngtest.elf`: full libpng read/write test linked
  with the SDK's zlib. Requires PNG files and a working runtime filesystem.

PR #2–#6 form a stack. PR #6 already preserves PR #1's Up 2x and Sub4 4x
loops as separately benchmarkable alternatives; integration retains those
alongside the original kernels. The extra write/color/Adam7 lab kernels
remain outside production libpng dispatch, as documented below.

All host regression programs, portable syntax checks and four Python parser
tests pass with MINGW32 GCC. The all-in-one ELF builds with
`-Wall -Wextra -Werror`; the library emits one unused-original-Up warning
when the Up 2x option replaces it. These new ELFs have not yet been run on
PCSX2 or real hardware. Benchmark timing uses `clock_ticks`, not EE cycles.

To repeat host checks with a Windows Python installation, set the Makefile's
`PYTHON` variable to its MSYS path:
`make -f ps2/Makefile.host test CC=gcc PYTHON=/c/path/to/python.exe`.

### Fast unified runtime

The newest display uses **four columns of numeric winner results on
one screen** (up to 72 unique filter/bpp contests). Candidate-by-candidate
A/B checks still run but no longer crowd the screen. Final **DONE: PASS**
is unambiguous. See the winner-first display section above.


The final EE screen now declares **SCALAR WIN**, **PLAN A WIN**,
**PLAN B WIN** (or a later plan letter), **TIE**, or **N/A** beside each
representative filter. Plan letters identify implementation candidates in
their *individual filter/bpp family*, in fixed benchmark declaration order:

- `PLAN E WIN  up-4x  41.241x` means that Up's plan E beat the scalar
  reference in the same 1024-byte, aligned-row shape. The named source is
  the fastest valid candidate, not a globally selected kernel for all sizes.
- `SCALAR WIN  sub4-...  1.250x` means scalar won by 1.250x against the
  fastest measured plan (whose name remains visible for diagnosis).
- `N/A` means the timer resolution or correctness checks did not support
  a winner. **One valid plan is enough** to compare against scalar; requiring
  two MMI plans would wrongly hide single-plan results.

The final `WIN COUNT` counts only the representative rows shown on screen.
`AUTO_WIN,...` CSV logs the result for *every* valid 64/1024/4096-byte,
alignment-0/1 group with source name, stable plan letter, net timing, and
ratio. `AUTO_TOTAL,...` sums representative-screen verdicts. The existing
`FASTEST,...` lines still rank only MMI candidates, while the old A/B totals
sum *all* matched measurements and are **not** win counts or a production
dispatch recommendation. Repeated EE measurements are needed to distinguish
small timing differences from noise.

The default ELF validates the final scalar/candidate outputs from the timed
batches directly, including output guards and previous-row preservation.
All 88 candidates remain available. It checks 18 lengths (short rows and
vector boundaries from 1 to 31, then 32/64/128/256/1024/4096/16384) and seven
alignments, skipping ineligible direct kernels. Failures are retained per item
and per side while the remaining checks continue.
The normal build skips the separate exhaustive suites; these sampled runtime
checks do not replace exhaustive host coverage, zero-length cases, disjoint
transforms or all seven Adam7 passes in those suites.

Short rows use one iteration; other rows use 2–64 iterations and one timer
measurement per batch. This reduces runtime, with noisier per-case estimates
than the original repeated benchmark. No EE speedup or wall-time reduction
has yet been measured on hardware/PCSX2.

For the original exhaustive suites followed by the fused sweep, rebuild:
`PNG_PS2_TEST_EXHAUSTIVE=1 bash ps2/build-all-pr-msys.sh`.
That mode restores 32–512 iterations and three measurements per batch.
The analyzer recognizes `FUSED_PASS` + `BENCH_DONE` as the unified completion
markers and verifies the number of validated rows; it still accepts older logs.

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

## Extended kernel coverage for fewer PS2 test cycles

The extended opt-in **standalone kernel lab** adds the PNG operations not
covered by the initial 16-byte read-filter experiments:

- Forward PNG Sub, Average and Paeth for **every common encoded byte
  stride: 1, 2, 3, 4, 6 and 8**. Each preserves original left bytes
  during backward processing; four consecutive byte residuals are
  subtracted using EE `PSUBB` when the portable host model is disabled.
- Palette index -> **RGB8**, and index -> **RGBA8 with the complete
  256-entry tRNS alpha table**, in-place or disjoint.
- 16-bit PNG byte-swap (aligned 16-byte `PSRLH/PSLLH/POR` kernel
  with scalar alignment/tail), 16->8 high-byte reduction,
  RGB/RGBA red-blue channel swap at **8-bit and 16-bit** depths,
  RGB8->RGBA8, RGBA8->RGB8, and RGBA8->ARGB8.
- PNG packed **1/2/4-bit** palette-index and grayscale sample unpacking
  to 8 bits with exact grayscale scaling, Gray8->RGB8 and
  Gray8->RGBA8 with gray tRNS, plus RGB8 tRNS->RGBA8.
- Adam7 horizontal pass row-scatter for byte-aligned 1/2/3/4/6/8-byte
  pixels and MSB-first packed 1/2/4-bit pixels across **all 7 passes**.
  Vertical pass scheduling remains the responsibility of libpng.

`extra_full_kernels.c` and `extra_color_kernels.c` contain the kernels.
`test_full_kernels.c` and `test_color_kernels.c` run a shared
host-and-EE correctness matrix against independent references. The
one-trip harness requires `PASS`, `EXTRA_PASS`, `FULL_PASS`,
`COLOR_PASS`, and `BENCH_DONE` before the analyzer accepts a log.
Failure output is tagged with `FULL_FAIL`, `COLOR_FAIL`, or
`BENCH_FAIL`. Tests check small/truncated rows, 16 pointer offsets,
output bounds and input preservation, plus wide-row samples.

The main BENCH CSV now includes **88 source-level benchmark variants**
under the full opt-in configuration. The extra kernels use
size-aware input/output buffer handling, so expanding palette/RGB rows
and shrinking 16bit/RGBA rows can be compared on identical test data
without writing beyond the test buffers. Timing units depend on
the selected timer; PS2 Linux defaults to wall-clock microseconds,
not CPU cycles.

**Performance status:** PSUBB on write filters and PSRLH/PSLLH/POR
on aligned 16-bit swaps are genuine experimental EE MMI instructions.
Indexed palette gather, bit-packed input and most Adam7 scatter
operations are table/scalar C candidates (not claimed to be SIMD
operations). All routes are intentionally **outside production
pngwutil.c/pngrtran.c/Adam7 dispatch** until cross-compilation, EE
execution and PNG end-to-end equivalence/benchmarking pass.
The new additions do not modify PNG file parsing, CRC, interlace
pass scheduling or zlib/DEFLATE.

## One-trip EE correctness and benchmark lab (recommended)

The preferred workflow is to **run one all-option ELF**, collect its
console output once and analyze it on the development machine. The new
`ps2/bench_filter_mmi.c` runs *after* the existing exhaustive
correctness harness, validates each benchmark row against an independent
scalar reference, then reports machine-readable `BENCH,` CSV lines.

**Before going to PS2**, run GCC/Clang portable source-level tests,
ASan/UBSan and the log-parser regression:

```sh
make -f ps2/Makefile.host test CC=gcc
make -f ps2/Makefile.host clean
make -f ps2/Makefile.host test CC=clang
```

**On a PS2SDK / bare EE setup**:

```sh
sh ps2/build_filter_variants.sh one
```

Run `ps2/variant-elfs/all-in-one.elf` on EE/PCSX2 and save the console
output (stdout/stderr). That single ELF checks all read filters,
the experimental PNG write Up/Sub4/Avg4/Paeth4 filters and RGBA palette
expansion, then benchmarks every compiled read/write/palette kernel.
To generate an additional baseline and **all 32 combinations** of the five
prefix families plus seven additional strategy combinations, use:

```sh
sh ps2/build_filter_variants.sh all
```

It creates 41 ELFs including the all-in-one and baseline. These extra
builds test interactions and serve as fallbacks; they are not required
for the first complete all-option run.

**On PS2 Linux userspace**, no PS2SDK is needed for the standalone test
program if an R5900/EE-aware Linux compiler is available:

```sh
make -f ps2/Makefile.ee-linux \
    EE_LINUX_CC=/path/to/your/ee-linux-gcc \
    EE_LINUX_CFLAGS="-O2 -Wall -Wextra"
```

Copy and run `ps2/test_filter_mmi.ee-linux` on the PS2 Linux system,
capturing its console output. Keep `PNG_PS2_BENCH_EE_PCCR` **disabled**
for Linux userspace: programming EE performance-counter control registers
may require privileged execution. The Linux build uses `gettimeofday` and records **wall-clock microseconds, not EE cycles**. Bare-metal PS2SDK builds without PCCR use
`clock()` ticks, which may not be implemented by every SDK. If the chosen timer is unimplemented or returns all zeroes, do not
interpret the CSV as a valid speed measurement.

On a *privileged bare-metal EE* environment only, optionally request
PCCR0 processor-cycle counts:

```sh
PNG_PS2_BENCH_USE_PCCR=1 sh ps2/build_filter_variants.sh one
```

This experimental PCCR mode has not been validated with every PS2SDK
toolchain and must not be used in unprivileged Linux applications.

**Process the captured log**:

```sh
python3 ps2/analyze_bench.py ps2-console.txt \
    --csv ps2-bench-raw.csv --top 30
```

The analysis rejects missing/failed correctness and incomplete benchmark
logs. It groups candidates by the same filter, bpp, row length and
alignment. The one-ELF sweep includes 32, 64, 128, 256, 1024, 4096 and
16384-byte rows and seven alignments. Per-case measurements include the
scalar baseline, candidate time and copy-only cost; batches use the best
of three repetitions to reduce scheduling noise. Copy subtraction can
produce zero or noisy times on short rows, so compare repeated runs and
end-to-end PNG loading too.

**Up/Sub4 unrolled candidates from the separate optimization PR are
included here as opt-ins.** Define `PNG_PS2_EE_MMI_UP_2X` to use the
two-vector Up loop, or `PNG_PS2_EE_MMI_SUB4_UNROLL4` for a four-pixel
Sub4 word loop when the Sub4 prefix candidate did not handle that row.
The all-in-one benchmark directly times the unrolled functions and
original packed Sub1/Sub2/Sub3/Sub4/Sub6/Sub8 and Average4 baselines
via a separate benchmark-only source snapshot. No default is switched
without actual EE speed and correctness evidence.

**Additional independent kernels**: `extra_kernels_mmi.c` contains
write-side Up, Sub4, Avg4 and Paeth4, plus table-based indexed-palette
expansion to RGBA8 (separate-buffer and in-place). The standalone
`test_extra_kernels.c` validates them on host and EE. These kernels
are **not yet installed into production libpng's pngwutil.c/pngrtran.c
dispatch**, so library-level end-to-end integration is still a separate
step. The same applies to hardware proof for every experimental kernel.

**Note:** `test_filter_mmi.elf` with no `PNG_PS2_BENCH_ENABLE` remains
a short, correctness-only harness, as before.

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

## Experimental RGB8 Sub3 128-bit prefix scan

Define `PNG_PS2_EE_MMI_SUB3_PREFIX` to opt into a 16-byte Sub3 reverse
filter. The code reconstructs each aligned vector using QFSRV byte shifts
of 3, 6 and 12 and PADDB, injecting the preceding three decoded bytes.
It saves/restores SA, inserts separation between SA-dependent instructions,
and uses scalar processing until 16-byte alignment and for the row tail.
Rows shorter than 64 bytes continue using the existing packed implementation.
The default Sub3 backend is unchanged.

Host tests run the actual alignment and dispatch logic with a portable
16-lane scan, comparing against an independent scalar reference for all
lengths 0..1024 and all 16 row alignments. This does **not** execute the
R5900 instructions or establish an EE speedup. Run the full EE filter
harness with:

```sh
make -C ps2 clean
make -C ps2 EE_OPTFLAGS="-O2 -DPNG_PS2_EE_MMI_SUB3_PREFIX"
```

Benchmark the opt-in path against the default packed RGB8 Sub3 kernel
using real PNGs and cycles per decoded byte. MTSAB/QFSRV scheduling,
SA state restoration, and cache behavior require EE hardware validation.

## Grayscale Sub1/Sub2 16-byte prefix alternatives

The default Sub1/Sub2 decoders continue to use the packed four-byte scan.
Define `PNG_PS2_EE_MMI_GRAY_PREFIX16` to test a full 16-byte decoder:
Sub1 uses QFSRV shifts by 1, 2, 4, and 8 bytes, and Sub2 uses shifts
by 2, 4, and 8 bytes. Both inject the previous decoded one/two bytes,
save and restore SA, process a scalar alignment prefix, and handle the
remaining bytes without out-of-range LQ/SQ. The opt-in path is attempted
only for rows of at least 64 bytes.

The portable variant uses the production function's alignment and tail
logic with a C simulation of the 128-bit scan. The full host target now
runs both the original and 16-byte grayscale variants, just as it does
for the RGB8 Sub3 candidate. Neither host execution nor syntax checks
verify the assembly or its performance on R5900.

## RGBA16 Sub8: three implementations to compare

Sub8 handles RGBA16, where each pixel consists of eight encoded bytes.
The default kernel still uses four-byte packed additions, splitting each
pixel into two halves. Two new variants can be selected independently:

- `PNG_PS2_EE_MMI_SUB8_WORDS`: for word-aligned rows with a complete
  number of eight-byte pixels and at least 32 bytes, load two 32-bit words
  per pixel directly and process two independent PADDB chains. All other
  rows use the unchanged packed fallback.
- `PNG_PS2_EE_MMI_SUB8_PREFIX16`: for rows of at least 64 bytes, align
  the current position, load a 16-byte raw vector, inject the previous
  decoded eight bytes, and perform one QFSRV(8) + PADDB prefix step.
  It saves/restores SA and finishes partial vector tails bytewise.
  No unaligned LQ, SQ, or LD is executed.

If both options are enabled, PREFIX16 is attempted first. WORDS handles
rows for which PREFIX16 declines, with the packed C/MMI implementation
as the final fallback. This is intentional for multi-variant testing.

The host regression suite now compiles the same production Sub8 source
under **three binaries** (packed, WORDS, PREFIX16), each checked with
the existing independent scalar Sub/Average/Paeth references and
post-row canaries. The prefix and word paths are experimental; a
source-level regression is not a real R5900 correctness or speed result.

## Multi-variant EE correctness matrix

Build **18 separate EE harness ELFs** with
`sh ps2/build_filter_variants.sh`: all sixteen combinations of
four prefix switches (Sub1/2, Sub3, Sub4 and Sub8), the separate
Sub8 two-word candidate, and the stress combination enabling all
prefixes, WORDS, Average and Paeth.

The output `ps2/variant-elfs/` must be executed on real EE hardware or
PCSX2, recording each variant's PASS count. Benchmark each filter
individually in cycles/byte and compare whole-PNG decoding times.
The 18 ELFs provide a correctness matrix, **not** a performance
measurement: do not rank variants by host model throughput.

On the host, `make -f ps2/Makefile.host test` runs the original and
both portable-prefix sources for RGB8 and grayscale through the same
0..1024-byte length and 16-alignment regressions. CI runs GCC, Clang,
and sanitizer configurations for the same suite.

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

The host syntax target now parses both the default MMI-only source
configuration and the all-opt-in source configuration. The GCC-only
`syntax-ee-gcc` target parses the actual inline-assembly operands in
both configurations without assembling R5900 instructions.

## One-boot dispatch and production palette-hook validation

The single `auto-fastest.elf` additionally executes **`DISPATCH_PASS`**
and **`PALETTE_HOOK_PASS`** after its already-integrated kernel
correctness/benchmark sweep. Both are intentionally **small,
nonduplicating integration checks**:

- The measured winner table is used to select an actual function pointer
  for each valid **64, 1024, or 4096 byte** row with precisely measured
  current-row/previous-row alignment combinations **(0/0 and 1/7)**.
  Only a validated plan that beat scalar can be selected; unknown,
  invalid or unmeasured shapes fall back to the scalar reference.
  `DISPATCH_PASS` means those selected callbacks, not merely candidate
  names, produced exactly the scalar result with intact previous rows and
  guard bytes. No additional full candidate search is performed.
  This is a **training-and-verification lab policy**, not automatically
  installed into production libpng and not a basis for interpolating other
  row widths or pointer alignments.
- The production `pngsimd.c` palette expansion hook can now be built
  explicitly with `PNG_PS2_EE_MMI_PALETTE` (when target-specific PNG
  read expansion is configured). It handles **8-bit palette rows**,
  in-place RGB8 without tRNS and RGBA8 with tRNS, defaults alpha to
  255 for indices outside the tRNS table, and updates every relevant
  `png_row_info` field. Non-8bit indexed rows return unhandled and
  are expanded by libpng's existing generic transform. The exact worker
  source is shared by the production target and
  `ps2/test_palette_hook.c`, which checks fallback, sentinels,
  metadata, width and alignment. `test_palette_hook_host` runs it
  on PC as well.

The existing write-filter and other color-conversion candidate tests
remain in the same ELF. Unlike palette expansion, production PNG writing
has no equivalent simple target-specific expansion hook; these kernels
are still **experimental**, not wired into `pngwutil.c`. A real PNG
encode/decode round-trip and hardware measurements remain necessary
before any write-kernel production registration.

The final screen now also reports
`DISPATCH: N passed, 0 failed (exact shapes)` and
`PALETTE: 8-bit RGB/RGBA hook OK`.
Neither implies that the optimal runtime dispatch is proven for
arbitrary image sizes or that PNG-wide throughput improved.

## Production dispatch after PCSX2 comparison

The standalone `auto-fastest.elf` benchmarks all Up/Paeth candidates, but
its winner list does **not** automatically rewrite the libpng production
dispatch. On the matched 1024-byte, aligned-row PCSX2 sample, `up-4x`
won and scalar Paeth4 beat the experimental packed Paeth4. The production
`ps2/ee_init.c` backend now supports **explicit opt-in**
`PNG_PS2_EE_MMI_UP_4X`, with priority over `PNG_PS2_EE_MMI_UP_2X`.
The 4x kernel's alignment and short-row fallback are unchanged. Example
for a libpng build that already enables the PS2 EE target:

```sh
# Add these as target-specific C compiler flags:
-DPNG_PS2_EE_MMI_UP_4X
```

When `PNG_PS2_EE_MMI_PAETH` is enabled, production keeps libpng's
existing **scalar Paeth4** dispatch by default, while all other bpp
Paeth candidates remain available under that flag. The previous
Paeth4 kernel is still in the standalone ELF's A/B sweep. Define
`PNG_PS2_EE_MMI_PAETH4_FORCE` alongside
`PNG_PS2_EE_MMI_PAETH` only to explicitly force the experimental
Paeth4 backend for further research.

Representative benchmarks now use **paired A/B timing**, alternate
scalar-vs-candidate execution order, and select the **median of up to
three copy-adjusted repetitions** rather than the single lowest time
of separately gathered copy/scalar/candidate samples. Nonrepresentative
widths retain one measured correctness batch. This limits lucky
zero-duration measurements and warmed-cache bias, but the raw `clock()`
timer may still be coarse; cycle-counter validation and end-to-end
PNG measurements remain necessary before declaring universal speedups.

## One-run automatic fastest-kernel selection (all PRs integrated)

**Build once, boot once, no runtime arguments:** after configuring PS2SDK,
run `bash ps2/build-pcsx2.sh` and boot `build-ps2-mmi/auto-fastest.elf` in
PCSX2 or on real EE hardware. For the PS2SDK Makefile, use
`sh ps2/build_filter_variants.sh one` and boot `ps2/variant-elfs/all-in-one.elf`.

The default standalone ELF is **not** the old correctness-only `all` ELF:
`PNG_PS2_BENCH_ENABLE` and all experimental comparison kernels are compiled
into it. The run checks the very same output buffers used by its timing
batches; it does **not** repeat the separate exhaustive filter loops.
A full 0..1024, all-16-alignment verification remains available by setting
`PNG_PS2_TEST_EXHAUSTIVE=1` at **build** time when the additional runtime
is acceptable. The default quick run checks 18 lengths and 7 alignments,
including short/truncated rows, and all kernels included in the sweep.

At completion, the ELF emits `FASTEST,...` entries comparing the **same
filter, bpp, rowbytes and pointer alignment** (64, 1024 and 4096 bytes;
aligned and 1-byte offset); `AUTO,...` is the on-screen readable summary
for the 1024-byte aligned cases. A missing/low-resolution timer shows N/A,
not a fictional speedup. Three paired, order-alternated repetitions are used for these representative
widths, with the median net cost reported; corner-case lengths retain one
correctness-and-timing batch.

The extra `Up` alternatives are original 1x, original 2x, interleaved 2x
loads, prefetch 1x, prefetch 2x, 4x, and prefetch 4x. These alternatives
address the EE Core User Manual's load-use interlocks, independently
schedulable loads and 128-bit SIMD pipeline. Since prefetch may worsen
cache behavior, **only measured correctness-passing implementations**
enter the winner table; it never claims a winner across unrelated filters.

The log parser `python3 ps2/analyze_bench.py capture.txt --csv result.csv`
rejects `RESULT,A,X`, `RESULT,B,X`, `A: X`, `B: X`, missing `BENCH_DONE`, and
incomplete FASTEST reports. No speed result is authoritative until the
R5900 build and execution pass: host GCC/Clang tests are portable models,
not EE assembly validation.
