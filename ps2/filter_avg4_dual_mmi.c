/* Experimental two-pixel Average4 pipeline for the EE.
 * The two raw words and two above words are independent loads; the
 * decoded left word is forwarded from the first pixel to the next.
 * The exact PNG floor average is computed without cross-byte carries.
 *
 * Included by filter_mmi.c, not compiled as its own libpng object.
 * SPDX-License-Identifier: libpng-2.0
 */
#include <string.h>

static png_uint_32
png_ps2_avg4_dual_add(png_uint_32 a, png_uint_32 b)
{
#ifdef PNG_PS2_AVG4_PORTABLE_ADD
   png_uint_32 result = 0;
   unsigned int j;
   for (j = 0; j < 4; ++j)
   {
      unsigned int shift = j * 8;
      result |= (png_uint_32)((((a >> shift) & 255U) +
          ((b >> shift) & 255U)) & 255U) << shift;
   }
   return result;
#else
   png_uint_32 result;
   __asm__ volatile ("paddb %0, %1, %2" : "=&r"(result)
       : "r"(a), "r"(b));
   return result;
#endif
}

static png_uint_32
png_ps2_avg4_dual_mean(png_uint_32 a, png_uint_32 b)
{
   return (a & b) + (((a ^ b) & 0xfefefefeU) >> 1);
}

static int
png_ps2_avg4_dual(png_byte *row, const png_byte *prev, size_t n)
{
   size_t i;
   png_uint_32 left = 0;
   if (n < 32 || (n & 7U) != 0 ||
       (((size_t)row | (size_t)prev) & 3U) != 0)
      return 0;

   for (i = 0; i < n; i += 8)
   {
      png_uint_32 raw0, raw1, above0, above1;
      png_uint_32 decoded0, decoded1;
      memcpy(&raw0, row + i, 4);
      memcpy(&raw1, row + i + 4, 4);
      memcpy(&above0, prev + i, 4);
      memcpy(&above1, prev + i + 4, 4);
      decoded0 = png_ps2_avg4_dual_add(raw0,
          png_ps2_avg4_dual_mean(left, above0));
      decoded1 = png_ps2_avg4_dual_add(raw1,
          png_ps2_avg4_dual_mean(decoded0, above1));
      memcpy(row + i, &decoded0, 4);
      memcpy(row + i + 4, &decoded1, 4);
      left = decoded1;
   }
   return 1;
}
