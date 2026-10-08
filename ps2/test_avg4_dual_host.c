/* Source-level correctness and canary regression for optional Average4
 * dual-pixel pipeline.  PADDB is simulated, never run on the host.
 * SPDX-License-Identifier: libpng-2.0
 */
#include <stddef.h>
#include <stdio.h>
#include <string.h>
typedef unsigned char png_byte;
typedef unsigned int png_uint_32;
#define PNG_PS2_AVG4_PORTABLE_ADD 1
#include "filter_avg4_dual_mmi.c"

#define TEST_LIMIT 1024U
#define CAPACITY (TEST_LIMIT + 64U)
static png_byte raw_storage[CAPACITY], above_storage[CAPACITY];
static png_byte expected[CAPACITY], old_above[CAPACITY], old_raw[CAPACITY];
static unsigned int rng = 0xa34ed16bU;

static unsigned int next_random(void)
{
   rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
   return rng;
}
static png_byte *align16(png_byte *p)
{
   return p + ((16U - ((size_t)p & 15U)) & 15U);
}
static void reference(png_byte *row, const png_byte *above, size_t n)
{
   size_t j;
   for (j = 0; j < n; ++j)
   {
      unsigned int left = j >= 4 ? row[j - 4] : 0;
      row[j] = (png_byte)((unsigned int)row[j] +
          ((left + (unsigned int)above[j]) >> 1));
   }
}
int main(void)
{
   static const unsigned int prev_offsets[] = {0, 3, 7, 15};
   unsigned int rep, ro, po;
   size_t n, i;
   unsigned long cases = 0, optimized = 0;
   for (rep = 0; rep < 2; ++rep)
      for (ro = 0; ro < 16; ++ro)
         for (po = 0; po < 4; ++po)
            for (n = 0; n <= TEST_LIMIT; ++n)
            {
               png_byte *row = align16(raw_storage) + ro;
               png_byte *prev = align16(above_storage) + prev_offsets[po];
               int used, eligible;
               for (i = 0; i < n + 16; ++i)
               {
                  row[i] = i < n ? (png_byte)next_random() : 0xa5;
                  prev[i] = i < n ? (png_byte)next_random() : 0x5a;
               }
               memcpy(old_raw, row, n + 16);
               memcpy(expected, row, n + 16);
               memcpy(old_above, prev, n + 16);
               reference(expected, prev, n);
               used = png_ps2_avg4_dual(row, prev, n);
               eligible = n >= 32 && (n & 7U) == 0 &&
                   (((size_t)row | (size_t)prev) & 3U) == 0;
               if (used != eligible || memcmp(old_above, prev, n + 16) != 0 ||
                   (used ? memcmp(expected, row, n + 16) :
                           memcmp(old_raw, row, n + 16)))
               {
                  printf("FAIL avg4dual n=%lu ro=%u po=%u rep=%u\n",
                      (unsigned long)n, ro, prev_offsets[po], rep);
                  return 1;
               }
               optimized += (unsigned int)used;
               ++cases;
            }
   printf("PASS avg4dual %lu cases, %lu optimized\n", cases, optimized);
   return 0;
}
