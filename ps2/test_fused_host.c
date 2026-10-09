/* Run the actual fused sweep with portable production transform kernels.
 * Inject wrong output, canary damage and previous-row writes to verify that
 * results are retained per item/side, and failed runs do not report success. */
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
typedef unsigned char png_byte;
typedef unsigned int png_uint_32;
typedef struct { size_t rowbytes; } png_row_info;
#define PNG_PS2_EXTRA_PORTABLE 1
#define PNG_PS2_RGB3_PORTABLE_ADD 1
#define PNG_PS2_WIDE_PORTABLE_ADD 1
#include "filter_rgb3.c"
#include "filter_wide_mmi.c"
#include "extra_kernels_mmi.c"
#include "extra_full_kernels.c"
#include "extra_color_kernels.c"
static unsigned int state = 12345, updates, fault;
static png_byte random_byte(void)
{
   state ^= state << 13; state ^= state >> 17; state ^= state << 5;
   return (png_byte)state;
}
static png_byte *aligned16(png_byte *p)
{ return p + ((16U - ((size_t)p & 15U)) & 15U); }
static void progress(const char *name, unsigned int i, unsigned int total,
    unsigned long width, unsigned int passed, unsigned int failed)
{
   (void)name; (void)width; (void)passed; (void)failed;
   assert(i > 0 && i <= total);
   ++updates;
}
static void candidate(png_row_info *, png_byte *, const png_byte *);
static void reference(png_row_info *, png_byte *, const png_byte *);
#define PS2_BENCH_REFERENCE reference
#define PS2_BENCH_PROGRESS progress
#define PNG_PS2_BENCH_TEST_VARIANTS \
   {"host-write",1,PS2_BENCH_WRITE_UP,1,candidate}, \
   {"host-rgb-rgba",3,PS2_BENCH_RGB_TO_RGBA,1,ps2_bench_rgb_to_rgba}, \
   {"host-rgba-rgb",4,PS2_BENCH_RGBA_TO_RGB,1,ps2_bench_rgba_to_rgb}, \
   {"host-palette",1,PS2_BENCH_PAL_TRNS,1,ps2_bench_palette_trns}, \
   {"host-swap16",2,PS2_BENCH_SWAP16,1,ps2_bench_swap16}, \
   {"host-gray",1,PS2_BENCH_GRAY_RGBA,1,ps2_bench_gray_rgba}, \
   {"host-adam-bytes",3,PS2_BENCH_ADAM_BYTES,1,ps2_bench_adam_bytes}, \
   {"host-adam-bits",2,PS2_BENCH_ADAM_BITS,1,ps2_bench_adam_bits},
#define PS2_BENCH_SEED_STATE(seed) (state = (seed))
#include "bench_filter_mmi.c"
static void reference(png_row_info *ri, png_byte *r, const png_byte *p)
{
   ps2_bench_scalar(ri, r, p);
   if (fault == 4 && ps2_bench_filter == PS2_BENCH_WRITE_UP)
      r[ri->rowbytes] = 0;
}
static void candidate(png_row_info *ri, png_byte *r, const png_byte *p)
{
   png_ps2_write_up_mmi(ri,r,p);
   if (fault == 1) r[0] ^= 1;
   if (fault == 2) r[ri->rowbytes] ^= 1;
   if (fault == 3) ((png_byte *)p)[0] ^= 1;
}
int main(void)
{
   /* Copy-adjusted benchmark selection must reject a one-off low outlier,
    * including the zero-tick artifact that creates fictional speedups. */
   {
      unsigned long three[3] = {100UL, 1UL, 101UL};
      unsigned long equal[3] = {8UL, 8UL, 8UL};
      unsigned long single[1] = {77UL};
      assert(ps2_bench_median(three, 3) == 100UL);
      assert(ps2_bench_median(equal, 3) == 8UL);
      assert(ps2_bench_median(single, 1) == 77UL);
   }
   assert(png_ps2_bench_all() == 0);
   assert(ps2_bench_ab.completed && ps2_bench_ab.cases == 8U * 18U * 7U);
   assert(updates == 8U * 20U);
   for (fault = 0; fault < 8; ++fault)
   {
      assert(ps2_bench_results[fault].state[0] == PS2_AB_PASS);
      assert(ps2_bench_results[fault].state[1] == PS2_AB_PASS);
      assert(ps2_bench_results[fault].checked == 18U * 7U);
   }
   for (fault = 1; fault <= 4; ++fault)
   {
      updates = 0;
      assert(png_ps2_bench_all() == 1);
      unsigned int i;
      assert(!ps2_bench_ab.completed);
      assert(ps2_bench_results[0].state[0] ==
          (fault == 4 ? PS2_AB_FAIL : PS2_AB_PASS));
      assert(ps2_bench_results[0].state[1] == PS2_AB_FAIL);
      assert(ps2_bench_results[0].checked == 18U * 7U);
      for (i = 1; i < 8; ++i)
      {
         assert(ps2_bench_results[i].state[0] == PS2_AB_PASS);
         assert(ps2_bench_results[i].state[1] == PS2_AB_PASS);
      }
      assert(updates > 8U * 20U);
   }
   puts("PASS per-item independent A/B results, progress and four injected faults");
   /* A contest never ranks unlike problems; ensure fast wins a matched
    * shape while a missing timer result cannot win. */
   memset(ps2_bench_winners, 0, sizeof ps2_bench_winners);
   ps2_bench_consider("slow", PS2_BENCH_UP, 1, 1024, 0, 30, 60, 0);
   ps2_bench_consider("fast", PS2_BENCH_UP, 1, 1024, 0, 10, 60, 1);
   ps2_bench_consider("invalid", PS2_BENCH_UP, 1, 1024, 0, 0, 60, 2);
   ps2_bench_consider("unrelated", PS2_BENCH_SUB, 1, 1024, 0, 1, 60, 0);
   assert(strcmp(ps2_bench_winners[PS2_BENCH_UP][1][1][0].name,
       "fast") == 0);
   assert(ps2_bench_winners[PS2_BENCH_UP][1][1][0].eligible == 2);
   assert(ps2_bench_winners[PS2_BENCH_UP][1][1][0].plan_index == 1);
   assert(strcmp(ps2_bench_winners[PS2_BENCH_SUB][1][1][0].name,
       "unrelated") == 0);
   puts("PASS fused sweep, progress, output/canary/input failure detection");
   return 0;
}
