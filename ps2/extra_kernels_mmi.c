/* Standalone experimental PNG write-side kernels and palette expansion.
 *
 * These are not installed into libpng by default.  They are exercised in
 * the same EE test ELF to compare their exact bytewise output and timing
 * before touching pngwutil.c / pngrtran.c dispatch integration.
 *
 * When an in-place PNG forward filter is processed, left must refer to
 * the ORIGINAL raw byte.  Process Sub/Avg/Paeth backwards, unlike their
 * reverse-filter decoding counterparts.
 *
 * SPDX-License-Identifier: libpng-2.0
 */
#include <string.h>

static png_uint_32
png_ps2_extra_sub4(png_uint_32 a, png_uint_32 b)
{
#ifdef PNG_PS2_EXTRA_PORTABLE
   png_uint_32 result = 0;
   unsigned int j;
   for (j = 0; j < 4; ++j)
   {
      unsigned int shift = j * 8;
      result |= (png_uint_32)((((a >> shift) & 255U) -
          ((b >> shift) & 255U)) & 255U) << shift;
   }
   return result;
#else
   png_uint_32 result;
   __asm__ volatile ("psubb %0, %1, %2" : "=&r"(result)
       : "r"(a), "r"(b));
   return result;
#endif
}

/* Forward Up: 16-byte modular subtraction, safe scalar alignment and tail. */
static void
png_ps2_write_up_mmi(png_row_info *ri, png_byte *row, const png_byte *prev)
{
   size_t n = ri->rowbytes;
   if (n >= 16 && (((size_t)row ^ (size_t)prev) & 15U) == 0)
   {
      size_t prefix = (16U - ((size_t)row & 15U)) & 15U;
      while (prefix-- != 0)
      {
         *row = (png_byte)((unsigned int)*row -
             (unsigned int)*prev);
         ++row;
         ++prev;
         --n;
      }

      if (n >= 16)
      {
         unsigned int blocks = (unsigned int)(n >> 4);
         n &= 15U;
#ifdef PNG_PS2_EXTRA_PORTABLE
         while (blocks-- != 0)
         {
            unsigned int j;
            for (j = 0; j < 16; ++j)
               row[j] = (png_byte)((unsigned int)row[j] -
                   (unsigned int)prev[j]);
            row += 16;
            prev += 16;
         }
#else
         __asm__ volatile (
            ".set push\n\t"
            ".set noreorder\n\t"
            "1:\n\t"
            "lq    $8, 0(%[row])\n\t"
            "lq    $9, 0(%[prev])\n\t"
            "psubb $8, $8, $9\n\t"
            "sq    $8, 0(%[row])\n\t"
            "addiu %[row], %[row], 16\n\t"
            "addiu %[prev], %[prev], 16\n\t"
            "addiu %[blocks], %[blocks], -1\n\t"
            "bnez  %[blocks], 1b\n\t"
            "nop\n\t"
            ".set pop\n\t"
            : [row] "+r"(row), [prev] "+r"(prev), [blocks] "+r"(blocks)
            :
            : "$8", "$9", "memory"
         );
#endif
      }
   }

   while (n-- != 0)
   {
      *row = (png_byte)((unsigned int)*row -
          (unsigned int)*prev);
      ++row;
      ++prev;
   }
}

/* Forward Sub4: scan backwards so original left is not overwritten. */
static void
png_ps2_write_sub4_mmi(png_row_info *ri, png_byte *row,
    const png_byte *prev)
{
   size_t n = ri->rowbytes, i;
   (void)prev;
   if (n <= 4)
      return;

   if (n >= 32 && (n & 3U) == 0 && ((size_t)row & 3U) == 0)
   {
      png_byte *ptr = row + n - 4;
      unsigned int count = (unsigned int)((n - 4) >> 2);
#ifdef PNG_PS2_EXTRA_PORTABLE
      while (count-- != 0)
      {
         png_uint_32 raw, left;
         memcpy(&raw, ptr, 4);
         memcpy(&left, ptr - 4, 4);
         raw = png_ps2_extra_sub4(raw, left);
         memcpy(ptr, &raw, 4);
         ptr -= 4;
      }
#else
      __asm__ volatile (
         ".set push\n\t"
         ".set noreorder\n\t"
         "1:\n\t"
         "lw    $8, 0(%[ptr])\n\t"
         "lw    $9, -4(%[ptr])\n\t"
         "psubb $8, $8, $9\n\t"
         "sw    $8, 0(%[ptr])\n\t"
         "addiu %[ptr], %[ptr], -4\n\t"
         "addiu %[count], %[count], -1\n\t"
         "bnez  %[count], 1b\n\t"
         "nop\n\t"
         ".set pop\n\t"
         : [ptr] "+r"(ptr), [count] "+r"(count)
         :
         : "$8", "$9", "memory"
      );
#endif
      return;
   }

   for (i = n; i-- > 4;)
      row[i] = (png_byte)((unsigned int)row[i] -
          (unsigned int)row[i - 4]);
}

