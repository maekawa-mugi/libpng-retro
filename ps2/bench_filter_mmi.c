/* Integrated correctness + relative performance sweep for the EE harness.
 * Included by test_filter_mmi.c, never linked into libpng itself.
 *
 * Output is machine-readable BENCH,... CSV.  The portable clock() source
 * reports CLOCK TICKS (not cycles); optional PCCR0 reports processor
 * cycles on a privileged PS2SDK/EE environment only.
 * SPDX-License-Identifier: libpng-2.0
 */
#include <time.h>
#if defined(PNG_PS2_BENCH_EE_PCCR) && defined(PNG_PS2_BENCH_POSIX_TIMER)
#error Choose only one EE benchmark timer source
#endif

#define PS2_BENCH_SUB   0U
#define PS2_BENCH_UP    1U
#define PS2_BENCH_AVG   2U
#define PS2_BENCH_PAETH 3U
#define PS2_BENCH_WRITE_UP    4U
#define PS2_BENCH_WRITE_SUB4  5U
#define PS2_BENCH_WRITE_AVG4  6U
#define PS2_BENCH_WRITE_PAETH 7U
#define PS2_BENCH_PALETTE     8U
#define PS2_BENCH_WRITE_SUB_ALL  9U
#define PS2_BENCH_WRITE_AVG_ALL 10U
#define PS2_BENCH_WRITE_PAE_ALL 11U
#define PS2_BENCH_PAL_RGB       12U
#define PS2_BENCH_PAL_TRNS      13U
#define PS2_BENCH_SWAP16        14U
#define PS2_BENCH_STRIP16       15U
#define PS2_BENCH_RGB_SWAP      16U
#define PS2_BENCH_RGBA_SWAP     17U
#define PS2_BENCH_RGB_TO_RGBA   18U
#define PS2_BENCH_RGBA_TO_RGB   19U
#define PS2_BENCH_ADAM_BYTES    20U
#define PS2_BENCH_ADAM_BITS     21U
#define PS2_BENCH_ROW_MAX 16384U
#define PS2_BENCH_CAP   (8U * PS2_BENCH_ROW_MAX + 64U)

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
static png_byte ps2_bench_rgb_table[768], ps2_bench_alpha_table[256];
static size_t ps2_bench_input_bytes, ps2_bench_output_bytes;
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
#elif defined(PNG_PS2_BENCH_POSIX_TIMER)
/* Unprivileged PS2 Linux option: wall-clock microseconds, not CPU cycles.
 * Kernel scheduling may add noise; the harness repeats measurements.
 * Old Linux libc offers gettimeofday even if clock_gettime is absent.
 */
#include <sys/time.h>
static void ps2_bench_clock_init(void) {}
static void ps2_bench_clock_done(void) {}
static unsigned long ps2_bench_now(void)
{
   struct timeval tv;
   if (gettimeofday(&tv, 0) != 0)
      return 0;
   return (unsigned long)tv.tv_sec * 1000000UL +
       (unsigned long)tv.tv_usec;
}
static unsigned long ps2_bench_delta(unsigned long a, unsigned long b)
{ return b - a; }
#define PS2_BENCH_UNIT "microseconds"
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

/* Shape-aware memory bounds for shrinking/expanding in-place transforms.
 * Existing read/write-filter rows are still measured in input bytes. */
static void
ps2_bench_shape(unsigned int filter, unsigned int bpp, size_t n,
    size_t *input, size_t *output, size_t *previous)
{
   *input = n;
   *output = n;
   *previous = n;
   switch (filter)
   {
      case PS2_BENCH_PALETTE: case PS2_BENCH_PAL_TRNS:
      case PS2_BENCH_RGB_TO_RGBA:
         *output = 4*n; break;
      case PS2_BENCH_PAL_RGB:
         *output = 3*n; break;
      case PS2_BENCH_SWAP16:
         *input = *output = 2*n; break;
      case PS2_BENCH_STRIP16:
         *input = 2*n; break;
      case PS2_BENCH_RGB_SWAP:
         *input = *output = 3*n; break;
      case PS2_BENCH_RGBA_SWAP:
         *input = *output = 4*n; break;
      case PS2_BENCH_RGBA_TO_RGB:
         *input = 4*n; *output = 3*n; break;
      case PS2_BENCH_ADAM_BYTES:
         *input = *output = n*bpp;
         *previous = ((n+1)/2)*bpp;
         break;
      case PS2_BENCH_ADAM_BITS:
         *input = *output = (n*bpp+7)/8;
         *previous = (((n+1)/2)*bpp+7)/8;
         break;
      default: break;
   }
   if (filter==PS2_BENCH_RGB_TO_RGBA)
      *input=3*n;
}

