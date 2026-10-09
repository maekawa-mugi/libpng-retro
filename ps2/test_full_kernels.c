/* Exhaustive small-row and selected wide-row regressions for the
 * experimental write, palette, 16-bit, channel and Adam7 kernels.
 * Included by test_extra_kernels.c after extra_full_kernels.c.
 * SPDX-License-Identifier: libpng-2.0
 */
#define PS2_FULL_LIMIT 1024U
#define PS2_FULL_CAP (4U * PS2_FULL_LIMIT + 64U)
static png_byte full_input[PS2_FULL_CAP], full_prev[PS2_FULL_CAP];
static png_byte full_output[PS2_FULL_CAP], full_expected[PS2_FULL_CAP];
static png_byte full_original[PS2_FULL_CAP], full_saved_prev[PS2_FULL_CAP];
static png_byte full_rgb_table[768], full_alpha_table[256];
static unsigned int full_state = 0xc19d732aU;

static png_byte
png_ps2_full_random(void)
{
   full_state ^= full_state << 13;
   full_state ^= full_state >> 17;
   full_state ^= full_state << 5;
   return (png_byte)full_state;
}

static unsigned int
png_ps2_full_paeth_ref(unsigned int a, unsigned int b, unsigned int c)
{
   int p = (int)a + (int)b - (int)c;
   int da = p - (int)a, db = p - (int)b, dc = p - (int)c;
   da = da < 0 ? -da : da;
   db = db < 0 ? -db : db;
   dc = dc < 0 ? -dc : dc;
   if (da <= db && da <= dc) return a;
   if (db <= dc) return b;
   return c;
}

static void
png_ps2_full_write_ref(png_byte *row, const png_byte *prev,
    size_t n, unsigned int bpp, unsigned int mode)
{
   size_t i;
   for (i = n; i-- > 0;)
   {
      unsigned int a = i >= bpp ? row[i-bpp] : 0;
      unsigned int b = prev[i];
      unsigned int c = i >= bpp ? prev[i-bpp] : 0;
      unsigned int predictor = mode == 0 ? a :
          mode == 1 ? (a+b)>>1 : png_ps2_full_paeth_ref(a,b,c);
      row[i] = (png_byte)((unsigned int)row[i] - predictor);
   }
}

