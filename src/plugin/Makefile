#!/usr/bin/make -f

include ../DPF/Makefile.base.mk

# ---------------------------------------------------------------------------------------------------------------------
# Project name, used for binaries

NAME = MOD-Desktop

# ---------------------------------------------------------------------------------------------------------------------
# Files to build

FILES_DSP = \
	DesktopPlugin.cpp \
	zita-resampler/resampler.cc \
	zita-resampler/resampler-table.cc

FILES_UI  = \
	DesktopUI.cpp

# ---------------------------------------------------------------------------------------------------------------------
# Do some magic

include ../DPF/Makefile.plugins.mk

all: jack

# ---------------------------------------------------------------------------------------------------------------------