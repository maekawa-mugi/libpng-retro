#!/usr/bin/env bash
# Run in MSYS2 MINGW32: bash ps2/build-all-pr-msys.sh
set -euo pipefail
source_dir=$(cd "$(dirname "$0")/.." && pwd)
cd "$source_dir"
export PS2DEV=${PS2DEV:-/usr/local/ps2dev}
export PS2SDK=${PS2SDK:-"$PS2DEV/ps2sdk"}
export PATH="$PS2DEV/ee/bin:$PS2DEV/iop/bin:$PS2DEV/bin:$PATH"
out=${1:-build-ps2-mmi/all-pr}
mkdir -p "$out/include" "$out/obj"
out=$(cd "$out" && pwd)
export PNG_PS2_VARIANTS_DIR="$out"
export EE_WARNFLAGS='-Wall -Wextra -Werror'
sh ps2/build_filter_variants.sh one

cp png.h pngconf.h "$out/include/"
cp pnglibconf.h.prebuilt "$out/include/pnglibconf.h"
flags=(-O2 -G0 -D_EE -DPNG_PS2_EE_MMI -Wall -Wextra
       "-I$out/include" "-I$PS2SDK/ports/include"
       "-I$PS2SDK/ee/include" "-I$PS2SDK/common/include")
for opt in SUB3_PREFIX GRAY_PREFIX16 SUB4_PREFIX SUB6_PREFIX16 \
           SUB8_PREFIX16 SUB8_WORDS UP_2X SUB4_UNROLL4 AVG4_DUAL \
           GRAY_AVG WIDE_AVG PAETH PAETH_MASK; do
    flags+=("-DPNG_PS2_EE_MMI_$opt")
done
sources=(png pngerror pngget pngmem pngpread pngread pngrio pngrtran
         pngrutil pngset pngtrans pngwio pngwrite pngwtran pngwutil pngsimd)
objects=()
for unit in "${sources[@]}"; do
    mips64r5900el-ps2-elf-gcc "${flags[@]}" -c "$unit.c" -o "$out/obj/$unit.o"
    objects+=("$out/obj/$unit.o")
done
mips64r5900el-ps2-elf-ar rcs "$out/libpng.a" "${objects[@]}"
# Link the full PNG read/write test to verify libpng and SDK zlib integration.
mips64r5900el-ps2-elf-gcc "${flags[@]}" pngtest.c \
    "-T$PS2SDK/ee/startup/linkfile" "-L$out" \
    "-L$PS2SDK/ports/lib" "-L$PS2SDK/ee/lib" \
    -Wl,-zmax-page-size=128 -lpng -lz -lm -o "$out/pngtest.elf"
printf 'Built: %s/all-in-one.elf, libpng.a, pngtest.elf\n' "$out"
