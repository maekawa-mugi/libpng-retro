#!/usr/bin/env bash
# Build one ready-to-boot, fully comparable EE ELF. Runtime needs NO options.
# It checks each measured row against the scalar reference in the same run,
# uses the recorded timings to select the fastest *matching* kernel shape,
# and prints AUTO/FASTEST lines automatically on console and stdout.
set -euo pipefail
source_dir=$(cd "$(dirname "$0")/.." && pwd)
: "${PS2DEV:?Set PS2DEV to the PS2 toolchain root}"
: "${PS2SDK:?Set PS2SDK to the PS2SDK root}"
output_dir=${1:-"$source_dir/build-ps2-mmi"}
mkdir -p "$output_dir"
output_dir=$(cd "$output_dir" && pwd)
cc=${CC:-"$PS2DEV/ee/bin/mips64r5900el-ps2-elf-gcc"}
# PS2SDK installations differ: crt0.o may live in the EE GCC
# target library or in the PS2SDK startup directory.  Honour explicit
# overrides first; never silently use an unrelated host toolchain.
linkfile=${PS2EE_LINKFILE:-"$PS2SDK/ee/startup/linkfile"}
crt_dir=${PS2EE_CRT_DIR:-}
if [[ -z "$crt_dir" ]]; then
    for candidate in \
        "$PS2DEV/ee/mips64r5900el-ps2-elf/lib" \
        "$PS2SDK/ee/startup" \
        "$PS2SDK/ee/lib" \
        "$PS2DEV/ee/lib" \
        "$PS2SDK/../ee/mips64r5900el-ps2-elf/lib"; do
        if [[ -f "$candidate/crt0.o" ]]; then
            crt_dir=$candidate
            break
        fi
    done
fi
if [[ ! -f "$linkfile" ]]; then
    printf 'ERROR: PS2SDK linker script not found: %s\n' "$linkfile" >&2
    printf 'PS2SDK=%s\n' "$PS2SDK" >&2
    printf 'Search with: find "$PS2DEV" -type f -name linkfile\n' >&2
    printf 'Then set PS2EE_LINKFILE to the correct path.\n' >&2
    exit 1
fi
if [[ -z "$crt_dir" || ! -f "$crt_dir/crt0.o" ]]; then
    printf 'ERROR: EE startup object crt0.o was not found.\n' >&2
    printf 'PS2DEV=%s PS2SDK=%s\n' "$PS2DEV" "$PS2SDK" >&2
    printf 'Search with: find "$PS2DEV" -type f -name crt0.o\n' >&2
    printf 'Then set PS2EE_CRT_DIR to the directory containing crt0.o.\n' >&2
    exit 1
fi
if [[ ! -x "$cc" ]] && ! command -v "$cc" >/dev/null 2>&1; then
    printf 'ERROR: EE C compiler not found: %s\n' "$cc" >&2
    exit 1
fi
printf 'EE compiler : %s\nEE crt0.o   : %s/crt0.o\nEE linkfile : %s\n' \
    "$cc" "$crt_dir" "$linkfile"
flags=(-O2 -march=r5900 -G0 -D_EE -std=c99 -Wall -Wextra -Werror
       -ffunction-sections -fdata-sections
       "-I$PS2SDK/ee/include" "-I$PS2SDK/common/include"
       -DPNG_PS2_BENCH_ENABLE
       -DPNG_PS2_EE_MMI_SUB3_PREFIX -DPNG_PS2_EE_MMI_GRAY_PREFIX16
       -DPNG_PS2_EE_MMI_SUB4_PREFIX -DPNG_PS2_EE_MMI_SUB6_PREFIX16
       -DPNG_PS2_EE_MMI_SUB8_PREFIX16 -DPNG_PS2_EE_MMI_SUB8_WORDS
       -DPNG_PS2_EE_MMI_UP_2X -DPNG_PS2_EE_MMI_UP_SCHEDULES
       -DPNG_PS2_EE_MMI_SUB4_UNROLL4 -DPNG_PS2_EE_MMI_AVG4_DUAL
       -DPNG_PS2_EE_MMI_GRAY_AVG -DPNG_PS2_EE_MMI_WIDE_AVG
       -DPNG_PS2_EE_MMI_PAETH -DPNG_PS2_EE_MMI_PAETH_MASK)
if [[ ${PNG_PS2_SPR_BENCH:-1} == 1 ]]; then
    flags+=(-DPNG_PS2_SPR_BENCH)
elif [[ ${PNG_PS2_SPR_BENCH:-1} != 0 ]]; then
    echo "PNG_PS2_SPR_BENCH must be 0 or 1" >&2
    exit 2
fi
name=auto-fastest
"$cc" "${flags[@]}" -c "$source_dir/ps2/test_filter_mmi.c" -o "$output_dir/$name.o"
"$cc" -march=r5900 -G0 "-B$crt_dir/" "-T$linkfile" \
    "-L$PS2SDK/ee/lib" -Wl,-zmax-page-size=128,--gc-sections \
    "-Wl,-Map,$output_dir/$name.map" "$output_dir/$name.o" \
    -Wl,--start-group -ldebug -lc -lcdvd -lcglue -lpthread -lpthreadglue \
    -lkernel -Wl,--end-group -o "$output_dir/$name.elf"
printf 'Boot %s/%s.elf directly: no arguments required.\n' "$output_dir" "$name"
