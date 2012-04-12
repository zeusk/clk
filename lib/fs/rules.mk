LOCAL_DIR := $(GET_LOCAL_DIR)

OBJS += \
	$(LOCAL_DIR)/fs.o

include lib/fs/vfat/rules.mk
