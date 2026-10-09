/* ps2/test_filter_mmi.c - small independent EE MMI row-filter test
 *
 * Build as an EE program (PS2SDK), not as part of libpng.
 * This code is released under the libpng license.
 */
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#ifdef _EE
#include <debug.h>
#include <kernel.h>
#include <stdarg.h>
#include <unistd.h>
static void test_log(const char *format, ...)
{
   char message[512];
   va_list args;
   va_start(args, format);
   vsnprintf(message, sizeof message, format, args);
   va_end(args);
   /* stdout only; the debug screen is drawn with fixed coordinates. */
   printf("%s", message);
   if (strncmp(message, "BENCH", 5) != 0 ||
       strncmp(message, "BENCH_DONE", 10) == 0 ||
       strncmp(message, "BENCH_FAIL", 10) == 0)
      fflush(stdout);
}
#define printf test_log
#endif

typedef unsigned char png_byte;
typedef unsigned int png_uint_32;
typedef struct png_row_info_test_struct
{
   size_t rowbytes;
   png_uint_32 width;
   png_byte color_type, channels, bit_depth, pixel_depth;
} png_row_info;
typedef struct { png_byte red, green, blue; } png_color;
#define PNG_COLOR_TYPE_PALETTE 3
#define PNG_COLOR_TYPE_RGB 2
#define PNG_COLOR_TYPE_RGB_ALPHA 6

#include "filter_mmi.c"
#ifdef PNG_PS2_EE_MMI_UP_SCHEDULES
#include "filter_up_schedules_mmi.c"
#include "filter_up4_mmi.c"
#endif

#define TEST_MAX 1024U
#define BUF_SIZE (TEST_MAX + 64U)

#if !defined(PNG_PS2_BENCH_ENABLE) || defined(PNG_PS2_TEST_EXHAUSTIVE)
static png_byte row_storage[BUF_SIZE];
static png_byte prev_storage[BUF_SIZE];
static png_byte expected[BUF_SIZE];
static png_byte prev_original[BUF_SIZE];
#endif
static unsigned int rng_state = 0x735a2dc1U;

static png_byte *
aligned16(png_byte *p)
{
   return p + ((16U - ((size_t)p & 15U)) & 15U);
}

static png_byte
random_byte(void)
{
   rng_state ^= rng_state << 13;
   rng_state ^= rng_state >> 17;
   rng_state ^= rng_state << 5;
   return (png_byte)rng_state;
}

#if !defined(PNG_PS2_BENCH_ENABLE) || defined(PNG_PS2_TEST_EXHAUSTIVE)
static void
reference_up(png_byte *row, const png_byte *prev, size_t n)
{
   size_t i;
   for (i = 0; i < n; ++i)
      row[i] = (png_byte)((unsigned int)row[i] + (unsigned int)prev[i]);
}

static void
reference_sub4(png_byte *row, size_t n)
{
   size_t i;
   for (i = 4; i < n; ++i)
      row[i] = (png_byte)((unsigned int)row[i] + (unsigned int)row[i - 4]);
}

static void
reference_sub3(png_byte *row, size_t n)
{
   size_t i;
   for (i = 3; i < n; ++i)
      row[i] = (png_byte)((unsigned int)row[i] + (unsigned int)row[i - 3]);
}

static void
reference_avg3(png_byte *row, const png_byte *prev, size_t n)
{
   size_t i;
   for (i = 0; i < n; ++i)
   {
      unsigned int left = i >= 3 ? row[i - 3] : 0;
      row[i] = (png_byte)((unsigned int)row[i] + ((left + prev[i]) >> 1));
   }
}

static void
reference_avg4(png_byte *row, const png_byte *prev, size_t n)
{
   size_t i;
   for (i = 0; i < n; ++i)
   {
      unsigned int left = i >= 4 ? row[i - 4] : 0;
      row[i] = (png_byte)((unsigned int)row[i] +
          ((left + (unsigned int)prev[i]) >> 1));
   }
}

