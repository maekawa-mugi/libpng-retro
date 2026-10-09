/* Extended, experimental EE PNG kernels: write-side all standard byte
 * strides, palette transforms, 16-bit output operations and Adam7 row
 * scatter. This source is standalone and opt-in for the EE lab. It must
 * not be linked to production libpng without verified integration.
 *
 * The write-side kernels run backwards to preserve the ORIGINAL left
 * pixel, not the in-place encoded residuals.  All packed arithmetic
 * is bytewise modulo 256; no endian-dependent 16-bit sample addition.
 * SPDX-License-Identifier: libpng-2.0
 */
#include <string.h>

/* Write filtering modes: 0=Sub, 1=Average, 2=Paeth.
 * Valid byte strides are 1, 2, 3, 4, 6 and 8.
 * A four-byte packed subtraction amortizes PADDB-like scalar operations;
 * the predictor is generated independently for each byte.
 */
static void
png_ps2_write_filter_packed(png_byte *row, const png_byte *prev,
    size_t n, unsigned int bpp, unsigned int mode)
{
   size_t end = n;
   while (end >= (size_t)bpp + 4U)
   {
      size_t start = end - 4;
      png_uint_32 raw, predictor = 0;
      unsigned int j;
      memcpy(&raw, row + start, 4);
      for (j = 0; j < 4; ++j)
      {
         size_t k = start + j;
         unsigned int a = row[k - bpp], b = prev[k];
         unsigned int c = prev[k - bpp], chosen;
         if (mode == 0)
            chosen = a;
         else if (mode == 1)
            chosen = (a + b) >> 1;
         else
            chosen = png_ps2_extra_paeth(a, b, c);
         predictor |= (png_uint_32)chosen << (j * 8);
      }
      raw = png_ps2_extra_sub4(raw, predictor);
      memcpy(row + start, &raw, 4);
      end = start;
   }

   while (end-- > 0)
   {
      unsigned int a = end >= bpp ? row[end - bpp] : 0;
      unsigned int b = prev[end];
      unsigned int c = end >= bpp ? prev[end - bpp] : 0;
      unsigned int chosen;
      if (mode == 0)
         chosen = a;
      else if (mode == 1)
         chosen = (a + b) >> 1;
      else
         chosen = png_ps2_extra_paeth(a, b, c);
      row[end] = (png_byte)((unsigned int)row[end] - chosen);
   }
}

/* Palette is 256 RGB triplets; alpha is an independent 256-byte table.
 * alpha[i]=255 for palette entries without tRNS alpha.
 * dst==src is permitted; other source/destination overlaps are not.
 */
static void
png_ps2_expand_palette_rgb3(png_byte *dst, const png_byte *src, size_t n,
    const png_byte *rgb)
{
   size_t i;
   if (dst == src)
   {
      for (i = n; i-- > 0;)
      {
         png_byte index = src[i];
         memcpy(dst + 3 * i, rgb + 3 * (size_t)index, 3);
      }
      return;
   }

   for (i = 0; n - i >= 4; i += 4)
   {
      png_byte a = src[i], b = src[i + 1];
      png_byte c = src[i + 2], d = src[i + 3];
      memcpy(dst + 3*i, rgb + 3*(size_t)a, 3);
      memcpy(dst + 3*i + 3, rgb + 3*(size_t)b, 3);
      memcpy(dst + 3*i + 6, rgb + 3*(size_t)c, 3);
      memcpy(dst + 3*i + 9, rgb + 3*(size_t)d, 3);
   }
   for (; i < n; ++i)
      memcpy(dst + 3 * i, rgb + 3 * (size_t)src[i], 3);
}

static void
png_ps2_expand_palette_rgba_trns(png_byte *dst, const png_byte *src,
    size_t n, const png_byte *rgb, const png_byte *alpha)
{
   size_t i;
   if (dst == src)
   {
      for (i = n; i-- > 0;)
      {
         png_byte index = src[i];
         memcpy(dst + 4 * i, rgb + 3 * (size_t)index, 3);
         dst[4*i + 3] = alpha[index];
      }
      return;
   }
   for (i = 0; n - i >= 4; i += 4)
   {
      unsigned int j;
      for (j = 0; j < 4; ++j)
      {
         png_byte index = src[i + j];
         memcpy(dst + 4*(i+j), rgb + 3*(size_t)index, 3);
         dst[4*(i+j)+3] = alpha[index];
      }
   }
   for (; i < n; ++i)
   {
      png_byte index = src[i];
      memcpy(dst + 4*i, rgb + 3*(size_t)index, 3);
      dst[4*i+3] = alpha[index];
   }
}

/* Swap the two bytes in each PNG 16-bit sample.  In the EE path:
 * PSRLH/PSLLH/POR act on all eight 16-bit lanes of an aligned LQ.
 * Odd pointers use the scalar path; vectors never overlap tail bytes.
 */
