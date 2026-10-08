/* PS2 EE MMI read filters for one and two bytes per pixel.
 * Included from filter_mmi.c, never compiled as a translation unit.
 * SPDX-License-Identifier: libpng-2.0
 *
 * The default path accelerates Sub1 and Sub2. Average1/2 are registered
 * only when PNG_PS2_EE_MMI_GRAY_AVG is explicitly enabled, since their
 * horizontal dependencies may make them slower than the generic C filters.
 */

static png_uint_32
png_ps2_gray_load4(const png_byte *p)
{
   return (png_uint_32)p[0] | ((png_uint_32)p[1] << 8) |
       ((png_uint_32)p[2] << 16) | ((png_uint_32)p[3] << 24);
}

static void
png_ps2_gray_store4(png_byte *p, png_uint_32 v)
{
   p[0] = (png_byte)v;
   p[1] = (png_byte)(v >> 8);
   p[2] = (png_byte)(v >> 16);
   p[3] = (png_byte)(v >> 24);
}

static png_uint_32
png_ps2_gray_add4(png_uint_32 a, png_uint_32 b)
{
#ifdef PNG_PS2_GRAY_PORTABLE_ADD
   png_uint_32 result = 0;
   unsigned int i;
   for (i = 0; i < 4; ++i)
   {
      unsigned int s = 8 * i;
      unsigned int x = (a >> s) & 255U;
      unsigned int y = (b >> s) & 255U;
      result |= (png_uint_32)((x + y) & 255U) << s;
   }
   return result;
#else
   png_uint_32 result;
   __asm__ volatile ("paddb %0, %1, %2" : "=&r" (result) : "r" (a), "r" (b));
   return result;
#endif
}

/* Parallel prefix scan for bpp=1, four contiguous bytes per group.
 * Inject carry into the first lane, then scan offsets 1 and 2 bytes.
 */
static void
png_read_filter_row_sub1_ps2(png_row_info *row_info, png_byte *row,
    const png_byte *prev_row)
{
   size_t i = 0, n = row_info->rowbytes;
   png_uint_32 carry = 0;
   (void)prev_row;

   for (; n - i >= 4; i += 4)
   {
      png_uint_32 scan = png_ps2_gray_add4(png_ps2_gray_load4(row + i), carry);
      scan = png_ps2_gray_add4(scan, scan << 8);
      scan = png_ps2_gray_add4(scan, scan << 16);
      png_ps2_gray_store4(row + i, scan);
      carry = scan >> 24;
   }
   for (; i < n; ++i)
      row[i] = (png_byte)((unsigned int)row[i] +
          (i == 0 ? 0U : (unsigned int)row[i - 1]));
}

/* Parallel prefix scan for bpp=2, two pixels per group.
 * Inject the previous 2-byte pixel into low lanes, then shift 2 bytes.
 */
static void
png_read_filter_row_sub2_ps2(png_row_info *row_info, png_byte *row,
    const png_byte *prev_row)
{
   size_t i = 0, n = row_info->rowbytes;
   png_uint_32 carry = 0;
   (void)prev_row;

   for (; n - i >= 4; i += 4)
   {
      png_uint_32 scan = png_ps2_gray_add4(png_ps2_gray_load4(row + i), carry);
      scan = png_ps2_gray_add4(scan, scan << 16);
      png_ps2_gray_store4(row + i, scan);
      carry = scan >> 16;
   }
   for (; i < n; ++i)
      row[i] = (png_byte)((unsigned int)row[i] +
          (i < 2 ? 0U : (unsigned int)row[i - 2]));
}

#ifdef PNG_PS2_EE_MMI_GRAY_AVG
/* Exact floor average for each 8-bit lane without carries between lanes. */
static png_uint_32
png_ps2_gray_average4(png_uint_32 a, png_uint_32 b)
{
   return (a & b) + (((a ^ b) & 0xfefefefeU) >> 1);
}

/* bpp=1 has a dependency every byte; opt-in only for benchmarking. */
static void
png_read_filter_row_avg1_ps2(png_row_info *row_info, png_byte *row,
    const png_byte *prev_row)
{
   size_t i, n = row_info->rowbytes;
   png_uint_32 left = 0;
   for (i = 0; i < n; ++i)
   {
      png_uint_32 predict = png_ps2_gray_average4(left, prev_row[i]);
      left = png_ps2_gray_add4(row[i], predict) & 255U;
      row[i] = (png_byte)left;
   }
}

/* bpp=2 processes a 16-bit pixel using two lanes of PADDB. */
static void
png_read_filter_row_avg2_ps2(png_row_info *row_info, png_byte *row,
    const png_byte *prev_row)
{
   size_t i = 0, n = row_info->rowbytes;
   png_uint_32 left = 0;
   for (; n - i >= 2; i += 2)
   {
      png_uint_32 up = (png_uint_32)prev_row[i] |
          ((png_uint_32)prev_row[i + 1] << 8);
      png_uint_32 raw = (png_uint_32)row[i] |
          ((png_uint_32)row[i + 1] << 8);
      png_uint_32 predict = png_ps2_gray_average4(left, up);
      left = png_ps2_gray_add4(raw, predict) & 0xffffU;
      row[i] = (png_byte)left;
      row[i + 1] = (png_byte)(left >> 8);
   }
   for (; i < n; ++i)
   {
      unsigned int a = i < 2 ? 0U : (unsigned int)row[i - 2];
      row[i] = (png_byte)((unsigned int)row[i] +
          ((a + (unsigned int)prev_row[i]) >> 1));
   }
}
#endif /* PNG_PS2_EE_MMI_GRAY_AVG */