#ifdef PNG_PS2_EE_MMI_PAETH
static unsigned int
paeth_abs_test(int value)
{
   return (unsigned int)(value < 0 ? -value : value);
}
static unsigned int
paeth_predict_test(unsigned int a, unsigned int b, unsigned int c)
{
   int p = (int)a + (int)b - (int)c;
   unsigned int da = paeth_abs_test(p - (int)a);
   unsigned int db = paeth_abs_test(p - (int)b);
   unsigned int dc = paeth_abs_test(p - (int)c);
   if (da <= db && da <= dc) return a;
   if (db <= dc) return b;
   return c;
}
static void
reference_paeth(png_byte *row, const png_byte *prev, size_t n,
    unsigned int bpp)
{
   size_t i;
   for (i = 0; i < n; ++i)
   {
      unsigned int a = i >= bpp ? row[i - bpp] : 0;
      unsigned int b = prev[i];
      unsigned int c = i >= bpp ? prev[i - bpp] : 0;
      row[i] = (png_byte)((unsigned int)row[i] +
          paeth_predict_test(a, b, c));
   }
}
#endif /* PNG_PS2_EE_MMI_PAETH */

static void
reference_sub1(png_byte *row, size_t n)
{
   size_t i;
   for (i = 1; i < n; ++i)
      row[i] = (png_byte)((unsigned int)row[i] + (unsigned int)row[i - 1]);
}

static void
reference_sub2(png_byte *row, size_t n)
{
   size_t i;
   for (i = 2; i < n; ++i)
      row[i] = (png_byte)((unsigned int)row[i] + (unsigned int)row[i - 2]);
}

#ifdef PNG_PS2_EE_MMI_GRAY_AVG
static void
reference_avg1(png_byte *row, const png_byte *prev, size_t n)
{
   size_t i;
   for (i = 0; i < n; ++i)
   {
      unsigned int left = i ? row[i - 1] : 0;
      row[i] = (png_byte)((unsigned int)row[i] + ((left + prev[i]) >> 1));
   }
}
static void
reference_avg2(png_byte *row, const png_byte *prev, size_t n)
{
   size_t i;
   for (i = 0; i < n; ++i)
   {
      unsigned int left = i >= 2 ? row[i - 2] : 0;
      row[i] = (png_byte)((unsigned int)row[i] + ((left + prev[i]) >> 1));
   }
}
#endif

static void
reference_wide_sub(png_byte *row, size_t n, unsigned int bpp)
{
   size_t i;
   for (i = bpp; i < n; ++i)
      row[i] = (png_byte)((unsigned int)row[i] + (unsigned int)row[i - bpp]);
}
#ifdef PNG_PS2_EE_MMI_WIDE_AVG
static void
reference_wide_avg(png_byte *row, const png_byte *prev, size_t n,
    unsigned int bpp)
{
   size_t i;
   for (i = 0; i < n; ++i)
   {
      unsigned int left = i >= bpp ? row[i - bpp] : 0;
      row[i] = (png_byte)((unsigned int)row[i] +
          ((left + (unsigned int)prev[i]) >> 1));
   }
}
#endif