static int
png_ps2_test_full(void)
{
   static const unsigned int strides[6] = {1,2,3,4,6,8};
   static const unsigned int special_lengths[8] =
       {257,258,509,511,512,513,767,1024};
   unsigned long write_cases=0, palette_cases=0, convert_cases=0, adam_cases=0;
   png_byte *src=png_ps2_extra_align16(full_input);
   png_byte *above=png_ps2_extra_align16(full_prev);
   png_byte *dst=png_ps2_extra_align16(full_output);
   png_byte *expect=png_ps2_extra_align16(full_expected);
   png_byte *orig=png_ps2_extra_align16(full_original);
   png_byte *save_prev=png_ps2_extra_align16(full_saved_prev);
   size_t i, n;
   unsigned int stride, mode, off, rep, l, pass, depth;

   /* Write Sub/Average/Paeth for EVERY supported byte stride. */
   for (rep=0;rep<2;++rep)
      for (stride=0;stride<6;++stride)
         for (mode=0;mode<3;++mode)
            for (off=0;off<16;++off)
               for (l=0;l<265;++l)
               {
                  if (l<=256) n=l;
                  else n=special_lengths[l-257];
                  {
                     png_byte *row=src+off;
                     png_byte *prev=above+((7*off+rep)&15U);
                     for (i=0;i<n+16;++i)
                     {
                        row[i]=i<n?png_ps2_full_random():0xa5;
                        prev[i]=i<n?png_ps2_full_random():0x5a;
                     }
                     memcpy(expect,row,n+16);
                     memcpy(save_prev,prev,n+16);
                     png_ps2_full_write_ref(expect,prev,n,strides[stride],mode);
                     png_ps2_write_filter_packed(row,prev,n,strides[stride],mode);
                     if (memcmp(row,expect,n+16)!=0 ||
                         memcmp(prev,save_prev,n+16)!=0)
                     {
                        printf("FULL_FAIL,write,%u,%u,%lu,%u,%u\n",
                            strides[stride],mode,(unsigned long)n,off,rep);
                        return 1;
                     }
                     ++write_cases;
                  }
               }

   for(i=0;i<768;++i)full_rgb_table[i]=png_ps2_full_random();
   for(i=0;i<256;++i)full_alpha_table[i]=png_ps2_full_random();

   /* Palette RGB and tRNS RGBA; both in-place and disjoint buffers. */
   for(mode=0;mode<2;++mode)
      for(rep=0;rep<2;++rep)
         for(off=0;off<16;++off)
            for(n=0;n<=256;++n)
            {
               png_byte *in=src+off, *out=rep?in:dst+off, *e=expect+off;
               size_t channel=mode?4:3, bytes=channel*n;
               for(i=0;i<n+16;++i)
                  in[i]=i<n?png_ps2_full_random():0x5a;
               memcpy(orig,in,n+16);
               memset(out+bytes,0xa5,16);
               memset(e+bytes,0xa5,16);
               for(i=0;i<n;++i)
               {
                  png_byte index=orig[i];
                  e[channel*i]=full_rgb_table[3*(size_t)index];
                  e[channel*i+1]=full_rgb_table[3*(size_t)index+1];
                  e[channel*i+2]=full_rgb_table[3*(size_t)index+2];
                  if(mode)e[4*i+3]=full_alpha_table[index];
               }
               if(mode)
                  png_ps2_expand_palette_rgba_trns(out,in,n,
                      full_rgb_table,full_alpha_table);
               else
                  png_ps2_expand_palette_rgb3(out,in,n,full_rgb_table);
               if(memcmp(out,e,bytes+16)!=0 ||
                   (!rep && memcmp(in,orig,n+16)!=0))
               {
                  printf("FULL_FAIL,palette,%u,%u,%lu,%u\n",
                      mode,rep,(unsigned long)n,off);
                  return 1;
               }
               ++palette_cases;
            }

   /* 16-bit swap and strip (both same-address and disjoint strip). */
   for(mode=0;mode<3;++mode)
      for(off=0;off<16;++off)
         for(n=0;n<=256;++n)
         {
            png_byte *in=src+off, *out=mode==1?dst+off:in;
            png_byte *e=expect+off;
            for(i=0;i<2*n+16;++i)
               in[i]=i<2*n?png_ps2_full_random():0xa5;
            memcpy(orig,in,2*n+16);
            if(mode==0)
            {
               for(i=0;i<n;++i)
               {
                  e[2*i]=orig[2*i+1];
                  e[2*i+1]=orig[2*i];
               }
               memset(e+2*n,0xa5,16);
               png_ps2_swap16_mmi(in,n);
               if(memcmp(in,e,2*n+16)!=0)
               {
                  printf("FULL_FAIL,swap16,%lu,%u\n",(unsigned long)n,off);
                  return 1;
               }
            }
            else
            {
               for(i=0;i<n;++i)e[i]=orig[2*i];
               memset(e+n,0xa5,16);
               if(mode==1)memset(out+n,0xa5,16);
               png_ps2_strip16_high(out,in,n);
               if(memcmp(out,e,n)!=0 ||
                   (mode==1&&memcmp(in,orig,2*n+16)!=0))
               {
                  printf("FULL_FAIL,strip16,%u,%lu,%u\n",
                      mode,(unsigned long)n,off);
                  return 1;
               }
            }
            ++convert_cases;
         }

   /* RGB/BGR, RGBA/BGRA, 3->4 opaque alpha and 4->3 drop alpha. */
   for(mode=0;mode<4;++mode)
      for(rep=0;rep<2;++rep)
         for(off=0;off<16;++off)
            for(n=0;n<=256;++n)
            {
               png_byte *in=src+off, *out=rep?in:dst+off, *e=expect+off;
               unsigned int input_ch=mode==1||mode==3?4:3;
               unsigned int output_ch=mode==2?4:mode==3?3:input_ch;
               size_t old_len=(size_t)input_ch*n, new_len=(size_t)output_ch*n;
               for(i=0;i<old_len+16;++i)
                  in[i]=i<old_len?png_ps2_full_random():0xa5;
               memcpy(orig,in,old_len+16);
               memset(e+new_len,0xa5,16);
               for(i=0;i<n;++i)
               {
                  e[output_ch*i]=mode<2?orig[input_ch*i+2]:orig[input_ch*i];
                  e[output_ch*i+1]=orig[input_ch*i+1];
                  e[output_ch*i+2]=mode<2?orig[input_ch*i]:orig[input_ch*i+2];
                  if(output_ch==4)
                     e[output_ch*i+3]=mode==2?0x7f:orig[input_ch*i+3];
               }
               if(!rep)memset(out+new_len,0xa5,16);
               if(mode<2)
               {
                  /* Swaps are intrinsically in-place. Work on out. */
                  if(!rep)memcpy(out,in,old_len);
                  png_ps2_swap_rb(out,n,input_ch);
               }
               else if(mode==2)png_ps2_rgb_to_rgba(out,in,n,0x7f);
               else png_ps2_rgba_to_rgb(out,in,n);
               if(memcmp(out,e,new_len)!=0 ||
                   (!rep&&memcmp(in,orig,old_len+16)!=0))
               {
                  printf("FULL_FAIL,channels,%u,%u,%lu,%u\n",
                      mode,rep,(unsigned long)n,off);
                  return 1;
               }
               ++convert_cases;
            }

   /* Adam7 byte-aligned horizontal scatter, seven passes, all bpp. */
   for(stride=0;stride<6;++stride)
      for(pass=0;pass<7;++pass)
         for(n=0;n<=128;++n)
         {
            size_t k=0, x, size=n*strides[stride];
            for(i=0;i<size+16;++i)
               dst[i]=i<size?png_ps2_full_random():0xa5;
            memcpy(expect,dst,size+16);
            for(i=0;i<128U*8U+16U;++i)
               src[i]=png_ps2_full_random();
            for(x=png_ps2_adam7_xstart[pass];x<n;
                x+=png_ps2_adam7_xstep[pass],++k)
               memcpy(expect+x*strides[stride],src+k*strides[stride],
                   strides[stride]);
            png_ps2_adam7_scatter_bytes(dst,src,n,strides[stride],pass);
            if(memcmp(dst,expect,size+16)!=0)
            {
               printf("FULL_FAIL,adam7-bytes,%u,%u,%lu\n",
                   strides[stride],pass,(unsigned long)n);
               return 1;
            }
            ++adam_cases;
         }

   /* Adam7 packed depths 1/2/4: independent per-pixel bit reference. */
   for(depth=1;depth<=4;depth*=2)
      for(pass=0;pass<7;++pass)
         for(n=0;n<=128;++n)
         {
            size_t bytes=(n*depth+7)>>3, x, k=0;
            for(i=0;i<bytes+16;++i)
               dst[i]=i<bytes?png_ps2_full_random():0xa5;
            memcpy(expect,dst,bytes+16);
            for(i=0;i<128U*8U+16U;++i)
               src[i]=png_ps2_full_random();
            for(x=png_ps2_adam7_xstart[pass];x<n;
                x+=png_ps2_adam7_xstep[pass],++k)
            {
               unsigned int j;
               for(j=0;j<depth;++j)
               {
                  size_t sb=k*depth+j, db=x*depth+j;
                  unsigned int srcbit=(unsigned int)((src[sb>>3] >>
                      (7U-(sb&7U)))&1U);
                  png_byte bit=(png_byte)(1U<<(7U-(db&7U)));
                  expect[db>>3]=(png_byte)((expect[db>>3]&~bit)|
                      (srcbit?bit:0));
               }
            }
            png_ps2_adam7_scatter_bits(dst,src,n,depth,pass);
            if(memcmp(dst,expect,bytes+16)!=0)
            {
               printf("FULL_FAIL,adam7-bits,%u,%u,%lu\n",
                   depth,pass,(unsigned long)n);
               return 1;
            }
            ++adam_cases;
         }

   printf("FULL_PASS,write=%lu,palette=%lu,convert=%lu,adam7=%lu\n",
       write_cases,palette_cases,convert_cases,adam_cases);
   return 0;
}
