/* ps2/filter_mmi.c - R5900 / PS2 Emotion Engine MMI PNG read filters
 *
 * Included by ee_init.c (which is itself included by pngsimd.c).
 * This code is released under the libpng license.
 *
 * Use named input/output operands and clobber $8/$9 explicitly.  PS2 EE
 * has 128-bit GPRs; PADDB performs 16 independent modulo-256 byte adds.
 * LQ and SQ mask off address bits 0..3, so guard 16-byte alignment.
 */

/* PNG Up: every byte is independent of all other bytes in the row.
 * A 16-byte LQ / PADDB / SQ loop followed by a scalar tail.
 */
static void
png_read_filter_row_up_ps2(png_row_info *row_info, png_byte *row,
    const png_byte *prev_row)
{
   size_t remaining = row_info->rowbytes;

   if (remaining >= 16 &&
       (((size_t)row | (size_t)prev_row) & 15U) == 0)
   {
      unsigned int blocks = (unsigned int)(remaining >> 4);
      remaining &= 15U;

      __asm__ volatile (
         ".set push\n\t"
         ".set noreorder\n\t"
         "1:\n\t"
         "lq    $8, 0(%[row])\n\t"
         "lq    $9, 0(%[prev])\n\t"
         "paddb $8, $8, $9\n\t"
         "sq    $8, 0(%[row])\n\t"
         "addiu %[row], %[row], 16\n\t"
         "addiu %[prev], %[prev], 16\n\t"
         "addiu %[blocks], %[blocks], -1\n\t"
         "bnez  %[blocks], 1b\n\t"
         "nop\n\t"
         ".set pop\n\t"
         : [row] "+r" (row), [prev] "+r" (prev_row),
           [blocks] "+r" (blocks)
         :
         : "$8", "$9", "memory"
      );
   }

   while (remaining-- != 0)
   {
      *row = (png_byte)((unsigned int)*row + (unsigned int)*prev_row);
      ++row;
      ++prev_row;
   }
}

/* PNG Sub with exactly four bytes per pixel (e.g. RGBA8).
 * The previous decoded pixel is four bytes, packed into the low word of
 * $8.  PADDB handles all four byte lanes including modulo-256 wrap.
 * This has a horizontal dependency: intentionally process one pixel
 * per iteration.  Other bpp values use libpng's generic filter.
 */
static void
png_read_filter_row_sub4_ps2(png_row_info *row_info, png_byte *row,
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
