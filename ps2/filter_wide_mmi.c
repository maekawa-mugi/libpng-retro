/* ps2/filter_wide_mmi.c - PS2 EE bytewise MMI for PNG bpp=6/8.
 *
 * RGB16 and RGBA16 PNG samples use big-endian 16-bit samples, but the
 * PNG reverse filters operate on BYTES, not 16-bit sample values.
 * All packed additions here are independent modulo-256 PADDB lanes.
 * No unaligned or out-of-bounds lw/lq loads are used.
 *
 * Included by filter_mmi.c, never compiled as a separate unit.
 * Released under the libpng license.
 */

static png_uint_32
png_ps2_wide_load(const png_byte *p, unsigned int count)
{
   png_uint_32 value = 0;
   unsigned int j;
   for (j = 0; j < count; ++j)
      value |= (png_uint_32)p[j] << (j * 8);
   return value;
}

static void
png_ps2_wide_store(png_byte *p, png_uint_32 value, unsigned int count)
{
   unsigned int j;
   for (j = 0; j < count; ++j)
      p[j] = (png_byte)(value >> (j * 8));
}

static png_uint_32
png_ps2_wide_add(png_uint_32 a, png_uint_32 b)
{
#ifdef PNG_PS2_WIDE_PORTABLE_ADD
   png_uint_32 result = 0;
   unsigned int j;
   for (j = 0; j < 4; ++j)
   {
      unsigned int shift = 8 * j;
      unsigned int x = (a >> shift) & 255U;
      unsigned int y = (b >> shift) & 255U;
      result |= (png_uint_32)((x + y) & 255U) << shift;
   }
   return result;
#else
   png_uint_32 result;
   __asm__ volatile ("paddb %0, %1, %2" : "=&r" (result) : "r" (a), "r" (b));
   return result;
#endif
}

/* Additional Sub8 candidate: two independent four-byte lanes per pixel.
 * Unlike the generic packed implementation, aligned words are loaded
 * directly and processed in an EE assembly loop.  Short/misaligned and
 * truncated rows retain the existing packed-byte implementation.
 */
#ifdef PNG_PS2_EE_MMI_SUB8_WORDS
static int
png_ps2_wide_sub8_words(png_byte *row, size_t n)
{
   png_byte *ptr;
   unsigned int count;

   if (n < 32 || (n & 7U) != 0 || ((size_t)row & 3U) != 0)
      return 0;

   ptr = row + 8;
   count = (unsigned int)((n - 8U) >> 3);
#ifdef PNG_PS2_WIDE_PORTABLE_ADD
   {
      png_uint_32 low = png_ps2_wide_load(row, 4);
      png_uint_32 high = png_ps2_wide_load(row + 4, 4);
      while (count-- != 0)
      {
         low = png_ps2_wide_add(low, png_ps2_wide_load(ptr, 4));
         high = png_ps2_wide_add(high, png_ps2_wide_load(ptr + 4, 4));
         png_ps2_wide_store(ptr, low, 4);
         png_ps2_wide_store(ptr + 4, high, 4);
         ptr += 8;
      }
   }
#else
   __asm__ volatile (
      ".set push\n\t"
      ".set noreorder\n\t"
      "lw    $8, -8(%[ptr])\n\t"
      "lw    $9, -4(%[ptr])\n\t"
      "1:\n\t"
      "lw    $10, 0(%[ptr])\n\t"
      "lw    $11, 4(%[ptr])\n\t"
      "paddb $8, $8, $10\n\t"
      "paddb $9, $9, $11\n\t"
      "sw    $8, 0(%[ptr])\n\t"
      "sw    $9, 4(%[ptr])\n\t"
      "addiu %[ptr], %[ptr], 8\n\t"
      "addiu %[count], %[count], -1\n\t"
      "bnez  %[count], 1b\n\t"
      "nop\n\t"
      ".set pop\n\t"
      : [ptr] "+r" (ptr), [count] "+r" (count)
      :
      : "$8", "$9", "$10", "$11", "memory"
   );
#endif
   return 1;
}
#endif

