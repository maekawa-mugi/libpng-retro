/* Benchmark-only snapshots of the original packed EE MMI routines.
 * Kept verbatim from eemmi before opt-in dispatch, with their function
 * names changed. Allows a single ELF to measure old/new implementations
 * side-by-side on identical inputs, rather than needing separate trips.
 * These routines are never registered into production libpng.
 * SPDX-License-Identifier: libpng-2.0
 */

/* PNG Sub with exactly four bytes per pixel (e.g. RGBA8).
 * The previous decoded pixel is four bytes, packed into the low word of
 * $8.  PADDB handles all four byte lanes including modulo-256 wrap.
 * This has a horizontal dependency: intentionally process one pixel
 * per iteration.  Other bpp values use libpng's generic filter.
 */
static void
png_ps2_bench_sub4_original(png_row_info *row_info, png_byte *row,
    const png_byte *prev_row)
{
   size_t rowbytes = row_info->rowbytes;
   size_t i;

   (void)prev_row;

   if (rowbytes <= 4)
      return;

   /* No halfword/word read beyond the row or from an unaligned address.
    * The fallback also handles unusual callers with truncated rows.
    */
   if ((rowbytes & 3U) == 0 && ((size_t)row & 3U) == 0)
   {
      png_byte *ptr = row + 4;
      unsigned int blocks = (unsigned int)((rowbytes >> 2) - 1);

      __asm__ volatile (
         ".set push\n\t"
         ".set noreorder\n\t"
         "lw    $8, -4(%[ptr])\n\t"
         "1:\n\t"
         "lw    $9, 0(%[ptr])\n\t"
         "paddb $8, $8, $9\n\t"
         "sw    $8, 0(%[ptr])\n\t"
         "addiu %[ptr], %[ptr], 4\n\t"
         "addiu %[blocks], %[blocks], -1\n\t"
         "bnez  %[blocks], 1b\n\t"
         "nop\n\t"
         ".set pop\n\t"
         : [ptr] "+r" (ptr), [blocks] "+r" (blocks)
         :
         : "$8", "$9", "memory"
      );
      return;
   }

   for (i = 4; i < rowbytes; ++i)
      row[i] = (png_byte)((unsigned int)row[i] + (unsigned int)row[i - 4]);
}



/* Average, bpp=4: floor((a+b)/2) on four independent byte lanes.
 * (a&b) + (((a^b)&0xfefefefe)>>1) is the exact non-rounding mean.
 * PADDB adds the residual modulo 256 without inter-byte carries.
 */
static void
png_ps2_bench_avg4_original(png_row_info *row_info, png_byte *row,
    const png_byte *prev_row)
{
   size_t rowbytes = row_info->rowbytes;
   size_t i;

   if (rowbytes >= 4 && (rowbytes & 3U) == 0 &&
       (((size_t)row | (size_t)prev_row) & 3U) == 0)
   {
      unsigned int pixels = (unsigned int)(rowbytes >> 2);
      __asm__ volatile (
         ".set push\n\t"
         ".set noreorder\n\t"
         "lui   $12, 0xfefe\n\t"
         "ori   $12, $12, 0xfefe\n\t"
         "move  $11, $zero\n\t"
         "1:\n\t"
         "lw    $8, 0(%[row])\n\t"
         "lw    $9, 0(%[prev])\n\t"
         "nop\n\t"
         "xor   $10, $11, $9\n\t"
         "and   $10, $10, $12\n\t"
         "srl   $10, $10, 1\n\t"
         "and   $13, $11, $9\n\t"
         "addu  $10, $10, $13\n\t"
         "paddb $8, $8, $10\n\t"
         "sw    $8, 0(%[row])\n\t"
         "move  $11, $8\n\t"
         "addiu %[row], %[row], 4\n\t"
         "addiu %[prev], %[prev], 4\n\t"
         "addiu %[pixels], %[pixels], -1\n\t"
         "bnez  %[pixels], 1b\n\t"
         "nop\n\t"
         ".set pop\n\t"
         : [row] "+r" (row), [prev] "+r" (prev_row),
           [pixels] "+r" (pixels)
         :
         : "$8", "$9", "$10", "$11", "$12", "$13", "memory"
      );
      return;
   }

   for (i = 0; i < rowbytes; ++i)
   {
      unsigned int left = i >= 4 ? row[i - 4] : 0;
      unsigned int above = prev_row[i];
      row[i] = (png_byte)((unsigned int)row[i] + ((left + above) >> 1));
   }
}


/* Parallel prefix scan for bpp=1, four contiguous bytes per group.
 * Inject carry into the first lane, then scan offsets 1 and 2 bytes.
 */
static void
png_ps2_bench_sub1_original(png_row_info *row_info, png_byte *row,
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
png_ps2_bench_sub2_original(png_row_info *row_info, png_byte *row,
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

