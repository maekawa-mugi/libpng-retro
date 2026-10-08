/* ps2/check.h - PS2 Emotion Engine MMI row-filter support
 *
 * This code is released under the libpng license.
 * Enable explicitly with PNG_PS2_EE_MMI, or use an EE compiler that
 * defines __R5900__.  The Loongson MMI implementation is unrelated.
 */
#if defined(PNG_PS2_EE_MMI) || defined(__R5900__)
#  ifdef PNG_TARGET_CODE_IMPLEMENTATION
#    error PS2 EE MMI conflicts with another target implementation
#  endif
#  define PNG_TARGET_CODE_IMPLEMENTATION "ps2/ee_init.c"
#  define PNG_TARGET_IMPLEMENTS_FILTERS
#  define PNG_TARGET_ROW_ALIGNMENT 16
#endif
