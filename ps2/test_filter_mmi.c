/* ps2/test_filter_mmi.c - small independent EE MMI row-filter test
 *
 * Build as an EE program (PS2SDK), not as part of libpng.
 * This code is released under the libpng license.
 */
#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef unsigned char png_byte;
typedef unsigned int png_uint_32;
typedef struct png_row_info_test_struct
{
   size_t rowbytes;
} png_row_info;

#include "filter_mmi.c"

#define TEST_MAX 1024U
#define BUF_SIZE (TEST_MAX + 64U)

static png_byte row_storage[BUF_SIZE];
static png_byte prev_storage[BUF_SIZE];
static png_byte expected[BUF_SIZE];
static png_byte prev_original[BUF_SIZE];
static unsigned int rng_state = 0x735a2dc1U;

static png_byte *
aligned16(png_byte *p)
{
   return p + ((16U - ((size_t)p & 15U)) & 15U);
}

static png_byte
random_byte(void)
{
   rng_state ^= rng_state << 13;
   rng_state ^= rng_state >> 17;
   rng_state ^= rng_state << 5;
   return (png_byte)rng_state;
}

static void
reference_up(png_byte *row, const png_byte *prev, size_t n)
{
   size_t i;
   for (i = 0; i < n; ++i)
      row[i] = (png_byte)((unsigned int)row[i] + (unsigned int)prev[i]);
}

static void
reference_sub4(png_byte *row, size_t n)
{
   size_t i;
   for (i = 4; i < n; ++i)
      row[i] = (png_byte)((unsigned int)row[i] + (unsigned int)row[i - 4]);
}

static void
reference_sub3(png_byte *row, size_t n)
{
   size_t i;
   for (i = 3; i < n; ++i)
      row[i] = (png_byte)((unsigned int)row[i] + (unsigned int)row[i - 3]);
}

static void
reference_avg3(png_byte *row, const png_byte *prev, size_t n)
{
   size_t i;
   for (i = 0; i < n; ++i)
   {
      unsigned int left = i >= 3 ? row[i - 3] : 0;
      row[i] = (png_byte)((unsigned int)row[i] + ((left + prev[i]) >> 1));
   }
}

static void
reference_avg4(png_byte *row, const png_byte *prev, size_t n)
{
   size_t i;
   for (i = 0; i < n; ++i)
   {
      unsigned int left = i >= 4 ? row[i - 4] : 0;
      row[i] = (png_byte)((unsigned int)row[i] +
          ((left + (unsigned int)prev[i]) >> 1));
   }
}

#ifdef PNG_PS2_EE_MMI_PAETH
static unsigned int
paeth_abs_test(int value)
{
   return (unsigned int)(value < 0 ? -value : value);
}
static unsigned int
paeth_predict_test(unsigned int a, unsigned int b, unsigned int c)
{
   int p = (int)a + (int)b - (int)c;
   unsigned int da = paeth_abs_test(p - (int)a);
   unsigned int db = paeth_abs_test(p - (int)b);
   unsigned int dc = paeth_abs_test(p - (int)c);
   if (da <= db && da <= dc) return a;
   if (db <= dc) return b;
   return c;
}
static void
reference_paeth(png_byte *row, const png_byte *prev, size_t n,
    unsigned int bpp)
{
   size_t i;
   for (i = 0; i < n; ++i)
   {
      unsigned int a = i >= bpp ? row[i - bpp] : 0;
      unsigned int b = prev[i];
      unsigned int c = i >= bpp ? prev[i - bpp] : 0;
      row[i] = (png_byte)((unsigned int)row[i] +
          paeth_predict_test(a, b, c));
   }
}
#endif /* PNG_PS2_EE_MMI_PAETH */

static void
reference_sub1(png_byte *row, size_t n)
{
   size_t i;
   for (i = 1; i < n; ++i)
      row[i] = (png_byte)((unsigned int)row[i] + (unsigned int)row[i - 1]);
}

static void
reference_sub2(png_byte *row, size_t n)
{
   size_t i;
   for (i = 2; i < n; ++i)
      row[i] = (png_byte)((unsigned int)row[i] + (unsigned int)row[i - 2]);
}

