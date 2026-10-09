/* Four independent 128-bit Up vectors per unrolled loop.  One variant
 * leaves the natural load schedule; the second adds two cache prefetches.
 * The EE manual identifies independent LS/I0 ordering and 8KiB D-cache
 * as reasons to measure both. No overrunning a short row or unaligned LQ.
 * SPDX-License-Identifier: libpng-2.0 */
#define PS2_UP4_ASM(PREF) \
   __asm__ volatile ( \
      ".set push\n\t" \
      ".set noreorder\n\t" \
      "1:\n\t" \
      PREF \
      "lq $8, 0(%[row])\n\t" \
      "lq $9, 0(%[prev])\n\t" \
      "lq $10, 16(%[row])\n\t" \
      "lq $11, 16(%[prev])\n\t" \
      "lq $12, 32(%[row])\n\t" \
      "lq $13, 32(%[prev])\n\t" \
      "lq $14, 48(%[row])\n\t" \
      "lq $15, 48(%[prev])\n\t" \
      "paddb $8, $8, $9\n\t" \
      "paddb $10, $10, $11\n\t" \
      "paddb $12, $12, $13\n\t" \
      "paddb $14, $14, $15\n\t" \
      "sq $8, 0(%[row])\n\t" \
      "sq $10, 16(%[row])\n\t" \
      "sq $12, 32(%[row])\n\t" \
      "sq $14, 48(%[row])\n\t" \
      "addiu %[row], %[row], 64\n\t" \
      "addiu %[prev], %[prev], 64\n\t" \
      "addiu %[blocks], %[blocks], -1\n\t" \
      "bnez %[blocks], 1b\n\t" \
      "nop\n\t" \
      ".set pop\n\t" \
      : [row] "+r" (row), [prev] "+r" (prev_row), \
        [blocks] "+r" (blocks) \
      : \
      : "$8", "$9", "$10", "$11", "$12", "$13", "$14", \
        "$15", "memory" \
   )

#define PS2_UP4_DEFINE(FN, PREF) \
static void FN(png_row_info *info, png_byte *row, const png_byte *prev_row) \
{ \
   size_t remaining = info->rowbytes; \
   png_row_info tail_info; \
   if (remaining >= 64 && \
       (((size_t)row ^ (size_t)prev_row) & 15U) == 0) \
   { \
      size_t prologue = (16U - ((size_t)row & 15U)) & 15U; \
      unsigned int blocks; \
      while (prologue-- != 0) \
      { \
         *row = (png_byte)((unsigned int)*row + (unsigned int)*prev_row); \
         ++row; ++prev_row; --remaining; \
      } \
      blocks = (unsigned int)(remaining >> 6); \
      remaining &= 63U; \
      if (blocks != 0) \
      { \
         /* Runtime instruction selection is static per compiled clone. */ \
         PS2_UP4_BODY(PREF); \
      } \
   } \
   tail_info.rowbytes = remaining; \
   png_read_filter_row_up_ps2(&tail_info, row, prev_row); \
}
#ifdef PNG_PS2_UP_PORTABLE_ADD
#define PS2_UP4_BODY(PREF) \
   do { while (blocks-- != 0) { \
      unsigned int j; \
      for (j=0; j<64; ++j) \
         row[j]=(png_byte)((unsigned int)row[j]+(unsigned int)prev_row[j]); \
      row+=64; prev_row+=64; \
   }} while (0)
#else
#define PS2_UP4_BODY(PREF) PS2_UP4_ASM(PREF)
#endif
PS2_UP4_DEFINE(png_read_filter_row_up_4x_ps2, "")
PS2_UP4_DEFINE(png_read_filter_row_up_4x_prefetch_ps2, \
    "pref 0, 128(%[row])\n\t" \
    "pref 0, 128(%[prev])\n\t")
#undef PS2_UP4_DEFINE
#undef PS2_UP4_BODY
#undef PS2_UP4_ASM
