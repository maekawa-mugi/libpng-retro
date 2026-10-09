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
#endif
