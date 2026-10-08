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


#if defined(PNG_PS2_EE_MMI_SUB4_PREFIX)
/* Experimental four-pixel, 128-bit Sub4 prefix scan.
 * QFSRV with SA=12 yields raw << 32. PCPYLD yields partial << 64.
 * A preceding decoded pixel is replicated across the four word lanes.
 * Preserve SA, and never issue unaligned or out-of-range LQ/SQ.
 */
static int
png_read_filter_row_sub4_prefix_ps2(png_byte *row, size_t rowbytes)
{
   size_t i, vector_end;
   png_byte *ptr;
   unsigned int blocks;

   if (rowbytes < 32 || (rowbytes & 3U) != 0 ||
       ((size_t)row & 15U) != 0)
      return 0;

   for (i = 4; i < 16; ++i)
      row[i] = (png_byte)((unsigned int)row[i] + (unsigned int)row[i - 4]);

   vector_end = 16 + ((rowbytes - 16) & ~(size_t)15U);
   blocks = (unsigned int)((vector_end - 16) >> 4);
   ptr = row + 16;

   __asm__ volatile (
      ".set push\n\t"
      ".set noreorder\n\t"
      "mfsa  $14\n\t"
      "nop\n\t"
      "nop\n\t"
      "nop\n\t"
      "mtsab $zero, 12\n\t"
      "nop\n\t"
      "nop\n\t"
      "nop\n\t"
      "1:\n\t"
      "lq     $8, 0(%[ptr])\n\t"
      "lw     $10, -4(%[ptr])\n\t"
      "qfsrv  $9, $8, $zero\n\t"
      "paddb  $8, $8, $9\n\t"
      "pcpyld $9, $8, $zero\n\t"
      "paddb  $8, $8, $9\n\t"
      "pextlw $11, $10, $10\n\t"
      "pcpyld $11, $11, $11\n\t"
      "paddb  $8, $8, $11\n\t"
      "sq     $8, 0(%[ptr])\n\t"
      "addiu  %[ptr], %[ptr], 16\n\t"
      "addiu  %[blocks], %[blocks], -1\n\t"
      "bnez   %[blocks], 1b\n\t"
      "nop\n\t"
      "mtsa   $14\n\t"
      ".set pop\n\t"
      : [ptr] "+r" (ptr), [blocks] "+r" (blocks)
      :
      : "$8", "$9", "$10", "$11", "$14", "memory"
   );

   for (i = vector_end; i < rowbytes; ++i)
      row[i] = (png_byte)((unsigned int)row[i] + (unsigned int)row[i - 4]);
   return 1;
}
#endif /* PNG_PS2_EE_MMI_SUB4_PREFIX */

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

#if defined(PNG_PS2_EE_MMI_SUB4_PREFIX)
   if (png_read_filter_row_sub4_prefix_ps2(row, rowbytes))
      return;
#endif

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
png_read_filter_row_avg4_ps2(png_row_info *row_info, png_byte *row,
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
