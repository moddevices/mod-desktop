#!/usr/bin/make -f

export DISTRHO_NAMESPACE = DesktopDISTRHO
export DGL_NAMESPACE = DesktopDGL
export FILE_BROWSER_DISABLED = true
export NVG_FONT_TEXTURE_FLAGS = NVG_IMAGE_NEAREST

include ../DPF/Makefile.base.mk

# ---------------------------------------------------------------------------------------------------------------------
# Project name, used for binaries

NAME = mod-desktop

# ---------------------------------------------------------------------------------------------------------------------
# Files to build

FILES_DSP = \
	DesktopPlugin.cpp \
	utils.cpp \
	zita-resampler/resampler.cc \
	zita-resampler/resampler-table.cc

FILES_UI = DesktopUI.cpp NanoButton.cpp utils.cpp

ifeq ($(MACOS),true)
FILES_UI += WebViewWK.mm
else ifeq ($(WINDOWS),true)
FILES_UI += WebViewCHOC.cpp
else
FILES_UI += WebViewX11.cpp
endif

# ---------------------------------------------------------------------------------------------------------------------
# Do some magic

DPF_BUILD_DIR = ../../build-plugin/build
DPF_TARGET_DIR = ../../build-plugin
USING_WEBVIEW = true

include ../DPF/Makefile.plugins.mk

TARGETS = features au clap lv2_sep vst2 vst3

BUILD_CXX_FLAGS += -DVERSION='"$(shell cat ../../VERSION)"'
BUILD_CXX_FLAGS += -pthread

ifeq ($(MACOS),true)
else ifeq ($(WINDOWS),true)
BUILD_CXX_FLAGS += -I../CHOC
else
BUILD_CXX_FLAGS += $(shell $(PKG_CONFIG) --cflags Qt5Core)
# -std=gnu++14
endif

LINK_FLAGS += -pthread

ifeq ($(MACOS),true)
LINK_FLAGS += -framework CoreFoundation -framework IOKit -framework WebKit
else ifeq ($(WINDOWS),true)
LINK_FLAGS += -lole32 -luuid -lwinmm
else
LINK_FLAGS += -ldl -lrt
endif

$(BUILD_DIR)/WebViewCHOC.cpp.o: BUILD_CXX_FLAGS += -std=gnu++17

# ---------------------------------------------------------------------------------------------------------------------

all: $(TARGETS)
