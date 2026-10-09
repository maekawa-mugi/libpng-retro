/* Integrated correctness + relative performance sweep for the EE harness.
 * Included by test_filter_mmi.c, never linked into libpng itself.
 *
 * Output is machine-readable BENCH,... CSV.  The portable clock() source
 * reports CLOCK TICKS (not cycles); optional PCCR0 reports processor
 * cycles on a privileged PS2SDK/EE environment only.
 * SPDX-License-Identifier: libpng-2.0
 */
#include <time.h>
#include "bench_ab_summary.h"
static ps2_bench_ab_summary ps2_bench_ab;
#ifndef PS2_BENCH_REFERENCE
#define PS2_BENCH_REFERENCE ps2_bench_scalar
#endif
#ifndef PS2_BENCH_SEED_STATE
#define PS2_BENCH_SEED_STATE(seed) ((void)(seed))
#endif
#ifndef PS2_BENCH_PROGRESS
#define PS2_BENCH_PROGRESS(name, index, total, width, passed, failed) ((void)0)
#endif
#ifndef PNG_PS2_BENCH_REPEATS
/* Nonrepresentative widths are correctness checks with one timed batch.
 * Three repetitions are used only on the representative autotuning
 * shapes, instead of tripling every edge-case test. */
#define PNG_PS2_BENCH_REPEATS 3U
#endif
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
#define PS2_BENCH_UNPACK_IDX    22U
#define PS2_BENCH_UNPACK_GRAY   23U
#define PS2_BENCH_GRAY_RGB      24U
#define PS2_BENCH_GRAY_RGBA     25U
#define PS2_BENCH_RGB_TRNS      26U
#define PS2_BENCH_SWAP_RB16     27U
#define PS2_BENCH_RGBA_ARGB     28U
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
/* One shape, many candidates. A winner is never inferred by summing
 * unrelated PNG transforms; different row widths are separate contests.
 * The two offsets model 16-byte aligned and 1-byte misaligned buffers. */
#define PS2_WIN_FILTERS 29U
#define PS2_WIN_BPPS 9U
#define PS2_WIN_WIDTHS 3U
#define PS2_WIN_ALIGNS 2U
typedef struct
{
   const char *name;
   unsigned long net_ticks, reference_ticks;
   unsigned int eligible;
   unsigned int plan_index;
} ps2_bench_winner;
static ps2_bench_winner ps2_bench_winners[PS2_WIN_FILTERS]
    [PS2_WIN_BPPS][PS2_WIN_WIDTHS][PS2_WIN_ALIGNS];
