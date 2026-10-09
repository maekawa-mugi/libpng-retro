/* PS2 EE MMI scratchpad placement lab.
 *
 * Included ONLY by the standalone PS2SDK test-filter runner after the
 * ordinary read/write, PNG palette-hook, and winner-dispatch gates.
 * Never included in production pngsimd.c, pngread.c or pngwrite.c.
 *
 * EE SPR: 0x70000000 .. 0x70003fff (16 KiB), exclusive ownership;
 * no DMA and no other SPR client while this ELF is running.
 *
 * Four 4-KiB banks: destination, previous-row, replay source, palette.
 * Each active working region leaves >=16 bytes of sentinel at its end.
 * Source rows, previous rows and lookup tables are validated separately.
 */
#ifndef PNG_PS2_SPR_BENCH_C
#define PNG_PS2_SPR_BENCH_C
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define PNG_SPR_SLOT 4096U
#define PNG_SPR_SAMPLES 6U
#define PNG_SPR_REPS 16U
#define PNG_SPR_STAGING 5U
#define PNG_SPR_PALETTE_STAGING 6U
#define PNG_SPR_GUARD 16U

#define PNG_SPR_PREV 1U
#define PNG_SPR_LUT  2U

typedef struct {
   const char *name;
   unsigned int kind, bpp, aux;
   ps2_bench_fn fn;
} png_spr_case;

static const png_spr_case png_spr_cases[] = {
   {"up-mmi", PS2_BENCH_UP, 1U, PNG_SPR_PREV,
       png_read_filter_row_up_ps2},
#ifdef PNG_PS2_EE_MMI_UP_SCHEDULES
   {"up-4x", PS2_BENCH_UP, 1U, PNG_SPR_PREV,
       png_read_filter_row_up_4x_ps2},
#endif
   {"sub4-mmi", PS2_BENCH_SUB, 4U, 0U,
       png_read_filter_row_sub4_ps2},
   {"avg4-mmi", PS2_BENCH_AVG, 4U, PNG_SPR_PREV,
       png_read_filter_row_avg4_ps2},
#ifdef PNG_PS2_EE_MMI_AVG4_DUAL
   {"avg4-dual", PS2_BENCH_AVG, 4U, PNG_SPR_PREV,
       ps2_bench_avg4_dual},
#endif
#ifdef PNG_PS2_EE_MMI_PAETH
   {"paeth4-mmi", PS2_BENCH_PAETH, 4U, PNG_SPR_PREV,
       png_read_filter_row_paeth4_ps2},
#endif
   {"write-up", PS2_BENCH_WRITE_UP, 1U, PNG_SPR_PREV,
       png_ps2_write_up_mmi},
   {"write-avg4", PS2_BENCH_WRITE_AVG4, 4U, PNG_SPR_PREV,
       png_ps2_write_avg4_mmi},
   {"write-paeth4", PS2_BENCH_WRITE_PAETH, 4U, PNG_SPR_PREV,
       png_ps2_write_paeth4_mmi},
   {"palette-rgba", PS2_BENCH_PALETTE, 1U, PNG_SPR_LUT,
       ps2_bench_palette_expand},
   {"palette-rgb", PS2_BENCH_PAL_RGB, 1U, PNG_SPR_LUT,
       ps2_bench_palette_rgb},
   {"palette-trns", PS2_BENCH_PAL_TRNS, 1U, PNG_SPR_LUT,
       ps2_bench_palette_trns},
   {"adam7-bpp2", PS2_BENCH_ADAM_BYTES, 2U, PNG_SPR_PREV,
       ps2_bench_adam_bytes},
   {"adam7-bpp4", PS2_BENCH_ADAM_BYTES, 4U, PNG_SPR_PREV,
       ps2_bench_adam_bytes},
   {"unpack-index4", PS2_BENCH_UNPACK_IDX, 4U, 0U,
       ps2_bench_unpack_index},
   {"gray-rgb", PS2_BENCH_GRAY_RGB, 1U, 0U,
       ps2_bench_gray_rgb},
   {"gray-rgba", PS2_BENCH_GRAY_RGBA, 1U, 0U,
       ps2_bench_gray_rgba},
   {"swap16", PS2_BENCH_SWAP16, 2U, 0U,
       ps2_bench_swap16}
};

static const char *const png_spr_modes[PNG_SPR_PALETTE_STAGING] = {
   "ram", "spr_row", "spr_aux", "spr_both", "spr_xfer",
   "spr_row_xfer_lut_hot"
};

static png_byte png_spr_input[PNG_SPR_SLOT+PNG_SPR_GUARD]
   __attribute__((aligned(16)));
