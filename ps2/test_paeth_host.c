/* Host-source-level Paeth 3/4 checks with scalar PADDB emulation. */
#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef unsigned char png_byte;
typedef unsigned int png_uint_32;
typedef struct { size_t rowbytes; } png_row_info;
#define PNG_PS2_PAETH_PORTABLE_ADD 1
#include "filter_paeth_mmi.c"

#define NMAX 1024U
#define SIZE (NMAX+64U)
static png_byte rows[SIZE], above[SIZE], expected[SIZE], saved_above[SIZE];
static unsigned int state = 0x5619a72eU;
static unsigned int rnd(void)
{
   state ^= state << 13;
   state ^= state >> 17;
   state ^= state << 5;
   return state;
}
static png_byte *align16(png_byte *p)
{
   return p + ((16U - ((size_t)p & 15U)) & 15U);
}
static unsigned int abs_ref(int value)
{
   return (unsigned int)(value < 0 ? -value : value);
}
static unsigned int paeth_ref(unsigned int a, unsigned int b, unsigned int c)
{
   int p = (int)a + (int)b - (int)c;
   unsigned int pa = abs_ref(p - (int)a);
   unsigned int pb = abs_ref(p - (int)b);
   unsigned int pc = abs_ref(p - (int)c);
   if (pa <= pb && pa <= pc) return a;
   if (pb <= pc) return b;
   return c;
}
static void reference(png_byte *row, const png_byte *prev, size_t n,
    size_t bpp)
{
   size_t i;
   for (i = 0; i < n; ++i)
   {
      unsigned int a = i >= bpp ? row[i - bpp] : 0;
      unsigned int b = prev[i];
      unsigned int c = i >= bpp ? prev[i - bpp] : 0;
      row[i] = (png_byte)(row[i] + paeth_ref(a, b, c));
   }
}
int main(void)
{
   size_t n, i;
   unsigned int offset, bpp, repeat;
   unsigned long cases = 0;
   for (repeat = 0; repeat < 3; ++repeat)
      for (bpp = 3; bpp <= 4; ++bpp)
         for (offset = 0; offset < 16; ++offset)
            for (n = 0; n <= NMAX; ++n)
            {
               png_row_info ri;
               png_byte *row = align16(rows) + offset;
               png_byte *prev = align16(above) + ((offset * 9) & 15U);
               for (i = 0; i < n + 16; ++i)
               {
                  row[i] = i < n ? (png_byte)rnd() : 0xA5;
                  prev[i] = i < n ? (png_byte)rnd() : 0x5A;
               }
               memcpy(expected, row, n + 16);
               memcpy(saved_above, prev, n + 16);
               ri.rowbytes = n;
               reference(expected, prev, n, bpp);
               if (bpp == 3)
                  png_read_filter_row_paeth3_ps2(&ri, row, prev);
               else
                  png_read_filter_row_paeth4_ps2(&ri, row, prev);
               if (memcmp(row, expected, n + 16) ||
                   memcmp(prev, saved_above, n + 16))
               {
                  printf("FAIL Paeth%u n=%lu offset=%u repeat=%u\n",
                      bpp, (unsigned long)n, offset, repeat);
                  return 1;
               }
               ++cases;
            }
   printf("PASS %lu Paeth source-level cases\n", cases);
   return 0;
}
