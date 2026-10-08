/* Shared host/EE regressions for the standalone write and palette
 * candidates.  Expect png_byte/png_uint_32/png_row_info typedefs in the
 * including translation unit.  Not a standalone libpng object.
 * SPDX-License-Identifier: libpng-2.0
 */
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "extra_kernels_mmi.c"

#define PS2_EXTRA_MAX 1024U
#define PS2_EXTRA_BUF (4U * PS2_EXTRA_MAX + 64U)
static png_byte extra_raw[PS2_EXTRA_BUF];
static png_byte extra_prev[PS2_EXTRA_BUF];
static png_byte extra_dst[PS2_EXTRA_BUF];
static png_byte extra_expected[PS2_EXTRA_BUF];
static png_byte extra_before[PS2_EXTRA_BUF];
static png_byte extra_palette[1024];
static unsigned int extra_rng = 0x9873ab24U;

static png_byte
png_ps2_extra_random(void)
{
   extra_rng ^= extra_rng << 13;
   extra_rng ^= extra_rng >> 17;
   extra_rng ^= extra_rng << 5;
   return (png_byte)extra_rng;
}
static png_byte *
png_ps2_extra_align16(png_byte *p)
{
   return p + ((16U - ((size_t)p & 15U)) & 15U);
}

static void
png_ps2_extra_write_reference(png_byte *row, const png_byte *prev,
    size_t n, unsigned int filter)
{
   size_t i;
   for (i = n; i-- > 0;)
   {
      unsigned int left = i >= 4 ? row[i - 4] : 0;
      unsigned int above = prev[i];
      unsigned int upper_left = i >= 4 ? prev[i - 4] : 0;
      unsigned int predictor = filter == 0 ? above :
          filter == 1 ? left :
          filter == 2 ? (left + above) >> 1 :
          png_ps2_extra_paeth(left, above, upper_left);
      row[i] = (png_byte)((unsigned int)row[i] - predictor);
   }
}

static int
png_ps2_test_extra(void)
{
   unsigned int mode, align, repeat, filter;
   size_t n, i;
   unsigned long write_cases = 0, palette_cases = 0;
   png_byte *row_base = png_ps2_extra_align16(extra_raw);
   png_byte *prev_base = png_ps2_extra_align16(extra_prev);
   png_byte *dst_base = png_ps2_extra_align16(extra_dst);
   png_byte *expected_base = png_ps2_extra_align16(extra_expected);
   png_byte *before_base = png_ps2_extra_align16(extra_before);

   for (repeat = 0; repeat < 2; ++repeat)
      for (filter = 0; filter < 4; ++filter)
         for (align = 0; align < 16; ++align)
            for (n = 0; n <= PS2_EXTRA_MAX; ++n)
            {
               png_row_info ri;
               png_byte *row = row_base + align;
               png_byte *prev = prev_base + ((align * 7U + repeat) & 15U);
               for (i = 0; i < n + 16; ++i)
               {
                  row[i] = i < n ? png_ps2_extra_random() : 0xa5;
                  prev[i] = i < n ? png_ps2_extra_random() : 0x5a;
               }
               memcpy(expected_base, row, n + 16);
               memcpy(before_base, prev, n + 16);
               ri.rowbytes = n;
               png_ps2_extra_write_reference(expected_base, prev, n, filter);
               if (filter == 0)
                  png_ps2_write_up_mmi(&ri, row, prev);
               else if (filter == 1)
                  png_ps2_write_sub4_mmi(&ri, row, prev);
               else if (filter == 2)
                  png_ps2_write_avg4_mmi(&ri, row, prev);
               else
                  png_ps2_write_paeth4_mmi(&ri, row, prev);
               if (memcmp(row, expected_base, n + 16) != 0 ||
                   memcmp(prev, before_base, n + 16) != 0)
               {
                  printf("EXTRA_FAIL,write,%u,%lu,%u,%u\n", filter,
                      (unsigned long)n, align, repeat);
                  return 1;
               }
               ++write_cases;
            }

   for (i = 0; i < sizeof extra_palette; ++i)
      extra_palette[i] = png_ps2_extra_random();

   for (mode = 0; mode < 2; ++mode)
      for (align = 0; align < 16; ++align)
         for (n = 0; n <= PS2_EXTRA_MAX; ++n)
         {
            png_byte *src = row_base + align;
            png_byte *dst = mode ? src : dst_base + align;
            png_byte *expect = expected_base + align;
            png_byte *saved = before_base + align;
            for (i = 0; i < n + 16; ++i)
               src[i] = i < n ? png_ps2_extra_random() : 0x5a;
            memcpy(saved, src, n + 16);
            memset(dst + 4 * n, 0xa5, 16);
            memset(expect + 4 * n, 0xa5, 16);

            for (i = 0; i < n; ++i)
               memcpy(expect + 4 * i,
                   extra_palette + 4 * (size_t)saved[i], 4);
            png_ps2_expand_palette_rgba4(dst, src, n, extra_palette);
            if (memcmp(dst, expect, 4 * n + 16) != 0 ||
                (!mode && memcmp(src, saved, n + 16) != 0))
            {
               printf("EXTRA_FAIL,palette,%u,%lu,%u\n",
                   mode, (unsigned long)n, align);
               return 1;
            }
            ++palette_cases;
         }

   printf("EXTRA_PASS,write=%lu,palette=%lu\n", write_cases, palette_cases);
   return 0;
}
