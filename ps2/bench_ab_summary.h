/* Matched scalar/candidate batches, excluding replay-copy time.
 * SPDX-License-Identifier: libpng-2.0 */
#ifndef PNG_PS2_BENCH_AB_SUMMARY_H
#define PNG_PS2_BENCH_AB_SUMMARY_H
typedef struct
{
   unsigned long long a_ticks, b_ticks;
   unsigned int cases;
   int completed;
} ps2_bench_ab_summary;
static void
ps2_bench_ab_add(ps2_bench_ab_summary *s, unsigned long copy,
    unsigned long scalar, unsigned long candidate)
{
   s->a_ticks += scalar > copy ? scalar - copy : 0;
   s->b_ticks += candidate > copy ? candidate - copy : 0;
   ++s->cases;
}
/* Thousandths of a millisecond, using the selected timer's frequency. */
static unsigned long long
ps2_bench_ab_ms1000(unsigned long long ticks, unsigned long frequency)
{
   return ticks / frequency * 1000000ULL +
       (ticks % frequency) * 1000000ULL / frequency;
}
/* A contest compares the *fastest passing plan* and scalar on an identical
 * row shape. The plan's ordinal is stable within its filter/bpp family.
 * No winner is claimed if subtraction of replay-copy time yielded zero.
 */
enum ps2_bench_verdict
{
   PS2_BENCH_UNMEASURED = 0,
   PS2_BENCH_SCALAR_WIN = 1,
   PS2_BENCH_PLAN_WIN = 2,
   PS2_BENCH_TIE = 3
};
static enum ps2_bench_verdict
ps2_bench_verdict(unsigned long scalar, unsigned long plan)
{
   if (scalar == 0UL || plan == 0UL) return PS2_BENCH_UNMEASURED;
   if (scalar < plan) return PS2_BENCH_SCALAR_WIN;
   if (plan < scalar) return PS2_BENCH_PLAN_WIN;
   return PS2_BENCH_TIE;
}
/* Winner's time relative to the slower implementation; always >= 1x.
 * Caller must have checked for nonzero valid timings. */
static unsigned long
ps2_bench_winner_ratio_x1000(unsigned long scalar, unsigned long plan)
{
   unsigned long faster = scalar < plan ? scalar : plan;
   unsigned long slower = scalar > plan ? scalar : plan;
   if (faster == 0UL) return 0UL;
   return (unsigned long)((unsigned long long)slower * 1000ULL / faster);
}
static char
ps2_bench_plan_letter(unsigned int ordinal)
{
   return ordinal < 26U ? (char)('A' + ordinal) : '?';
}
#endif