#ifdef PNG_PS2_EE_MMI_GRAY_AVG
#define PNG_PS2_TEST_GRAY_AVG_COUNT 2
#else
#define PNG_PS2_TEST_GRAY_AVG_COUNT 0
#endif
#ifdef PNG_PS2_EE_MMI_PAETH
#define PNG_PS2_TEST_PAETH_COUNT 4
#else
#define PNG_PS2_TEST_PAETH_COUNT 0
#endif
#define PNG_PS2_TEST_PAETH_START (7 + PNG_PS2_TEST_GRAY_AVG_COUNT)
#define PNG_PS2_TEST_WIDE_SUB_START (PNG_PS2_TEST_PAETH_START + PNG_PS2_TEST_PAETH_COUNT)
#ifdef PNG_PS2_EE_MMI_WIDE_AVG
#define PNG_PS2_TEST_WIDE_AVG_COUNT 2
#else
#define PNG_PS2_TEST_WIDE_AVG_COUNT 0
#endif
#define PNG_PS2_TEST_WIDE_AVG_START (PNG_PS2_TEST_WIDE_SUB_START + 2)
#define PNG_PS2_TEST_WIDE_PAETH_START (PNG_PS2_TEST_WIDE_AVG_START + PNG_PS2_TEST_WIDE_AVG_COUNT)
#ifdef PNG_PS2_EE_MMI_PAETH
#define PNG_PS2_TEST_WIDE_PAETH_COUNT 2
#else
#define PNG_PS2_TEST_WIDE_PAETH_COUNT 0
#endif
#define PNG_PS2_TEST_TOTAL (PNG_PS2_TEST_WIDE_PAETH_START + PNG_PS2_TEST_WIDE_PAETH_COUNT)
#endif

/* The default benchmark uses its measured outputs for correctness checks.
 * PNG_PS2_TEST_EXHAUSTIVE additionally runs the original exhaustive suites. */
#ifdef PNG_PS2_BENCH_ENABLE
#ifdef PNG_PS2_TEST_EXHAUSTIVE
#include "test_extra_kernels.c"
#else
#include "extra_kernels_mmi.c"
#include "extra_full_kernels.c"
#include "extra_color_kernels.c"
#endif
#ifdef _EE
static void test_live(const char *, unsigned int, unsigned int,
    unsigned long, unsigned int, unsigned int);
#define PS2_BENCH_PROGRESS test_live
#endif
#define PS2_BENCH_SEED_STATE(seed) (rng_state = (seed))
#include "bench_filter_mmi.c"
/* Opt-in palette's actual production worker: test it through the same
 * png_row_info contract on EE, not just through microkernel adapters. */
#include "palette_production.c"
#include "test_palette_hook.c"
#if defined(_EE) && defined(PNG_PS2_SPR_BENCH)
#include "spr_bench.c"
#endif
#endif

