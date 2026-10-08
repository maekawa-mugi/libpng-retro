/* Source-level RGB16/RGBA16 Sub, Average, Paeth regressions.
 * The real filtering loops are compiled with PADDB emulation on host.
 * This does not execute R5900 instructions.
 * Released under the libpng license.
 */
#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef unsigned char png_byte;
typedef unsigned int png_uint_32;
typedef struct { size_t rowbytes; } png_row_info;
#define PNG_PS2_WIDE_PORTABLE_ADD 1
#define PNG_PS2_EE_MMI_WIDE_AVG 1
#define PNG_PS2_EE_MMI_PAETH 1
#include "filter_wide_mmi.c"

#define NMAX 1024U
#define STORAGE (NMAX + 64U)
static png_byte row_storage[STORAGE], prev_storage[STORAGE];
static png_byte expected[STORAGE], saved_prev[STORAGE];
static unsigned int state = 0x7a43c592U;

static unsigned int
random_uint(void)
{
   state ^= state << 13;
   state ^= state >> 17;
   state ^= state << 5;
   return state;
}

static png_byte *
aligned16(png_byte *p)
{
   return p + ((16U - ((size_t)p & 15U)) & 15U);
}

static unsigned int
paeth_reference(unsigned int a, unsigned int b, unsigned int c)
{
   int p = (int)a + (int)b - (int)c;
   int pa = p - (int)a;
   int pb = p - (int)b;
   int pc = p - (int)c;
   pa = pa < 0 ? -pa : pa;
   pb = pb < 0 ? -pb : pb;
   pc = pc < 0 ? -pc : pc;
   if (pa <= pb && pa <= pc) return a;
   if (pb <= pc) return b;
   return c;
}

static void
reference(png_byte *row, const png_byte *prev, size_t n,
    unsigned int bpp, unsigned int filter)
{
   size_t i;
   for (i = 0; i < n; ++i)
   {
      unsigned int a = i >= bpp ? row[i - bpp] : 0;
      unsigned int b = prev[i];
      unsigned int c = i >= bpp ? prev[i - bpp] : 0;
      unsigned int predictor = filter == 0 ? a :
          filter == 1 ? (a + b) >> 1 : paeth_reference(a, b, c);
      row[i] = (png_byte)((unsigned int)row[i] + predictor);
   }
}

int
main(void)
{
   unsigned int rep, bpp, filter, offset;
   size_t n, i;
   unsigned long cases = 0;
   for (rep = 0; rep < 2; ++rep)
      for (bpp = 6; bpp <= 8; bpp += 2)
         for (filter = 0; filter < 3; ++filter)
            for (offset = 0; offset < 16; ++offset)
               for (n = 0; n <= NMAX; ++n)
               {
                  png_row_info ri;
                  png_byte *row = aligned16(row_storage) + offset;
                  png_byte *prev = aligned16(prev_storage) +
                      ((offset * 11U + rep) & 15U);
                  for (i = 0; i < n + 16; ++i)
                  {
                     row[i] = i < n ? (png_byte)random_uint() : 0xa5;
                     prev[i] = i < n ? (png_byte)random_uint() : 0x5a;
                  }
                  memcpy(expected, row, n + 16);
                  memcpy(saved_prev, prev, n + 16);
                  ri.rowbytes = n;
                  reference(expected, prev, n, bpp, filter);
                  if (filter == 0)
                  {
                     if (bpp == 6) png_read_filter_row_sub6_ps2(&ri, row, prev);
                     else png_read_filter_row_sub8_ps2(&ri, row, prev);
                  }
                  else if (filter == 1)
                  {
                     if (bpp == 6) png_read_filter_row_avg6_ps2(&ri, row, prev);
                     else png_read_filter_row_avg8_ps2(&ri, row, prev);
                  }
                  else
                  {
                     if (bpp == 6) png_read_filter_row_paeth6_ps2(&ri, row, prev);
                     else png_read_filter_row_paeth8_ps2(&ri, row, prev);
                  }
                  if (memcmp(row, expected, n + 16) != 0 ||
                      memcmp(prev, saved_prev, n + 16) != 0)
                  {
                     printf("FAIL wide bpp=%u filter=%u n=%lu offset=%u rep=%u\n",
                         bpp, filter, (unsigned long)n, offset, rep);
                     return 1;
                  }
                  ++cases;
               }
   printf("PASS: %lu wide MMI source-level cases\n", cases);
   return 0;
}
