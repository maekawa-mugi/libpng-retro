/* Compile-only validation of the production EE filter registration.
 * This is intentionally not an x86 execution of EE assembly. */
#include <stddef.h>
typedef unsigned char png_byte;
typedef unsigned int png_uint_32;
typedef struct
{
   size_t rowbytes;
   png_uint_32 width;
   png_byte color_type, channels, bit_depth, pixel_depth;
} png_row_info;
typedef struct { png_byte red,green,blue; } png_color;
#define PNG_COLOR_TYPE_PALETTE 3
#define PNG_COLOR_TYPE_RGB 2
#define PNG_COLOR_TYPE_RGB_ALPHA 6
#define PNG_UNUSED(value) ((void)(value))
typedef void (*ps2_filter_fn)(png_row_info *, png_byte *, const png_byte *);
typedef struct { ps2_filter_fn read_filter[4]; } png_struct;
#define PNG_FILTER_VALUE_SUB  1
#define PNG_FILTER_VALUE_UP   2
#define PNG_FILTER_VALUE_AVG  3
#define PNG_FILTER_VALUE_PAETH 4
#ifdef PNG_PS2_TEST_PALETTE_DISPATCH
#define PNG_TARGET_IMPLEMENTS_EXPAND_PALETTE
#endif
#include "ee_init.c"

void png_ps2_test_production_dispatch(png_struct *p, unsigned int bpp)
{
   png_init_filter_functions_ps2(p, bpp);
}

#ifdef PNG_PS2_TEST_PALETTE_DISPATCH
int png_ps2_test_real_palette_hook(png_struct *pp, png_row_info *ri,
    png_byte *row, const png_color *pal, const png_byte *alpha, int num)
{
   return png_target_do_expand_palette_ps2(pp, ri, row, pal, alpha, num);
}
#endif
