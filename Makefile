BOARD := canable_v2
COMPONENTS_PATH := $(CURDIR)/components
TINYUSB_PATH := $(COMPONENTS_PATH)/tinyusb
include $(TINYUSB_PATH)/examples/make.mk

# Useful info
$(info BOARD = $(BOARD))
$(info TINYUSB_PATH = $(TINYUSB_PATH))
$(info TOP = $(TOP))
$(info BOARD_PATH = $(BOARD_PATH))
$(info FAMILY = $(FAMILY))
$(info FAMILY_PATH = $(FAMILY_PATH))
$(info CPU_CORE = $(CPU_CORE))

INC += \
    main \
    $(TOP)/hw \

SRC_C += \
    $(CURDIR)/main/main.c \
    $(CURDIR)/main/usb_descriptors.c \

include $(TINYUSB_PATH)/examples/rules.mk