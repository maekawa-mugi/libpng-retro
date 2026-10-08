#!/bin/sh
# Build multiple standalone EE regression ELFs without changing the
# production libpng dispatch or claiming an unmeasured speedup.
# Invoke as: sh ps2/build_filter_variants.sh
# Requires the PS2SDK environment, just like make -C ps2.
set -eu

: "${PS2SDK:?Set PS2SDK and source your EE toolchain environment first}"
out=${PNG_PS2_VARIANTS_DIR:-ps2/variant-elfs}
mkdir -p "$out"

build_variant()
{
    name=$1
    shift
    flags="-O2"
    for opt
    do
        flags="$flags -D$opt"
    done
    printf '\n=== Building EE variant %s (%s) ===\n' "$name" "$flags"
    make -C ps2 clean
    make -C ps2 EE_OPTFLAGS="$flags"
    cp ps2/test_filter_mmi.elf "$out/$name.elf"
}

# Four independent opt-in prefix implementations: Sub3, gray Sub1/Sub2,
# Sub4, and Sub8.  Generate all 2^4 combinations so integration mistakes
# in simultaneous SA-using kernels are visible in the EE test harness.
mask=0
while [ "$mask" -lt 16 ]
do
    set --
    if [ $((mask & 1)) -ne 0 ]; then
        set -- "$@" PNG_PS2_EE_MMI_SUB3_PREFIX
    fi
    if [ $((mask & 2)) -ne 0 ]; then
        set -- "$@" PNG_PS2_EE_MMI_GRAY_PREFIX16
    fi
    if [ $((mask & 4)) -ne 0 ]; then
        set -- "$@" PNG_PS2_EE_MMI_SUB4_PREFIX
    fi
    if [ $((mask & 8)) -ne 0 ]; then
        set -- "$@" PNG_PS2_EE_MMI_SUB8_PREFIX16
    fi
    build_variant "mask-$mask" "$@"
    mask=$((mask + 1))
done

# Compare the third Sub8 implementation, two directly loaded word lanes.
build_variant sub8-words PNG_PS2_EE_MMI_SUB8_WORDS

# Stress the precedence of both Sub8 opt-ins plus optional Average/Paeth.
# PREFIX16 takes precedence at >=64 bytes; WORDS handles 32..63 bytes.
build_variant all-optional PNG_PS2_EE_MMI_SUB3_PREFIX PNG_PS2_EE_MMI_GRAY_PREFIX16 PNG_PS2_EE_MMI_SUB4_PREFIX PNG_PS2_EE_MMI_SUB8_PREFIX16 PNG_PS2_EE_MMI_SUB8_WORDS PNG_PS2_EE_MMI_GRAY_AVG PNG_PS2_EE_MMI_WIDE_AVG PNG_PS2_EE_MMI_PAETH

printf '\nBuilt 18 variants in %s. Run every ELF on EE/PCSX2 and compare PASS counts.\n' "$out"