static int
run_filters(void)
{
#if defined(PNG_PS2_BENCH_ENABLE) && !defined(PNG_PS2_TEST_EXHAUSTIVE)
   if (png_ps2_bench_all() != 0) return 1;
   return png_ps2_test_palette_hook();
#else
   unsigned int filter;
   unsigned int offset;
   size_t len;
   unsigned int cases = 0;

   for (filter = 0; filter < PNG_PS2_TEST_TOTAL; ++filter)
   {
      printf("FILTER %u/%u RUNNING\n", filter + 1,
          (unsigned int)PNG_PS2_TEST_TOTAL);
      for (offset = 0; offset < 16; ++offset)
      {
         for (len = 0; len <= TEST_MAX; ++len)
         {
            png_row_info row_info;
            png_byte *row = aligned16(row_storage) + offset;
            png_byte *prev = aligned16(prev_storage) + ((offset * 7) & 15U);
            size_t i;

            for (i = 0; i < len + 16; ++i)
            {
               row[i] = i < len ? random_byte() : 0xa5;
               prev[i] = i < len ? random_byte() : 0x5a;
            }

            memcpy(expected, row, len + 16);
            memcpy(prev_original, prev, len + 16);
            row_info.rowbytes = len;

            if (filter == 0)
            {
               reference_up(expected, prev, len);
               png_read_filter_row_up_ps2(&row_info, row, prev);
            }
            else if (filter == 1)
            {
               reference_sub4(expected, len);
               png_read_filter_row_sub4_ps2(&row_info, row, prev);
            }
            else if (filter == 2)
            {
               reference_avg4(expected, prev, len);
               png_read_filter_row_avg4_ps2(&row_info, row, prev);
            }
            else if (filter == 3)
            {
               reference_sub3(expected, len);
               png_read_filter_row_sub3_ps2(&row_info, row, prev);
            }
            else if (filter == 4)
            {
               reference_avg3(expected, prev, len);
               png_read_filter_row_avg3_ps2(&row_info, row, prev);
            }
            else if (filter == 5)
            {
               reference_sub1(expected, len);
               png_read_filter_row_sub1_ps2(&row_info, row, prev);
            }
            else if (filter == 6)
            {
               reference_sub2(expected, len);
               png_read_filter_row_sub2_ps2(&row_info, row, prev);
            }
#ifdef PNG_PS2_EE_MMI_GRAY_AVG
            else if (filter == 7)
            {
               reference_avg1(expected, prev, len);
               png_read_filter_row_avg1_ps2(&row_info, row, prev);
            }
            else if (filter == 8)
            {
               reference_avg2(expected, prev, len);
               png_read_filter_row_avg2_ps2(&row_info, row, prev);
            }
#endif
#ifdef PNG_PS2_EE_MMI_PAETH
            else if (filter < PNG_PS2_TEST_WIDE_SUB_START)
            {
               unsigned int bpp = filter - PNG_PS2_TEST_PAETH_START + 1;
               reference_paeth(expected, prev, len, bpp);
               if (bpp == 1)
                  png_read_filter_row_paeth1_ps2(&row_info, row, prev);
               else if (bpp == 2)
                  png_read_filter_row_paeth2_ps2(&row_info, row, prev);
               else if (bpp == 3)
                  png_read_filter_row_paeth3_ps2(&row_info, row, prev);
               else
                  png_read_filter_row_paeth4_ps2(&row_info, row, prev);
            }
#endif
            else if (filter == PNG_PS2_TEST_WIDE_SUB_START)
            {
               reference_wide_sub(expected, len, 6);
               png_read_filter_row_sub6_ps2(&row_info, row, prev);
            }
            else if (filter == PNG_PS2_TEST_WIDE_SUB_START + 1)
            {
               reference_wide_sub(expected, len, 8);
               png_read_filter_row_sub8_ps2(&row_info, row, prev);
            }
#ifdef PNG_PS2_EE_MMI_WIDE_AVG
            else if (filter == PNG_PS2_TEST_WIDE_AVG_START)
            {
               reference_wide_avg(expected, prev, len, 6);
               png_read_filter_row_avg6_ps2(&row_info, row, prev);
            }
            else if (filter == PNG_PS2_TEST_WIDE_AVG_START + 1)
            {
               reference_wide_avg(expected, prev, len, 8);
               png_read_filter_row_avg8_ps2(&row_info, row, prev);
            }
#endif
#ifdef PNG_PS2_EE_MMI_PAETH
            else if (filter == PNG_PS2_TEST_WIDE_PAETH_START)
            {
               reference_paeth(expected, prev, len, 6);
               png_read_filter_row_paeth6_ps2(&row_info, row, prev);
            }
            else
            {
               reference_paeth(expected, prev, len, 8);
               png_read_filter_row_paeth8_ps2(&row_info, row, prev);
            }
#endif

            if (memcmp(expected, row, len + 16) != 0 ||
                memcmp(prev_original, prev, len + 16) != 0)
            {
               printf("FAIL: %s len=%lu offset=%u\n",
                   filter == 0 ? "Up" : filter == 1 ? "Sub4" :
                       filter == 2 ? "Average4" :
                       filter == 3 ? "Sub3" :
                       filter == 4 ? "Average3" :
                       filter == 5 ? "Sub1" :
                       filter == 6 ? "Sub2" :
                       filter < PNG_PS2_TEST_PAETH_START ?
                          (filter == 7 ? "Average1" : "Average2") :
                       filter < PNG_PS2_TEST_WIDE_SUB_START ?
                          (filter == PNG_PS2_TEST_PAETH_START ? "Paeth1" :
                           filter == PNG_PS2_TEST_PAETH_START + 1 ? "Paeth2" :
                           filter == PNG_PS2_TEST_PAETH_START + 2 ? "Paeth3" : "Paeth4") :
                       filter == PNG_PS2_TEST_WIDE_SUB_START ? "Sub6" :
                       filter == PNG_PS2_TEST_WIDE_SUB_START + 1 ? "Sub8" :
                       filter < PNG_PS2_TEST_WIDE_PAETH_START ?
                          (filter == PNG_PS2_TEST_WIDE_AVG_START ?
                              "Average6" : "Average8") :
                       filter == PNG_PS2_TEST_WIDE_PAETH_START ?
                          "Paeth6" : "Paeth8",
                   (unsigned long)len, offset);
               return 1;
            }
            ++cases;
         }
      }
   }

   printf("PASS: %u PS2 EE MMI filter cases\n", cases);
#ifdef PNG_PS2_BENCH_ENABLE
   if (png_ps2_test_extra() != 0)
      return 1;
   if (png_ps2_test_full() != 0)
      return 1;
   if (png_ps2_test_color() != 0)
      return 1;
   if (png_ps2_bench_all() != 0)
      return 1;
   if (png_ps2_test_palette_hook() != 0)
      return 1;
#endif
   return 0;
#endif
}

