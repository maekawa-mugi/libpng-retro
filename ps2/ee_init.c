/* ps2/ee_init.c - install PS2 EE MMI PNG read filters
 *
 * This file is included by pngsimd.c after pngpriv.h, not compiled as
 * a separate translation unit.
 * This code is released under the libpng license.
 */
#define png_target_impl "ps2-ee-mmi"

#include "filter_mmi.c"

/* The experimentally measured Up-4x path is opt-in for production.  It
 * includes its own safe alignment prologue and scalar fallback for short
 * rows.  The standalone autotuning ELF compares it independently. */
#ifdef PNG_PS2_EE_MMI_UP_4X
#include "filter_up4_mmi.c"
#endif

static void
png_init_filter_functions_ps2(png_struct *pp, unsigned int bpp)
{
#ifdef PNG_PS2_EE_MMI_UP_4X
   pp->read_filter[PNG_FILTER_VALUE_UP-1] = png_read_filter_row_up_4x_ps2;
#elif defined(PNG_PS2_EE_MMI_UP_2X)
   pp->read_filter[PNG_FILTER_VALUE_UP-1] = png_read_filter_row_up_2x_ps2;
#else
   pp->read_filter[PNG_FILTER_VALUE_UP-1] = png_read_filter_row_up_ps2;
#endif

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
/* The unqualified Paeth4 MMI candidate was slower than the scalar
       * baseline at 1024B on PCSX2.  Preserve libpng's generic Paeth4
       * dispatch unless a future benchmark explicitly justifies forcing
       * this implementation.  Other bpp Paeth candidates are unchanged. */
#if defined(PNG_PS2_EE_MMI_PAETH) && defined(PNG_PS2_EE_MMI_PAETH4_FORCE)
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
   else if (bpp == 6 || bpp == 8)
   {
      pp->read_filter[PNG_FILTER_VALUE_SUB-1] =
          bpp == 6 ? png_read_filter_row_sub6_ps2 : png_read_filter_row_sub8_ps2;
#ifdef PNG_PS2_EE_MMI_WIDE_AVG
      pp->read_filter[PNG_FILTER_VALUE_AVG-1] =
          bpp == 6 ? png_read_filter_row_avg6_ps2 : png_read_filter_row_avg8_ps2;
#endif
#ifdef PNG_PS2_EE_MMI_PAETH
      pp->read_filter[PNG_FILTER_VALUE_PAETH-1] =
          bpp == 6 ? png_read_filter_row_paeth6_ps2 :
              png_read_filter_row_paeth8_ps2;
#endif
   }
}

#define png_target_init_filter_functions_impl png_init_filter_functions_ps2

/* The libpng palette target hook receives the already-allocated output
 * row buffer and must set row_info exactly as the generic fallback would.
 * Enable only for 8-bit indexed source rows; packed 1/2/4-bit rows
 * transparently fall back to libpng's own PNG transformation. */
#ifdef PNG_TARGET_IMPLEMENTS_EXPAND_PALETTE
#include "palette_production.c"
static int
png_target_do_expand_palette_ps2(png_struct *pp, png_row_info *ri,
    png_byte *row, const png_color *palette,
    const png_byte *trans_alpha, int num_trans)
{
   PNG_UNUSED(pp);
   return png_ps2_expand_palette_row_8(ri, row,
       palette, trans_alpha, num_trans);
}
#define png_target_do_expand_palette_impl png_target_do_expand_palette_ps2
#endif
