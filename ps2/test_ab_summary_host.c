/* Timer conversion and matched-batch aggregation regression. */
#include <assert.h>
#include <stdio.h>
#include "bench_ab_summary.h"
int main(void)
{
   ps2_bench_ab_summary summary = {0, 0, 0, 0};
   ps2_bench_ab_add(&summary, 10, 30, 20);
   ps2_bench_ab_add(&summary, 10, 5, 15);
   assert(summary.cases == 2);
   assert(summary.a_ticks == 20 && summary.b_ticks == 15);
   assert(!summary.completed);
   assert(ps2_bench_ab_ms1000(576000, 576000) == 1000000ULL);
   assert(ps2_bench_ab_ms1000(294912, 294912000) == 1000ULL);
   assert(ps2_bench_ab_ms1000(1234567, 1000000) == 1234567ULL);
   assert(ps2_bench_ab_ms1000(1, 1000) == 1000ULL);
   assert(ps2_bench_ab_ms1000(5000000000ULL, 1000) == 5000000000000ULL);
   assert(ps2_bench_verdict(200, 100) == PS2_BENCH_PLAN_WIN);
   assert(ps2_bench_verdict(100, 200) == PS2_BENCH_SCALAR_WIN);
   assert(ps2_bench_verdict(200, 200) == PS2_BENCH_TIE);
   assert(ps2_bench_verdict(0, 100) == PS2_BENCH_UNMEASURED);
   assert(ps2_bench_verdict(100, 0) == PS2_BENCH_UNMEASURED);
   assert(ps2_bench_winner_ratio_x1000(200, 100) == 2000UL);
   assert(ps2_bench_winner_ratio_x1000(100, 200) == 2000UL);
   assert(ps2_bench_winner_ratio_x1000(200, 200) == 1000UL);
   assert(ps2_bench_winner_ratio_x1000(0, 0) == 0UL);
   assert(ps2_bench_plan_letter(0) == 'A');
   assert(ps2_bench_plan_letter(1) == 'B');
   assert(ps2_bench_plan_letter(25) == 'Z');
   assert(ps2_bench_plan_letter(26) == '?');
   puts("PASS A/B batch totals and milliseconds for all timer sources");
   return 0;
}