static void
ps2_bench_write_sub_all(png_row_info *ri, png_byte *r,
    const png_byte *p)
{
   png_ps2_write_filter_packed(r,p,ri->rowbytes,ps2_bench_bpp,0);
}
static void
ps2_bench_write_avg_all(png_row_info *ri, png_byte *r,
    const png_byte *p)
{
   png_ps2_write_filter_packed(r,p,ri->rowbytes,ps2_bench_bpp,1);
}
static void
ps2_bench_write_pae_all(png_row_info *ri, png_byte *r,
    const png_byte *p)
{
   png_ps2_write_filter_packed(r,p,ri->rowbytes,ps2_bench_bpp,2);
}
static void
ps2_bench_palette_rgb(png_row_info *ri, png_byte *r,
    const png_byte *p)
{
   (void)p;
   png_ps2_expand_palette_rgb3(r,r,ri->rowbytes,ps2_bench_rgb_table);
}
static void
ps2_bench_palette_trns(png_row_info *ri, png_byte *r,
    const png_byte *p)
{
   (void)p;
   png_ps2_expand_palette_rgba_trns(r,r,ri->rowbytes,
       ps2_bench_rgb_table,ps2_bench_alpha_table);
}
static void
ps2_bench_swap16(png_row_info *ri, png_byte *r,
    const png_byte *p)
{
   (void)p;
   png_ps2_swap16_mmi(r,ri->rowbytes);
}
static void
ps2_bench_strip16(png_row_info *ri, png_byte *r,
    const png_byte *p)
{
   (void)p;
   png_ps2_strip16_high(r,r,ri->rowbytes);
}
static void
ps2_bench_swap_rgb(png_row_info *ri, png_byte *r,
    const png_byte *p)
{
   (void)p;
   png_ps2_swap_rb(r,ri->rowbytes,3);
}
static void
ps2_bench_swap_rgba(png_row_info *ri, png_byte *r,
    const png_byte *p)
{
   (void)p;
   png_ps2_swap_rb(r,ri->rowbytes,4);
}
static void
ps2_bench_rgb_to_rgba(png_row_info *ri, png_byte *r,
    const png_byte *p)
{
   (void)p;
   png_ps2_rgb_to_rgba(r,r,ri->rowbytes,0x7f);
}
static void
ps2_bench_rgba_to_rgb(png_row_info *ri, png_byte *r,
    const png_byte *p)
{
   (void)p;
   png_ps2_rgba_to_rgb(r,r,ri->rowbytes);
}
static void
ps2_bench_adam_bytes(png_row_info *ri, png_byte *r,
    const png_byte *p)
{
   png_ps2_adam7_scatter_bytes(r,p,ri->rowbytes,ps2_bench_bpp,4);
}
static void
ps2_bench_adam_bits(png_row_info *ri, png_byte *r,
    const png_byte *p)
{
   png_ps2_adam7_scatter_bits(r,p,ri->rowbytes,ps2_bench_bpp,4);
}

