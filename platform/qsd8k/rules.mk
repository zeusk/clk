LOCAL_DIR := $(GET_LOCAL_DIR)

INCLUDES += -I$(LOCAL_DIR)/include

OBJS += \
	$(LOCAL_DIR)/interrupts.o \
	$(LOCAL_DIR)/gpio.o \
	$(LOCAL_DIR)/mmc.o \
	$(LOCAL_DIR)/sdcc.o

LINKER_SCRIPT += $(BUILDDIR)/system-onesegment.ld

include platform/msm_shared/rules.mk