/* Exact floor average; two 32-bit packed byte predictors can be
 * evaluated from the unfiltered row even though we store in place.
 */
static void
png_ps2_write_avg4_mmi(png_row_info *ri, png_byte *row,
    const png_byte *prev)
{
   size_t i, n = ri->rowbytes;
   if (n >= 32 && (n & 3U) == 0 &&
       (((size_t)row | (size_t)prev) & 3U) == 0)
   {
      for (i = n; i != 0; i -= 4)
      {
         png_uint_32 raw, above, left = 0, predict;
         memcpy(&raw, row + i - 4, 4);
         memcpy(&above, prev + i - 4, 4);
         if (i >= 8)
            memcpy(&left, row + i - 8, 4);
         predict = (left & above) +
             (((left ^ above) & 0xfefefefeU) >> 1);
         raw = png_ps2_extra_sub4(raw, predict);
         memcpy(row + i - 4, &raw, 4);
      }
      return;
   }
   for (i = n; i-- > 0;)
   {
      unsigned int a = i >= 4 ? row[i - 4] : 0;
      row[i] = (png_byte)((unsigned int)row[i] -
          ((a + (unsigned int)prev[i]) >> 1));
   }
}

static unsigned int
png_ps2_extra_paeth(unsigned int a, unsigned int b, unsigned int c)
{
   int pa = (int)b - (int)c, pb = (int)a - (int)c;
   int pc = (int)a + (int)b - 2 * (int)c;
   pa = pa < 0 ? -pa : pa;
   pb = pb < 0 ? -pb : pb;
   pc = pc < 0 ? -pc : pc;
   if (pa <= pb && pa <= pc) return a;
   if (pb <= pc) return b;
   return c;
}

/* Forward Paeth4: predictor uses original row and unmodified prev row. */
static void
png_ps2_write_paeth4_mmi(png_row_info *ri, png_byte *row,
    const png_byte *prev)
{
   size_t i, n = ri->rowbytes;
   if (n >= 32 && (n & 3U) == 0 &&
       (((size_t)row | (size_t)prev) & 3U) == 0)
   {
      for (i = n; i != 0; i -= 4)
      {
         png_uint_32 raw, predictor = 0;
         unsigned int j;
         memcpy(&raw, row + i - 4, 4);
         for (j = 0; j < 4; ++j)
         {
            size_t k = i - 4 + j;
            unsigned int a = k >= 4 ? row[k - 4] : 0;
            unsigned int b = prev[k];
            unsigned int c = k >= 4 ? prev[k - 4] : 0;
            predictor |= (png_uint_32)png_ps2_extra_paeth(a, b, c)
                << (j * 8);
         }
         raw = png_ps2_extra_sub4(raw, predictor);
         memcpy(row + i - 4, &raw, 4);
      }
      return;
   }
   for (i = n; i-- > 0;)
   {
      unsigned int a = i >= 4 ? row[i - 4] : 0;
      unsigned int b = prev[i];
      unsigned int c = i >= 4 ? prev[i - 4] : 0;
      row[i] = (png_byte)((unsigned int)row[i] -
          png_ps2_extra_paeth(a, b, c));
   }
}

/* Indexed palette -> RGBA8 expansion. No MMI gather exists on EE; an
 * unrolled table-load approach may be faster.  dst==src is supported
 * by processing backwards; otherwise src/dst MUST NOT overlap.
 * Palette contains exactly 256 entries of four bytes each.
 */
static void
png_ps2_expand_palette_rgba4(png_byte *dst, const png_byte *src, size_t n,
    const png_byte *palette)
{
   size_t i;
   if (dst == src)
   {
      for (i = n; i-- > 0;)
      {
         png_byte index = src[i];
         memcpy(dst + 4 * i, palette + 4 * (size_t)index, 4);
      }
      return;
   }

   for (i = 0; n - i >= 4; i += 4)
   {
      png_byte a = src[i], b = src[i+1], c = src[i+2], d = src[i+3];
      memcpy(dst + 4*i,     palette + 4*(size_t)a, 4);
      memcpy(dst + 4*i + 4, palette + 4*(size_t)b, 4);
      memcpy(dst + 4*i + 8, palette + 4*(size_t)c, 4);
      memcpy(dst + 4*i + 12,palette + 4*(size_t)d, 4);
   }
   for (; i < n; ++i)
      memcpy(dst + 4*i, palette + 4*(size_t)src[i], 4);
}
