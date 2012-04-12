LOCAL_DIR := $(GET_LOCAL_DIR)

INCLUDES += \
		-I$(LOCAL_DIR)/include

OBJS += \
	$(LOCAL_DIR)/fat_table.o \
	$(LOCAL_DIR)/fat_filelib.o \
	$(LOCAL_DIR)/fat_access.o \
	$(LOCAL_DIR)/fat_cache.o \
	$(LOCAL_DIR)/fat_format.o \
	$(LOCAL_DIR)/fat_misc.o \
	$(LOCAL_DIR)/fat_string.o \
	$(LOCAL_DIR)/fat_write.o 

