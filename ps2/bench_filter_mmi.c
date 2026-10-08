/* Integrated correctness + relative performance sweep for the EE harness.
 * Included by test_filter_mmi.c, never linked into libpng itself.
 *
 * Output is machine-readable BENCH,... CSV.  The portable clock() source
 * reports CLOCK TICKS (not cycles); optional PCCR0 reports processor
 * cycles on a privileged PS2SDK/EE environment only.
 * SPDX-License-Identifier: libpng-2.0
 */
#include <time.h>

#define PS2_BENCH_SUB   0U
#define PS2_BENCH_UP    1U
#define PS2_BENCH_AVG   2U
#define PS2_BENCH_PAETH 3U
#define PS2_BENCH_WRITE_UP    4U
#define PS2_BENCH_WRITE_SUB4  5U
#define PS2_BENCH_WRITE_AVG4  6U
#define PS2_BENCH_WRITE_PAETH 7U
#define PS2_BENCH_PALETTE     8U
#define PS2_BENCH_ROW_MAX 16384U
#define PS2_BENCH_CAP   (4U * PS2_BENCH_ROW_MAX + 64U)

typedef void (*ps2_bench_fn)(png_row_info *, png_byte *, const png_byte *);
typedef struct {
   const char *name;
   unsigned int bpp, filter, minlen;
   ps2_bench_fn fn;
} ps2_bench_variant;

static png_byte ps2_bench_source[PS2_BENCH_CAP];
static png_byte ps2_bench_row[PS2_BENCH_CAP];
static png_byte ps2_bench_prev[PS2_BENCH_CAP];
static png_byte ps2_bench_expect[PS2_BENCH_CAP];
static png_byte ps2_bench_prev_save[PS2_BENCH_CAP];
static png_byte ps2_bench_palette_table[1024];
static volatile unsigned int ps2_bench_sink;
static unsigned int ps2_bench_bpp, ps2_bench_filter;

#ifdef PNG_PS2_BENCH_EE_PCCR
/* PCCR.EVENT0=1 counts EE processor cycles.  Requires privileged EE
 * execution: do NOT enable on PS2 Linux userspace.  Save/restore
 * PCCR and PCR0; each timed batch stays below the 31-bit overflow.
 */
static png_uint_32 ps2_bench_old_pccr, ps2_bench_old_pcr;
static void
ps2_bench_clock_init(void)
{
   png_uint_32 mode = 0x8000003eU; /* CTE, event=1, EXL/K/S/U */
   png_uint_32 zero = 0;
   __asm__ volatile ("mfps %0, 0" : "=r"(ps2_bench_old_pccr));
   __asm__ volatile ("mfpc %0, 0" : "=r"(ps2_bench_old_pcr));
   __asm__ volatile ("mtps %0, 0" : : "r"(zero) : "memory");
   __asm__ volatile ("mtpc %0, 0" : : "r"(zero) : "memory");
   __asm__ volatile ("mtps %0, 0" : : "r"(mode) : "memory");
}
static unsigned long
ps2_bench_now(void)
{
   png_uint_32 result;
   __asm__ volatile ("mfpc %0, 0" : "=r"(result) : : "memory");
   return (unsigned long)(result & 0x7fffffffU);
}
static unsigned long
ps2_bench_delta(unsigned long a, unsigned long b)
{
   return (b - a) & 0x7fffffffUL;
}
static void
ps2_bench_clock_done(void)
{
   png_uint_32 zero = 0;
   __asm__ volatile ("mtps %0, 0" : : "r"(zero) : "memory");
   __asm__ volatile ("mtpc %0, 0" : : "r"(ps2_bench_old_pcr) : "memory");
   __asm__ volatile ("mtps %0, 0" : : "r"(ps2_bench_old_pccr) : "memory");
}
#define PS2_BENCH_UNIT "ee_cycles"
#else
static void ps2_bench_clock_init(void) {}
static void ps2_bench_clock_done(void) {}
static unsigned long ps2_bench_now(void) { return (unsigned long)clock(); }
static unsigned long ps2_bench_delta(unsigned long a, unsigned long b)
{ return b - a; }
#define PS2_BENCH_UNIT "clock_ticks"
#endif

static unsigned int
ps2_bench_paeth(unsigned int a, unsigned int b, unsigned int c)
{
   int pa = (int)b - (int)c;
   int pb = (int)a - (int)c;
   int pc = (int)a + (int)b - 2 * (int)c;
   pa = pa < 0 ? -pa : pa;
   pb = pb < 0 ? -pb : pb;
   pc = pc < 0 ? -pc : pc;
   if (pa <= pb && pa <= pc) return a;
   if (pb <= pc) return b;
   return c;
}

