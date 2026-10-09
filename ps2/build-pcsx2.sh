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
crt_dir=${PS2EE_CRT_DIR:-"$PS2DEV/ee/mips64r5900el-ps2-elf/lib"}
if [[ ! -f "$crt_dir/crt0.o" && -f "$PS2SDK/../ee/mips64r5900el-ps2-elf/lib/crt0.o" ]]; then
    crt_dir="$PS2SDK/../ee/mips64r5900el-ps2-elf/lib"
fi
[[ -f "$crt_dir/crt0.o" && -f "$PS2SDK/ee/startup/linkfile" ]] || {
    echo 'Missing PS2SDK linkfile or crt0.o' >&2; exit 1;
}
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
name=auto-fastest
"$cc" "${flags[@]}" -c "$source_dir/ps2/test_filter_mmi.c" -o "$output_dir/$name.o"
"$cc" -march=r5900 -G0 "-B$crt_dir/" "-T$PS2SDK/ee/startup/linkfile" \
    "-L$PS2SDK/ee/lib" -Wl,-zmax-page-size=128,--gc-sections \
    "-Wl,-Map,$output_dir/$name.map" "$output_dir/$name.o" \
    -Wl,--start-group -ldebug -lc -lcdvd -lcglue -lpthread -lpthreadglue \
    -lkernel -Wl,--end-group -o "$output_dir/$name.elf"
printf 'Boot %s/%s.elf directly: no arguments required.\n' "$output_dir" "$name"
