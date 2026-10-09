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
   puts("PASS A/B batch totals and milliseconds for all timer sources");
   return 0;
}
