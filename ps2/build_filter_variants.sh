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

# All three prefix implementations are separate binary switches.
# The full power set is built to detect interactions between optional
# paths, in addition to testing each one against the default path.
build_variant baseline
build_variant sub3 PNG_PS2_EE_MMI_SUB3_PREFIX
build_variant gray PNG_PS2_EE_MMI_GRAY_PREFIX16
build_variant sub4 PNG_PS2_EE_MMI_SUB4_PREFIX
build_variant sub3-gray PNG_PS2_EE_MMI_SUB3_PREFIX PNG_PS2_EE_MMI_GRAY_PREFIX16
build_variant sub3-sub4 PNG_PS2_EE_MMI_SUB3_PREFIX PNG_PS2_EE_MMI_SUB4_PREFIX
build_variant gray-sub4 PNG_PS2_EE_MMI_GRAY_PREFIX16 PNG_PS2_EE_MMI_SUB4_PREFIX
build_variant all-prefix PNG_PS2_EE_MMI_SUB3_PREFIX PNG_PS2_EE_MMI_GRAY_PREFIX16 PNG_PS2_EE_MMI_SUB4_PREFIX

# Include the other optional filters in a stress-test combination.
build_variant all-optional PNG_PS2_EE_MMI_SUB3_PREFIX PNG_PS2_EE_MMI_GRAY_PREFIX16 PNG_PS2_EE_MMI_SUB4_PREFIX PNG_PS2_EE_MMI_GRAY_AVG PNG_PS2_EE_MMI_WIDE_AVG PNG_PS2_EE_MMI_PAETH

printf '\nBuilt 9 variants in %s. Run each ELF on PS2/PCSX2 and compare PASS counts.\n' "$out"
