#
# This make variable must be set before the demos or examples
# can be built.  It must be set to either dm355 or dm6446
#

MVTOOL_DIR := /home/pamsimochen/workdir/toolchain/montavista/pro/devkit/arm/v5t_le

BASE_HOME := /home/pamsimochen/arable_land/acs1910

HOME := $(BASE_HOME)/ipnc
DVSDK_BASE_DIR := $(BASE_HOME)/dvsdk_2_10_01_18

TARGET_FS := $(HOME)/target/filesys
KERNELDIR := $(HOME)/ti-davinci
BASE_DIR := $(HOME)/av_capture/build
TFTP_HOME := $(HOME)/tftp

#SYSTEM := DVR
#SYSTEM := EVM
SYSTEM := IPNC

#HARDWARE :=DM365
HARDWARE :=DM368

CONFIG :=

include $(DVSDK_BASE_DIR)/Rules.make


# Where to copy the resulting executables and data to (when executing 'make
# install') in a proper file structure. This EXEC_DIR should either be visible
# from the target, or you will have to copy this (whole) directory onto the
# target filesystem.
EXEC_DIR=$(TARGET_FS)/opt/ipnc

# The directory that points to the IPNC software package
IPNC_DIR=$(HOME)/ipnc_app

# The directory to application interface
IPNC_INTERFACE_DIR=$(IPNC_DIR)/interface

# The directory to application interface include files
PUBLIC_INCLUDE_DIR=$(IPNC_INTERFACE_DIR)/inc

# The directory to application interface library files
APP_LIB_DIR=$(IPNC_INTERFACE_DIR)/lib

# The directory to root file system
ROOT_FILE_SYS = $(TARGET_FS)

AEWB_LIBS :=

ifeq ($(SYSTEM), DVR)
BOARD_ID := BOARD_UD_DVR
IMGS_ID := IMGS_NONE
TARGET_FS_DIR := $(TARGET_FS)/opt/dvr
AEWB_ID := AEWB_NONE
endif

ifeq ($(SYSTEM), IPNC)
BOARD_ID := BOARD_AP_IPNC
IMGS_ID := IMGS_MICRON_MT9P031_5MP
TARGET_FS_DIR := $(TARGET_FS)/opt/ipnc
AEWB_ID := AEWB_ENABLE
endif

ifeq ($(SYSTEM), EVM)
BOARD_ID := BOARD_TI_EVM
IMGS_ID := IMGS_MICRON_MT9P031_5MP
#IMGS_ID := IMGS_MICRON_MT9D131_2MP
#IMGS_ID := IMGS_TVP514X
TARGET_FS_DIR := $(TARGET_FS)/opt/ipnc
AEWB_ID := AEWB_ENABLE
endif

export IPNC_DIR
export PUBLIC_INCLUDE_DIR
export APP_LIB_DIR
export TARGET_FS
export SYSTEM
export LINUXKERNEL_INSTALL_DIR
export EXEC_DIR
export TARGET_FS_DIR
export KERNELDIR
export BASE_DIR
export CONFIG
export DVSDK_BASE_DIR
export CMEM_INSTALL_DIR
export MVTOOL_PREFIX
export IMGS_ID
export BOARD_ID
export DVSDK_DEMOS_DIR
export AEWB_ID
export AEWB_LIBS
export MVTOOL_DIR
