/* Host test for compact PS2 80x25 text screen page packing.
 * Geometry is separate from PS2SDK so GCC and Clang can verify it. */
#include <stdio.h>
#include "compact_layout.h"

int main(void)
{
   unsigned int i, j;
   if (ps2_ab_page_count(93U) != 2U ||
       ps2_ab_page_count(102U) != 2U ||
       ps2_ab_page_count(103U) != 3U ||
       ps2_ab_page_count(0U) != 0U)
      return 1;
   if (PS2_AB_SUMMARY_FIRST_ROW + PS2_AB_SUMMARY_ROWS !=
       PS2_AB_CANDIDATE_FIRST_ROW ||
       PS2_AB_CANDIDATE_LAST_ROW + 1U != PS2_AB_FOOTER_FIRST_ROW ||
       PS2_AB_LAST_SCREEN_ROW != 24U)
      return 2;
   for (i = 0U; i < 250U; ++i)
   {
      unsigned int x = ps2_ab_x_of(i);
      unsigned int y = ps2_ab_row_of(i);
      if (x + 25U > PS2_AB_SCREEN_COLUMNS ||
          y < PS2_AB_CANDIDATE_FIRST_ROW ||
          y > PS2_AB_CANDIDATE_LAST_ROW ||
          ps2_ab_column_of(i) >= PS2_AB_COLUMNS ||
          ps2_ab_page_of(i) != i / PS2_AB_PAGE_SIZE)
         return 3;
      for (j = i+1U; j < 250U; ++j)
         if (ps2_ab_page_of(i) == ps2_ab_page_of(j) &&
             x == ps2_ab_x_of(j) && y == ps2_ab_row_of(j))
            return 4;
   }
   puts("PASS compact EE screen layout: 93 candidates, two 3-column pages");
   return 0;
}