#ifdef PNG_PS2_EE_MMI_SUB8_PREFIX16
#include "filter_sub8_prefix_mmi.c"
#endif

#if defined(PNG_PS2_EE_MMI_SUB6_PREFIX16)
#include "filter_sub6_prefix_mmi.c"
#endif

/* bpp=6 uses a 4+2 layout; bpp=8 uses 4+4. For each pixel,
 * decoded[i] = (raw[i] + decoded[i-bpp]) mod 256 independently.
 */
static void
png_ps2_wide_sub(png_row_info *row_info, png_byte *row,
    const png_byte *prev_row, unsigned int bpp)
{
   size_t i = 0, n = row_info->rowbytes;
   unsigned int last_count = bpp - 4;
   png_uint_32 left0 = 0, left1 = 0;
   (void)prev_row;

   for (; n - i >= bpp; i += bpp)
   {
      png_uint_32 raw0 = png_ps2_wide_load(row + i, 4);
      png_uint_32 raw1 = png_ps2_wide_load(row + i + 4, last_count);
      left0 = png_ps2_wide_add(raw0, left0);
      left1 = png_ps2_wide_add(raw1, left1);
      png_ps2_wide_store(row + i, left0, 4);
      png_ps2_wide_store(row + i + 4, left1, last_count);
   }
   for (; i < n; ++i)
      row[i] = (png_byte)((unsigned int)row[i] +
          (i >= bpp ? (unsigned int)row[i - bpp] : 0U));
}

static void
png_read_filter_row_sub6_ps2(png_row_info *row_info, png_byte *row,
    const png_byte *prev_row)
{
#if defined(PNG_PS2_EE_MMI_SUB6_PREFIX16)
   if (png_ps2_wide_sub6_prefix16(row, row_info->rowbytes))
      return;
#endif
   png_ps2_wide_sub(row_info, row, prev_row, 6);
}

static void
png_read_filter_row_sub8_ps2(png_row_info *row_info, png_byte *row,
    const png_byte *prev_row)
{
#if defined(PNG_PS2_EE_MMI_SUB8_PREFIX16)
   if (png_ps2_wide_sub8_prefix16(row, row_info->rowbytes))
      return;
#endif
#if defined(PNG_PS2_EE_MMI_SUB8_WORDS)
   if (png_ps2_wide_sub8_words(row, row_info->rowbytes))
      return;
#endif
   png_ps2_wide_sub(row_info, row, prev_row, 8);
}

#ifdef PNG_PS2_EE_MMI_WIDE_AVG
/* floor((a+b)/2) on four independent bytes, no cross-lane carry. */
static png_uint_32
png_ps2_wide_average(png_uint_32 a, png_uint_32 b)
{
   return (a & b) + (((a ^ b) & 0xfefefefeU) >> 1);
}

static void
png_ps2_wide_avg(png_row_info *row_info, png_byte *row,
    const png_byte *prev_row, unsigned int bpp)
{
   size_t i = 0, n = row_info->rowbytes;
   unsigned int last_count = bpp - 4;
   png_uint_32 left0 = 0, left1 = 0;

   for (; n - i >= bpp; i += bpp)
   {
      png_uint_32 up0 = png_ps2_wide_load(prev_row + i, 4);
      png_uint_32 up1 = png_ps2_wide_load(prev_row + i + 4, last_count);
      png_uint_32 raw0 = png_ps2_wide_load(row + i, 4);
      png_uint_32 raw1 = png_ps2_wide_load(row + i + 4, last_count);
      left0 = png_ps2_wide_add(raw0, png_ps2_wide_average(left0, up0));
      left1 = png_ps2_wide_add(raw1, png_ps2_wide_average(left1, up1));
      png_ps2_wide_store(row + i, left0, 4);
      png_ps2_wide_store(row + i + 4, left1, last_count);
   }
   for (; i < n; ++i)
   {
      unsigned int a = i >= bpp ? row[i - bpp] : 0;
      unsigned int b = prev_row[i];
      row[i] = (png_byte)((unsigned int)row[i] + ((a + b) >> 1));
   }
}

