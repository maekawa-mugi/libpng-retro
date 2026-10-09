/* Independent regressions for packed PNG input and color transforms.
 * Runs as part of the existing host/EE one-trip correctness matrix.
 * SPDX-License-Identifier: libpng-2.0
 */
static int
png_ps2_test_color(void)
{
   static const unsigned int depths[3]={1,2,4};
   unsigned int rep, off, mode, bit, channels;
   size_t n,i;
   unsigned long passes=0;
   png_byte *in=png_ps2_extra_align16(full_input);
   png_byte *out=png_ps2_extra_align16(full_output);
   png_byte *expect=png_ps2_extra_align16(full_expected);
   png_byte *saved=png_ps2_extra_align16(full_original);

   for(rep=0;rep<2;++rep)
      for(off=0;off<16;++off)
         for(n=0;n<=256;++n)
         {
            png_byte *src=in+off,*dst=rep?src:out+off,*e=expect+off;
            size_t j;
            for(bit=0;bit<3;++bit)
               for(mode=0;mode<2;++mode)
               {
                  unsigned int depth=depths[bit];
                  unsigned int max=(1U<<depth)-1U;
                  size_t bytes=(n*depth+7U)/8U;
                  for(j=0;j<bytes+16;++j)
                     src[j]=j<bytes?png_ps2_full_random():0xa5;
                  memcpy(saved,src,bytes+16);
                  memset(e+n,0xa5,16);
                  if(!rep)memset(dst+n,0xa5,16);
                  for(j=0;j<n;++j)
                  {
                     unsigned int value=0,k;
                     for(k=0;k<depth;++k)
                     {
                        size_t pos=j*depth+k;
                        value=(value<<1)|((saved[pos>>3]>>
                            (7U-(pos&7U)))&1U);
                     }
                     e[j]=(png_byte)(mode?value*(255U/max):value);
                  }
                  png_ps2_unpack_packed8(dst,src,n,depth,(int)mode);
                  if(memcmp(dst,e,n)!=0 ||
                      (!rep&&memcmp(src,saved,bytes+16)!=0))
                  {
                     printf("COLOR_FAIL,packed,%u,%u,%lu,%u,%u\n",
                         depth,mode,(unsigned long)n,off,rep);
                     return 1;
                  }
                  ++passes;
               }

            for(channels=3;channels<=4;++channels)
            {
               size_t size=channels*n;
               for(j=0;j<n+16;++j)
                  src[j]=j<n?png_ps2_full_random():0xa5;
               for(j=0;j<n;j+=7)src[j]=127;
               memcpy(saved,src,n+16);
               memset(e+size,0xa5,16);
               if(!rep)memset(dst+size,0xa5,16);
               for(j=0;j<n;++j)
               {
                  png_byte gray=saved[j];
                  e[channels*j]=gray;
                  e[channels*j+1]=gray;
                  e[channels*j+2]=gray;
                  if(channels==4)e[4*j+3]=gray==127?0:255;
               }
               png_ps2_gray8_to_rgb(dst,src,n,channels,127);
               if(memcmp(dst,e,size)!=0 ||
                   (!rep&&memcmp(src,saved,n+16)!=0))
               {
                  printf("COLOR_FAIL,gray,%u,%lu,%u,%u\n",
                      channels,(unsigned long)n,off,rep);
                  return 1;
               }
               ++passes;
            }

            {
               size_t bytes=3*n;
               for(j=0;j<bytes+16;++j)
                  src[j]=j<bytes?png_ps2_full_random():0xa5;
               for(j=0;j<n;j+=7)
               {
                  src[3*j]=0x11;src[3*j+1]=0x22;src[3*j+2]=0x33;
               }
               memcpy(saved,src,bytes+16);
               if(!rep)memset(dst+4*n,0xa5,16);
               memset(e+4*n,0xa5,16);
               for(j=0;j<n;++j)
               {
                  png_byte red=saved[3*j],green=saved[3*j+1],blue=saved[3*j+2];
                  e[4*j]=red;
                  e[4*j+1]=green;
                  e[4*j+2]=blue;
                  e[4*j+3]=red==0x11&&green==0x22&&blue==0x33?0:255;
               }
               png_ps2_rgb8_trns_to_rgba(dst,src,n,0x11,0x22,0x33);
               if(memcmp(dst,e,4*n)!=0 ||
                   (!rep&&memcmp(src,saved,bytes+16)!=0))
               {
                  printf("COLOR_FAIL,trns,%lu,%u,%u\n",
                      (unsigned long)n,off,rep);
                  return 1;
               }
               ++passes;
            }

            for(channels=3;channels<=4;++channels)
            {
               size_t bytes=2*channels*n;
               for(j=0;j<bytes+16;++j)
                  src[j]=j<bytes?png_ps2_full_random():0xa5;
               memcpy(e,src,bytes+16);
               for(j=0;j<n;++j)
               {
                  png_byte h=e[2*channels*j],lo=e[2*channels*j+1];
                  e[2*channels*j]=e[2*channels*j+4];
                  e[2*channels*j+1]=e[2*channels*j+5];
                  e[2*channels*j+4]=h;
                  e[2*channels*j+5]=lo;
               }
               png_ps2_swap_rb16(src,n,channels);
               if(memcmp(src,e,bytes+16)!=0)
               {
                  printf("COLOR_FAIL,swap16channels,%u,%lu,%u\n",
                      channels,(unsigned long)n,off);
                  return 1;
               }
               ++passes;
            }

            for(j=0;j<4*n+16;++j)
               (in+off)[j]=j<4*n?png_ps2_full_random():0xa5;
            memcpy(e,in+off,4*n+16);
            for(j=0;j<n;++j)
            {
               png_byte red=e[4*j],green=e[4*j+1];
               png_byte blue=e[4*j+2],alpha=e[4*j+3];
               e[4*j]=alpha;
               e[4*j+1]=red;
               e[4*j+2]=green;
               e[4*j+3]=blue;
            }
            png_ps2_rgba_to_argb(in+off,n);
            if(memcmp(in+off,e,4*n+16)!=0)
            {
               printf("COLOR_FAIL,argb,%lu,%u\n",(unsigned long)n,off);
               return 1;
            }
            ++passes;
         }
   printf("COLOR_PASS,cases=%lu\n",passes);
   return 0;
}