#ifdef PNG_PS2_EE_MMI_GRAY_AVG
static void
reference_avg1(png_byte *row, const png_byte *prev, size_t n)
{
   size_t i;
   for (i = 0; i < n; ++i)
   {
      unsigned int left = i ? row[i - 1] : 0;
      row[i] = (png_byte)((unsigned int)row[i] + ((left + prev[i]) >> 1));
   }
}
static void
reference_avg2(png_byte *row, const png_byte *prev, size_t n)
{
   size_t i;
   for (i = 0; i < n; ++i)
   {
      unsigned int left = i >= 2 ? row[i - 2] : 0;
      row[i] = (png_byte)((unsigned int)row[i] + ((left + prev[i]) >> 1));
   }
}
#endif

static void
reference_wide_sub(png_byte *row, size_t n, unsigned int bpp)
{
   size_t i;
   for (i = bpp; i < n; ++i)
      row[i] = (png_byte)((unsigned int)row[i] + (unsigned int)row[i - bpp]);
}
#ifdef PNG_PS2_EE_MMI_WIDE_AVG
static void
reference_wide_avg(png_byte *row, const png_byte *prev, size_t n,
    unsigned int bpp)
{
   size_t i;
   for (i = 0; i < n; ++i)
   {
      unsigned int left = i >= bpp ? row[i - bpp] : 0;
      row[i] = (png_byte)((unsigned int)row[i] +
          ((left + (unsigned int)prev[i]) >> 1));
   }
}
#endif

#ifdef PNG_PS2_EE_MMI_GRAY_AVG
#define PNG_PS2_TEST_GRAY_AVG_COUNT 2
#else
#define PNG_PS2_TEST_GRAY_AVG_COUNT 0
#endif
#ifdef PNG_PS2_EE_MMI_PAETH
#define PNG_PS2_TEST_PAETH_COUNT 4
#else
#define PNG_PS2_TEST_PAETH_COUNT 0
#endif
#define PNG_PS2_TEST_PAETH_START (7 + PNG_PS2_TEST_GRAY_AVG_COUNT)
#define PNG_PS2_TEST_WIDE_SUB_START (PNG_PS2_TEST_PAETH_START + PNG_PS2_TEST_PAETH_COUNT)
#ifdef PNG_PS2_EE_MMI_WIDE_AVG
#define PNG_PS2_TEST_WIDE_AVG_COUNT 2
#else
#define PNG_PS2_TEST_WIDE_AVG_COUNT 0
#endif
#define PNG_PS2_TEST_WIDE_AVG_START (PNG_PS2_TEST_WIDE_SUB_START + 2)
#define PNG_PS2_TEST_WIDE_PAETH_START (PNG_PS2_TEST_WIDE_AVG_START + PNG_PS2_TEST_WIDE_AVG_COUNT)
#ifdef PNG_PS2_EE_MMI_PAETH
#define PNG_PS2_TEST_WIDE_PAETH_COUNT 2
#else
#define PNG_PS2_TEST_WIDE_PAETH_COUNT 0
#endif
#define PNG_PS2_TEST_TOTAL (PNG_PS2_TEST_WIDE_PAETH_START + PNG_PS2_TEST_WIDE_PAETH_COUNT)

