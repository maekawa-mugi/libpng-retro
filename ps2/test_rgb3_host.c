/* Host-side memory-safety and arithmetic regression of the exact RGB3
 * filter source, using a portable equivalent of the EE PADDB instruction.
 */
#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef unsigned char png_byte;
typedef unsigned int png_uint_32;
typedef struct { size_t rowbytes; } png_row_info;

#define PNG_PS2_RGB3_PORTABLE_ADD 1
#include "filter_rgb3.c"

#define MAX_LENGTH 1024U
#define STORAGE (MAX_LENGTH + 64U)
static png_byte row_base[STORAGE], prev_base[STORAGE];
static png_byte expected[STORAGE], prev_expected[STORAGE];
static unsigned int rng = 0xa93745b1U;

static unsigned int
rnd(void)
{
   rng ^= rng << 13;
   rng ^= rng >> 17;
   rng ^= rng << 5;
   return rng;
}

static png_byte *align16(png_byte *p)
{
   return p + ((16U - ((size_t)p & 15U)) & 15U);
}

static void sub3_ref(png_byte *row, size_t n)
{
   size_t i;
   for (i = 3; i < n; ++i)
      row[i] = (png_byte)(row[i] + row[i - 3]);
}

static void avg3_ref(png_byte *row, const png_byte *prev, size_t n)
{
   size_t i;
   for (i = 0; i < n; ++i)
   {
      unsigned int left = i >= 3 ? row[i - 3] : 0;
      row[i] = (png_byte)(row[i] + ((left + prev[i]) / 2));
   }
}

int main(void)
{
   unsigned int filter, offset, repeats;
   size_t n, i;
   unsigned long cases = 0;
   for (repeats = 0; repeats < 3; ++repeats)
      for (filter = 0; filter < 2; ++filter)
         for (offset = 0; offset < 16; ++offset)
            for (n = 0; n <= MAX_LENGTH; ++n)
            {
               png_row_info ri;
               png_byte *row = align16(row_base) + offset;
               png_byte *prev = align16(prev_base) + ((offset * 7) & 15U);
               for (i = 0; i < n + 16; ++i)
               {
                  row[i] = i < n ? (png_byte)rnd() : 0xA5;
                  prev[i] = i < n ? (png_byte)rnd() : 0x5A;
               }
               memcpy(expected, row, n + 16);
               memcpy(prev_expected, prev, n + 16);
               ri.rowbytes = n;
               if (filter == 0)
               {
                  sub3_ref(expected, n);
                  png_read_filter_row_sub3_ps2(&ri, row, prev);
               }
               else
               {
                  avg3_ref(expected, prev, n);
                  png_read_filter_row_avg3_ps2(&ri, row, prev);
               }
               if (memcmp(row, expected, n + 16) ||
                   memcmp(prev, prev_expected, n + 16))
               {
                  printf("FAIL: %s len=%lu offset=%u repetition=%u\n",
                      filter == 0 ? "Sub3" : "Average3", (unsigned long)n,
                      offset, repeats);
                  return 1;
               }
               ++cases;
            }
   printf("PASS: %lu RGB3 source-level regression cases\n", cases);
   return 0;
}