static png_byte png_spr_output[PNG_SPR_SLOT+PNG_SPR_GUARD]
   __attribute__((aligned(16)));
static png_byte png_spr_previous[PNG_SPR_SLOT+PNG_SPR_GUARD]
   __attribute__((aligned(16)));
static png_byte png_spr_expected[PNG_SPR_SLOT+PNG_SPR_GUARD]
   __attribute__((aligned(16)));
static png_byte png_spr_prev_saved[PNG_SPR_SLOT+PNG_SPR_GUARD]
   __attribute__((aligned(16)));
static volatile unsigned int png_spr_sink;
static unsigned int png_spr_completed, png_spr_cases_done, png_spr_timer_na;
static unsigned int png_spr_ratio_x100[3];

static unsigned long
png_spr_median6(const unsigned long values[PNG_SPR_SAMPLES])
{
   unsigned long a[PNG_SPR_SAMPLES], x;
   unsigned int i, j;
   for (i=0;i<PNG_SPR_SAMPLES;++i) a[i]=values[i];
   for (i=1;i<PNG_SPR_SAMPLES;++i)
   {
      x=a[i];j=i;
      while(j && a[j-1]>x) { a[j]=a[j-1];--j; }
      a[j]=x;
   }
   return a[2]+(a[3]-a[2])/2UL;
}

static void
png_spr_seed(unsigned int c, unsigned int width,
    size_t inbytes, size_t prevbytes)
{
   size_t i;
   for(i=0;i<inbytes;++i)
      png_spr_input[i]=(png_byte)((i*57U+c*41U+width*3U+(i>>3))&255U);
   for(i=0;i<prevbytes;++i)
      png_spr_previous[i]=(png_byte)((i*23U+c*7U+(i>>2)+19U)&255U);
   /* Exercise transparent and nontransparent grayscale on every length. */
   if (png_spr_cases[c].kind==PS2_BENCH_GRAY_RGBA)
      for (i=0;i<inbytes;i+=7U) png_spr_input[i]=127U;
}

static void
png_spr_load_lut(void)
{
   memcpy((void *)(uintptr_t)0x70003000U,
       ps2_bench_palette_table,1024U);
}

static void
png_spr_palette_rgb_table(void)
{
   png_byte *const spr=(png_byte *)(uintptr_t)0x70003000U;
   memcpy(spr,ps2_bench_rgb_table,768U);
   memcpy(spr+768U,ps2_bench_alpha_table,256U);
}

static void
png_spr_invoke(const png_spr_case *t, png_row_info *ri,
    png_byte *row, const png_byte *prev, int spr_lookup)
{
   const png_byte *table;
   if(t->kind==PS2_BENCH_PALETTE)
   {
      table=spr_lookup?(const png_byte *)(uintptr_t)0x70003000U:
          ps2_bench_palette_table;
      png_ps2_expand_palette_rgba4(row,row,ri->rowbytes,table);
   }
   else if(t->kind==PS2_BENCH_PAL_RGB)
   {
      table=spr_lookup?(const png_byte *)(uintptr_t)0x70003000U:
          ps2_bench_rgb_table;
      png_ps2_expand_palette_rgb3(row,row,ri->rowbytes,table);
   }
   else if(t->kind==PS2_BENCH_PAL_TRNS)
   {
      table=spr_lookup?(const png_byte *)(uintptr_t)0x70003000U:
          ps2_bench_rgb_table;
      png_ps2_expand_palette_rgba_trns(row,row,ri->rowbytes,
          table,spr_lookup?table+768U:ps2_bench_alpha_table);
   }
   else
      t->fn(ri,row,prev);
}

/* Reset cost is included for ALL placements; xfer modes additionally
 * transfer the actual source/previous/LUT and copy the output to RAM
 * inside the timer. The raw ratio is a replay-batch ratio, not an isolated
 * MMI instruction throughput figure. */
