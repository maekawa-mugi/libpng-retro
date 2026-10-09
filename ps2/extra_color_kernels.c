/* Further PNG color/bit-depth transform candidates for the PS2 EE lab.
 * The EE has no general 128-bit byte gather; low-bit-depth palette
 * extraction and grayscale expansion are deliberately scalar-unrolled.
 * Unlike Sub/Up PSUBB or swap16 PSRLH/PSLLH, these are not MMI vector
 * kernels.  They must beat the stock implementation in end-to-end tests
 * before production registration. SPDX-License-Identifier: libpng-2.0
 */
#include <string.h>

/* MSB-first PNG packed sample indices (bits=1,2,4) to one byte each.
 * Optional grayscale scaling is exact for each legal sample value.
 * dst==src is permitted. Other overlaps are not permitted.
 */
static void
png_ps2_unpack_packed8(png_byte *dst, const png_byte *src,
    size_t pixels, unsigned int bits, int grayscale_scale)
{
   size_t i;
   unsigned int mask=(1U<<bits)-1U;
   if (dst==src)
   {
      for (i=pixels;i-- > 0;)
      {
         size_t bit=i*bits;
         unsigned int raw=(src[bit>>3] >>
             (8U-bits-(unsigned int)(bit&7U))) & mask;
         dst[i]=(png_byte)(grayscale_scale ?
             raw*(255U/mask) : raw);
      }
   }
   else
   {
      for (i=0;i<pixels;++i)
      {
         size_t bit=i*bits;
         unsigned int raw=(src[bit>>3] >>
             (8U-bits-(unsigned int)(bit&7U))) & mask;
         dst[i]=(png_byte)(grayscale_scale ?
             raw*(255U/mask) : raw);
      }
   }
}

/* Gray8->RGB8/RGBA8; with tRNS gray key when key is >=0. */
static void
png_ps2_gray8_to_rgb(png_byte *dst,const png_byte *src,size_t pixels,
    unsigned int channels,int transparent_key)
{
   size_t i;
   if (dst==src)
   {
      for (i=pixels;i-- > 0;)
      {
         png_byte gray=src[i];
         dst[channels*i]=gray;
         dst[channels*i+1]=gray;
         dst[channels*i+2]=gray;
         if(channels==4)
            dst[4*i+3]=(png_byte)(transparent_key>=0 &&
                gray==(png_byte)transparent_key ? 0U : 255U);
      }
   }
   else
   {
      for(i=0;i<pixels;++i)
      {
         png_byte gray=src[i];
         dst[channels*i]=gray;
         dst[channels*i+1]=gray;
         dst[channels*i+2]=gray;
         if(channels==4)
            dst[4*i+3]=(png_byte)(transparent_key>=0 &&
                gray==(png_byte)transparent_key ? 0U : 255U);
      }
   }
}

/* PNG RGB8 tRNS transparent color expands to RGBA; exactly matching
 * all three key components is transparent; others are fully opaque.
 */
static void
png_ps2_rgb8_trns_to_rgba(png_byte *dst,const png_byte *src,
    size_t pixels,png_byte tr,png_byte tg,png_byte tb)
{
   size_t i;
   if(dst==src)
   {
      for(i=pixels;i-- > 0;)
      {
         png_byte r=src[3*i],g=src[3*i+1],b=src[3*i+2];
         dst[4*i]=r;
         dst[4*i+1]=g;
         dst[4*i+2]=b;
         dst[4*i+3]=(png_byte)(r==tr&&g==tg&&b==tb?0U:255U);
      }
   }
   else
   {
      for(i=0;i<pixels;++i)
      {
         png_byte r=src[3*i],g=src[3*i+1],b=src[3*i+2];
         dst[4*i]=r;
         dst[4*i+1]=g;
         dst[4*i+2]=b;
         dst[4*i+3]=(png_byte)(r==tr&&g==tg&&b==tb?0U:255U);
      }
   }
}

/* RGB16/RGBA16 R/B channel reorder, preserving every 16-bit sample's
 * high-low byte ordering and (optionally) the alpha channel.
 */
static void
png_ps2_swap_rb16(png_byte *row,size_t pixels,unsigned int channels)
{
   size_t i;
   unsigned int stride=2*channels;
   for(i=0;i<pixels;++i)
   {
      png_byte hi=row[stride*i],lo=row[stride*i+1];
      row[stride*i]=row[stride*i+4];
      row[stride*i+1]=row[stride*i+5];
      row[stride*i+4]=hi;
      row[stride*i+5]=lo;
   }
}

/* RGBA8 <-> ARGB8, same reversible rotation for channel order. */
static void
png_ps2_rgba_to_argb(png_byte *row,size_t pixels)
{
   size_t i;
   for(i=0;i<pixels;++i)
   {
      png_byte r=row[4*i],g=row[4*i+1];
      png_byte b=row[4*i+2],a=row[4*i+3];
      row[4*i]=a;
      row[4*i+1]=r;
      row[4*i+2]=g;
      row[4*i+3]=b;
   }
}
