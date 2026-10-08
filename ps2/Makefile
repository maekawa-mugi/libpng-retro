# PS2SDK standalone EE MMI filter-harness build.
# Build from this directory: make -C ps2
#
# For the opt-in 128-bit Sub4 prefix path:
#   make -C ps2 clean
#   make -C ps2 EE_OPTFLAGS="-O2 -DPNG_PS2_EE_MMI_SUB4_PREFIX"
#
# This is NOT a full libpng build; it tests the MMI filter translation
# unit directly with the EE toolchain, then runs on an EE console/emulator.

ifndef PS2SDK
$(error PS2SDK is not set; source your PS2SDK environment first)
endif

EE_BIN = test_filter_mmi.elf
EE_OBJS = test_filter_mmi.o
EE_LIBS += -ldebug
EE_OPTFLAGS ?= -O2
EE_WARNFLAGS ?= -Wall -Wextra

.PHONY: all clean
all: $(EE_BIN)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal

clean:
	$(RM) $(EE_BIN) $(EE_OBJS)

# Included production sources must trigger harness recompilation.
test_filter_mmi.o: filter_mmi.c filter_up_mmi.c filter_gray_mmi.c filter_rgb3.c filter_paeth_mmi.c filter_wide_mmi.c
