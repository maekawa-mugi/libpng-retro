/* Experimental four-pixel word-loop alternative. Generated from the separate
 * Up/Sub4 unroll branch; does not depend on the prefix-scan candidate.
 * Included by filter_mmi.c. SPDX-License-Identifier: libpng-2.0
 */
/* PNG Sub with exactly four bytes per pixel (e.g. RGBA8).
 * The previous decoded pixel is four bytes, packed into the low word of
 * $8.  PADDB handles all four byte lanes including modulo-256 wrap.
 * This has a horizontal dependency: intentionally process one pixel
 * per iteration.  Other bpp values use libpng's generic filter.
 */
static void
png_read_filter_row_sub4_unroll4_ps2(png_row_info *row_info, png_byte *row,
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
      unsigned int groups = (unsigned int)((rowbytes - 4U) >> 4);
      unsigned int blocks = (unsigned int)(((rowbytes - 4U) & 15U) >> 2);

      /* Decode four pixels per branch.  Each PADDB depends on the
       * preceding decoded pixel, so load the independent raw words
       * first, then chain the four additions and write the results.
       * Only aligned word accesses inside the row are used.
       */
      if (groups != 0)
      {
         __asm__ volatile (
            ".set push\n\t"
            ".set noreorder\n\t"
            "lw    $8, -4(%[ptr])\n\t"
            "1:\n\t"
            "lw    $9, 0(%[ptr])\n\t"
            "lw    $10, 4(%[ptr])\n\t"
            "lw    $11, 8(%[ptr])\n\t"
            "lw    $12, 12(%[ptr])\n\t"
            "paddb $9, $9, $8\n\t"
            "paddb $10, $10, $9\n\t"
            "paddb $11, $11, $10\n\t"
            "paddb $8, $12, $11\n\t"
            "sw    $9, 0(%[ptr])\n\t"
            "sw    $10, 4(%[ptr])\n\t"
            "sw    $11, 8(%[ptr])\n\t"
            "sw    $8, 12(%[ptr])\n\t"
            "addiu %[ptr], %[ptr], 16\n\t"
            "addiu %[groups], %[groups], -1\n\t"
            "bnez  %[groups], 1b\n\t"
            "nop\n\t"
            ".set pop\n\t"
            : [ptr] "+r" (ptr), [groups] "+r" (groups)
            :
            : "$8", "$9", "$10", "$11", "$12", "memory"
         );
      }

      /* Zero to three whole pixels remain after the unrolled loop. */
      if (blocks != 0)
      {
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
      }
      return;
   }

   for (i = 4; i < rowbytes; ++i)
      row[i] = (png_byte)((unsigned int)row[i] + (unsigned int)row[i - 4]);
}


