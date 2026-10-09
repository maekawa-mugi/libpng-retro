/* Host syntax-only C translation-unit check, NOT an R5900 assembler check.
 * Compile with GCC and Clang using -fsyntax-only.
 * SPDX-License-Identifier: libpng-2.0
 */
#include <stddef.h>
typedef unsigned char png_byte;
typedef unsigned int png_uint_32;
typedef struct { size_t rowbytes; } png_row_info;
/* Test default registration and all optional filters independently.
 * The build passes PNG_PS2_SYNTAX_DEFAULT to exercise the no-opt-in path.
 */
#ifndef PNG_PS2_SYNTAX_DEFAULT
#define PNG_PS2_EE_MMI_GRAY_AVG 1
#define PNG_PS2_EE_MMI_WIDE_AVG 1
#define PNG_PS2_EE_MMI_PAETH 1
#define PNG_PS2_EE_MMI_SUB3_PREFIX 1
#define PNG_PS2_EE_MMI_GRAY_PREFIX16 1
#define PNG_PS2_EE_MMI_SUB8_WORDS 1
#define PNG_PS2_EE_MMI_SUB8_PREFIX16 1
#define PNG_PS2_EE_MMI_SUB6_PREFIX16 1
#define PNG_PS2_EE_MMI_PAETH_MASK 1
#endif
#ifdef PNG_PS2_SYNTAX_PORTABLE
#define PNG_PS2_UP_PORTABLE_ADD 1
#define PNG_PS2_GRAY_PORTABLE_ADD 1
#define PNG_PS2_RGB3_PORTABLE_ADD 1
#define PNG_PS2_PAETH_PORTABLE_ADD 1
#define PNG_PS2_WIDE_PORTABLE_ADD 1
#endif
#include "filter_up_mmi.c"
#include "filter_up_unrolled_mmi.c"
#ifndef PNG_PS2_SYNTAX_PORTABLE
#include "filter_sub4_unrolled_mmi.c"
#endif
#include "filter_gray_mmi.c"
#include "filter_rgb3.c"
#include "filter_wide_mmi.c"
#include "filter_paeth_mmi.c"
