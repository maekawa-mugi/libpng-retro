/* Fixed 80-column/25-row geometry for the EE debug renderer.
 * Five two-column winner rows, then seventeen three-column candidate rows.
 * For 93 candidates only two pages are required. */
#ifndef PS2_COMPACT_LAYOUT_H
#define PS2_COMPACT_LAYOUT_H

#define PS2_AB_COLUMNS 3U
#define PS2_AB_ROWS_PER_COLUMN 17U
#define PS2_AB_PAGE_SIZE (PS2_AB_COLUMNS * PS2_AB_ROWS_PER_COLUMN)
#define PS2_AB_CANDIDATE_FIRST_ROW 6U
#define PS2_AB_CANDIDATE_LAST_ROW 22U
#define PS2_AB_CANDIDATE_FIRST_X 1U
#define PS2_AB_CANDIDATE_COL_WIDTH 26U
#define PS2_AB_SUMMARY_FIRST_ROW 1U
#define PS2_AB_SUMMARY_ROWS 5U
#define PS2_AB_SUMMARY_RIGHT_X 41U
#define PS2_AB_FOOTER_FIRST_ROW 23U
#define PS2_AB_LAST_SCREEN_ROW 24U
#define PS2_AB_SCREEN_COLUMNS 80U

static unsigned int
ps2_ab_page_count(unsigned int total)
{
   return (total + PS2_AB_PAGE_SIZE - 1U) / PS2_AB_PAGE_SIZE;
}
static unsigned int
ps2_ab_page_of(unsigned int index)
{
   return index / PS2_AB_PAGE_SIZE;
}
static unsigned int
ps2_ab_column_of(unsigned int index)
{
   return (index % PS2_AB_PAGE_SIZE) / PS2_AB_ROWS_PER_COLUMN;
}
static unsigned int
ps2_ab_row_of(unsigned int index)
{
   return PS2_AB_CANDIDATE_FIRST_ROW +
       (index % PS2_AB_ROWS_PER_COLUMN);
}
static unsigned int
ps2_ab_x_of(unsigned int index)
{
   return PS2_AB_CANDIDATE_FIRST_X +
       ps2_ab_column_of(index) * PS2_AB_CANDIDATE_COL_WIDTH;
}
#endif /* PS2_COMPACT_LAYOUT_H */
