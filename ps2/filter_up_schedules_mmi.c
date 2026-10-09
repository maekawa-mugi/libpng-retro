/* Experimental EE Up schedules: independent-load ordering and L1 prefetch.
 * EE Core User Manual 1.3/2.3.2: a load-use dependency can trigger
 * interlocks. These candidates keep the arithmetic identical, change
 * only independent load ordering / prefetch to compare on real EE.
 * Do not assume PREF always helps (cache misses and memory regions vary).
 * Included in the standalone EE comparison lab, not default dispatch.
 * SPDX-License-Identifier: libpng-2.0
 */
/* PS2 EE MMI Up filter: 16-byte modulo-256 addition with an alignment
 * prologue, a complete-vector loop, and a scalar tail.
 * Included by filter_mmi.c, not a separate translation unit.
 * SPDX-License-Identifier: libpng-2.0
 */
static void
png_read_filter_row_up_2x_interleaved_ps2(png_row_info *row_info, png_byte *row,
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
         unsigned int pairs = blocks >> 1;
         remaining &= 15U;
#ifdef PNG_PS2_UP_PORTABLE_ADD
         /* Match the production 32-byte loop and its final 16-byte tail. */
         while (pairs-- != 0)
         {
            unsigned int j;
            for (j = 0; j < 32; ++j)
               row[j] = (png_byte)((unsigned int)row[j] +
                   (unsigned int)prev_row[j]);
            row += 32;
            prev_row += 32;
         }
         if ((blocks & 1U) != 0)
         {
            unsigned int j;
            for (j = 0; j < 16; ++j)
               row[j] = (png_byte)((unsigned int)row[j] +
                   (unsigned int)prev_row[j]);
            row += 16;
            prev_row += 16;
         }
#else
         /* Two independent 128-bit loads and adds per iteration.  Read
          * both input vectors before storing the outputs to reduce the
          * load/use dependency and halve the loop branch overhead.
          */
         if (pairs != 0)
         {
            __asm__ volatile (
               ".set push\n\t"
               ".set noreorder\n\t"
               "1:\n\t"
               "lq    $8, 0(%[row])\n\t"
               "lq    $9, 0(%[prev])\n\t"
               "lq    $10, 16(%[row])\n\t"
               "lq    $11, 16(%[prev])\n\t"
               "paddb $8, $8, $9\n\t"
               "paddb $10, $10, $11\n\t"
               "sq    $8, 0(%[row])\n\t"
               "sq    $10, 16(%[row])\n\t"
               "addiu %[row], %[row], 32\n\t"
               "addiu %[prev], %[prev], 32\n\t"
               "addiu %[pairs], %[pairs], -1\n\t"
               "bnez  %[pairs], 1b\n\t"
               "nop\n\t"
               ".set pop\n\t"
               : [row] "+r" (row), [prev] "+r" (prev_row),
                 [pairs] "+r" (pairs)
               :
               : "$8", "$9", "$10", "$11", "memory"
            );
         }

         /* A single vector remains when the original block count is odd. */
         if ((blocks & 1U) != 0)
         {
            __asm__ volatile (
               ".set push\n\t"
               ".set noreorder\n\t"
               "lq    $8, 0(%[row])\n\t"
               "lq    $9, 0(%[prev])\n\t"
               "paddb $8, $8, $9\n\t"
               "sq    $8, 0(%[row])\n\t"
               ".set pop\n\t"
               :
               : [row] "r" (row), [prev] "r" (prev_row)
               : "$8", "$9", "memory"
            );
            row += 16;
            prev_row += 16;
         }
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

/* PS2 EE MMI Up filter: 16-byte modulo-256 addition with an alignment
 * prologue, a complete-vector loop, and a scalar tail.
 * Included by filter_mmi.c, not a separate translation unit.
 * SPDX-License-Identifier: libpng-2.0
 */
static void
png_read_filter_row_up_2x_prefetch_ps2(png_row_info *row_info, png_byte *row,
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
         unsigned int pairs = blocks >> 1;
         remaining &= 15U;
#ifdef PNG_PS2_UP_PORTABLE_ADD
         /* Match the production 32-byte loop and its final 16-byte tail. */
         while (pairs-- != 0)
         {
            unsigned int j;
            for (j = 0; j < 32; ++j)
               row[j] = (png_byte)((unsigned int)row[j] +
                   (unsigned int)prev_row[j]);
            row += 32;
            prev_row += 32;
         }
         if ((blocks & 1U) != 0)
         {
            unsigned int j;
            for (j = 0; j < 16; ++j)
               row[j] = (png_byte)((unsigned int)row[j] +
                   (unsigned int)prev_row[j]);
            row += 16;
            prev_row += 16;
         }
#else
         /* Two independent 128-bit loads and adds per iteration.  Read
          * both input vectors before storing the outputs to reduce the
          * load/use dependency and halve the loop branch overhead.
          */
         if (pairs != 0)
         {
            __asm__ volatile (
               ".set push\n\t"
               ".set noreorder\n\t"
               "1:\n\t"
               "pref  0, 64(%[row])\n\t"
               "pref  0, 64(%[prev])\n\t"
               "lq    $8, 0(%[row])\n\t"
               "lq    $10, 16(%[row])\n\t"
               "lq    $9, 0(%[prev])\n\t"
               "lq    $11, 16(%[prev])\n\t"
               "paddb $8, $8, $9\n\t"
               "paddb $10, $10, $11\n\t"
               "sq    $8, 0(%[row])\n\t"
               "sq    $10, 16(%[row])\n\t"
               "addiu %[row], %[row], 32\n\t"
               "addiu %[prev], %[prev], 32\n\t"
               "addiu %[pairs], %[pairs], -1\n\t"
               "bnez  %[pairs], 1b\n\t"
               "nop\n\t"
               ".set pop\n\t"
               : [row] "+r" (row), [prev] "+r" (prev_row),
                 [pairs] "+r" (pairs)
               :
               : "$8", "$9", "$10", "$11", "memory"
            );
         }

         /* A single vector remains when the original block count is odd. */
         if ((blocks & 1U) != 0)
         {
            __asm__ volatile (
               ".set push\n\t"
               ".set noreorder\n\t"
               "lq    $8, 0(%[row])\n\t"
               "lq    $9, 0(%[prev])\n\t"
               "paddb $8, $8, $9\n\t"
               "sq    $8, 0(%[row])\n\t"
               ".set pop\n\t"
               :
               : [row] "r" (row), [prev] "r" (prev_row)
               : "$8", "$9", "memory"
            );
            row += 16;
            prev_row += 16;
         }
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

/* PS2 EE MMI Up filter: 16-byte modulo-256 addition with an alignment
 * prologue, a complete-vector loop, and a scalar tail.
 * Included by filter_mmi.c, not a separate translation unit.
 * SPDX-License-Identifier: libpng-2.0
 */
static void
png_read_filter_row_up_1x_prefetch_ps2(png_row_info *row_info, png_byte *row,
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
            "pref  0, 64(%[row])\n\t"
            "pref  0, 64(%[prev])\n\t"
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