static void
png_ps2_swap16_mmi(png_byte *row, size_t samples)
{
   size_t i = 0;
   if (((size_t)row & 1U) == 0 && samples >= 8)
   {
      png_byte *ptr;
      unsigned int blocks;
      while (i < samples && (((size_t)(row + 2*i) & 15U) != 0))
      {
         png_byte t = row[2*i];
         row[2*i] = row[2*i+1];
         row[2*i+1] = t;
         ++i;
      }
      blocks = (unsigned int)((samples - i) >> 3);
      ptr = row + 2*i;
#ifdef PNG_PS2_EXTRA_PORTABLE
      while (blocks-- != 0)
      {
         unsigned int j;
         for (j = 0; j < 16; j += 2)
         {
            png_byte t = ptr[j];
            ptr[j] = ptr[j+1];
            ptr[j+1] = t;
         }
         ptr += 16;
      }
#else
      if (blocks != 0)
      {
         __asm__ volatile (
            ".set push\n\t"
            ".set noreorder\n\t"
            "1:\n\t"
            "lq    $8, 0(%[ptr])\n\t"
            "psrlh $9, $8, 8\n\t"
            "psllh $10, $8, 8\n\t"
            "por   $8, $9, $10\n\t"
            "sq    $8, 0(%[ptr])\n\t"
            "addiu %[ptr], %[ptr], 16\n\t"
            "addiu %[blocks], %[blocks], -1\n\t"
            "bnez  %[blocks], 1b\n\t"
            "nop\n\t"
            ".set pop\n\t"
            : [ptr] "+r"(ptr), [blocks] "+r"(blocks)
            :
            : "$8", "$9", "$10", "memory"
         );
      }
#endif
      i = (size_t)(ptr - row) >> 1;
   }
   for (; i < samples; ++i)
   {
      png_byte t = row[2*i];
      row[2*i] = row[2*i+1];
      row[2*i+1] = t;
   }
}

/* Convert big-endian 16-bit PNG samples to the high byte only.
 * May reduce in place (dst==src), or write to a disjoint destination.
 */
static void
png_ps2_strip16_high(png_byte *dst, const png_byte *src, size_t samples)
{
   size_t i;
   for (i = 0; i < samples; ++i)
      dst[i] = src[2*i];
}

/* BGR/BGRA swaps the red/blue bytes in place. */
static void
png_ps2_swap_rb(png_byte *row, size_t pixels, unsigned int channels)
{
   size_t i;
   for (i = 0; i < pixels; ++i)
   {
      png_byte t = row[channels*i];
      row[channels*i] = row[channels*i+2];
      row[channels*i+2] = t;
   }
}

/* Alpha insertion and removal can both operate in place, with the
 * insertion writing backwards and the removal writing forwards.
 */
static void
png_ps2_rgb_to_rgba(png_byte *dst, const png_byte *src,
    size_t pixels, png_byte alpha)
{
   size_t i;
   if (dst == src)
   {
      for (i = pixels; i-- > 0;)
      {
         png_byte r = src[3*i], g = src[3*i+1], b = src[3*i+2];
         dst[4*i] = r;
         dst[4*i+1] = g;
         dst[4*i+2] = b;
         dst[4*i+3] = alpha;
      }
   }
   else
      for (i = 0; i < pixels; ++i)
      {
         memcpy(dst + 4*i, src + 3*i, 3);
         dst[4*i+3] = alpha;
      }
}

static void
png_ps2_rgba_to_rgb(png_byte *dst, const png_byte *src, size_t pixels)
{
   size_t i;
   for (i = 0; i < pixels; ++i)
   {
      png_byte r = src[4*i], g = src[4*i+1], b = src[4*i+2];
      dst[3*i] = r;
      dst[3*i+1] = g;
      dst[3*i+2] = b;
   }
}

/* Adam7 horizontal pass scatter.  Destination row is pre-populated.
 * The caller controls the seven vertical passes and row numbers.
 * For packed depths (1,2,4), pixels are MSB-first in each byte.
 */
static const unsigned int png_ps2_adam7_xstart[7] = {0,4,0,2,0,1,0};
static const unsigned int png_ps2_adam7_xstep[7]  = {8,8,4,4,2,2,1};

static void
png_ps2_adam7_scatter_bytes(png_byte *dst, const png_byte *passrow,
    size_t width, unsigned int bpp, unsigned int pass)
{
   size_t x, source = 0;
   for (x = png_ps2_adam7_xstart[pass]; x < width;
       x += png_ps2_adam7_xstep[pass])
   {
      memcpy(dst + x*bpp, passrow + source*bpp, bpp);
      ++source;
   }
}

static void
png_ps2_adam7_scatter_bits(png_byte *dst, const png_byte *passrow,
    size_t width, unsigned int depth, unsigned int pass)
{
   size_t x, source = 0;
   unsigned int mask = (1U << depth) - 1U;
   for (x = png_ps2_adam7_xstart[pass]; x < width;
       x += png_ps2_adam7_xstep[pass], ++source)
   {
      size_t srcbit = source * depth, dstbit = x * depth;
      unsigned int srcshift = 8U - depth - (unsigned int)(srcbit & 7U);
      unsigned int dstshift = 8U - depth - (unsigned int)(dstbit & 7U);
      png_byte value = (png_byte)((passrow[srcbit >> 3] >>
          srcshift) & mask);
      png_byte keep = (png_byte)~(mask << dstshift);
      dst[dstbit >> 3] = (png_byte)((dst[dstbit >> 3] & keep) |
          (value << dstshift));
   }
}