/* Independent scalar reference, reused for the baseline timing. */
static void
ps2_bench_scalar(png_row_info *ri, png_byte *row, const png_byte *prev)
{
   size_t i;
   if (ps2_bench_filter == PS2_BENCH_PALETTE)
   {
      for (i = ri->rowbytes; i-- > 0;)
      {
         png_byte index = row[i];
         memcpy(row + 4 * i,
             ps2_bench_palette_table + 4 * (size_t)index, 4);
      }
      return;
   }
   /* Encoding kernels operate in reverse so their left predictor is
    * the original row data rather than previously written residuals. */
   if (ps2_bench_filter >= PS2_BENCH_WRITE_UP)
   {
      for (i = ri->rowbytes; i-- > 0;)
      {
         unsigned int a = i >= ps2_bench_bpp ?
             row[i - ps2_bench_bpp] : 0;
         unsigned int b = prev[i];
         unsigned int c = i >= ps2_bench_bpp ?
             prev[i - ps2_bench_bpp] : 0;
         unsigned int predictor =
             ps2_bench_filter == PS2_BENCH_WRITE_UP ? b :
             ps2_bench_filter == PS2_BENCH_WRITE_SUB4 ? a :
             ps2_bench_filter == PS2_BENCH_WRITE_AVG4 ? (a + b) >> 1 :
             ps2_bench_paeth(a, b, c);
         row[i] = (png_byte)((unsigned int)row[i] - predictor);
      }
      return;
   }
   for (i = 0; i < ri->rowbytes; ++i)
   {
      unsigned int a = i >= ps2_bench_bpp ? row[i - ps2_bench_bpp] : 0;
      unsigned int b = prev[i];
      unsigned int c = i >= ps2_bench_bpp ? prev[i - ps2_bench_bpp] : 0;
      unsigned int predictor = ps2_bench_filter == PS2_BENCH_SUB ? a :
          ps2_bench_filter == PS2_BENCH_UP ? b :
          ps2_bench_filter == PS2_BENCH_AVG ? (a + b) >> 1 :
          ps2_bench_paeth(a, b, c);
      row[i] = (png_byte)((unsigned int)row[i] + predictor);
   }
}

#ifdef PNG_PS2_EE_MMI_SUB3_PREFIX
static void ps2_bench_sub3_prefix(png_row_info *ri, png_byte *r,
    const png_byte *p) { (void)p; (void)png_read_filter_row_sub3_prefix_ps2(r, ri->rowbytes); }
#endif
#ifdef PNG_PS2_EE_MMI_GRAY_PREFIX16
static void ps2_bench_sub1_prefix(png_row_info *ri, png_byte *r,
    const png_byte *p) { (void)p; (void)png_ps2_gray_sub_prefix16(r, ri->rowbytes, 1); }
static void ps2_bench_sub2_prefix(png_row_info *ri, png_byte *r,
    const png_byte *p) { (void)p; (void)png_ps2_gray_sub_prefix16(r, ri->rowbytes, 2); }
#endif
#ifdef PNG_PS2_EE_MMI_SUB4_PREFIX
static void ps2_bench_sub4_prefix(png_row_info *ri, png_byte *r,
    const png_byte *p) { (void)p; (void)png_read_filter_row_sub4_prefix_ps2(r, ri->rowbytes); }
#endif
#ifdef PNG_PS2_EE_MMI_SUB6_PREFIX16
static void ps2_bench_sub6_prefix(png_row_info *ri, png_byte *r,
    const png_byte *p) { (void)p; (void)png_ps2_wide_sub6_prefix16(r, ri->rowbytes); }
#endif
#ifdef PNG_PS2_EE_MMI_SUB8_PREFIX16
static void ps2_bench_sub8_prefix(png_row_info *ri, png_byte *r,
    const png_byte *p) { (void)p; (void)png_ps2_wide_sub8_prefix16(r, ri->rowbytes); }
#endif
#ifdef PNG_PS2_EE_MMI_SUB8_WORDS
static void ps2_bench_sub8_words(png_row_info *ri, png_byte *r,
    const png_byte *p) { (void)p; (void)png_ps2_wide_sub8_words(r, ri->rowbytes); }
#endif

