/* ps2/ee_init.c - install PS2 EE MMI PNG read filters
 *
 * This file is included by pngsimd.c after pngpriv.h, not compiled as
 * a separate translation unit.
 * This code is released under the libpng license.
 */
#define png_target_impl "ps2-ee-mmi"

#include "filter_mmi.c"

static void
png_init_filter_functions_ps2(png_struct *pp, unsigned int bpp)
{
   pp->read_filter[PNG_FILTER_VALUE_UP-1] = png_read_filter_row_up_ps2;

   if (bpp == 4)
   {
      pp->read_filter[PNG_FILTER_VALUE_SUB-1] = png_read_filter_row_sub4_ps2;
      pp->read_filter[PNG_FILTER_VALUE_AVG-1] = png_read_filter_row_avg4_ps2;
   }
}

#define png_target_init_filter_functions_impl png_init_filter_functions_ps2