int
main(void)
{
   unsigned int filter;
   unsigned int offset;
   size_t len;
   unsigned int cases = 0;

   for (filter = 0; filter < PNG_PS2_TEST_TOTAL; ++filter)
   {
      for (offset = 0; offset < 16; ++offset)
      {
         for (len = 0; len <= TEST_MAX; ++len)
         {
            png_row_info row_info;
            png_byte *row = aligned16(row_storage) + offset;
            png_byte *prev = aligned16(prev_storage) + ((offset * 7) & 15U);
            size_t i;

            for (i = 0; i < len + 16; ++i)
            {
               row[i] = i < len ? random_byte() : 0xa5;
               prev[i] = i < len ? random_byte() : 0x5a;
            }

            memcpy(expected, row, len + 16);
            memcpy(prev_original, prev, len + 16);
            row_info.rowbytes = len;

            if (filter == 0)
            {
               reference_up(expected, prev, len);
               png_read_filter_row_up_ps2(&row_info, row, prev);
            }
            else if (filter == 1)
            {
               reference_sub4(expected, len);
               png_read_filter_row_sub4_ps2(&row_info, row, prev);
            }
            else if (filter == 2)
            {
               reference_avg4(expected, prev, len);
               png_read_filter_row_avg4_ps2(&row_info, row, prev);
            }
            else if (filter == 3)
            {
               reference_sub3(expected, len);
               png_read_filter_row_sub3_ps2(&row_info, row, prev);
            }
            else if (filter == 4)
            {
               reference_avg3(expected, prev, len);
               png_read_filter_row_avg3_ps2(&row_info, row, prev);
            }
            else if (filter == 5)
            {
               reference_sub1(expected, len);
               png_read_filter_row_sub1_ps2(&row_info, row, prev);
            }
            else if (filter == 6)
            {
               reference_sub2(expected, len);
               png_read_filter_row_sub2_ps2(&row_info, row, prev);
            }
#ifdef PNG_PS2_EE_MMI_GRAY_AVG
            else if (filter == 7)
            {
               reference_avg1(expected, prev, len);
               png_read_filter_row_avg1_ps2(&row_info, row, prev);
            }
            else if (filter == 8)
            {
               reference_avg2(expected, prev, len);
               png_read_filter_row_avg2_ps2(&row_info, row, prev);
            }
#endif
#ifdef PNG_PS2_EE_MMI_PAETH
            else if (filter < PNG_PS2_TEST_WIDE_SUB_START)
            {
               unsigned int bpp = filter - PNG_PS2_TEST_PAETH_START + 1;
               reference_paeth(expected, prev, len, bpp);
               if (bpp == 1)
                  png_read_filter_row_paeth1_ps2(&row_info, row, prev);
               else if (bpp == 2)
                  png_read_filter_row_paeth2_ps2(&row_info, row, prev);
               else if (bpp == 3)
                  png_read_filter_row_paeth3_ps2(&row_info, row, prev);
               else
                  png_read_filter_row_paeth4_ps2(&row_info, row, prev);
            }
#endif
            else if (filter == PNG_PS2_TEST_WIDE_SUB_START)
            {
               reference_wide_sub(expected, len, 6);
               png_read_filter_row_sub6_ps2(&row_info, row, prev);
            }
            else if (filter == PNG_PS2_TEST_WIDE_SUB_START + 1)
            {
               reference_wide_sub(expected, len, 8);
               png_read_filter_row_sub8_ps2(&row_info, row, prev);
            }
#ifdef PNG_PS2_EE_MMI_WIDE_AVG
            else if (filter == PNG_PS2_TEST_WIDE_AVG_START)
            {
               reference_wide_avg(expected, prev, len, 6);
               png_read_filter_row_avg6_ps2(&row_info, row, prev);
            }
            else if (filter == PNG_PS2_TEST_WIDE_AVG_START + 1)
            {
               reference_wide_avg(expected, prev, len, 8);
               png_read_filter_row_avg8_ps2(&row_info, row, prev);
            }
#endif
#ifdef PNG_PS2_EE_MMI_PAETH
            else if (filter == PNG_PS2_TEST_WIDE_PAETH_START)
            {
               reference_paeth(expected, prev, len, 6);
               png_read_filter_row_paeth6_ps2(&row_info, row, prev);
            }
            else
            {
               reference_paeth(expected, prev, len, 8);
               png_read_filter_row_paeth8_ps2(&row_info, row, prev);
            }
#endif

            if (memcmp(expected, row, len + 16) != 0 ||
                memcmp(prev_original, prev, len + 16) != 0)
            {
               printf("FAIL: %s len=%lu offset=%u\n",
                   filter == 0 ? "Up" : filter == 1 ? "Sub4" :
                       filter == 2 ? "Average4" :
                       filter == 3 ? "Sub3" :
                       filter == 4 ? "Average3" :
                       filter == 5 ? "Sub1" :
                       filter == 6 ? "Sub2" :
                       filter < PNG_PS2_TEST_PAETH_START ?
                          (filter == 7 ? "Average1" : "Average2") :
                       filter < PNG_PS2_TEST_WIDE_SUB_START ?
                          (filter == PNG_PS2_TEST_PAETH_START ? "Paeth1" :
                           filter == PNG_PS2_TEST_PAETH_START + 1 ? "Paeth2" :
                           filter == PNG_PS2_TEST_PAETH_START + 2 ? "Paeth3" : "Paeth4") :
                       filter == PNG_PS2_TEST_WIDE_SUB_START ? "Sub6" :
                       filter == PNG_PS2_TEST_WIDE_SUB_START + 1 ? "Sub8" :
                       filter == PNG_PS2_TEST_WIDE_AVG_START ? "Average6" :
                       filter == PNG_PS2_TEST_WIDE_AVG_START + 1 ? "Average8" :
                       filter == PNG_PS2_TEST_WIDE_PAETH_START ? "Paeth6" : "Paeth8",
                   (unsigned long)len, offset);
               return 1;
            }
            ++cases;
         }
      }
   }

   printf("PASS: %u PS2 EE MMI filter cases\n", cases);
   return 0;
}