static unsigned long
png_spr_run(const png_spr_case *t, size_t n, size_t inbytes,
    size_t outbytes, size_t prevbytes, unsigned int mode,
    unsigned int repetitions, int *valid)
{
   png_byte *const sr=(png_byte *)(uintptr_t)0x70000000U;
   png_byte *const sp=(png_byte *)(uintptr_t)0x70001000U;
   png_byte *const ss=(png_byte *)(uintptr_t)0x70002000U;
   png_byte *const sl=(png_byte *)(uintptr_t)0x70003000U;
   const int move_row=mode==1U || mode==3U || mode==4U || mode==5U;
   const int move_aux=mode==2U || mode==3U || mode==4U || mode==5U;
   const int inclusive=mode>=4U;
   const int spr_lookup=move_aux && t->aux==PNG_SPR_LUT;
   png_byte *const dst=move_row?sr:png_spr_output;
   const png_byte *const source=move_row && !inclusive?ss:png_spr_input;
   const png_byte *const previous=move_aux && t->aux==PNG_SPR_PREV?
       sp:png_spr_previous;
   png_row_info ri;
   unsigned long before, after, elapsed;
   unsigned int rep;
   ri.rowbytes=n;
   ri.width=(png_uint_32)n;
   ri.color_type=0U; ri.bit_depth=8U; ri.channels=1U;
   ri.pixel_depth=(png_byte)(t->bpp*8U);
   ps2_bench_bpp=t->bpp;
   ps2_bench_filter=t->kind;
   *valid=1;

   if (move_row && !inclusive) memcpy(ss,png_spr_input,inbytes);
   if (move_aux && t->aux==PNG_SPR_PREV && !inclusive)
      memcpy(sp,png_spr_previous,prevbytes);
   if(spr_lookup)
   {
      if(t->kind==PS2_BENCH_PALETTE) png_spr_load_lut();
      else png_spr_palette_rgb_table();
   }
   before=ps2_bench_now();
   for(rep=0;rep<repetitions;++rep)
   {
      /* All paths replay the original row before the destructive filter. */
      if (inclusive)
      {
         memcpy(sr,png_spr_input,inbytes);
         if(t->aux==PNG_SPR_PREV)
            memcpy(sp,png_spr_previous,prevbytes);
         if(spr_lookup && mode==4U)
         {
            if(t->kind==PS2_BENCH_PALETTE)
               memcpy(sl,ps2_bench_palette_table,1024U);
            else
            {
               memcpy(sl,ps2_bench_rgb_table,768U);
               memcpy(sl+768U,ps2_bench_alpha_table,256U);
            }
         }
      }
      else memcpy(dst,source,inbytes);
      if(outbytes>inbytes)
         memset(dst+inbytes,0xa5,outbytes-inbytes+PNG_SPR_GUARD);
      else memset(dst+outbytes,0xa5,PNG_SPR_GUARD);
      png_spr_invoke(t,&ri,dst,previous,spr_lookup);
      if(inclusive)
         memcpy(png_spr_output,sr,outbytes+PNG_SPR_GUARD);
   }
   after=ps2_bench_now();
   elapsed=ps2_bench_delta(before,after);
   if (move_row && !inclusive)
      memcpy(png_spr_output,sr,outbytes+PNG_SPR_GUARD);
   if(memcmp(png_spr_output,png_spr_expected,
       outbytes+PNG_SPR_GUARD)!=0)
      *valid=0;
   if(memcmp(png_spr_previous,png_spr_prev_saved,
       prevbytes)!=0)
      *valid=0;
   if(move_aux && t->aux==PNG_SPR_PREV &&
      memcmp(sp,png_spr_prev_saved,prevbytes)!=0)
      *valid=0;
   png_spr_sink^=png_spr_output[0];
   return elapsed;
}