#ifdef PNG_PS2_EE_MMI_AVG4_DUAL
static void ps2_bench_avg4_dual(png_row_info *ri, png_byte *r,
    const png_byte *p) { (void)png_ps2_avg4_dual(r, p, ri->rowbytes); }
#endif

/* Keep a direct benchmark entry for the pre-prefix packed Sub3 loop.
 * It uses exactly the same production packed-load/add/store helpers.
 */
static void
ps2_bench_sub3_packed(png_row_info *ri, png_byte *row, const png_byte *prev)
{
   size_t i, n = ri->rowbytes;
   png_uint_32 left;
   (void)prev;
   if (n <= 3)
      return;
   left = png_ps2_pack_rgb3(row);
   for (i = 3; n - i >= 3; i += 3)
   {
      png_uint_32 decoded = png_ps2_add_rgb3(
          png_ps2_pack_rgb3(row + i), left);
      png_ps2_store_rgb3(row + i, decoded);
      left = decoded;
   }
   for (; i < n; ++i)
      row[i] = (png_byte)((unsigned int)row[i] +
          (unsigned int)row[i - 3]);
}

static void
ps2_bench_sub6_packed(png_row_info *ri, png_byte *row, const png_byte *prev)
{
   png_ps2_wide_sub(ri, row, prev, 6);
}

static void
ps2_bench_sub8_packed(png_row_info *ri, png_byte *row, const png_byte *prev)
{
   png_ps2_wide_sub(ri, row, prev, 8);
}

static void
ps2_bench_palette_expand(png_row_info *ri, png_byte *row,
    const png_byte *prev)
{
   (void)prev;
   png_ps2_expand_palette_rgba4(row, row, ri->rowbytes,
       ps2_bench_palette_table);
}

static const ps2_bench_variant ps2_bench_variants[] = {
   {"write-up-mmi", 1, PS2_BENCH_WRITE_UP, 1, png_ps2_write_up_mmi},
   {"write-sub4-mmi", 4, PS2_BENCH_WRITE_SUB4, 1, png_ps2_write_sub4_mmi},
   {"write-avg4-mmi", 4, PS2_BENCH_WRITE_AVG4, 1, png_ps2_write_avg4_mmi},
   {"write-paeth4-mmi", 4, PS2_BENCH_WRITE_PAETH, 1, png_ps2_write_paeth4_mmi},
   {"palette-rgba4", 1, PS2_BENCH_PALETTE, 1, ps2_bench_palette_expand},
   {"up-mmi", 1, PS2_BENCH_UP, 1, png_read_filter_row_up_ps2},
   {"sub1", 1, PS2_BENCH_SUB, 1, png_read_filter_row_sub1_ps2},
   {"sub2", 2, PS2_BENCH_SUB, 1, png_read_filter_row_sub2_ps2},
   {"sub3", 3, PS2_BENCH_SUB, 1, png_read_filter_row_sub3_ps2},
   {"sub4", 4, PS2_BENCH_SUB, 1, png_read_filter_row_sub4_ps2},
   {"sub6", 6, PS2_BENCH_SUB, 1, png_read_filter_row_sub6_ps2},
   {"sub8", 8, PS2_BENCH_SUB, 1, png_read_filter_row_sub8_ps2},
   {"sub3-packed-direct", 3, PS2_BENCH_SUB, 1, ps2_bench_sub3_packed},
   {"sub6-packed-direct", 6, PS2_BENCH_SUB, 1, ps2_bench_sub6_packed},
   {"sub8-packed-direct", 8, PS2_BENCH_SUB, 1, ps2_bench_sub8_packed},
   {"avg3", 3, PS2_BENCH_AVG, 1, png_read_filter_row_avg3_ps2},
   {"avg4", 4, PS2_BENCH_AVG, 1, png_read_filter_row_avg4_ps2},
#ifdef PNG_PS2_EE_MMI_AVG4_DUAL
   {"avg4-dual-direct", 4, PS2_BENCH_AVG, 32, ps2_bench_avg4_dual},
#endif
#ifdef PNG_PS2_EE_MMI_GRAY_AVG
   {"avg1", 1, PS2_BENCH_AVG, 1, png_read_filter_row_avg1_ps2},
   {"avg2", 2, PS2_BENCH_AVG, 1, png_read_filter_row_avg2_ps2},
#endif
#ifdef PNG_PS2_EE_MMI_WIDE_AVG
   {"avg6", 6, PS2_BENCH_AVG, 1, png_read_filter_row_avg6_ps2},
   {"avg8", 8, PS2_BENCH_AVG, 1, png_read_filter_row_avg8_ps2},
#endif
#ifdef PNG_PS2_EE_MMI_PAETH
   {"paeth1", 1, PS2_BENCH_PAETH, 1, png_read_filter_row_paeth1_ps2},
   {"paeth2", 2, PS2_BENCH_PAETH, 1, png_read_filter_row_paeth2_ps2},
   {"paeth3", 3, PS2_BENCH_PAETH, 1, png_read_filter_row_paeth3_ps2},
   {"paeth4", 4, PS2_BENCH_PAETH, 1, png_read_filter_row_paeth4_ps2},
   {"paeth6", 6, PS2_BENCH_PAETH, 1, png_read_filter_row_paeth6_ps2},
   {"paeth8", 8, PS2_BENCH_PAETH, 1, png_read_filter_row_paeth8_ps2},
#endif
#ifdef PNG_PS2_EE_MMI_SUB3_PREFIX
   {"sub3-prefix-direct", 3, PS2_BENCH_SUB, 64, ps2_bench_sub3_prefix},
#endif
#ifdef PNG_PS2_EE_MMI_GRAY_PREFIX16
   {"sub1-prefix-direct", 1, PS2_BENCH_SUB, 64, ps2_bench_sub1_prefix},
   {"sub2-prefix-direct", 2, PS2_BENCH_SUB, 64, ps2_bench_sub2_prefix},
#endif
#ifdef PNG_PS2_EE_MMI_SUB4_PREFIX
   {"sub4-prefix-direct", 4, PS2_BENCH_SUB, 32, ps2_bench_sub4_prefix},
#endif
#ifdef PNG_PS2_EE_MMI_SUB6_PREFIX16
   {"sub6-prefix-direct", 6, PS2_BENCH_SUB, 64, ps2_bench_sub6_prefix},
#endif
#ifdef PNG_PS2_EE_MMI_SUB8_PREFIX16
   {"sub8-prefix-direct", 8, PS2_BENCH_SUB, 64, ps2_bench_sub8_prefix},
#endif
#ifdef PNG_PS2_EE_MMI_SUB8_WORDS
   {"sub8-words-direct", 8, PS2_BENCH_SUB, 32, ps2_bench_sub8_words},
#endif
};

