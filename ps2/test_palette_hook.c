/* Independent contract regression for the real libpng palette hook.
 * Compiled unchanged on native hosts and in the single-boot EE lab.
 * Not a second exhaustive kernel benchmark: this tests row_info updates,
 * tRNS default alpha, in-place expansion, guard bytes, and fallback.
 */
#include <string.h>
#include <stdio.h>
static png_byte ps2_pal_test_row[1024U*4U + 48U];
static png_byte ps2_pal_test_expected[1024U*4U + 48U];
static png_byte ps2_pal_test_source[1024U+48U];
static png_color ps2_pal_test_table[256];
static png_byte ps2_pal_test_alpha[256];
static unsigned int ps2_pal_test_rng = 0x28916283U;

static png_byte
ps2_pal_test_rand(void)
{
   ps2_pal_test_rng ^= ps2_pal_test_rng << 13;
   ps2_pal_test_rng ^= ps2_pal_test_rng >> 17;
   ps2_pal_test_rng ^= ps2_pal_test_rng << 5;
   return (png_byte)ps2_pal_test_rng;
}

static int
png_ps2_test_palette_hook(void)
{
   static const size_t widths[] = {0,1,2,3,4,7,8,15,16,31,64,255,256,257,511,1024};
   static const int alpha_counts[] = {0,1,16,256};
   unsigned int ai, off, wi, i, cases = 0;
   for (i=0;i<256;++i)
   {
      ps2_pal_test_table[i].red = ps2_pal_test_rand();
      ps2_pal_test_table[i].green = ps2_pal_test_rand();
      ps2_pal_test_table[i].blue = ps2_pal_test_rand();
      ps2_pal_test_alpha[i] = ps2_pal_test_rand();
   }
   for (wi=0;wi<sizeof(widths)/sizeof(widths[0]);++wi)
      for (ai=0;ai<sizeof(alpha_counts)/sizeof(alpha_counts[0]);++ai)
         for (off=0;off<16;++off)
         {
            const size_t n = widths[wi];
            const size_t channels = alpha_counts[ai] ? 4 : 3;
            const size_t outbytes = n * channels;
            png_byte *row = ps2_pal_test_row + off;
            png_byte *e = ps2_pal_test_expected + off;
            png_row_info ri;
            size_t x;
            memset(ps2_pal_test_row, 0xa5, sizeof ps2_pal_test_row);
            memset(ps2_pal_test_expected, 0xa5, sizeof ps2_pal_test_expected);
            for (x=0;x<n;++x)
            {
               ps2_pal_test_source[x] = ps2_pal_test_rand();
               row[x] = ps2_pal_test_source[x];
               e[channels*x] = ps2_pal_test_table[ps2_pal_test_source[x]].red;
               e[channels*x+1] = ps2_pal_test_table[ps2_pal_test_source[x]].green;
               e[channels*x+2] = ps2_pal_test_table[ps2_pal_test_source[x]].blue;
               if (channels == 4)
                  e[4*x+3] = ps2_pal_test_source[x] < alpha_counts[ai] ?
                      ps2_pal_test_alpha[ps2_pal_test_source[x]] : 255;
            }
            ri.width = (png_uint_32)n;
            ri.rowbytes = n;
            ri.channels = 1;
            ri.bit_depth = 8;
            ri.pixel_depth = 8;
            ri.color_type = PNG_COLOR_TYPE_PALETTE;
            if (n != 0 && !png_ps2_expand_palette_row_8(&ri,row,
                    ps2_pal_test_table,ps2_pal_test_alpha,alpha_counts[ai]))
               goto fail;
            if (n == 0)
            {
               if (png_ps2_expand_palette_row_8(&ri,row,
                       ps2_pal_test_table,ps2_pal_test_alpha,alpha_counts[ai]))
                  goto fail;
            }
            else if (memcmp(row,e,outbytes+16) ||
                     ri.rowbytes != outbytes ||
                     ri.channels != channels ||
                     ri.bit_depth != 8 ||
                     ri.pixel_depth != channels*8 ||
                     ri.color_type != (channels==4 ?
                         PNG_COLOR_TYPE_RGB_ALPHA : PNG_COLOR_TYPE_RGB))
               goto fail;

            /* Unsupported 1/2/4-bit palette rows must not be touched:
             * the generic libpng code still handles them. */
            for (i=1;i<=4;i*=2)
            {
               ri.width = (png_uint_32)n;
               ri.rowbytes = (n*i+7)/8;
               ri.channels = 1;
               ri.bit_depth = (png_byte)i;
               ri.pixel_depth = (png_byte)i;
               ri.color_type = PNG_COLOR_TYPE_PALETTE;
               memcpy(row,ps2_pal_test_source,n);
               memcpy(e,row,n);
               if (png_ps2_expand_palette_row_8(&ri,row,
                    ps2_pal_test_table,ps2_pal_test_alpha,alpha_counts[ai]) ||
                   memcmp(row,e,n) || ri.bit_depth != i ||
                   ri.color_type != PNG_COLOR_TYPE_PALETTE)
                  goto fail;
            }
            ++cases;
            continue;
         fail:
            printf("PALETTE_HOOK_FAIL,width=%lu,alpha=%d,align=%u\n",
                (unsigned long)n,alpha_counts[ai],off);
            return 1;
         }
   printf("PALETTE_HOOK_PASS,cases=%u\n",cases);
   return 0;
}