/* Independent scalar reference, reused for the baseline timing. */
static void
ps2_bench_scalar(png_row_info *ri, png_byte *row, const png_byte *prev)
{
   size_t i, n = ri->rowbytes;
   unsigned int kind = ps2_bench_filter, bpp = ps2_bench_bpp;

   if (kind == PS2_BENCH_PALETTE || kind == PS2_BENCH_PAL_RGB ||
       kind == PS2_BENCH_PAL_TRNS)
   {
      unsigned int stride = kind == PS2_BENCH_PAL_RGB ? 3 : 4;
      for (i = n; i-- > 0;)
      {
         png_byte index = row[i];
         if (kind == PS2_BENCH_PALETTE)
            memcpy(row + 4*i, ps2_bench_palette_table+4*(size_t)index, 4);
         else
         {
            memcpy(row + stride*i, ps2_bench_rgb_table+3*(size_t)index, 3);
            if (kind == PS2_BENCH_PAL_TRNS)
               row[4*i+3] = ps2_bench_alpha_table[index];
         }
      }
      return;
   }
   if (kind == PS2_BENCH_SWAP16)
   {
      for (i=0;i<n;++i)
      {
         png_byte a=row[2*i];
         row[2*i]=row[2*i+1];
         row[2*i+1]=a;
      }
      return;
   }
   if (kind == PS2_BENCH_STRIP16)
   {
      for (i=0;i<n;++i) row[i]=row[2*i];
      return;
   }
   if (kind == PS2_BENCH_RGB_SWAP || kind == PS2_BENCH_RGBA_SWAP)
   {
      unsigned int stride=kind==PS2_BENCH_RGB_SWAP?3:4;
      for (i=0;i<n;++i)
      {
         png_byte a=row[stride*i];
         row[stride*i]=row[stride*i+2];
         row[stride*i+2]=a;
      }
      return;
   }
   if (kind == PS2_BENCH_RGB_TO_RGBA)
   {
      for (i=n;i-- > 0;)
      {
         png_byte a=row[3*i], c=row[3*i+1], d=row[3*i+2];
         row[4*i]=a;
         row[4*i+1]=c;
         row[4*i+2]=d;
         row[4*i+3]=0x7f;
      }
      return;
   }
   if (kind == PS2_BENCH_RGBA_TO_RGB)
   {
      for (i=0;i<n;++i)
      {
         png_byte a=row[4*i], c=row[4*i+1], d=row[4*i+2];
         row[3*i]=a;
         row[3*i+1]=c;
         row[3*i+2]=d;
      }
      return;
   }
   if (kind == PS2_BENCH_ADAM_BYTES)
   {
      for (i=0;i<n;i+=2)
         memcpy(row+i*bpp,prev+(i/2)*bpp,bpp);
      return;
   }
   if (kind == PS2_BENCH_ADAM_BITS)
   {
      size_t k=0;
      for (i=0;i<n;i+=2,++k)
      {
         unsigned int j;
         for (j=0;j<bpp;++j)
         {
            size_t srcbit=k*bpp+j, dstbit=i*bpp+j;
            unsigned int bit=(prev[srcbit>>3]>>(7U-(srcbit&7U)))&1U;
            png_byte mask=(png_byte)(1U<<(7U-(dstbit&7U)));
            row[dstbit>>3]=(png_byte)((row[dstbit>>3]&~mask)|
                (bit?mask:0U));
         }
      }
      return;
   }

   if ((kind >= PS2_BENCH_WRITE_UP && kind <= PS2_BENCH_WRITE_PAETH) ||
       (kind >= PS2_BENCH_WRITE_SUB_ALL &&
        kind <= PS2_BENCH_WRITE_PAE_ALL))
   {
      for (i=n;i-- > 0;)
      {
         unsigned int a=i>=bpp?row[i-bpp]:0, up=prev[i];
         unsigned int c=i>=bpp?prev[i-bpp]:0;
         unsigned int predictor =
             kind==PS2_BENCH_WRITE_UP ? up :
             kind==PS2_BENCH_WRITE_SUB4 ||
                 kind==PS2_BENCH_WRITE_SUB_ALL ? a :
             kind==PS2_BENCH_WRITE_AVG4 ||
                 kind==PS2_BENCH_WRITE_AVG_ALL ? (a+up)>>1 :
             ps2_bench_paeth(a,up,c);
         row[i]=(png_byte)((unsigned int)row[i]-predictor);
      }
      return;
   }

   for (i=0;i<n;++i)
   {
      unsigned int a=i>=bpp?row[i-bpp]:0, up=prev[i];
      unsigned int c=i>=bpp?prev[i-bpp]:0;
      unsigned int predictor =
          kind==PS2_BENCH_SUB?a:
          kind==PS2_BENCH_UP?up:
          kind==PS2_BENCH_AVG?(a+up)>>1:
          ps2_bench_paeth(a,up,c);
      row[i]=(png_byte)((unsigned int)row[i]+predictor);
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

/* Benchmark-only snapshots of unchanged eemmi packed filter kernels. */
#include "bench_baseline_mmi.c"

static const ps2_bench_variant ps2_bench_variants[] = {
   {"write-up-mmi", 1, PS2_BENCH_WRITE_UP, 1, png_ps2_write_up_mmi},
   {"write-sub1-packed", 1, PS2_BENCH_WRITE_SUB_ALL, 1, ps2_bench_write_sub_all},
   {"write-avg1-packed", 1, PS2_BENCH_WRITE_AVG_ALL, 1, ps2_bench_write_avg_all},
   {"write-paeth1-packed", 1, PS2_BENCH_WRITE_PAE_ALL, 1, ps2_bench_write_pae_all},
   {"write-sub2-packed", 2, PS2_BENCH_WRITE_SUB_ALL, 1, ps2_bench_write_sub_all},
   {"write-avg2-packed", 2, PS2_BENCH_WRITE_AVG_ALL, 1, ps2_bench_write_avg_all},
   {"write-paeth2-packed", 2, PS2_BENCH_WRITE_PAE_ALL, 1, ps2_bench_write_pae_all},
   {"write-sub3-packed", 3, PS2_BENCH_WRITE_SUB_ALL, 1, ps2_bench_write_sub_all},
   {"write-avg3-packed", 3, PS2_BENCH_WRITE_AVG_ALL, 1, ps2_bench_write_avg_all},
   {"write-paeth3-packed", 3, PS2_BENCH_WRITE_PAE_ALL, 1, ps2_bench_write_pae_all},
   {"write-sub4-packed", 4, PS2_BENCH_WRITE_SUB_ALL, 1, ps2_bench_write_sub_all},
   {"write-avg4-packed", 4, PS2_BENCH_WRITE_AVG_ALL, 1, ps2_bench_write_avg_all},
   {"write-paeth4-packed", 4, PS2_BENCH_WRITE_PAE_ALL, 1, ps2_bench_write_pae_all},
   {"write-sub6-packed", 6, PS2_BENCH_WRITE_SUB_ALL, 1, ps2_bench_write_sub_all},
   {"write-avg6-packed", 6, PS2_BENCH_WRITE_AVG_ALL, 1, ps2_bench_write_avg_all},
   {"write-paeth6-packed", 6, PS2_BENCH_WRITE_PAE_ALL, 1, ps2_bench_write_pae_all},
   {"write-sub8-packed", 8, PS2_BENCH_WRITE_SUB_ALL, 1, ps2_bench_write_sub_all},
   {"write-avg8-packed", 8, PS2_BENCH_WRITE_AVG_ALL, 1, ps2_bench_write_avg_all},
   {"write-paeth8-packed", 8, PS2_BENCH_WRITE_PAE_ALL, 1, ps2_bench_write_pae_all},
   {"write-sub4-mmi", 4, PS2_BENCH_WRITE_SUB4, 1, png_ps2_write_sub4_mmi},
   {"write-avg4-mmi", 4, PS2_BENCH_WRITE_AVG4, 1, png_ps2_write_avg4_mmi},
   {"write-paeth4-mmi", 4, PS2_BENCH_WRITE_PAETH, 1, png_ps2_write_paeth4_mmi},
   {"palette-rgba4", 1, PS2_BENCH_PALETTE, 1, ps2_bench_palette_expand},
   {"palette-rgb3", 1, PS2_BENCH_PAL_RGB, 1, ps2_bench_palette_rgb},
   {"palette-trns-rgba", 1, PS2_BENCH_PAL_TRNS, 1, ps2_bench_palette_trns},
   {"swap16", 2, PS2_BENCH_SWAP16, 1, ps2_bench_swap16},
   {"strip16", 2, PS2_BENCH_STRIP16, 1, ps2_bench_strip16},
   {"rgb-bgr", 3, PS2_BENCH_RGB_SWAP, 1, ps2_bench_swap_rgb},
   {"rgba-bgra", 4, PS2_BENCH_RGBA_SWAP, 1, ps2_bench_swap_rgba},
   {"rgb-rgba", 3, PS2_BENCH_RGB_TO_RGBA, 1, ps2_bench_rgb_to_rgba},
   {"rgba-rgb", 4, PS2_BENCH_RGBA_TO_RGB, 1, ps2_bench_rgba_to_rgb},
   {"adam7bytes1",1,PS2_BENCH_ADAM_BYTES,1,ps2_bench_adam_bytes},
   {"adam7bytes2",2,PS2_BENCH_ADAM_BYTES,1,ps2_bench_adam_bytes},
   {"adam7bytes3",3,PS2_BENCH_ADAM_BYTES,1,ps2_bench_adam_bytes},
   {"adam7bytes4",4,PS2_BENCH_ADAM_BYTES,1,ps2_bench_adam_bytes},
   {"adam7bytes6",6,PS2_BENCH_ADAM_BYTES,1,ps2_bench_adam_bytes},
   {"adam7bytes8",8,PS2_BENCH_ADAM_BYTES,1,ps2_bench_adam_bytes},
   {"adam7bits1",1,PS2_BENCH_ADAM_BITS,1,ps2_bench_adam_bits},
   {"adam7bits2",2,PS2_BENCH_ADAM_BITS,1,ps2_bench_adam_bits},
   {"adam7bits4",4,PS2_BENCH_ADAM_BITS,1,ps2_bench_adam_bits},

   {"up-mmi", 1, PS2_BENCH_UP, 1, png_read_filter_row_up_ps2},
#ifdef PNG_PS2_EE_MMI_UP_2X
   {"up-2x-direct", 1, PS2_BENCH_UP, 16, png_read_filter_row_up_2x_ps2},
#endif
#ifdef PNG_PS2_EE_MMI_SUB4_UNROLL4
   {"sub4-unroll4-direct", 4, PS2_BENCH_SUB, 4,
       png_read_filter_row_sub4_unroll4_ps2},
#endif
   {"sub1", 1, PS2_BENCH_SUB, 1, png_read_filter_row_sub1_ps2},
   {"sub2", 2, PS2_BENCH_SUB, 1, png_read_filter_row_sub2_ps2},
   {"sub3", 3, PS2_BENCH_SUB, 1, png_read_filter_row_sub3_ps2},
   {"sub4", 4, PS2_BENCH_SUB, 1, png_read_filter_row_sub4_ps2},
   {"sub6", 6, PS2_BENCH_SUB, 1, png_read_filter_row_sub6_ps2},
   {"sub8", 8, PS2_BENCH_SUB, 1, png_read_filter_row_sub8_ps2},
   {"sub3-packed-direct", 3, PS2_BENCH_SUB, 1, ps2_bench_sub3_packed},
   {"sub6-packed-direct", 6, PS2_BENCH_SUB, 1, ps2_bench_sub6_packed},
   {"sub8-packed-direct", 8, PS2_BENCH_SUB, 1, ps2_bench_sub8_packed},
   {"sub1-original-direct", 1, PS2_BENCH_SUB, 1, png_ps2_bench_sub1_original},
   {"sub2-original-direct", 2, PS2_BENCH_SUB, 1, png_ps2_bench_sub2_original},
   {"sub4-original-direct", 4, PS2_BENCH_SUB, 1, png_ps2_bench_sub4_original},
   {"avg4-original-direct", 4, PS2_BENCH_AVG, 1, png_ps2_bench_avg4_original},
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