#ifdef PNG_PS2_BENCH_ENABLE
/* The exact same fixed ten contest shapes are emitted to CSV and shown at
 * the top of BOTH results pages. No third summary page is needed. */
typedef struct
{
   const char *label;
   unsigned int filter, bpp;
} ps2_bench_target;
static const ps2_bench_target ps2_bench_targets[] = {
   {"Up", PS2_BENCH_UP, 1},
   {"Sub1", PS2_BENCH_SUB, 1},
   {"Sub2", PS2_BENCH_SUB, 2},
   {"Sub3", PS2_BENCH_SUB, 3},
   {"Sub4", PS2_BENCH_SUB, 4},
   {"Sub6", PS2_BENCH_SUB, 6},
   {"Sub8", PS2_BENCH_SUB, 8},
   {"Avg4", PS2_BENCH_AVG, 4},
   {"Paeth4", PS2_BENCH_PAETH, 4},
   {"WrSub4", PS2_BENCH_WRITE_SUB4, 4}
};
#endif

#if defined(_EE) && defined(PNG_PS2_BENCH_ENABLE)
#include "compact_layout.h"
#define PS2_SCREEN_VARIANTS \
   (sizeof ps2_bench_variants / sizeof ps2_bench_variants[0])
static unsigned int screen_first[PS2_SCREEN_VARIANTS];
static unsigned int screen_group[PS2_SCREEN_VARIANTS];
static unsigned int screen_groups;
static int screen_ready;

static void
screen_init(void)
{
   unsigned int i,g;
   screen_groups=0;
   for (i=0;i<PS2_SCREEN_VARIANTS;++i)
   {
      const ps2_bench_variant *v=&ps2_bench_variants[i];
      for (g=0;g<screen_groups;++g)
      {
         const ps2_bench_variant *other=&ps2_bench_variants[screen_first[g]];
         if (v->filter==other->filter && v->bpp==other->bpp) break;
      }
      if (g==screen_groups) screen_first[screen_groups++]=i;
      screen_group[i]=g;
   }
   /* Ordinal-only scoreboard; preserve a complete, stable ID->name map
    * in stdout without consuming a single row on the EE display. */
   printf("GROUP_MAP_HEADER,group,filter,bpp\n");
   for (g=0;g<screen_groups;++g)
   {
      const ps2_bench_variant *v=&ps2_bench_variants[screen_first[g]];
      printf("GROUP_MAP,%u,%u,%u\n",g+1U,v->filter,v->bpp);
   }
   printf("PLAN_MAP_HEADER,id,group,filter,bpp,name\n");
   for (i=0;i<PS2_SCREEN_VARIANTS;++i)
      printf("PLAN_MAP,%u,%u,%u,%u,%s\n",i+1U,screen_group[i]+1U,
          ps2_bench_variants[i].filter,ps2_bench_variants[i].bpp,
          ps2_bench_variants[i].name);
   screen_ready=1;
}

