/* ps2/filter_rgb3.c - PS2 EE packed-byte read filters for RGB8.
 *
 * Included by filter_mmi.c, after pngpriv.h; this is not a standalone
 * libpng translation unit.  The standalone host test defines
 * PNG_PS2_RGB3_PORTABLE_ADD to replace the EE instruction with C.
 * Released under the libpng license.
 */

static png_uint_32
png_ps2_pack_rgb3(const png_byte *src)
{
   return (png_uint_32)src[0] |
       ((png_uint_32)src[1] << 8) | ((png_uint_32)src[2] << 16);
}

static void
png_ps2_store_rgb3(png_byte *dst, png_uint_32 packed)
{
   dst[0] = (png_byte)packed;
   dst[1] = (png_byte)(packed >> 8);
   dst[2] = (png_byte)(packed >> 16);
}

/* Three low byte lanes are meaningful; no carry between channels.
 * GCC's R5900 integer register constraint selects EE 128-bit GPRs.
 */
static png_uint_32
png_ps2_add_rgb3(png_uint_32 a, png_uint_32 b)
{
#ifdef PNG_PS2_RGB3_PORTABLE_ADD
   png_uint_32 sum = 0;
   unsigned int lane;
   for (lane = 0; lane < 3; ++lane)
   {
      unsigned int shift = lane * 8;
      unsigned int x = (a >> shift) & 255U;
      unsigned int y = (b >> shift) & 255U;
      sum |= (png_uint_32)((x + y) & 255U) << shift;
   }
   return sum;
#else
   png_uint_32 sum;
   __asm__ volatile ("paddb %0, %1, %2" : "=&r" (sum) : "r" (a), "r" (b));
   return sum;
#endif
}

#if defined(PNG_PS2_EE_MMI_SUB3_PREFIX)
#include "filter_sub3_prefix_mmi.c"
#endif

/* No unaligned word loads: three byte loads per pixel, including row tails. */
static void
png_read_filter_row_sub3_ps2(png_row_info *row_info, png_byte *row,
    const png_byte *prev_row)
{
   size_t i, n = row_info->rowbytes;
   png_uint_32 left;
   (void)prev_row;

   if (n <= 3)
      return;
#if defined(PNG_PS2_EE_MMI_SUB3_PREFIX)
   if (png_read_filter_row_sub3_prefix_ps2(row, n))
      return;
#endif
   left = png_ps2_pack_rgb3(row);
   for (i = 3; n - i >= 3; i += 3)
   {
      png_uint_32 decoded = png_ps2_add_rgb3(png_ps2_pack_rgb3(row + i), left);
      png_ps2_store_rgb3(row + i, decoded);
      left = decoded;
   }
   for (; i < n; ++i)
      row[i] = (png_byte)((unsigned int)row[i] + (unsigned int)row[i - 3]);
}

/* The packed mean is exact: no carry can cross a byte boundary because
 * (a&b)+((a^b & 0xfefefefe)>>1) is at most 255 in each byte.
 */
static void
png_read_filter_row_avg3_ps2(png_row_info *row_info, png_byte *row,
    const png_byte *prev_row)
{
   size_t i, n = row_info->rowbytes;
   png_uint_32 left = 0;

   for (i = 0; n - i >= 3; i += 3)
   {
      png_uint_32 above = png_ps2_pack_rgb3(prev_row + i);
      png_uint_32 predictor = (left & above) +
          (((left ^ above) & 0x00fefefeU) >> 1);
      png_uint_32 decoded = png_ps2_add_rgb3(
          png_ps2_pack_rgb3(row + i), predictor);
      png_ps2_store_rgb3(row + i, decoded);
      left = decoded;
   }
   for (; i < n; ++i)
   {
      unsigned int a = i >= 3 ? row[i - 3] : 0;
      unsigned int b = prev_row[i];
      row[i] = (png_byte)((unsigned int)row[i] + ((a + b) >> 1));
   }
}
