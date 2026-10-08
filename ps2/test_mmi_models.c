/* Portable arithmetic-model tests for the PS2 EE read-filter algorithms.
 * These tests do not execute R5900 machine instructions.  Run the EE
 * filter harness on actual hardware to validate assembler and timing.
 * Released under the libpng license.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_MAX 1024U
static uint32_t state = UINT32_C(0x735a2dc1);

static uint32_t
random_u32(void)
{
   state ^= state << 13;
   state ^= state >> 17;
   state ^= state << 5;
   return state;
}

/* Exact four-lane floor((a+b)/2), without cross-byte carries. */
static uint32_t
average4_packed(uint32_t a, uint32_t b)
{
   return (a & b) + (((a ^ b) & UINT32_C(0xfefefefe)) >> 1);
}

/* Software model of QFSRV(12) + PADDB, PCPYLD + PADDB, then
 * PEXTLW/PCPYLD broadcast of the previous decoded four-byte pixel.
 */
static void
sub4_prefix_model(unsigned char *row, size_t n)
{
   size_t i, j;

   for (i = 4; i < 16 && i < n; ++i)
      row[i] = (unsigned char)(row[i] + row[i - 4]);

   for (; i + 16 <= n; i += 16)
   {
      unsigned char raw[16], sum[16], shift[16];
      memcpy(raw, row + i, 16);
      memset(shift, 0, sizeof shift);
      for (j = 4; j < 16; ++j)
         shift[j] = raw[j - 4];
      for (j = 0; j < 16; ++j)
         sum[j] = (unsigned char)(raw[j] + shift[j]);

      memset(shift, 0, sizeof shift);
      for (j = 8; j < 16; ++j)
         shift[j] = sum[j - 8];
      for (j = 0; j < 16; ++j)
         sum[j] = (unsigned char)(sum[j] + shift[j]);

      for (j = 0; j < 16; ++j)
         row[i + j] = (unsigned char)(sum[j] + row[i - 4 + (j & 3U)]);
   }

   for (; i < n; ++i)
      row[i] = (unsigned char)(row[i] + row[i - 4]);
}

static void
sub4_reference(unsigned char *row, size_t n)
{
   size_t i;
   for (i = 4; i < n; ++i)
      row[i] = (unsigned char)(row[i] + row[i - 4]);
}

int
main(void)
{
   unsigned int a, b, repetition;
   size_t n, i;
   unsigned char original[TEST_MAX + 1], actual[TEST_MAX + 1];
   unsigned long tests = 0;

   for (a = 0; a < 256; ++a)
      for (b = 0; b < 256; ++b)
      {
         uint32_t packed_a = a * UINT32_C(0x01010101);
         uint32_t packed_b = b * UINT32_C(0x01010101);
         uint32_t got = average4_packed(packed_a, packed_b);
         for (i = 0; i < 4; ++i)
            if (((got >> (i * 8)) & 255U) != (a + b) / 2)
               return puts("FAILED: exhaustive Average4"), 1;
         ++tests;
      }

   for (repetition = 0; repetition < 100000; ++repetition)
   {
      uint32_t x = random_u32(), y = random_u32();
      uint32_t got = average4_packed(x, y);
      for (i = 0; i < 4; ++i)
      {
         unsigned int xa = (x >> (i * 8)) & 255U;
         unsigned int ya = (y >> (i * 8)) & 255U;
         if (((got >> (i * 8)) & 255U) != (xa + ya) / 2)
            return puts("FAILED: random Average4"), 1;
      }
      ++tests;
   }

   for (repetition = 0; repetition < 16; ++repetition)
      for (n = 0; n <= TEST_MAX; n += 4)
      {
         for (i = 0; i <= n; ++i)
            original[i] = (unsigned char)random_u32();
         memcpy(actual, original, n + 1);
         sub4_reference(original, n);
         sub4_prefix_model(actual, n);
         if (memcmp(original, actual, n + 1) != 0)
         {
            printf("FAILED: Sub4 length=%lu run=%u\n", (unsigned long)n,
                repetition);
            return 1;
         }
         ++tests;
      }

   printf("PASS: %lu portable MMI arithmetic model cases\n", tests);
   return 0;
}