static unsigned int
screen_plan_id(const ps2_bench_variant *v, const char *name)
{
   unsigned int i;
   if (!name) return 0;
   for (i=0;i<PS2_SCREEN_VARIANTS;++i)
      if (ps2_bench_variants[i].filter==v->filter &&
          ps2_bench_variants[i].bpp==v->bpp &&
          strcmp(ps2_bench_variants[i].name,name)==0) return i+1U;
   return 0;
}

/* 1024-byte aligned row: one unique (filter,bpp) contest per cell.
 * 'GG:NN WIN 43.3x' means group GG, candidate NN in stdout MAP.
 * Other timings and alignments remain available in unabridged CSV. */
static void
screen_cell(unsigned int g)
{
   const ps2_bench_variant *v;
   const ps2_bench_winner *win;
   enum ps2_bench_verdict verdict;
   unsigned long ratio;
   unsigned int id;
   char t[32];
   if (g>=screen_groups || g>=PS2_WIN_CAPACITY) return;
   v=&ps2_bench_variants[screen_first[g]];
   win=&ps2_bench_winners[v->filter][v->bpp][1][0];
   verdict=win->name ? ps2_bench_verdict(win->reference_ticks,win->net_ticks) :
       PS2_BENCH_UNMEASURED;
   ratio=ps2_bench_winner_ratio_x1000(win->reference_ticks,win->net_ticks);
   id=screen_plan_id(v,win->name);
   if (verdict==PS2_BENCH_PLAN_WIN && id)
   {
      if (ratio>=1000000UL)
         snprintf(t,sizeof t,"%02u:%02u WIN >999x",g+1U,id);
      else
         snprintf(t,sizeof t,"%02u:%02u WIN %lu.%03lux",
             g+1U,id,ratio/1000UL,ratio%1000UL);
   }
   else if (verdict==PS2_BENCH_SCALAR_WIN)
   {
      if (ratio>=1000000UL)
         snprintf(t,sizeof t,"%02u:S WIN >999x",g+1U);
      else
         snprintf(t,sizeof t,"%02u:S WIN %lu.%03lux",
             g+1U,ratio/1000UL,ratio%1000UL);
   }
   else if (verdict==PS2_BENCH_TIE)
      snprintf(t,sizeof t,"%02u:= TIE",g+1U);
   else
      snprintf(t,sizeof t,"%02u:-- N/A",g+1U);
   scr_setfontcolor(verdict==PS2_BENCH_PLAN_WIN?0x00ff00U:
       verdict==PS2_BENCH_SCALAR_WIN?0x00ffffU:
       verdict==PS2_BENCH_TIE?0xffffffU:0x808080U);
   scr_setXY(ps2_win_x(g),ps2_win_y(g));
   /* 18 chars max and no newline: never reach rightmost cell or wrap. */
   scr_printf("%-18.18s",t);
}

static void
screen_counts(void)
{
   unsigned int g,plan=0,scalar=0,tie=0,unknown=0;
   for (g=0;g<screen_groups;++g)
   {
      const ps2_bench_variant *v=&ps2_bench_variants[screen_first[g]];
      const ps2_bench_winner *w=&ps2_bench_winners[v->filter][v->bpp][1][0];
      enum ps2_bench_verdict verdict=w->name ?
          ps2_bench_verdict(w->reference_ticks,w->net_ticks):
          PS2_BENCH_UNMEASURED;
      if (verdict==PS2_BENCH_PLAN_WIN) ++plan;
      else if (verdict==PS2_BENCH_SCALAR_WIN) ++scalar;
      else if (verdict==PS2_BENCH_TIE) ++tie;
      else ++unknown;
   }
   scr_setXY(0,PS2_WIN_STATUS_Y);
   scr_setfontcolor(0xffffffU);
   scr_printf("PLAN WIN:%u  SCALAR WIN:%u  TIE:%u  N/A:%u       ",
       plan,scalar,tie,unknown);
}