static int
png_ps2_spr_benchmark(void)
{
   static const unsigned int widths[]={64U,256U,512U,1008U,1024U};
   unsigned long timings[PNG_SPR_PALETTE_STAGING][PNG_SPR_SAMPLES];
   unsigned long med[PNG_SPR_PALETTE_STAGING];
   unsigned int c,w,m,s,step,methods,cases=0U,unmeasured=0U;
   int okay;
   ps2_bench_clock_init();
   printf("SPR_META,EE_MMI,16KiB,mode=RAM_ROW_AUX_BOTH_XFER,"
       "samples=6,reps=16,timer=%s\n",PS2_BENCH_UNIT);
   printf("SPR_HEADER,kernel,width,mode,ticks,ram_over_spr,"
       "status,output_checked\n");
   for(c=0;c<sizeof png_spr_cases/sizeof png_spr_cases[0];++c)
   {
      const png_spr_case *t=&png_spr_cases[c];
      for(w=0;w<sizeof widths/sizeof widths[0];++w)
      {
         size_t inbytes,outbytes,prevbytes,n=widths[w];
         png_row_info ri;
         unsigned int successful=1U;
         if(t->kind==PS2_BENCH_ADAM_BYTES && n==1008U) continue;
         ps2_bench_shape(t->kind,t->bpp,n,
             &inbytes,&outbytes,&prevbytes);
         if(inbytes+PNG_SPR_GUARD>PNG_SPR_SLOT ||
            outbytes+PNG_SPR_GUARD>PNG_SPR_SLOT ||
            prevbytes+PNG_SPR_GUARD>PNG_SPR_SLOT)
            continue;
         png_spr_seed(c,widths[w],inbytes,prevbytes);
         memset(png_spr_output,0xa5,sizeof png_spr_output);
         memset(png_spr_expected,0xa5,sizeof png_spr_expected);
         memcpy(png_spr_prev_saved,png_spr_previous,prevbytes);
         memcpy(png_spr_expected,png_spr_input,inbytes);
         ri.rowbytes=n;
         ri.width=(png_uint_32)n;
         ri.bit_depth=8U;
         ri.channels=1U;
         ri.pixel_depth=(png_byte)(t->bpp*8U);
         ri.color_type=0U;
         ps2_bench_filter=t->kind;
         ps2_bench_bpp=t->bpp;
         ps2_bench_scalar(&ri,png_spr_expected,png_spr_previous);
         /* Independently validate the actual RAM MMI kernel against C. */
         memcpy(png_spr_output,png_spr_input,inbytes);
         png_spr_invoke(t,&ri,png_spr_output,png_spr_previous,0);
         if(memcmp(png_spr_output,png_spr_expected,
             outbytes+PNG_SPR_GUARD)!=0)
         {
            printf("SPR_FAIL,%s,%lu,ram,reference_mismatch\n",
                t->name,(unsigned long)n);
            ps2_bench_clock_done();
            return 1;
         }
         methods=t->aux==PNG_SPR_LUT?
             PNG_SPR_PALETTE_STAGING:PNG_SPR_STAGING;
         /* No auxiliary data for Sub, unpack, gray or swap: only RAM,
          * SPR row and transfer-inclusive modes are meaningful. */
         for(m=0;m<methods;++m)
         {
            if(!t->aux && (m==2U || m==3U)) continue;
            (void)png_spr_run(t,n,inbytes,outbytes,prevbytes,m,1U,&okay);
            if(!okay)
            {
               printf("SPR_FAIL,%s,%lu,%s,correctness\n",
                   t->name,(unsigned long)n,png_spr_modes[m]);
               ps2_bench_clock_done();
               return 1;
            }
         }
         memset(timings,0,sizeof timings);
         for(s=0;s<PNG_SPR_SAMPLES;++s)
            for(step=0;step<methods;++step)
            {
               m=(s+step)%methods;
               if(!t->aux && (m==2U || m==3U))continue;
               timings[m][s]=png_spr_run(t,n,inbytes,outbytes,
                   prevbytes,m,PNG_SPR_REPS,&okay);
               if(!okay)
               {
                  printf("SPR_FAIL,%s,%lu,%s,sample=%u\n",
                      t->name,(unsigned long)n,png_spr_modes[m],s);
                  ps2_bench_clock_done();
                  return 1;
               }
            }
         for(m=0;m<methods;++m)
         {
            
            if(!t->aux && (m==2U || m==3U))continue;
            med[m]=png_spr_median6(timings[m]);
            if(m==0 && med[m]==0U)successful=0U;
            if(m>0 && med[m]==0U) successful=0U;
         }
         if(!successful)++unmeasured;
         for(m=0;m<methods;++m)
         {
            double speed=0.;
            if(!t->aux && (m==2U || m==3U))continue;
            if(successful && med[m]!=0U)
               speed=(double)med[0]/(double)med[m];
            printf("SPR_CASE,%s,%lu,%s,%lu,%.5f,%s,PASS\n",
                t->name,(unsigned long)n,png_spr_modes[m],
                med[m],speed,successful?"MEASURED":"TIMER_NA");
         }
         if(n==512U && successful)
         {
            unsigned int slot=t->kind==PS2_BENCH_UP?0U:
                t->kind==PS2_BENCH_PALETTE?1U:
                t->kind==PS2_BENCH_ADAM_BYTES && t->bpp==4U?2U:3U;
            if(slot<3U && med[3U]>0U)
                png_spr_ratio_x100[slot]=(unsigned int)(
                    (unsigned long long)med[0U]*100ULL/med[3U]);
         }
         ++cases;
      }
   }
   ps2_bench_clock_done();
   png_spr_completed=1U;
   png_spr_cases_done=cases;
   png_spr_timer_na=unmeasured;
   printf("SPR_RESULT,PASS,cases=%u,timer_na=%u,sink=%u\n",
       cases,unmeasured,(unsigned int)png_spr_sink);
   fflush(stdout);
   /* The main 71-group scoreboard and DONE footer are left untouched.
    * Complete machine-readable SPR_CASE rows are always on stdout. */
   return 0;
}
#endif /* PNG_PS2_SPR_BENCH_C */
