/* Experimental libpng target hook for 8-bit PNG palette expansion.
 * Shared verbatim by production pngsimd.c and the EE/host hook contract test.
 * Deliberately leave 1/2/4-bit unpacking to the verified generic libpng
 * implementation; no startup palette caches or hidden allocations.
 *
 * Included by ee_init.c rather than separately compiled.
 * SPDX-License-Identifier: libpng-2.0
 */
static int
png_ps2_expand_palette_row_8(png_row_info *ri, png_byte *row,
    const png_color *palette, const png_byte *trans_alpha, int num_trans)
{
   size_t i;
   const size_t pixels = (size_t)ri->width;
   if (ri->color_type != PNG_COLOR_TYPE_PALETTE ||
       ri->bit_depth != 8 || pixels == 0 || palette == NULL)
      return 0;

   if (num_trans > 0)
   {
      if (trans_alpha == NULL) return 0;
      /* Production libpng allocates its row buffer for the expanded output
       * before invoking the target-specific hook.  Iterate backwards so
       * original palette indices cannot be overwritten before reading. */
      for (i = pixels; i-- > 0;)
      {
         const png_byte index = row[i];
         const png_color *color = palette + index;
         row[4*i] = color->red;
         row[4*i+1] = color->green;
         row[4*i+2] = color->blue;
         row[4*i+3] = index < (unsigned int)num_trans ?
             trans_alpha[index] : 255;
      }
      ri->bit_depth = 8;
      ri->pixel_depth = 32;
      ri->rowbytes = pixels * 4;
      ri->color_type = PNG_COLOR_TYPE_RGB_ALPHA;
      ri->channels = 4;
   }
   else
   {
      for (i = pixels; i-- > 0;)
      {
         const png_byte index = row[i];
         const png_color *color = palette + index;
         row[3*i] = color->red;
         row[3*i+1] = color->green;
         row[3*i+2] = color->blue;
      }
      ri->bit_depth = 8;
      ri->pixel_depth = 24;
      ri->rowbytes = pixels * 3;
      ri->color_type = PNG_COLOR_TYPE_RGB;
      ri->channels = 3;
   }
   return 1;
}
