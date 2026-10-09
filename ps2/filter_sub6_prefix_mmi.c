/* Experimental 128-bit Sub6 reverse filter for RGB16 PNG rows.
 * The filter operates on encoded bytes (not on 16-bit samples).
 * The previous decoded 6-byte span is injected at the bottom of a
 * 16-byte vector; a 6/12 byte prefix scan reconstructs the rest.
 * LQ/SQ and LD are used only at their required alignments.
 * SPDX-License-Identifier: libpng-2.0
 */
#ifdef PNG_PS2_WIDE_PORTABLE_ADD
#include <string.h>
#endif

static int
png_ps2_wide_sub6_prefix16(png_byte *row, size_t n)
{
   size_t i = 6, vector_end;
   png_byte *ptr;
   unsigned int blocks;

   if (n < 64)
      return 0;

   while (((size_t)(row + i) & 15U) != 0)
   {
      row[i] = (png_byte)((unsigned int)row[i] +
          (unsigned int)row[i - 6]);
      ++i;
   }

   vector_end = i + ((n - i) & ~(size_t)15U);
   blocks = (unsigned int)((vector_end - i) >> 4);
   ptr = row + i;

#ifdef PNG_PS2_WIDE_PORTABLE_ADD
   while (blocks-- != 0)
   {
      png_byte lanes[16];
      unsigned int j, shift;
      memcpy(lanes, ptr, 16);
      for (j = 0; j < 6; ++j)
         lanes[j] = (png_byte)((unsigned int)lanes[j] +
             (unsigned int)ptr[(int)j - 6]);
      for (shift = 6; shift < 16; shift <<= 1)
         for (j = 16; j-- > shift;)
            lanes[j] = (png_byte)((unsigned int)lanes[j] +
                (unsigned int)lanes[j - shift]);
      memcpy(ptr, lanes, 16);
      ptr += 16;
   }
#else
   /* ptr is aligned to 16: LD at ptr-8 is naturally 8-byte aligned.
    * The low 64-bit word consists of bytes ptr-8..ptr-1; DSRL 16
    * selects exactly the six preceding decoded bytes.
    */
   __asm__ volatile (
      ".set push\n\t"
      ".set noreorder\n\t"
      "mfsa  $14\n\t"
      "nop\n\t"
      "nop\n\t"
      "nop\n\t"
      "1:\n\t"
      "lq     $8, 0(%[ptr])\n\t"
      "ld     $10, -8(%[ptr])\n\t"
      "dsrl   $10, $10, 16\n\t"
      "pcpyld $10, $zero, $10\n\t"
      "paddb  $8, $8, $10\n\t"
      "mtsab  $zero, 10\n\t"
      "nop\n\t"
      "nop\n\t"
      "nop\n\t"
      "qfsrv  $9, $8, $zero\n\t"
      "paddb  $8, $8, $9\n\t"
      "nop\n\t"
      "nop\n\t"
      "mtsab  $zero, 4\n\t"
      "nop\n\t"
      "nop\n\t"
      "nop\n\t"
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

   for (i = vector_end; i < n; ++i)
      row[i] = (png_byte)((unsigned int)row[i] +
          (unsigned int)row[i - 6]);

   return 1;
}
