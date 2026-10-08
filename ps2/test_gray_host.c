/* Host test compiles the same PS2 grayscale filter source with a portable
 * PADDB emulation. This is not an EE assembler/execution test.
 * SPDX-License-Identifier: libpng-2.0
 */
#include <stddef.h>
#include <stdio.h>
#include <string.h>
typedef unsigned char png_byte;
typedef unsigned int png_uint_32;
typedef struct { size_t rowbytes; } png_row_info;
#define PNG_PS2_GRAY_PORTABLE_ADD 1
#define PNG_PS2_EE_MMI_GRAY_AVG 1
#include "filter_gray_mmi.c"

#define NMAX 1024U
#define CAP (NMAX + 64U)
static png_byte rows[CAP], above[CAP], expected[CAP], saved_above[CAP];
static unsigned int rng = 0x1a7325b9U;
static unsigned int rnd(void)
{
   rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
   return rng;
}
static png_byte *aligned16(png_byte *p)
{
   return p + ((16U - ((size_t)p & 15U)) & 15U);
}
static void reference(png_byte *row, const png_byte *prev, size_t n,
    unsigned int bpp, unsigned int is_avg)
{
   size_t i;
   for (i = 0; i < n; ++i)
   {
      unsigned int left = i >= bpp ? row[i - bpp] : 0;
      unsigned int predict = is_avg ? (left + prev[i]) >> 1 : left;
      row[i] = (png_byte)((unsigned int)row[i] + predict);
   }
}
int main(void)
{
   unsigned int bpp, is_avg, offset, rep;
   size_t n, i;
   unsigned long cases = 0;
   for (rep = 0; rep < 3; ++rep)
      for (bpp = 1; bpp <= 2; ++bpp)
         for (is_avg = 0; is_avg <= 1; ++is_avg)
            for (offset = 0; offset < 16; ++offset)
               for (n = 0; n <= NMAX; ++n)
               {
                  png_row_info ri;
                  png_byte *row = aligned16(rows) + offset;
                  png_byte *prev = aligned16(above) + ((offset * 7) & 15U);
                  for (i = 0; i < n + 16; ++i)
                  {
                     row[i] = i < n ? (png_byte)rnd() : 0xa5;
                     prev[i] = i < n ? (png_byte)rnd() : 0x5a;
                  }
                  memcpy(expected, row, n + 16);
                  memcpy(saved_above, prev, n + 16);
                  ri.rowbytes = n;
                  reference(expected, prev, n, bpp, is_avg);
                  if (bpp == 1 && !is_avg)
                     png_read_filter_row_sub1_ps2(&ri, row, prev);
                  if (bpp == 2 && !is_avg)
                     png_read_filter_row_sub2_ps2(&ri, row, prev);
                  if (bpp == 1 && is_avg)
                     png_read_filter_row_avg1_ps2(&ri, row, prev);
                  if (bpp == 2 && is_avg)
                     png_read_filter_row_avg2_ps2(&ri, row, prev);
                  if (memcmp(row, expected, n + 16) ||
                      memcmp(prev, saved_above, n + 16))
                  {
                     printf("FAIL bpp=%u avg=%u n=%lu offset=%u rep=%u\n",
                         bpp, is_avg, (unsigned long)n, offset, rep);
                     return 1;
                  }
                  ++cases;
               }
   printf("PASS: %lu gray MMI source-level regression cases\n", cases);
   return 0;
}
