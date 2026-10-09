/* Standalone native C syntax checks for the EE SPR-only lab.
 * It type-checks its genuine source with synthetic PNG declarations.
 * NEVER executes fixed EE scratchpad addresses on the host.
 * Cross-assembly and real R5900 hardware validation remain necessary.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef unsigned char png_byte;
typedef unsigned int png_uint_32;
typedef struct {
    size_t rowbytes;
    png_uint_32 width;
    png_byte color_type, channels, bit_depth, pixel_depth;
} png_row_info;
typedef void (*ps2_bench_fn)(png_row_info *,png_byte *,const png_byte *);

#define PS2_BENCH_SUB          0U
#define PS2_BENCH_UP           1U
#define PS2_BENCH_AVG          2U
#define PS2_BENCH_PAETH        3U
#define PS2_BENCH_WRITE_UP     4U
#define PS2_BENCH_WRITE_AVG4   6U
#define PS2_BENCH_WRITE_PAETH  7U
#define PS2_BENCH_PALETTE      8U
#define PS2_BENCH_PAL_RGB     12U
#define PS2_BENCH_PAL_TRNS    13U
#define PS2_BENCH_SWAP16      14U
#define PS2_BENCH_ADAM_BYTES  20U
#define PS2_BENCH_UNPACK_IDX  22U
#define PS2_BENCH_GRAY_RGB    24U
#define PS2_BENCH_GRAY_RGBA   25U
#define PS2_BENCH_UNIT "clock_ticks"

extern png_byte ps2_bench_palette_table[1024];
extern png_byte ps2_bench_rgb_table[768],ps2_bench_alpha_table[256];
extern unsigned int ps2_bench_bpp,ps2_bench_filter;

extern void png_read_filter_row_up_ps2(png_row_info *,png_byte *,const png_byte *);
extern void png_read_filter_row_up_4x_ps2(png_row_info *,png_byte *,const png_byte *);
extern void png_read_filter_row_sub4_ps2(png_row_info *,png_byte *,const png_byte *);
extern void png_read_filter_row_avg4_ps2(png_row_info *,png_byte *,const png_byte *);
extern void png_read_filter_row_paeth4_ps2(png_row_info *,png_byte *,const png_byte *);
extern void ps2_bench_avg4_dual(png_row_info *,png_byte *,const png_byte *);
extern void png_ps2_write_up_mmi(png_row_info *,png_byte *,const png_byte *);
extern void png_ps2_write_avg4_mmi(png_row_info *,png_byte *,const png_byte *);
extern void png_ps2_write_paeth4_mmi(png_row_info *,png_byte *,const png_byte *);
extern void ps2_bench_palette_expand(png_row_info *,png_byte *,const png_byte *);
extern void ps2_bench_palette_rgb(png_row_info *,png_byte *,const png_byte *);
extern void ps2_bench_palette_trns(png_row_info *,png_byte *,const png_byte *);
extern void ps2_bench_adam_bytes(png_row_info *,png_byte *,const png_byte *);
extern void ps2_bench_unpack_index(png_row_info *,png_byte *,const png_byte *);
extern void ps2_bench_gray_rgb(png_row_info *,png_byte *,const png_byte *);
extern void ps2_bench_gray_rgba(png_row_info *,png_byte *,const png_byte *);
extern void ps2_bench_swap16(png_row_info *,png_byte *,const png_byte *);

extern void png_ps2_expand_palette_rgba4(png_byte *,const png_byte *,size_t,const png_byte *);
extern void png_ps2_expand_palette_rgb3(png_byte *,const png_byte *,size_t,const png_byte *);
extern void png_ps2_expand_palette_rgba_trns(png_byte *,const png_byte *,size_t,const png_byte *,const png_byte *);
extern void ps2_bench_shape(unsigned int,unsigned int,size_t,size_t *,size_t *,size_t *);
extern void ps2_bench_scalar(png_row_info *,png_byte *,const png_byte *);
extern void ps2_bench_clock_init(void);
extern void ps2_bench_clock_done(void);
extern unsigned long ps2_bench_now(void);
extern unsigned long ps2_bench_delta(unsigned long,unsigned long);

#include "spr_bench.c"
