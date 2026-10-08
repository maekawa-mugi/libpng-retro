/* Experimental 128-bit Sub3 prefix scan for RGB8 PNG scanlines.
 *
 * Included by filter_rgb3.c only with PNG_PS2_EE_MMI_SUB3_PREFIX.
 * Each output byte depends on the decoded byte three positions earlier.
 * A 16-byte block is reconstructed with shifts of 3, 6 and 12 bytes;
 * inject the previous decoded three bytes before the parallel scan.
 *
 * LQ/SQ only access complete 16-byte aligned blocks within the row.
 * Scalar prologue/tail also cover arbitrary alignment and truncated rows.
 * Released under the libpng license.
 */

static int
png_read_filter_row_sub3_prefix_ps2(png_byte *row, size_t rowbytes)
{
   size_t i, vector_end;
   png_byte *ptr;
   unsigned int blocks;

   /* Avoid the SA save/restore and alignment prologue on short rows. */
   if (rowbytes < 64)
      return 0;

   i = 3;
   while (((size_t)(row + i) & 15U) != 0)
   {
      row[i] = (png_byte)((unsigned int)row[i] +
          (unsigned int)row[i - 3]);
      ++i;
   }

   vector_end = i + ((rowbytes - i) & ~(size_t)15U);
   blocks = (unsigned int)((vector_end - i) >> 4);
   ptr = row + i;

#ifdef PNG_PS2_RGB3_PORTABLE_ADD
   /* Simulate the exact register-level scan for source-level host tests.
    * Descending lane traversal gives simultaneous left shifts per stage.
    */
   while (blocks-- != 0)
   {
      png_byte lanes[16];
      unsigned int j, stage;
      memcpy(lanes, ptr, sizeof lanes);
      for (j = 0; j < 3; ++j)
         lanes[j] = (png_byte)((unsigned int)lanes[j] +
             (unsigned int)ptr[(int)j - 3]);
      for (stage = 3; stage <= 12; stage *= 2)
         for (j = 16; j-- > stage;)
            lanes[j] = (png_byte)((unsigned int)lanes[j] +
                (unsigned int)lanes[j - stage]);
      memcpy(ptr, lanes, sizeof lanes);
      ptr += 16;
   }
#else
   /* SA is a shared special register.  Respect the three-instruction
    * separation between MFSA/QFSRV and a subsequent SA write.
    * Every QFSRV uses a full 128-bit GPR, not an unaligned word load.
    */
   __asm__ volatile (
      ".set push\n\t"
      ".set noreorder\n\t"
      "mfsa  $14\n\t"
      "nop\n\t"
      "nop\n\t"
      "nop\n\t"
      "1:\n\t"
      "lq    $8, 0(%[ptr])\n\t"
      "lbu   $10, -3(%[ptr])\n\t"
      "lbu   $11, -2(%[ptr])\n\t"
      "lbu   $12, -1(%[ptr])\n\t"
      "sll   $11, $11, 8\n\t"
      "sll   $12, $12, 16\n\t"
      "or    $10, $10, $11\n\t"
      "or    $10, $10, $12\n\t"
      "paddb $8, $8, $10\n\t"
      /* QFSRV with SA=13 shifts the low vector left by three bytes. */
      "mtsab $zero, 13\n\t"
      "nop\n\t"
      "nop\n\t"
      "nop\n\t"
      "qfsrv $9, $8, $zero\n\t"
      "paddb $8, $8, $9\n\t"
      "nop\n\t"
      "nop\n\t"
      /* Then shift the partial sums left by six bytes. */
      "mtsab $zero, 10\n\t"
      "nop\n\t"
      "nop\n\t"
      "nop\n\t"
      "qfsrv $9, $8, $zero\n\t"
      "paddb $8, $8, $9\n\t"
      "nop\n\t"
      "nop\n\t"
      /* Finally shift by twelve bytes. */
      "mtsab $zero, 4\n\t"
      "nop\n\t"
      "nop\n\t"
      "nop\n\t"
      "qfsrv $9, $8, $zero\n\t"
      "paddb $8, $8, $9\n\t"
      "sq    $8, 0(%[ptr])\n\t"
      "addiu %[ptr], %[ptr], 16\n\t"
      "addiu %[blocks], %[blocks], -1\n\t"
      "bnez  %[blocks], 1b\n\t"
      "nop\n\t"
      "mtsa  $14\n\t"
      ".set pop\n\t"
      : [ptr] "+r" (ptr), [blocks] "+r" (blocks)
      :
      : "$8", "$9", "$10", "$11", "$12", "$14", "memory"
   );
#endif

   for (i = vector_end; i < rowbytes; ++i)
      row[i] = (png_byte)((unsigned int)row[i] +
          (unsigned int)row[i - 3]);
   return 1;
}
