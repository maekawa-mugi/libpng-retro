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

   if (bpp == 1 || bpp == 2)
   {
      pp->read_filter[PNG_FILTER_VALUE_SUB-1] =
          bpp == 1 ? png_read_filter_row_sub1_ps2 : png_read_filter_row_sub2_ps2;
#ifdef PNG_PS2_EE_MMI_PAETH
      pp->read_filter[PNG_FILTER_VALUE_PAETH-1] =
          bpp == 1 ? png_read_filter_row_paeth1_ps2 : png_read_filter_row_paeth2_ps2;
#endif
#ifdef PNG_PS2_EE_MMI_GRAY_AVG
      pp->read_filter[PNG_FILTER_VALUE_AVG-1] =
          bpp == 1 ? png_read_filter_row_avg1_ps2 : png_read_filter_row_avg2_ps2;
#endif
   }
   else if (bpp == 4)
   {
      pp->read_filter[PNG_FILTER_VALUE_SUB-1] = png_read_filter_row_sub4_ps2;
      pp->read_filter[PNG_FILTER_VALUE_AVG-1] = png_read_filter_row_avg4_ps2;
#ifdef PNG_PS2_EE_MMI_PAETH
      pp->read_filter[PNG_FILTER_VALUE_PAETH-1] =
          png_read_filter_row_paeth4_ps2;
#endif
   }
   else if (bpp == 3)
   {
      pp->read_filter[PNG_FILTER_VALUE_SUB-1] = png_read_filter_row_sub3_ps2;
      pp->read_filter[PNG_FILTER_VALUE_AVG-1] = png_read_filter_row_avg3_ps2;
#ifdef PNG_PS2_EE_MMI_PAETH
      pp->read_filter[PNG_FILTER_VALUE_PAETH-1] =
          png_read_filter_row_paeth3_ps2;
#endif
   }
}

#define png_target_init_filter_functions_impl png_init_filter_functions_ps2
