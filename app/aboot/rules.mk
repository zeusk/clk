LOCAL_DIR := $(GET_LOCAL_DIR)

INCLUDES += \
		-I$(LOCAL_DIR)/include

OBJS += \
	$(LOCAL_DIR)/aboot.o \
	$(LOCAL_DIR)/fastboot.o \
	$(LOCAL_DIR)/recovery.o

