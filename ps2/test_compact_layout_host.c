/* Validate one-screen 80x25 geometry with 4x18 winner cells.
 * PS2SDK not required; compiler must reject warnings as errors. */
#include <stdio.h>
#include "compact_layout.h"
int main(void)
{
   unsigned int i,j;
   if (PS2_WIN_CAPACITY != 72U ||
       PS2_WIN_STATUS_Y <= PS2_WIN_FIRST_Y+PS2_WIN_ROWS-1U ||
       PS2_WIN_DONE_Y >= PS2_WIN_SCREEN_ROWS)
      return 1;
   for(i=0;i<PS2_WIN_CAPACITY;++i)
   {
      const unsigned int x=ps2_win_x(i), y=ps2_win_y(i);
      if (x+PS2_WIN_CELL_TEXT>=PS2_WIN_SCREEN_COLUMNS ||
          y<PS2_WIN_FIRST_Y || y>=PS2_WIN_STATUS_Y)
         return 2;
      for(j=i+1U;j<PS2_WIN_CAPACITY;++j)
         if (ps2_win_x(i)==ps2_win_x(j) &&
             ps2_win_y(i)==ps2_win_y(j))
            return 3;
   }
   /* The largest in-range three-decimal winner label must fit in a
    * single 18-character cell with no terminal text-column wrap. */
   {
      char cell[32];
      int n = snprintf(cell,sizeof cell,"%02u:%02u WIN %lu.%03lux",
          71U,93U,999UL,999UL);
      if (n != 18 || cell[18] != 0) return 4;
      n = snprintf(cell,sizeof cell,"%02u:S WIN %lu.%03lux",
          71U,999UL,999UL);
      if (n > (int)PS2_WIN_CELL_TEXT) return 5;
   }
   puts("PASS PS2 71 groups in 72 non-overlapping winner cells and ratio labels");
   return 0;
}
