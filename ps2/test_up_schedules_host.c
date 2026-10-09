/* Portable arithmetic and bounds tests for all independent Up schedules. */
#include <stddef.h>
#include <stdio.h>
#include <string.h>
typedef unsigned char png_byte;
typedef unsigned int png_uint_32;
typedef struct { size_t rowbytes; } png_row_info;
#define PNG_PS2_UP_PORTABLE_ADD 1
#include "filter_up_mmi.c"
#include "filter_up_schedules_mmi.c"
#include "filter_up4_mmi.c"
static png_byte raw[1088], above[1088], expect[1088], saved[1088];
static unsigned int state=0x21496371U;
static png_byte rnd(void)
{ state ^= state<<13;state^=state>>17;state^=state<<5;return (png_byte)state; }
static png_byte *align16(png_byte *p)
{ return p + ((16U-((size_t)p&15U))&15U); }
int main(void)
{
   static void (*const funcs[])(png_row_info *,png_byte *,const png_byte *) = {
      png_read_filter_row_up_2x_interleaved_ps2,
      png_read_filter_row_up_2x_prefetch_ps2,
      png_read_filter_row_up_1x_prefetch_ps2,
      png_read_filter_row_up_4x_ps2,
      png_read_filter_row_up_4x_prefetch_ps2
   };
   static const size_t big[] = {257,258,383,511,512,1023,1024};
   unsigned int v,off,po,trial;
   size_t n,i,cases=0;
   for(v=0;v<sizeof funcs/sizeof funcs[0];++v)
      for(off=0;off<16;++off)
         for(po=0;po<16;++po)
            for(trial=0;trial<264;++trial)
            {
               png_row_info ri;
               png_byte *r=align16(raw)+off;
               png_byte *p=align16(above)+po;
               n=trial<=256?trial:big[trial-257];
               ri.rowbytes=n;
               for(i=0;i<n+16;++i)
               {
                  r[i]=i<n?rnd():0xa5;
                  p[i]=i<n?rnd():0x5a;
               }
               memcpy(saved,p,n+16);
               memcpy(expect,r,n+16);
               for(i=0;i<n;++i) expect[i]=(png_byte)(expect[i]+p[i]);
               funcs[v](&ri,r,p);
               if(memcmp(r,expect,n+16)||memcmp(p,saved,n+16))
               {
                  printf("FAIL Up schedule=%u n=%lu row=%u prev=%u\n",
                      v,(unsigned long)n,off,po);
                  return 1;
               }
               ++cases;
            }
   printf("PASS Up schedules %lu cases\n",(unsigned long)cases);
   return 0;
}
