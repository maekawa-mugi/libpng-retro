/* The same palette hook source compiled with minimal libpng-compatible
 * structs; the production pngsimd path uses libpng's real png_row_info. */
#include <stddef.h>
typedef unsigned char png_byte;
typedef unsigned int png_uint_32;
typedef struct
{
   png_uint_32 width;
   size_t rowbytes;
   png_byte color_type, channels, bit_depth, pixel_depth;
} png_row_info;
typedef struct
{
   png_byte red,green,blue;
} png_color;
#define PNG_COLOR_TYPE_PALETTE 3
#define PNG_COLOR_TYPE_RGB 2
#define PNG_COLOR_TYPE_RGB_ALPHA 6
#include "palette_production.c"
#include "test_palette_hook.c"
int main(void) { return png_ps2_test_palette_hook(); }