static void
screen_draw(void)
{
   unsigned int g;
   scr_clear();
   scr_setXY(0,0);
   scr_setfontcolor(0xffffffU);
   scr_printf("LIBPNG EE MMI | 1024B ALIGNED | %u GROUPS / %u PLANS",
       screen_groups,(unsigned int)PS2_SCREEN_VARIANTS);
   for (g=0;g<screen_groups && g<PS2_WIN_CAPACITY;++g)
      screen_cell(g);
   screen_counts();
}

static void
screen_finish(int failed)
{
   if (!screen_ready) screen_init();
   screen_draw();
   scr_setfontcolor(0xffffffU);
   scr_setXY(0,PS2_WIN_COUNTS_Y);
   scr_printf("GG:GROUP  NN:PLAN#  S:SCALAR  --:N/A  x:WIN RATIO");
   scr_setXY(0,PS2_WIN_DETAIL_Y);
   if (ps2_bench_dispatch_passed==0 && ps2_bench_dispatch_failed==0)
      scr_printf("DISPATCH:N/A  PALETTE:%s",
          failed?"CHECK LOG":"PASS");
   else
      scr_printf("DISPATCH:%u PASS %u FAIL  PALETTE:%s",
          ps2_bench_dispatch_passed,ps2_bench_dispatch_failed,
          failed?"CHECK LOG":"PASS");
   scr_setXY(0,PS2_WIN_DONE_Y);
   scr_setfontcolor(failed?0x0000ffU:0x00ff00U);
   scr_printf("DONE: %s | %u PLANS | %u GROUPS",
       failed?"FAIL":"PASS",(unsigned int)PS2_SCREEN_VARIANTS,
       screen_groups);
}

static void
test_live(const char *name, unsigned int index, unsigned int total,
    unsigned long width, unsigned int passed, unsigned int failed)
{
   (void)name;
   if (!screen_ready) { screen_init(); screen_draw(); }
   if (index==0U || index>PS2_SCREEN_VARIANTS) return;
   if (width==1024UL || width==0UL)
   {
      screen_cell(screen_group[index-1U]);
      screen_counts();
   }
   scr_setfontcolor(0xffffffU);
   scr_setXY(0,PS2_WIN_COUNTS_Y);
   scr_printf("RUN:%u/%u  ROW:%-5lu  PASS:%u  FAIL:%u          ",
       index,total,width,passed,failed);
}
#endif
#ifdef PNG_PS2_BENCH_ENABLE
static void
ps2_show_fastest_panel(void)
{
   unsigned int i, scalar_wins = 0, plan_wins = 0, ties = 0, unknown = 0;
   /* AUTO_HEADER is logged but not echoed a second time to the screen. */
   printf("AUTO_HEADER,1024,0,scalar-versus-fastest-plan\n");
   for (i=0;i<sizeof ps2_bench_targets/sizeof ps2_bench_targets[0];++i)
   {
      const ps2_bench_winner *win =
         &ps2_bench_winners[ps2_bench_targets[i].filter][ps2_bench_targets[i].bpp][1][0];
      enum ps2_bench_verdict verdict = win->name ?
          ps2_bench_verdict(win->reference_ticks, win->net_ticks) :
          PS2_BENCH_UNMEASURED;
      unsigned long ratio = ps2_bench_winner_ratio_x1000(
          win->reference_ticks, win->net_ticks);
      char outcome[16];
      if (verdict == PS2_BENCH_PLAN_WIN)
      {
         ++plan_wins;
         snprintf(outcome, sizeof outcome, "PLAN %c WIN",
             ps2_bench_plan_letter(win->plan_index));
      }
      else if (verdict == PS2_BENCH_SCALAR_WIN)
      {
         ++scalar_wins;
         snprintf(outcome, sizeof outcome, "SCALAR WIN");
      }
      else if (verdict == PS2_BENCH_TIE)
      {
         ++ties;
         snprintf(outcome, sizeof outcome, "TIE");
      }
      else
      {
         ++unknown;
         snprintf(outcome, sizeof outcome, "N/A");
      }
      printf("AUTO,%s,%s,%s,%lu.%03lux,plan=%c,competitors=%u\n",
          ps2_bench_targets[i].label, outcome, win->name ? win->name : "-",
          ratio/1000UL,ratio%1000UL,
          ps2_bench_plan_letter(win->plan_index), win->eligible);
   }
   printf("AUTO_TOTAL,plan=%u,scalar=%u,tie=%u,unknown=%u\n",
       plan_wins, scalar_wins, ties, unknown);
}
#endif

