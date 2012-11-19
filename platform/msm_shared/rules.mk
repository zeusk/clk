LOCAL_DIR := $(GET_LOCAL_DIR)

INCLUDES += \
		-I$(LOCAL_DIR)/include

DEFINES += $(TARGET_XRES)
DEFINES += $(TARGET_YRES)

OBJS += \
	$(LOCAL_DIR)/board.o \
	$(LOCAL_DIR)/timer.o \
	$(LOCAL_DIR)/debug.o \
	$(LOCAL_DIR)/smem.o \
	$(LOCAL_DIR)/smem_ptable.o \
	$(LOCAL_DIR)/jtag_hook.o \
	$(LOCAL_DIR)/jtag.o \
	$(LOCAL_DIR)/uart.o \
	$(LOCAL_DIR)/adm.o \
	$(LOCAL_DIR)/i2c_qup.o \
	$(LOCAL_DIR)/mmc.o \
	$(LOCAL_DIR)/msm_i2c.o \
	$(LOCAL_DIR)/pcom.o \
	$(LOCAL_DIR)/pcom_clients.o
