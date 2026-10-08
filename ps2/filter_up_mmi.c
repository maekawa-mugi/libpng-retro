/* PS2 EE MMI Up filter: 16-byte modulo-256 addition with an alignment
 * prologue, a complete-vector loop, and a scalar tail.
 * Included by filter_mmi.c, not a separate translation unit.
 * SPDX-License-Identifier: libpng-2.0
 */
static void
png_read_filter_row_up_ps2(png_row_info *row_info, png_byte *row,
    const png_byte *prev_row)
{
   size_t remaining = row_info->rowbytes;

   /* If the two buffers have the same alignment modulo 16, a small
    * scalar prefix brings BOTH pixel pointers onto a 16-byte boundary.
    * Different offsets cannot both be aligned for a pair of LQ loads.
    */
   if (remaining >= 16 &&
       (((size_t)row ^ (size_t)prev_row) & 15U) == 0)
   {
      size_t prefix = (16U - ((size_t)row & 15U)) & 15U;
      while (prefix != 0)
      {
         *row = (png_byte)((unsigned int)*row + (unsigned int)*prev_row);
         ++row;
         ++prev_row;
         --remaining;
         --prefix;
      }

      if (remaining >= 16)
      {
         unsigned int blocks = (unsigned int)(remaining >> 4);
         remaining &= 15U;
#ifdef PNG_PS2_UP_PORTABLE_ADD
         /* Test-only C equivalent of 16-lane PADDB. */
         while (blocks-- != 0)
         {
            unsigned int j;
            for (j = 0; j < 16; ++j)
               row[j] = (png_byte)((unsigned int)row[j] +
                   (unsigned int)prev_row[j]);
            row += 16;
            prev_row += 16;
         }
#else
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
#endif
      }
   }

   while (remaining-- != 0)
   {
      *row = (png_byte)((unsigned int)*row + (unsigned int)*prev_row);
      ++row;
      ++prev_row;
   }
}
