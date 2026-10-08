#!/bin/sh
# Build a one-run correctness+benchmark ELF and, optionally, a matrix.
# Use: sh ps2/build_filter_variants.sh [one|all|matrix]
# PCCR is privileged EE-only; PS2 Linux userspace must not enable it.
set -eu
: "\${PS2SDK:?Set PS2SDK and source your PS2SDK environment first}"

out=\${PNG_PS2_VARIANTS_DIR:-ps2/variant-elfs}
mode=\${1:-all}
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
    if [ "\${PNG_PS2_BENCH_USE_PCCR:-0}" = 1 ]; then
        case " $flags " in
            *" PNG_PS2_BENCH_ENABLE "*) flags="$flags -DPNG_PS2_BENCH_EE_PCCR" ;;
        esac
    fi
    printf '\nBUILD,%s,%s\n' "$name" "$flags"
    make -C ps2 clean
    make -C ps2 EE_OPTFLAGS="$flags"
    cp ps2/test_filter_mmi.elf "$out/$name.elf"
}

# Run this ELF first: complete correctness then all-kernel benchmark CSV.
build_variant all-in-one PNG_PS2_BENCH_ENABLE \
    PNG_PS2_EE_MMI_SUB3_PREFIX PNG_PS2_EE_MMI_GRAY_PREFIX16 \
    PNG_PS2_EE_MMI_SUB4_PREFIX PNG_PS2_EE_MMI_SUB6_PREFIX16 \
    PNG_PS2_EE_MMI_SUB8_PREFIX16 PNG_PS2_EE_MMI_SUB8_WORDS \
    PNG_PS2_EE_MMI_AVG4_DUAL PNG_PS2_EE_MMI_GRAY_AVG \
    PNG_PS2_EE_MMI_WIDE_AVG PNG_PS2_EE_MMI_PAETH \
    PNG_PS2_EE_MMI_PAETH_MASK

if [ "$mode" = one ]; then
    printf '\nONE,all-in-one,%s\n' "$out/all-in-one.elf"
    exit 0
fi

build_variant baseline PNG_PS2_BENCH_ENABLE

if [ "$mode" = all ] || [ "$mode" = matrix ]; then
    # Five independent full-width prefix families: 2^5 combinations.
    mask=0
    while [ "$mask" -lt 32 ]
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
        if [ $((mask & 16)) -ne 0 ]; then
            set -- "$@" PNG_PS2_EE_MMI_SUB6_PREFIX16
        fi
        build_variant "prefix-$mask" "$@"
        mask=$((mask + 1))
    done

    build_variant sub8-words PNG_PS2_EE_MMI_SUB8_WORDS
    build_variant paeth-mask PNG_PS2_EE_MMI_PAETH PNG_PS2_EE_MMI_PAETH_MASK
    build_variant avg4-dual PNG_PS2_EE_MMI_AVG4_DUAL
    build_variant combined-small PNG_PS2_EE_MMI_SUB8_WORDS \
        PNG_PS2_EE_MMI_PAETH PNG_PS2_EE_MMI_PAETH_MASK \
        PNG_PS2_EE_MMI_AVG4_DUAL
fi
printf '\nDONE,out=%s,mode=%s\n' "$out" "$mode"