/* Returns the timer tick span of a batch; includes row replay by design.
 * A copy-only batch is measured separately and subtracted. */
static unsigned long
ps2_bench_batch(ps2_bench_fn fn, png_row_info *ri, png_byte *dst,
    const png_byte *src, const png_byte *prev, unsigned int loops)
{
   unsigned long start, finish;
   unsigned int k;
   start = ps2_bench_now();
   for (k = 0; k < loops; ++k)
   {
      memcpy(dst, src, ri->rowbytes);
      if (fn != 0)
         fn(ri, dst, prev);
      ps2_bench_sink += dst[ri->rowbytes - 1];
   }
   finish = ps2_bench_now();
   return ps2_bench_delta(start, finish);
}

static unsigned long
ps2_bench_best(ps2_bench_fn fn, png_row_info *ri, png_byte *dst,
    const png_byte *src, const png_byte *prev, unsigned int loops)
{
   unsigned int rep;
   unsigned long best = ~0UL;
   for (rep = 0; rep < 3; ++rep)
   {
      unsigned long elapsed = ps2_bench_batch(fn, ri, dst, src, prev, loops);
      if (elapsed < best) best = elapsed;
   }
   return best;
}

static int
png_ps2_bench_all(void)
{
   static const unsigned int widths[] = {32, 64, 128, 256, 1024, 4096, 16384};
   static const unsigned int offsets[] = {0, 1, 3, 4, 8, 12, 15};
   unsigned int index, wi, oi, j, fails = 0, successes = 0;
   png_byte *src = aligned16(ps2_bench_source);
   png_byte *dst = aligned16(ps2_bench_row);
   png_byte *prev = aligned16(ps2_bench_prev);
   png_byte *exp = aligned16(ps2_bench_expect);
   png_byte *saved = aligned16(ps2_bench_prev_save);

   printf("BENCH_INFO,unit=%s,clock_per_sec=%lu,repeats=3,subtract_copy=1\n",
       PS2_BENCH_UNIT, (unsigned long)CLOCKS_PER_SEC);
   printf("BENCH_HEADER,variant,filter,bpp,rowbytes,row_align,prev_align,"
          "loops,copy_ticks,scalar_ticks,optimized_ticks,"
          "scalar_net_ticks,optimized_net_ticks,optimized_ticks_per_byte_x1000\n");
   for (j = 0; j < 1024U; ++j)
      ps2_bench_palette_table[j] = random_byte();
   ps2_bench_clock_init();

   for (index = 0; index < sizeof ps2_bench_variants /
       sizeof ps2_bench_variants[0]; ++index)
   {
      const ps2_bench_variant *v = &ps2_bench_variants[index];
      for (wi = 0; wi < sizeof widths / sizeof widths[0]; ++wi)
      {
         size_t n = widths[wi];
         unsigned int loops = 32768U / (unsigned int)n;
         if (n < v->minlen) continue;
         if (loops < 32U) loops = 32U;
         if (loops > 512U) loops = 512U;

         for (oi = 0; oi < sizeof offsets / sizeof offsets[0]; ++oi)
         {
            /* Direct opt-in kernels return 0 without modifying the
             * row if their alignment preconditions are not satisfied.
             * Skip those cases rather than benchmarking a no-op. */
#ifdef PNG_PS2_EE_MMI_SUB4_PREFIX
            if (v->fn == ps2_bench_sub4_prefix && offsets[oi] != 0)
               continue;
#endif
#ifdef PNG_PS2_EE_MMI_SUB8_WORDS
            if (v->fn == ps2_bench_sub8_words && (offsets[oi] & 3U))
               continue;
#endif
#ifdef PNG_PS2_EE_MMI_AVG4_DUAL
            if (v->fn == ps2_bench_avg4_dual &&
                ((offsets[oi] | ((offsets[oi] * 7U) & 15U)) & 3U))
               continue;
#endif
            png_row_info ri;
            png_byte *s = src + offsets[oi];
            png_byte *r = dst + offsets[oi];
            png_byte *p = prev + ((offsets[oi] * 7U) & 15U);
            png_byte *e = exp + offsets[oi];
            png_byte *q = saved + ((offsets[oi] * 7U) & 15U);
            unsigned long copy_time, scalar_time, candidate_time;
            unsigned long net_scalar, net_candidate, scaled;
            ri.rowbytes = n;
            ps2_bench_bpp = v->bpp;
            ps2_bench_filter = v->filter;

            for (j = 0; j < n + 16; ++j)
            {
               s[j] = j < n ? random_byte() : 0xa5;
               p[j] = j < n ? random_byte() : 0x5a;
            }
            memcpy(q, p, n + 16);
            memcpy(e, s, n + 16);
            memcpy(r, s, n + 16);
            if (v->filter == PS2_BENCH_PALETTE)
            {
               /* Expansion fills 4*n bytes.  Check the entire region
                * and sixteen sentinel bytes after it. */
               memset(e + n, 0xa5, 3 * n + 16);
               memset(r + n, 0xa5, 3 * n + 16);
            }
            ps2_bench_scalar(&ri, e, p);
            v->fn(&ri, r, p);

            if (memcmp(e, r,
                    (v->filter == PS2_BENCH_PALETTE ? 4 * n : n) + 16) ||
                memcmp(q, p, n + 16))
            {
               ++fails;
               printf("BENCH_FAIL,%s,%u,%lu,%u\n", v->name,
                   v->bpp, (unsigned long)n, offsets[oi]);
               continue;
            }
            ++successes;

            copy_time = ps2_bench_best(0, &ri, r, s, p, loops);
            scalar_time = ps2_bench_best(ps2_bench_scalar, &ri, r, s,
                p, loops);
            candidate_time = ps2_bench_best(v->fn, &ri, r, s, p, loops);

            net_scalar = scalar_time > copy_time ?
                scalar_time - copy_time : 0;
            net_candidate = candidate_time > copy_time ?
                candidate_time - copy_time : 0;
            scaled = n && loops ?
                (unsigned long)(((unsigned long long)net_candidate * 1000ULL) /
                    ((unsigned long long)n * loops)) : 0;
            printf("BENCH,%s,%u,%u,%lu,%u,%u,%u,%lu,%lu,%lu,%lu,%lu,%lu\n",
                v->name, v->filter, v->bpp, (unsigned long)n,
                offsets[oi], ((offsets[oi] * 7U) & 15U), loops,
                copy_time, scalar_time, candidate_time,
                net_scalar, net_candidate, scaled);
         }
      }
   }
   ps2_bench_clock_done();
   printf("BENCH_DONE,passed=%u,failed=%u,sink=%u\n",
       successes, fails, ps2_bench_sink);
   return fails ? 1 : 0;
}
