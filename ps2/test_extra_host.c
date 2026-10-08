/* Host driver for the exact standalone EE-write/palette source code. */
#include <stddef.h>
typedef unsigned char png_byte;
typedef unsigned int png_uint_32;
typedef struct { size_t rowbytes; } png_row_info;
#define PNG_PS2_EXTRA_PORTABLE 1
#include "test_extra_kernels.c"
int main(void) { return png_ps2_test_extra(); }
