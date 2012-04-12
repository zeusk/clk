# top level project rules for the qsd8250_surf project
#
LOCAL_DIR := $(GET_LOCAL_DIR)

TARGET := htcleo
# if basic sd driver is completed, set to 1
WITH_SDC_DRIVER := 0

MODULES += app/aboot

#DEFINES += WITH_DEBUG_DCC=1
#DEFINES += WITH_DEBUG_UART=1
DEFINES += WITH_DEBUG_FBCON=1
