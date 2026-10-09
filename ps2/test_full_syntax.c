/* Portable build of the original and expanded experimental kernels.
 * Separate from the correctness suite to catch accidental missing
 * declarations and exercise the C branch without an R5900 toolchain.
 */
#include <stddef.h>
typedef unsigned char png_byte;
typedef unsigned int png_uint_32;
typedef struct { size_t rowbytes; } png_row_info;
#include "extra_kernels_mmi.c"
#include "extra_full_kernels.c"
int main(void) { return 0; }
