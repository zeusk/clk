LOCAL_DIR := $(GET_LOCAL_DIR)

INCLUDES += -I$(LOCAL_DIR)/include -I$(LK_TOP_DIR)/platform/msm_shared

ARCH 					:= arm
ARM_CPU 				:= cortex-a8
PLATFORM 				:= qsd8k
CPU 					:= generic
MSM_NAND_WINCE       	:= 1
WITH_CPU_EARLY_INIT		:= 1
WITH_CPU_WARM_BOOT		:= 1
KEYS_USE_GPIO_KEY    	:= 1
KEYS_USE_GPIO_KEYPAD 	:= 1
DISPLAY_TYPE_LCDC		:= 1
WSPL_VADDR           	:= 0x80000000
MEMBASE 				:= 0x28000000
MEMSIZE 				:= 0x00100000 
BASE_ADDR        		:= 0x11800000
TAGS_ADDR        		:= "(BASE_ADDR+0x00000100)"
KERNEL_ADDR      		:= "(BASE_ADDR+0x00008000)"
RAMDISK_ADDR     		:= "(BASE_ADDR+0x00a00000)"
SCRATCH_ADDR     		:= "(BASE_ADDR+0x01400000)"

CFLAGS += -mlittle-endian -mfpu=neon
LDFLAGS += -EL
DEVS += fbcon

MODULES += \
	dev/fbcon \
	dev/battery \
	dev/keys \
	lib/debug \
	lib/ptable \
	lib/devinfo
#	lib/fs \
#	lib/bcache \
#	lib/bio

DEFINES += \
	WITH_CPU_EARLY_INIT=$(WITH_CPU_EARLY_INIT)\
	WITH_CPU_WARM_BOOT=$(WITH_CPU_WARM_BOOT) \
	MEMBASE=$(MEMBASE)\
	MEMSIZE=$(MEMSIZE) \
	BASE_ADDR=$(BASE_ADDR) \
	TAGS_ADDR=$(TAGS_ADDR) \
	WSPL_VADDR=$(WSPL_VADDR) \
	KERNEL_ADDR=$(KERNEL_ADDR) \
	RAMDISK_ADDR=$(RAMDISK_ADDR) \
	SCRATCH_ADDR=$(SCRATCH_ADDR) \
	DISPLAY_TYPE_LCDC=$(DISPLAY_TYPE_LCDC)

OBJS += \
	$(LOCAL_DIR)/acpuclock.o \
	$(LOCAL_DIR)/board_htcleo.o \
	$(LOCAL_DIR)/clock.o \
	$(LOCAL_DIR)/gpio_keys.o \
	$(LOCAL_DIR)/hsusb.o \
	$(LOCAL_DIR)/htcleo_ts.o \
	$(LOCAL_DIR)/init.o \
	$(LOCAL_DIR)/lcdc.o \
	$(LOCAL_DIR)/microp.o \
	$(LOCAL_DIR)/nand.o \
	$(LOCAL_DIR)/platform.o \
	$(LOCAL_DIR)/dex_vreg.o \
	$(LOCAL_DIR)/dex_comm.o
#	$(LOCAL_DIR)/htcleo_ls.o \
	