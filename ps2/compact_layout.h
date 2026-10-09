/* PS2SDK 80-column by 25-row text console.
 * Seventy-one filter/bpp result groups fit onto one fixed 4x18 grid.
 * Keep the rightmost column short: avoid debug-font wrap artifacts.
 */
#ifndef PS2_COMPACT_LAYOUT_H
#define PS2_COMPACT_LAYOUT_H
#define PS2_WIN_COLUMNS 4U
#define PS2_WIN_ROWS 18U
#define PS2_WIN_CAPACITY (PS2_WIN_COLUMNS * PS2_WIN_ROWS)
#define PS2_WIN_CELL_WIDTH 20U
#define PS2_WIN_CELL_TEXT 18U
#define PS2_WIN_FIRST_Y 1U
#define PS2_WIN_STATUS_Y 19U
#define PS2_WIN_COUNTS_Y 20U
#define PS2_WIN_DETAIL_Y 21U
#define PS2_WIN_DONE_Y 22U
#define PS2_WIN_SCREEN_COLUMNS 80U
#define PS2_WIN_SCREEN_ROWS 25U

static unsigned int ps2_win_x(unsigned int group)
{
   return (group / PS2_WIN_ROWS) * PS2_WIN_CELL_WIDTH;
}
static unsigned int ps2_win_y(unsigned int group)
{
   return PS2_WIN_FIRST_Y + (group % PS2_WIN_ROWS);
}
#endif