int
main(void)
{
   int result;
   static char output_buffer[8192];
   setvbuf(stdout, output_buffer, _IOFBF, sizeof output_buffer);
#ifdef _EE
   init_scr();
   /* No white cursor blocks between tightly packed result cells. */
   scr_setCursor(0);
#endif
   printf("libpng PS2 live correctness and benchmark lab\n");
   result = run_filters();
#if defined(_EE) && defined(PNG_PS2_BENCH_ENABLE) && defined(PNG_PS2_SPR_BENCH)
   /* Entire original regression/dispatch/palette gate must pass first. */
   if (!result) result = png_ps2_spr_benchmark();
   else printf("SPR_RESULT,SKIP,previous_correctness_failure\n");
#endif
   printf("\nTEST: %s! code=%d\n", result ? "FAIL" : "OK", result);
#ifdef PNG_PS2_BENCH_ENABLE
   if (!result) ps2_show_fastest_panel();
#endif
#ifdef _EE
   scr_setfontcolor(0xffffffU);
#endif
#ifdef PNG_PS2_BENCH_ENABLE
   printf("FASTEST winners are determined per filter, bpp, size and alignment\n");
   printf("A = generic C / B = all measured candidates (not winner totals)\n");
   printf("Matched kernel sweep; copy time subtracted\n");
   printf("O = tests passed / X = tests failed or incomplete\n\n");
   {
      unsigned int side;
      unsigned long long times[2];
      int passed = result == 0 && ps2_bench_ab.completed && ps2_bench_ab.cases;
      times[0] = ps2_bench_ab_ms1000(ps2_bench_ab.a_ticks,
          PS2_BENCH_FREQUENCY);
      times[1] = ps2_bench_ab_ms1000(ps2_bench_ab.b_ticks,
          PS2_BENCH_FREQUENCY);
      for (side = 0; side < 2; ++side)
      {
#ifdef _EE
         scr_setfontcolor(passed ? 0x00ff00U : 0x0000ffU);
#endif
         printf("%c: %c (%llu.%03llu ms)\n", side ? 'B' : 'A',
             passed ? 'O' : 'X', times[side] / 1000ULL,
             times[side] % 1000ULL);
      }
#ifdef _EE
      scr_setfontcolor(0xffffffU);
#endif
      if (!passed) printf("Comparison failed or incomplete\n");
      printf("%u matched batches; %u measurement(s) per batch\n",
          ps2_bench_ab.cases, (unsigned int)PNG_PS2_BENCH_REPEATS);
      printf("Timer: %s\n", PS2_BENCH_UNIT);
   }
#endif
#ifdef _EE
   scr_setfontcolor(0x00ff00U);
#endif
   printf("\nEND!\n");
#ifdef _EE
   scr_setfontcolor(0xffffffU);
#endif
#ifdef _EE
#ifdef PNG_PS2_BENCH_ENABLE
   screen_finish(result);
#endif
   /* Display persists. No page rotation and no artificial halt text. */
   SleepThread();
#endif
   return result;
}