static unsigned int ps2_bench_winner_cases, ps2_bench_winner_groups;
static void
ps2_bench_consider(const char *name, unsigned int filter, unsigned int bpp,
    size_t n, unsigned int align, unsigned long candidate,
    unsigned long reference, unsigned int plan_index)
{
   unsigned int width_index;
   ps2_bench_winner *best;
   if (filter >= PS2_WIN_FILTERS || bpp >= PS2_WIN_BPPS ||
       (align != 0U && align != 1U) || candidate == 0U ||
       reference == 0U)
      return;
   width_index = n == 64 ? 0 : n == 1024 ? 1 : n == 4096 ? 2 : 3;
   if (width_index == 3U) return;
   best = &ps2_bench_winners[filter][bpp][width_index][align];
   ++best->eligible;
   ++ps2_bench_winner_cases;
   if (best->name == 0 || candidate < best->net_ticks)
   {
      best->name = name;
      best->net_ticks = candidate;
      best->reference_ticks = reference;
      best->plan_index = plan_index;
   }
}
static void
ps2_bench_print_winners(void)
{
   static const unsigned int width_values[3] = {64, 1024, 4096};
   unsigned int f, b, w, a;
   printf("FASTEST_HEADER,filter,bpp,rowbytes,alignment,winner,"
       "net_ticks,speedup_x1000,candidates\n");
   printf("AUTO_WIN_HEADER,filter,bpp,rowbytes,alignment,result,plan,"
       "candidate,scalar_ticks,plan_ticks,winning_ratio_x1000,plans\n");
   for (f = 0; f < PS2_WIN_FILTERS; ++f)
      for (b = 1; b < PS2_WIN_BPPS; ++b)
         for (w = 0; w < PS2_WIN_WIDTHS; ++w)
            for (a = 0; a < PS2_WIN_ALIGNS; ++a)
            {
               ps2_bench_winner *best = &ps2_bench_winners[f][b][w][a];
               unsigned long speedup;
               if (best->name == 0) continue;
               speedup = (unsigned long)(((unsigned long long)
                   best->reference_ticks * 1000ULL) / best->net_ticks);
               printf("FASTEST,%u,%u,%u,%u,%s,%lu,%lu,%u\n",f,b,
                   width_values[w],a,best->name,best->net_ticks,
                   speedup,best->eligible);
               {
                  enum ps2_bench_verdict verdict = ps2_bench_verdict(
                      best->reference_ticks, best->net_ticks);
                  const char *result = verdict == PS2_BENCH_PLAN_WIN ?
                      "PLAN_WIN" : verdict == PS2_BENCH_SCALAR_WIN ?
                      "SCALAR_WIN" : verdict == PS2_BENCH_TIE ?
                      "TIE" : "N/A";
                  unsigned long ratio = ps2_bench_winner_ratio_x1000(
                      best->reference_ticks, best->net_ticks);
                  printf("AUTO_WIN,%u,%u,%u,%u,%s,%c,%s,%lu,%lu,%lu,%u\n",
                      f,b,width_values[w],a,result,
                      ps2_bench_plan_letter(best->plan_index),best->name,
                      best->reference_ticks,best->net_ticks,ratio,
                      best->eligible);
               }
               ++ps2_bench_winner_groups;
            }
   printf("FASTEST_DONE,groups=%u,comparisons=%u\n",
       ps2_bench_winner_groups,ps2_bench_winner_cases);
}

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
#define PS2_BENCH_FREQUENCY 294912000UL
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
#define PS2_BENCH_FREQUENCY 1000000UL
#else
static void ps2_bench_clock_init(void) {}
static void ps2_bench_clock_done(void) {}
static unsigned long ps2_bench_now(void) { return (unsigned long)clock(); }
static unsigned long ps2_bench_delta(unsigned long a, unsigned long b)
{ return b - a; }
#define PS2_BENCH_UNIT "clock_ticks"
#define PS2_BENCH_FREQUENCY ((unsigned long)CLOCKS_PER_SEC)
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
      case PS2_BENCH_UNPACK_IDX: case PS2_BENCH_UNPACK_GRAY:
         *input=(n*bpp+7)/8; *output=n; break;
      case PS2_BENCH_GRAY_RGB:
         *output=3*n; break;
      case PS2_BENCH_GRAY_RGBA:
         *output=4*n; break;
      case PS2_BENCH_RGB_TRNS:
         *input=3*n; *output=4*n; break;
      case PS2_BENCH_SWAP_RB16:
         *input=*output=2*n*bpp; break;
      case PS2_BENCH_RGBA_ARGB:
         *input=*output=4*n; break;
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
ps2_bench_unpack_index(png_row_info *ri,png_byte *row,
    const png_byte *prev)
{
   (void)prev;
   png_ps2_unpack_packed8(row,row,ri->rowbytes,ps2_bench_bpp,0);
}
static void
ps2_bench_unpack_gray(png_row_info *ri,png_byte *row,
    const png_byte *prev)
{
   (void)prev;
   png_ps2_unpack_packed8(row,row,ri->rowbytes,ps2_bench_bpp,1);
}
static void
ps2_bench_gray_rgb(png_row_info *ri,png_byte *row,
    const png_byte *prev)
{
   (void)prev;
   png_ps2_gray8_to_rgb(row,row,ri->rowbytes,3,-1);
}
static void
ps2_bench_gray_rgba(png_row_info *ri,png_byte *row,
    const png_byte *prev)
{
   (void)prev;
   png_ps2_gray8_to_rgb(row,row,ri->rowbytes,4,127);
}
static void
ps2_bench_rgb_trns(png_row_info *ri,png_byte *row,
    const png_byte *prev)
{
   (void)prev;
   png_ps2_rgb8_trns_to_rgba(row,row,ri->rowbytes,0x11,0x22,0x33);
}
static void
ps2_bench_rb16(png_row_info *ri,png_byte *row,
    const png_byte *prev)
{
   (void)prev;
   png_ps2_swap_rb16(row,ri->rowbytes,ps2_bench_bpp);
}
static void
ps2_bench_argb(png_row_info *ri,png_byte *row,
    const png_byte *prev)
{
   (void)prev;
   png_ps2_rgba_to_argb(row,ri->rowbytes);
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
   if (kind == PS2_BENCH_UNPACK_IDX ||
       kind == PS2_BENCH_UNPACK_GRAY)
   {
      unsigned int mask=(1U<<bpp)-1U;
      for(i=n;i-- > 0;)
      {
         size_t bit=i*bpp;
         unsigned int sample=(row[bit>>3] >>
             (8U-bpp-(unsigned int)(bit&7U)))&mask;
         row[i]=(png_byte)(kind==PS2_BENCH_UNPACK_GRAY ?
             sample*(255U/mask) : sample);
      }
      return;
   }
   if (kind == PS2_BENCH_GRAY_RGB || kind == PS2_BENCH_GRAY_RGBA)
   {
      unsigned int stride=kind==PS2_BENCH_GRAY_RGB?3:4;
      for(i=n;i-- > 0;)
      {
         png_byte sample=row[i];
         row[stride*i]=sample;
         row[stride*i+1]=sample;
         row[stride*i+2]=sample;
         if(stride==4)row[4*i+3]=(png_byte)(sample==127?0:255);
      }
      return;
   }
   if (kind == PS2_BENCH_RGB_TRNS)
   {
      for(i=n;i-- > 0;)
      {
         png_byte a=row[3*i], c=row[3*i+1], d=row[3*i+2];
         row[4*i]=a;row[4*i+1]=c;row[4*i+2]=d;
         row[4*i+3]=(png_byte)(a==0x11&&c==0x22&&d==0x33?0:255);
      }
      return;
   }
   if (kind == PS2_BENCH_SWAP_RB16)
   {
      unsigned int stride=2*bpp;
      for(i=0;i<n;++i)
      {
         png_byte h=row[stride*i], l=row[stride*i+1];
         row[stride*i]=row[stride*i+4];
         row[stride*i+1]=row[stride*i+5];
         row[stride*i+4]=h;
         row[stride*i+5]=l;
      }
      return;
   }
   if (kind == PS2_BENCH_RGBA_ARGB)
   {
      for(i=0;i<n;++i)
      {
         png_byte a=row[4*i],c=row[4*i+1],d=row[4*i+2],e=row[4*i+3];
         row[4*i]=e;row[4*i+1]=a;row[4*i+2]=c;row[4*i+3]=d;
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
#ifndef PNG_PS2_BENCH_TEST_VARIANTS
#include "bench_baseline_mmi.c"
#endif

static const ps2_bench_variant ps2_bench_variants[] = {
#ifdef PNG_PS2_BENCH_TEST_VARIANTS
   PNG_PS2_BENCH_TEST_VARIANTS
#else
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
   {"unpack-index1",1,PS2_BENCH_UNPACK_IDX,1,ps2_bench_unpack_index},
   {"unpack-index2",2,PS2_BENCH_UNPACK_IDX,1,ps2_bench_unpack_index},
   {"unpack-index4",4,PS2_BENCH_UNPACK_IDX,1,ps2_bench_unpack_index},
   {"unpack-gray1",1,PS2_BENCH_UNPACK_GRAY,1,ps2_bench_unpack_gray},
   {"unpack-gray2",2,PS2_BENCH_UNPACK_GRAY,1,ps2_bench_unpack_gray},
   {"unpack-gray4",4,PS2_BENCH_UNPACK_GRAY,1,ps2_bench_unpack_gray},
   {"gray-rgb",1,PS2_BENCH_GRAY_RGB,1,ps2_bench_gray_rgb},
   {"gray-rgba-trns",1,PS2_BENCH_GRAY_RGBA,1,ps2_bench_gray_rgba},
   {"rgb-trns-rgba",3,PS2_BENCH_RGB_TRNS,1,ps2_bench_rgb_trns},
   {"swap-rb16rgb",3,PS2_BENCH_SWAP_RB16,1,ps2_bench_rb16},
   {"swap-rb16rgba",4,PS2_BENCH_SWAP_RB16,1,ps2_bench_rb16},
   {"rgba-to-argb",4,PS2_BENCH_RGBA_ARGB,1,ps2_bench_argb},

   {"up-mmi", 1, PS2_BENCH_UP, 1, png_read_filter_row_up_ps2},
#ifdef PNG_PS2_EE_MMI_UP_SCHEDULES
   {"up-2x-interleaved", 1, PS2_BENCH_UP, 16,
       png_read_filter_row_up_2x_interleaved_ps2},
   {"up-2x-prefetch", 1, PS2_BENCH_UP, 16,
       png_read_filter_row_up_2x_prefetch_ps2},
   {"up-1x-prefetch", 1, PS2_BENCH_UP, 16,
       png_read_filter_row_up_1x_prefetch_ps2},
   {"up-4x", 1, PS2_BENCH_UP, 64,
       png_read_filter_row_up_4x_ps2},
   {"up-4x-prefetch", 1, PS2_BENCH_UP, 64,
       png_read_filter_row_up_4x_prefetch_ps2},
#endif
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
#endif /* PNG_PS2_BENCH_TEST_VARIANTS */
};

static ps2_bench_ab_result ps2_bench_results[
    sizeof ps2_bench_variants / sizeof ps2_bench_variants[0]];

/* Return batch timer ticks, including row replay. A separate copy-only
 * batch measures the replay overhead to subtract from each side. */
static unsigned long
ps2_bench_batch(ps2_bench_fn fn, png_row_info *ri, png_byte *dst,
    const png_byte *src, const png_byte *prev, unsigned int loops)
{
   unsigned long start, finish;
   unsigned int k;
   start = ps2_bench_now();
   for (k = 0; k < loops; ++k)
   {
      memcpy(dst, src, ps2_bench_input_bytes);
      /* Each replay restores the expansion area to a poison value.
       * Without this, a previous repetition could hide an incomplete
       * in-place conversion in a later repetition.  It is part of the
       * copy-only baseline too, so the reported net time subtracts it. */
      if (ps2_bench_output_bytes > ps2_bench_input_bytes)
         memset(dst + ps2_bench_input_bytes, 0xa5,
             ps2_bench_output_bytes - ps2_bench_input_bytes + 16U);
      if (fn != 0)
         fn(ri, dst, prev);
      ps2_bench_sink += dst[0];
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
   unsigned int rounds = (ri->rowbytes == 64U ||
       ri->rowbytes == 1024U || ri->rowbytes == 4096U) ?
       PNG_PS2_BENCH_REPEATS : 1U;
   for (rep = 0; rep < rounds; ++rep)
   {
      unsigned long elapsed = ps2_bench_batch(fn, ri, dst, src, prev, loops);
      if (elapsed < best) best = elapsed;
   }
   return best;
}

static int
png_ps2_bench_all(void)
{
   static const unsigned int widths[] = {
       1, 2, 3, 4, 5, 7, 8, 15, 16, 17, 31,
       32, 64, 128, 256, 1024, 4096, 16384};
   static const unsigned int offsets[] = {0, 1, 3, 4, 8, 12, 15};
   unsigned int index, wi, oi, j, fails = 0, successes = 0;
   png_byte *src = aligned16(ps2_bench_source);
   png_byte *dst = aligned16(ps2_bench_row);
   png_byte *prev = aligned16(ps2_bench_prev);
   png_byte *exp = aligned16(ps2_bench_expect);
   png_byte *saved = aligned16(ps2_bench_prev_save);

   memset(&ps2_bench_ab, 0, sizeof ps2_bench_ab);
   memset(ps2_bench_results, 0, sizeof ps2_bench_results);
   memset(ps2_bench_winners, 0, sizeof ps2_bench_winners);
   ps2_bench_winner_cases = ps2_bench_winner_groups = 0;

   printf("BENCH_INFO,unit=%s,clock_per_sec=%lu,repeats=%u,subtract_copy=1\n",
       PS2_BENCH_UNIT, (unsigned long)CLOCKS_PER_SEC,
       (unsigned int)PNG_PS2_BENCH_REPEATS);
   printf("BENCH_HEADER,variant,filter,bpp,rowbytes,row_align,prev_align,"
          "loops,copy_ticks,scalar_ticks,optimized_ticks,"
          "scalar_net_ticks,optimized_net_ticks,optimized_ticks_per_byte_x1000\n");
   for (j = 0; j < 1024U; ++j)
      ps2_bench_palette_table[j] = random_byte();
   for (j = 0; j < 768U; ++j)
      ps2_bench_rgb_table[j] = random_byte();
   for (j = 0; j < 256U; ++j)
      ps2_bench_alpha_table[j] = random_byte();
   ps2_bench_clock_init();

   for (index = 0; index < sizeof ps2_bench_variants /
       sizeof ps2_bench_variants[0]; ++index)
   {
      const ps2_bench_variant *v = &ps2_bench_variants[index];
      ps2_bench_ab_result *item = &ps2_bench_results[index];
      item->state[0] = item->state[1] = PS2_AB_RUNNING;
      /* Stable plan labels per filter/bpp, independent of row width,
       * pointer alignment or which paths become eligible on a row. */
      unsigned int previous, plan_index = 0;
      for (previous = 0; previous < index; ++previous)
         if (ps2_bench_variants[previous].filter == v->filter &&
             ps2_bench_variants[previous].bpp == v->bpp)
            ++plan_index;
      PS2_BENCH_PROGRESS(v->name, index + 1,
          sizeof ps2_bench_variants / sizeof ps2_bench_variants[0],
          0, successes, fails);
      for (wi = 0; wi < sizeof widths / sizeof widths[0]; ++wi)
      {
         size_t n = widths[wi];
         unsigned int loops;
#ifdef PNG_PS2_TEST_EXHAUSTIVE
         loops = 32768U / (unsigned int)n;
         if (loops < 32U) loops = 32U;
         if (loops > 512U) loops = 512U;
#else
         loops = n < 32 ? 1U : 8192U / (unsigned int)n;
         if (loops < 2U && n >= 32) loops = 2U;
         if (loops > 64U) loops = 64U;
#endif
         if (n == 64U && loops < 128U) loops = 128U;
         if (n == 1024U && loops < 32U) loops = 32U;
         if (n == 4096U && loops < 8U) loops = 8U;
         if (n < v->minlen) continue;

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
            int a_failed = 0, b_failed;
            size_t previous_bytes, checked_bytes;
            ri.rowbytes = n;
            ps2_bench_shape(v->filter, v->bpp, n,
                &ps2_bench_input_bytes, &ps2_bench_output_bytes,
                &previous_bytes);
            checked_bytes = ps2_bench_input_bytes > ps2_bench_output_bytes ?
                ps2_bench_input_bytes : ps2_bench_output_bytes;
            if (checked_bytes + offsets[oi] + 16 > PS2_BENCH_CAP ||
                previous_bytes + ((offsets[oi] * 7U) & 15U) + 16 >
                    PS2_BENCH_CAP)
               continue;
            ps2_bench_bpp = v->bpp;
            ps2_bench_filter = v->filter;

            /* Every competing candidate receives IDENTICAL pseudorandom
             * row data for a given filter/bpp/length/alignment shape.
             * Repeated runs are reproducible independent of candidate
             * declaration order. */
            PS2_BENCH_SEED_STATE((0x735a2dc1U ^
                (v->filter * 0x9e3779b9U) ^
                (v->bpp * 0x85ebca6bU) ^
                ((unsigned int)n * 0xc2b2ae35U) ^ offsets[oi]) | 1U);
            for (j = 0; j < ps2_bench_input_bytes + 16; ++j)
               s[j] = j < ps2_bench_input_bytes ? random_byte() : 0xa5;
            for (j = 0; j < previous_bytes + 16; ++j)
               p[j] = j < previous_bytes ? random_byte() : 0x5a;
            if (v->filter == PS2_BENCH_GRAY_RGBA)
               for (j=0;j<n;j+=7) s[j]=127;
            if (v->filter == PS2_BENCH_RGB_TRNS)
               for (j=0;j<n;j+=7)
               {
                  s[3*j]=0x11;
                  s[3*j+1]=0x22;
                  s[3*j+2]=0x33;
               }
            memcpy(q, p, previous_bytes + 16);
            memcpy(e, s, ps2_bench_input_bytes + 16);
            memcpy(r, s, ps2_bench_input_bytes + 16);
            if (ps2_bench_output_bytes > ps2_bench_input_bytes)
            {
               memset(e + ps2_bench_input_bytes + 16, 0xa5,
                   ps2_bench_output_bytes - ps2_bench_input_bytes);
               memset(r + ps2_bench_input_bytes + 16, 0xa5,
                   ps2_bench_output_bytes - ps2_bench_input_bytes);
            }
            /* Timed batches produce the outputs to validate: no extra run. */
            copy_time = ps2_bench_best(0, &ri, r, s, p, loops);
            scalar_time = ps2_bench_best(PS2_BENCH_REFERENCE, &ri, e, s, p, loops);
            /* A is the scalar reference. Check its bounds and input
             * preservation independently, before B can touch the buffers. */
            for (j = 0; j < 16; ++j)
               if (e[checked_bytes + j] != 0xa5) a_failed = 1;
            if (memcmp(q, p, previous_bytes + 16)) a_failed = 1;
            memcpy(p, q, previous_bytes + 16);
            candidate_time = ps2_bench_best(v->fn, &ri, r, s, p, loops);

            net_scalar = scalar_time > copy_time ? scalar_time - copy_time : 0;
            net_candidate = candidate_time > copy_time ? candidate_time - copy_time : 0;
            item->ticks[0] += net_scalar;
            item->ticks[1] += net_candidate;
            ++item->checked;
            b_failed = a_failed || memcmp(e, r, checked_bytes + 16) ||
                memcmp(q, p, previous_bytes + 16);
            item->failures[0] += a_failed != 0;
            item->failures[1] += b_failed != 0;
            if (a_failed || b_failed)
            {
               ++fails;
               printf("BENCH_FAIL,%s,%u,%lu,%u\n", v->name,
                   v->bpp, (unsigned long)n, offsets[oi]);
               PS2_BENCH_PROGRESS(v->name, index + 1,
                   sizeof ps2_bench_variants / sizeof ps2_bench_variants[0],
                   (unsigned long)n, successes, fails);
               /* Finish the matrix so each side has its own complete result. */
               continue;
            }
            ++successes;

            ps2_bench_ab_add(&ps2_bench_ab, copy_time, scalar_time,
                candidate_time);
            ps2_bench_consider(v->name, v->filter, v->bpp, n,
                offsets[oi], net_candidate, net_scalar, plan_index);
            scaled = ps2_bench_output_bytes && loops ?
                (unsigned long)(((unsigned long long)net_candidate * 1000ULL) /
                    ((unsigned long long)ps2_bench_output_bytes * loops)) : 0;
            printf("BENCH,%s,%u,%u,%lu,%u,%u,%u,%lu,%lu,%lu,%lu,%lu,%lu\n",
                v->name, v->filter, v->bpp, (unsigned long)n,
                offsets[oi], ((offsets[oi] * 7U) & 15U), loops,
                copy_time, scalar_time, candidate_time,
                net_scalar, net_candidate, scaled);
         }
         PS2_BENCH_PROGRESS(v->name, index + 1,
             sizeof ps2_bench_variants / sizeof ps2_bench_variants[0],
             (unsigned long)n, successes, fails);
      }
      item->state[0] = item->failures[0] ? PS2_AB_FAIL : PS2_AB_PASS;
      item->state[1] = item->failures[1] ? PS2_AB_FAIL : PS2_AB_PASS;
      printf("RESULT,%s,A,%s,%llu,B,%s,%llu,cases=%u,unit=%s\n",
          v->name, item->failures[0] ? "X" : "O", item->ticks[0],
          item->failures[1] ? "X" : "O", item->ticks[1],
          item->checked, PS2_BENCH_UNIT);
      PS2_BENCH_PROGRESS(v->name, index + 1,
          sizeof ps2_bench_variants / sizeof ps2_bench_variants[0],
          0, successes, fails);
   }
   ps2_bench_clock_done();
   ps2_bench_print_winners();
   ps2_bench_ab.completed = fails == 0;
   if (!fails) printf("FUSED_PASS,cases=%u,variants=%u\n", successes,
       (unsigned int)(sizeof ps2_bench_variants / sizeof ps2_bench_variants[0]));
   printf("BENCH_DONE,passed=%u,failed=%u,sink=%u\n",
       successes, fails, ps2_bench_sink);
   return fails ? 1 : 0;
}
