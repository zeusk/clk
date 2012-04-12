LOCAL_DIR := $(GET_LOCAL_DIR)

INCLUDES += -I$(LOCAL_DIR)/include -I$(LK_TOP_DIR)/platform/msm_shared

PLATFORM := qsd8k

#cedesmith note: MEMBASE requires edit in platform/qsd8k/rules.mk
# maximum partition size will be about 340mb ( MEMBASE-SCRATCH_ADDR)
MEMBASE := 0x28000000
MEMSIZE := 0x00100000 
DEFINES += WSPL_VADDR=0x80000000

BASE_ADDR        := 0x11800000
TAGS_ADDR        := "(BASE_ADDR+0x00000100)"
KERNEL_ADDR      := "(BASE_ADDR+0x00008000)"
RAMDISK_ADDR     := "(BASE_ADDR+0x00a00000)"
SCRATCH_ADDR     := "(BASE_ADDR+0x01400000)"

KEYS_USE_GPIO_KEYPAD := 1

DEFINES += DISPLAY_TYPE_LCDC=1

CFLAGS += -mlittle-endian -mfpu=neon
LDFLAGS += -EL

MODULES += \
	dev/keys \
	lib/debug \
	lib/ptable \
	lib/devinfo

ifeq ($(WITH_SDC_DRIVER),1)
MODULES += \
	lib/fs \
	dev/sd
endif

DEFINES += \
	MEMBASE=$(MEMBASE)\
	MEMSIZE=$(MEMSIZE) \
	BASE_ADDR=$(BASE_ADDR) \
	TAGS_ADDR=$(TAGS_ADDR) \
	KERNEL_ADDR=$(KERNEL_ADDR) \
	RAMDISK_ADDR=$(RAMDISK_ADDR) \
	SCRATCH_ADDR=$(SCRATCH_ADDR)

OBJS += \
	$(LOCAL_DIR)/init.o \
	$(LOCAL_DIR)/nand.o \
	$(LOCAL_DIR)/keypad.o \
	$(LOCAL_DIR)/atags.o \
	$(LOCAL_DIR)/htcleo_boot.o \
	$(LOCAL_DIR)/htcleo_boot_s.o \
	$(LOCAL_DIR)/platform.o 
