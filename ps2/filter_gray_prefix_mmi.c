/* Optional 16-byte grayscale Sub1/Sub2 prefix filters for R5900 MMI.
 * Included by filter_gray_mmi.c, not a standalone translation unit.
 * A vector reconstructs 16 bytes by injecting the decoded previous 1/2
 * bytes and scanning shifts bpp, 2*bpp, 4*bpp, ... modulo 256.
 * LQ/SQ are restricted to complete aligned vectors within the row.
 * Released under the libpng license.
 */
#ifdef PNG_PS2_GRAY_PORTABLE_ADD
#include <string.h>
#endif

static int
png_ps2_gray_sub_prefix16(png_byte *row, size_t n, unsigned int bpp)
{
   size_t i = bpp, vector_end;
   png_byte *ptr;
   unsigned int blocks;

   if (n < 64)
      return 0;

   /* Align the current pointer, not the start of the image row. */
   while (((size_t)(row + i) & 15U) != 0)
   {
      row[i] = (png_byte)((unsigned int)row[i] +
          (unsigned int)row[i - bpp]);
      ++i;
   }

   vector_end = i + ((n - i) & ~(size_t)15U);
   blocks = (unsigned int)((vector_end - i) >> 4);
   ptr = row + i;

#ifdef PNG_PS2_GRAY_PORTABLE_ADD
   while (blocks-- != 0)
   {
      png_byte lanes[16];
      unsigned int j, shift;
      memcpy(lanes, ptr, sizeof lanes);
      for (j = 0; j < bpp; ++j)
         lanes[j] = (png_byte)((unsigned int)lanes[j] +
             (unsigned int)ptr[(int)j - (int)bpp]);
      for (shift = bpp; shift < 16; shift <<= 1)
         for (j = 16; j-- > shift;)
            lanes[j] = (png_byte)((unsigned int)lanes[j] +
                (unsigned int)lanes[j - shift]);
      memcpy(ptr, lanes, sizeof lanes);
      ptr += 16;
   }
#else
   if (bpp == 1)
   {
      __asm__ volatile (
         ".set push\n\t"
         ".set noreorder\n\t"
         "mfsa  $14\n\t"
         "nop\n\t"
         "nop\n\t"
         "nop\n\t"
         "1:\n\t"
         "lq    $8, 0(%[ptr])\n\t"
         "lbu   $10, -1(%[ptr])\n\t"
         "pcpyld $10, $zero, $10\n\t"
         "paddb $8, $8, $10\n\t"
         "mtsab $zero, 15\n\t"
         "nop\n\t"
         "nop\n\t"
         "nop\n\t"
         "qfsrv $9, $8, $zero\n\t"
         "paddb $8, $8, $9\n\t"
         "nop\n\t"
         "nop\n\t"
         "mtsab $zero, 14\n\t"
         "nop\n\t"
         "nop\n\t"
         "nop\n\t"
         "qfsrv $9, $8, $zero\n\t"
         "paddb $8, $8, $9\n\t"
         "nop\n\t"
         "nop\n\t"
         "mtsab $zero, 12\n\t"
         "nop\n\t"
         "nop\n\t"
         "nop\n\t"
         "qfsrv $9, $8, $zero\n\t"
         "paddb $8, $8, $9\n\t"
         "nop\n\t"
         "nop\n\t"
         "mtsab $zero, 8\n\t"
         "nop\n\t"
         "nop\n\t"
         "nop\n\t"
         "qfsrv $9, $8, $zero\n\t"
         "paddb $8, $8, $9\n\t"
         "nop\n\t"
         "nop\n\t"
         "sq    $8, 0(%[ptr])\n\t"
         "addiu %[ptr], %[ptr], 16\n\t"
         "addiu %[blocks], %[blocks], -1\n\t"
         "bnez  %[blocks], 1b\n\t"
         "nop\n\t"
         "mtsa  $14\n\t"
         ".set pop\n\t"
         : [ptr] "+r" (ptr), [blocks] "+r" (blocks)
         :
         : "$8", "$9", "$10", "$14", "memory"
      );
   }
   else
   {
      __asm__ volatile (
         ".set push\n\t"
         ".set noreorder\n\t"
         "mfsa  $14\n\t"
         "nop\n\t"
         "nop\n\t"
         "nop\n\t"
         "1:\n\t"
         "lq    $8, 0(%[ptr])\n\t"
         "lbu   $10, -2(%[ptr])\n\t"
         "lbu   $11, -1(%[ptr])\n\t"
         "sll   $11, $11, 8\n\t"
         "or    $10, $10, $11\n\t"
         "pcpyld $10, $zero, $10\n\t"
         "paddb $8, $8, $10\n\t"
         "mtsab $zero, 14\n\t"
         "nop\n\t"
         "nop\n\t"
         "nop\n\t"
         "qfsrv $9, $8, $zero\n\t"
         "paddb $8, $8, $9\n\t"
         "nop\n\t"
         "nop\n\t"
         "mtsab $zero, 12\n\t"
         "nop\n\t"
         "nop\n\t"
         "nop\n\t"
         "qfsrv $9, $8, $zero\n\t"
         "paddb $8, $8, $9\n\t"
         "nop\n\t"
         "nop\n\t"
         "mtsab $zero, 8\n\t"
         "nop\n\t"
         "nop\n\t"
         "nop\n\t"
         "qfsrv $9, $8, $zero\n\t"
         "paddb $8, $8, $9\n\t"
         "nop\n\t"
         "nop\n\t"
         "sq    $8, 0(%[ptr])\n\t"
         "addiu %[ptr], %[ptr], 16\n\t"
         "addiu %[blocks], %[blocks], -1\n\t"
         "bnez  %[blocks], 1b\n\t"
         "nop\n\t"
         "mtsa  $14\n\t"
         ".set pop\n\t"
         : [ptr] "+r" (ptr), [blocks] "+r" (blocks)
         :
         : "$8", "$9", "$10", "$11", "$14", "memory"
      );
   }
#endif

   for (i = vector_end; i < n; ++i)
      row[i] = (png_byte)((unsigned int)row[i] +
          (unsigned int)row[i - bpp]);
   return 1;
}
