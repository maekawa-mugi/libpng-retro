/* Compile-only validation of the production EE filter registration.
 * This is intentionally not an x86 execution of EE assembly. */
#include <stddef.h>
typedef unsigned char png_byte;
typedef unsigned int png_uint_32;
typedef struct { size_t rowbytes; } png_row_info;
typedef void (*ps2_filter_fn)(png_row_info *, png_byte *, const png_byte *);
typedef struct { ps2_filter_fn read_filter[4]; } png_struct;
#define PNG_FILTER_VALUE_SUB  1
#define PNG_FILTER_VALUE_UP   2
#define PNG_FILTER_VALUE_AVG  3
#define PNG_FILTER_VALUE_PAETH 4
#include "ee_init.c"

void png_ps2_test_production_dispatch(png_struct *p, unsigned int bpp)
{
   png_init_filter_functions_ps2(p, bpp);
}
