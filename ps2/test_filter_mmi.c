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
static void test_log(const char *format, ...)
{
   char message[512];
   va_list args;
   va_start(args, format);
   vsnprintf(message, sizeof message, format, args);
   va_end(args);
   /* CSV stays on stdout; drawing thousands of rows dominates EE runtime. */
   /* Screen uses fixed progress and final winner panel. Keep the CSV
    * streaming to stdout without flooding the PS2 debug renderer. */
   if (strncmp(message, "BENCH", 5) != 0 &&
       strncmp(message, "FUSED", 5) != 0 &&
       strncmp(message, "FASTEST", 7) != 0 &&
       strncmp(message, "AUTO,", 5) != 0 &&
       strncmp(message, "AUTO_", 5) != 0)
      scr_printf("%s", message);
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
} png_row_info;

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
#endif

static int
run_filters(void)
{
#if defined(PNG_PS2_BENCH_ENABLE) && !defined(PNG_PS2_TEST_EXHAUSTIVE)
   return png_ps2_bench_all();
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
#endif
   return 0;
#endif
}

#if defined(_EE) && defined(PNG_PS2_BENCH_ENABLE)
static void
test_live(const char *name, unsigned int index, unsigned int total,
    unsigned long width, unsigned int passed, unsigned int failed)
{
   char line[96];
   unsigned int side;
   unsigned long long ticks[2];
   ticks[0] = ps2_bench_ab.a_ticks;
   ticks[1] = ps2_bench_ab.b_ticks;
   scr_setfontcolor(0xffffffU);
   scr_setXY(0, 0);
   scr_printf("libpng LIVE test + benchmark\n");
   snprintf(line, sizeof line, "Variant %u/%u: %s", index, total, name);
   scr_printf("%-68s\n", line);
   snprintf(line, sizeof line, "Row %lu / PASS %u / FAIL %u", width, passed, failed);
   scr_printf("%-68s\n", line);
   scr_printf("A = generic C / B = MMI candidates\n");
   scr_printf("Running totals, copy subtracted (provisional)\n");
   scr_printf("O = tests passed so far / X = test failure\n");
   for (side = 0; side < 2; ++side)
   {
      unsigned long long ms = ps2_bench_ab_ms1000(ticks[side], PS2_BENCH_FREQUENCY);
      char status = failed ? 'X' : passed ? 'O' : '-';
      scr_setfontcolor(failed ? 0x0000ffU : passed ? 0x00ff00U : 0xffffffU);
      snprintf(line, sizeof line, "%c: %c (%llu.%03llu ms)",
          side ? 'B' : 'A', status, ms / 1000ULL, ms % 1000ULL);
      scr_printf("%-68s\n", line);
   }
   scr_setfontcolor(0xffffffU);
}
#endif

/* Human-readable single-screen answer, in addition to machine-readable
 * FASTEST and AUTO_WIN CSV. Scalar is an eligible competitor, so a single
 * valid MMI plan is sufficient to determine a shape-matched winner.
 * Missing timer precision is N/A, never a declared speedup.
 * Representative row: 1024 bytes, aligned current/previous pointers. */
#ifdef PNG_PS2_BENCH_ENABLE
static void
ps2_show_fastest_panel(void)
{
   static const struct
   {
      const char *label;
      unsigned int filter, bpp;
   } targets[] = {
      {"Up", PS2_BENCH_UP, 1},
      {"Sub1", PS2_BENCH_SUB, 1},
      {"Sub2", PS2_BENCH_SUB, 2},
      {"Sub3", PS2_BENCH_SUB, 3},
      {"Sub4", PS2_BENCH_SUB, 4},
      {"Sub6", PS2_BENCH_SUB, 6},
      {"Sub8", PS2_BENCH_SUB, 8},
      {"Average4", PS2_BENCH_AVG, 4},
      {"Paeth4", PS2_BENCH_PAETH, 4},
      {"Write Sub4", PS2_BENCH_WRITE_SUB4, 4}
   };
   unsigned int i, scalar_wins = 0, plan_wins = 0, ties = 0, unknown = 0;
#ifdef _EE
   scr_setfontcolor(0xffffffU);
   scr_printf("AUTO WINNERS: 1024 B / align 0 (vs scalar)\n");
   scr_printf("Best PLAN name shown even when SCALAR wins\n");
#endif
   /* AUTO_HEADER is logged but not echoed a second time to the screen. */
   printf("AUTO_HEADER,1024,0,scalar-versus-fastest-plan\n");
   for (i=0;i<sizeof targets/sizeof targets[0];++i)
   {
      const ps2_bench_winner *win =
         &ps2_bench_winners[targets[i].filter][targets[i].bpp][1][0];
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
#ifdef _EE
      scr_setfontcolor(verdict == PS2_BENCH_PLAN_WIN ? 0x00ff00U :
          verdict == PS2_BENCH_SCALAR_WIN ? 0x00ffffU : 0xffffffU);
      if (verdict == PS2_BENCH_UNMEASURED)
         scr_printf("%-10.10s %-11.11s %-24.24s   --\n",
             targets[i].label, outcome, win->name ? win->name : "-");
      else
         scr_printf("%-10.10s %-11.11s %-24.24s %lu.%03lux\n",
             targets[i].label, outcome, win->name, ratio/1000UL,
             ratio%1000UL);
#endif
      printf("AUTO,%s,%s,%s,%lu.%03lux,plan=%c,competitors=%u\n",
          targets[i].label, outcome, win->name ? win->name : "-",
          ratio/1000UL,ratio%1000UL,
          ps2_bench_plan_letter(win->plan_index), win->eligible);
   }
#ifdef _EE
   scr_setfontcolor(0xffffffU);
   scr_printf("WIN COUNT: PLAN %u | SCALAR %u | TIE %u | N/A %u\n",
       plan_wins, scalar_wins, ties, unknown);
#endif
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
#endif
   printf("libpng PS2 live correctness and benchmark lab\n");
   result = run_filters();
#ifdef _EE
   scr_clear();
   scr_setXY(0, 0);
   scr_setfontcolor(result ? 0x0000ffU : 0x00ff00U);
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
   SleepThread();
#endif
   return result;
}
