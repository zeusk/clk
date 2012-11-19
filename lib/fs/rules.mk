LOCAL_DIR := $(GET_LOCAL_DIR)

#MODULES += \
#	lib/fs/ext2

OBJS += \
	$(LOCAL_DIR)/fs.o

include lib/fs/fat/rules.mk
include lib/fs/ext2/rules.mk
