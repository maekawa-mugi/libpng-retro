/* Optional 128-bit EE MMI Sub8 reverse filter for RGBA16 scanlines.
 *
 * PNG Sub works on bytes, even for 16-bit RGBA pixels.  Each 16-byte
 * vector spans two pixels: add previous decoded 8 bytes to the low
 * half, then shift that decoded half into the high half with QFSRV.
 *
 * The caller keeps the original four-byte packed implementation as
 * default.  This experimental kernel only uses aligned LQ/SQ and LD.
 * SPDX-License-Identifier: libpng-2.0
 */
#ifdef PNG_PS2_WIDE_PORTABLE_ADD
#include <string.h>
#endif

static int
png_ps2_wide_sub8_prefix16(png_byte *row, size_t n)
{
   size_t i = 8, vector_end;
   png_byte *ptr;
   unsigned int blocks;

   /* Keep the small-image path free from alignment and SA overhead. */
   if (n < 64)
      return 0;

   /* The initial eight raw bytes represent the first unfiltered pixel.
    * Advance to the next aligned 16-byte address using the scalar rule.
    */
   while (((size_t)(row + i) & 15U) != 0)
   {
      row[i] = (png_byte)((unsigned int)row[i] +
          (unsigned int)row[i - 8]);
      ++i;
   }

   vector_end = i + ((n - i) & ~(size_t)15U);
   blocks = (unsigned int)((vector_end - i) >> 4);
   ptr = row + i;

#ifdef PNG_PS2_WIDE_PORTABLE_ADD
   /* Full-width C model, using the production alignment/dispatch logic. */
   while (blocks-- != 0)
   {
      png_byte lanes[16];
      unsigned int j;
      memcpy(lanes, ptr, sizeof lanes);

      for (j = 0; j < 8; ++j)
         lanes[j] = (png_byte)((unsigned int)lanes[j] +
             (unsigned int)ptr[(int)j - 8]);

      /* The upper eight bytes depend on the just-decoded low eight. */
      for (j = 8; j < 16; ++j)
         lanes[j] = (png_byte)((unsigned int)lanes[j] +
             (unsigned int)lanes[j - 8]);

      memcpy(ptr, lanes, sizeof lanes);
      ptr += 16;
   }
#else
   /* MTSAB 8 makes QFSRV produce a logical left shift by eight bytes
    * (the low half moves to the upper half, filling low bytes with 0).
    * The carry is a fully decoded 8-byte pixel at ptr-8.  LD reads
    * only aligned 8-byte storage; PCPYLD clears its upper 64 bits.
    * Save/restore SA because the C calling convention does not convey
    * ownership of this special EE register.
    */
   __asm__ volatile (
      ".set push\n\t"
      ".set noreorder\n\t"
      "mfsa   $14\n\t"
      "nop\n\t"
      "nop\n\t"
      "nop\n\t"
      "mtsab  $zero, 8\n\t"
      "nop\n\t"
      "nop\n\t"
      "nop\n\t"
      "1:\n\t"
      "lq     $8, 0(%[ptr])\n\t"
      "ld     $10, -8(%[ptr])\n\t"
      "pcpyld $10, $zero, $10\n\t"
      "paddb  $8, $8, $10\n\t"
      "qfsrv  $9, $8, $zero\n\t"
      "paddb  $8, $8, $9\n\t"
      "sq     $8, 0(%[ptr])\n\t"
      "addiu  %[ptr], %[ptr], 16\n\t"
      "addiu  %[blocks], %[blocks], -1\n\t"
      "bnez   %[blocks], 1b\n\t"
      "nop\n\t"
      "mtsa   $14\n\t"
      ".set pop\n\t"
      : [ptr] "+r" (ptr), [blocks] "+r" (blocks)
      :
      : "$8", "$9", "$10", "$14", "memory"
   );
#endif

   /* No partial vector stores: finish the row one byte at a time. */
   for (i = vector_end; i < n; ++i)
      row[i] = (png_byte)((unsigned int)row[i] +
          (unsigned int)row[i - 8]);

   return 1;
}
