/* Host source-level Up filter tests, including alignment prologue,
 * vector blocks and scalar tails.  The EE PADDB loop is simulated.
 * SPDX-License-Identifier: libpng-2.0
 */
#include <stddef.h>
#include <stdio.h>
#include <string.h>
typedef unsigned char png_byte;
typedef struct { size_t rowbytes; } png_row_info;
#define PNG_PS2_UP_PORTABLE_ADD 1
#ifdef PNG_PS2_UP_2X_HOST_TEST
#include "filter_up_unrolled_mmi.c"
#define png_read_filter_row_up_ps2 png_read_filter_row_up_2x_ps2
#else
#include "filter_up_mmi.c"
#endif
#define NMAX 1024U
#define CAP (NMAX + 64U)
static png_byte rows[CAP], prevs[CAP], expected[CAP], savedprev[CAP];
static unsigned int rng = 0x0b115b1dU;
static unsigned int rnd(void)
{
   rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
   return rng;
}
static png_byte *aligned16(png_byte *p)
{
   return p + ((16U - ((size_t)p & 15U)) & 15U);
}
int main(void)
{
   unsigned int row_offset, prev_offset;
   size_t n, i;
   unsigned long cases = 0;
   for (row_offset = 0; row_offset < 16; ++row_offset)
      for (prev_offset = 0; prev_offset < 16; ++prev_offset)
         for (n = 0; n <= NMAX; ++n)
         {
            png_byte *row = aligned16(rows) + row_offset;
            png_byte *prev = aligned16(prevs) + prev_offset;
            png_row_info ri;
            for (i = 0; i < n + 16; ++i)
            {
               row[i] = i < n ? (png_byte)rnd() : 0xa5;
               prev[i] = i < n ? (png_byte)rnd() : 0x5a;
            }
            memcpy(expected, row, n + 16);
            memcpy(savedprev, prev, n + 16);
            for (i = 0; i < n; ++i)
               expected[i] = (png_byte)((unsigned int)expected[i] +
                   (unsigned int)prev[i]);
            ri.rowbytes = n;
            png_read_filter_row_up_ps2(&ri, row, prev);
            if (memcmp(row, expected, n + 16) ||
                memcmp(prev, savedprev, n + 16))
            {
               printf("FAIL: Up n=%lu offsets=%u/%u\n", (unsigned long)n,
                   row_offset, prev_offset);
               return 1;
            }
            ++cases;
         }
   printf("PASS: %lu Up source-level regression cases\n", cases);
   return 0;
}