static void
png_read_filter_row_avg6_ps2(png_row_info *row_info, png_byte *row,
    const png_byte *prev_row)
{
   png_ps2_wide_avg(row_info, row, prev_row, 6);
}

static void
png_read_filter_row_avg8_ps2(png_row_info *row_info, png_byte *row,
    const png_byte *prev_row)
{
   png_ps2_wide_avg(row_info, row, prev_row, 8);
}
#endif /* PNG_PS2_EE_MMI_WIDE_AVG */

#ifdef PNG_PS2_EE_MMI_PAETH
/* Scalar predictor selection is expensive: these are experimental.
 * The packed byte additions still use EE PADDB on four lanes at a time.
 */
static unsigned int
png_ps2_wide_paeth_predict(unsigned int a, unsigned int b, unsigned int c)
{
   int pa = (int)b - (int)c;
   int pb = (int)a - (int)c;
   int pc = (int)a + (int)b - 2 * (int)c;
   pa = pa < 0 ? -pa : pa;
   pb = pb < 0 ? -pb : pb;
   pc = pc < 0 ? -pc : pc;
#ifdef PNG_PS2_EE_MMI_PAETH_MASK
   {
      unsigned int choose_a = (unsigned int)(pa <= pb) &
          (unsigned int)(pa <= pc);
      unsigned int choose_b = (choose_a ^ 1U) &
          (unsigned int)(pb <= pc);
      unsigned int ma = 0U - choose_a;
      unsigned int mb = 0U - choose_b;
      return (a & ma) | (b & mb) | (c & ~(ma | mb));
   }
#else
   if (pa <= pb && pa <= pc) return a;
   if (pb <= pc) return b;
   return c;
#endif
}

static void
png_ps2_wide_paeth(png_row_info *row_info, png_byte *row,
    const png_byte *prev_row, unsigned int bpp)
{
   size_t i = 0, n = row_info->rowbytes;
   unsigned int last_count = bpp - 4, j;
   png_byte left[8] = {0}, upper_left[8] = {0};

   for (; n - i >= bpp; i += bpp)
   {
      png_uint_32 predictor0 = 0, predictor1 = 0;
      png_uint_32 decoded0, decoded1;
      for (j = 0; j < bpp; ++j)
      {
         unsigned int above = prev_row[i + j];
         png_uint_32 p = png_ps2_wide_paeth_predict(left[j], above,
             upper_left[j]);
         if (j < 4) predictor0 |= p << (j * 8);
         else predictor1 |= p << ((j - 4) * 8);
         upper_left[j] = (png_byte)above;
      }
      decoded0 = png_ps2_wide_add(png_ps2_wide_load(row + i, 4), predictor0);
      decoded1 = png_ps2_wide_add(png_ps2_wide_load(row + i + 4,
          last_count), predictor1);
      png_ps2_wide_store(row + i, decoded0, 4);
      png_ps2_wide_store(row + i + 4, decoded1, last_count);
      for (j = 0; j < bpp; ++j)
         left[j] = row[i + j];
   }
   for (; i < n; ++i)
   {
      unsigned int a = i >= bpp ? row[i - bpp] : 0;
      unsigned int b = prev_row[i];
      unsigned int c = i >= bpp ? prev_row[i - bpp] : 0;
      row[i] = (png_byte)((unsigned int)row[i] +
          png_ps2_wide_paeth_predict(a, b, c));
   }
}

static void
png_read_filter_row_paeth6_ps2(png_row_info *row_info, png_byte *row,
    const png_byte *prev_row)
{
   png_ps2_wide_paeth(row_info, row, prev_row, 6);
}

static void
png_read_filter_row_paeth8_ps2(png_row_info *row_info, png_byte *row,
    const png_byte *prev_row)
{
   png_ps2_wide_paeth(row_info, row, prev_row, 8);
}
#endif /* PNG_PS2_EE_MMI_PAETH */
