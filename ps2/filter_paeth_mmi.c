/* ps2/filter_paeth_mmi.c - experimental bpp=3/4 packed-byte Paeth.
 *
 * Included by filter_mmi.c only when PNG_PS2_EE_MMI_PAETH is enabled.
 * The host regression uses the same filtering loops with portable add.
 * Released under the libpng license.
 */

static int
png_ps2_paeth_abs(int value)
{
   return value < 0 ? -value : value;
}

/* Exact PNG tie-breaking: a first, then b, then c. */
static unsigned int
png_ps2_paeth_select(unsigned int a, unsigned int b, unsigned int c)
{
   int pa = png_ps2_paeth_abs((int)b - (int)c);
   int pb = png_ps2_paeth_abs((int)a - (int)c);
   int pc = png_ps2_paeth_abs((int)a + (int)b - 2 * (int)c);
   if (pa <= pb && pa <= pc)
      return a;
   if (pb <= pc)
      return b;
   return c;
}

/* The register value uses only its low 'bpp' bytes (bpp is 3 or 4). */
static png_uint_32
png_ps2_paeth_load(const png_byte *src, unsigned int bpp)
{
   unsigned int j;
   png_uint_32 result = 0;
   for (j = 0; j < bpp; ++j)
      result |= (png_uint_32)src[j] << (j * 8);
   return result;
}

static void
png_ps2_paeth_store(png_byte *dst, png_uint_32 value, unsigned int bpp)
{
   unsigned int j;
   for (j = 0; j < bpp; ++j)
      dst[j] = (png_byte)(value >> (j * 8));
}

static png_uint_32
png_ps2_paeth_add(png_uint_32 a, png_uint_32 b)
{
#ifdef PNG_PS2_PAETH_PORTABLE_ADD
   png_uint_32 result = 0;
   unsigned int j;
   for (j = 0; j < 4; ++j)
   {
      unsigned int sh = j * 8;
      unsigned int x = (a >> sh) & 255U;
      unsigned int y = (b >> sh) & 255U;
      result |= (png_uint_32)((x + y) & 255U) << sh;
   }
   return result;
#else
   png_uint_32 result;
   __asm__ volatile ("paddb %0, %1, %2" : "=&r" (result) : "r" (a), "r" (b));
   return result;
#endif
}

static void
png_ps2_paeth_filter(png_row_info *row_info, png_byte *row,
    const png_byte *prev_row, unsigned int bpp)
{
   size_t i, n = row_info->rowbytes;
   png_uint_32 left = 0, upper_left = 0;

   for (i = 0; n - i >= bpp; i += bpp)
   {
      png_uint_32 raw = png_ps2_paeth_load(row + i, bpp);
      png_uint_32 above = png_ps2_paeth_load(prev_row + i, bpp);
      png_uint_32 pred = 0;
      unsigned int j;
      for (j = 0; j < bpp; ++j)
      {
         unsigned int sh = j * 8;
         unsigned int a = (left >> sh) & 255U;
         unsigned int b = (above >> sh) & 255U;
         unsigned int c = (upper_left >> sh) & 255U;
         pred |= (png_uint_32)png_ps2_paeth_select(a, b, c) << sh;
      }
      left = png_ps2_paeth_add(raw, pred);
      png_ps2_paeth_store(row + i, left, bpp);
      upper_left = above;
   }
   /* Rare truncated rows used by tests or unusual callers. */
   for (; i < n; ++i)
   {
      unsigned int a = i >= bpp ? row[i - bpp] : 0;
      unsigned int b = prev_row[i];
      unsigned int c = i >= bpp ? prev_row[i - bpp] : 0;
      row[i] = (png_byte)((unsigned int)row[i] +
          png_ps2_paeth_select(a, b, c));
   }
}

static void
png_read_filter_row_paeth3_ps2(png_row_info *row_info, png_byte *row,
    const png_byte *prev_row)
{
   png_ps2_paeth_filter(row_info, row, prev_row, 3);
}

static void
png_read_filter_row_paeth4_ps2(png_row_info *row_info, png_byte *row,
    const png_byte *prev_row)
{
   png_ps2_paeth_filter(row_info, row, prev_row, 4);
}
