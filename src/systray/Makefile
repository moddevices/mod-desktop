#!/usr/bin/make -f

CC ?= gcc
CXX ?= g++
PKG_CONFIG ?= pkg-config

CXXFLAGS += -fPIC -std=gnu++14 -I. -DVERSION='"$(shell cat ../../VERSION)"'

TARGET_MACHINE := $(shell $(CC) -dumpmachine)

# ---------------------------------------------------------------------------------------------------------------------

ifneq (,$(findstring linux,$(TARGET_MACHINE)))
LINUX = true
WINDOWS = false
PAWPAW_TARGET = linux-$(shell uname -m)
else ifneq (,$(findstring apple,$(TARGET_MACHINE)))
MACOS = true
WINDOWS = false
PAWPAW_TARGET = macos-universal-10.15
else ifneq (,$(findstring mingw,$(TARGET_MACHINE)))
WINDOWS = true
PAWPAW_TARGET = win64
else ifneq (,$(findstring wasm,$(TARGET_MACHINE)))
WASM = true
WINDOWS = false
PAWPAW_TARGET = wasm
else
$(error unknown target, cannot continue)
endif

# ---------------------------------------------------------------------------------------------------------------------

PAWPAW_DIR = ~/PawPawBuilds
PAWPAW_PREFIX = $(PAWPAW_DIR)/targets/$(PAWPAW_TARGET)

# ---------------------------------------------------------------------------------------------------------------------

ifeq ($(WINDOWS),true)
TARGET = mod-app.exe
EXTRAS = Qt5Core.dll Qt5Gui.dll Qt5Svg.dll Qt5Widgets.dll
LDFLAGS  += -Wl,-subsystem,windows -ldwmapi
WINDRES   = $(subst gcc,windres,$(CC))
else
TARGET = mod-app
endif

ifeq ($(LINUX),true)
CXXFLAGS += $(shell $(PKG_CONFIG) --cflags alsa)
LDFLAGS  += $(shell $(PKG_CONFIG) --libs alsa)
endif

ifeq ($(MACOS),true)
LDFLAGS  += -framework CoreAudio
else
CXXFLAGS += $(shell $(PKG_CONFIG) --cflags portaudio-2.0)
LDFLAGS  += $(shell $(PKG_CONFIG) --libs portaudio-2.0)
endif

ifneq ($(MACOS)$(WINDOWS),true)
LDFLAGS += -ldl
endif

ifeq ($(shell $(PKG_CONFIG) --exists Qt5Core Qt5Gui Qt5Svg Qt5Widgets && echo true)$(WINDOWS),truefalse)
QT5_HOSTBINS = $(shell $(PKG_CONFIG) --variable=host_bins Qt5Core)
QT5_FLAGS    = $(shell $(PKG_CONFIG) --cflags Qt5Core Qt5Gui Qt5Svg Qt5Widgets)
QT5_LIBS     = $(shell $(PKG_CONFIG) --libs Qt5Core Qt5Gui Qt5Svg Qt5Widgets)
else
QT5_PREFIX   = $(shell $(PKG_CONFIG) --variable=prefix Qt5OpenGLExtensions)
QT5_HOSTBINS = $(QT5_PREFIX)/bin
QT5_FLAGS    = -DQT_CORE_LIB -DQT_GUI_LIB -DQT_SVG_LIB -DQT_WIDGETS_LIB -I$(QT5_PREFIX)/include/qt5
ifeq ($(MACOS),true)
QT5_LIBS     = -F$(QT5_PREFIX)/lib -framework QtCore -framework QtGui -framework QtSvg -framework QtWidgets
else
QT5_LIBS     = -L$(QT5_PREFIX)/lib -lQt5Core -lQt5Gui -lQt5Svg -lQt5Widgets
endif
endif

OBJS  = main.cpp.o

ifeq ($(WINDOWS),true)
OBJS += mod-app.rc.o
endif

# ---------------------------------------------------------------------------------------------------------------------

all: $(TARGET) $(EXTRAS)

clean:
	rm -f $(TARGET) *.dll *.exe *.o qrc_mod-app.hpp ui_mod-app.hpp

$(TARGET): $(OBJS)
	@$(shell mkdir -p $(@D))
	$(CXX) $^ $(LDFLAGS) $(QT5_LIBS) -o $@
ifeq ($(MACOS),true)
	install_name_tool -change "@rpath/QtCore.framework/Versions/5/QtCore" "@executable_path/../Frameworks/QtCore.framework/QtCore" $@
	install_name_tool -change "@rpath/QtGui.framework/Versions/5/QtGui" "@executable_path/../Frameworks/QtGui.framework/QtGui" $@
	install_name_tool -change "@rpath/QtOpenGL.framework/Versions/5/QtOpenGL" "@executable_path/../Frameworks/QtOpenGL.framework/QtOpenGL" $@
	install_name_tool -change "@rpath/QtPrintSupport.framework/Versions/5/QtPrintSupport" "@executable_path/../Frameworks/QtPrintSupport.framework/QtPrintSupport" $@
	install_name_tool -change "@rpath/QtSvg.framework/Versions/5/QtSvg" "@executable_path/../Frameworks/QtSvg.framework/QtSvg" $@
	install_name_tool -change "@rpath/QtWidgets.framework/Versions/5/QtWidgets" "@executable_path/../Frameworks/QtWidgets.framework/QtWidgets" $@
endif

main.cpp.o: main.cpp mod-app.hpp qrc_mod-app.hpp ui_mod-app.hpp widgets.hpp
	$(CXX) $< $(CXXFLAGS) $(QT5_FLAGS) -c -o $@

mod-app.rc.o: mod-app.rc
	$(WINDRES) $< -O coff -o $@

qrc_mod-app.hpp: mod-app.qrc
	$(QT5_HOSTBINS)/rcc $< -o $@

ui_mod-app.hpp: mod-app.ui
	$(QT5_HOSTBINS)/uic $< -o $@

Qt5%.dll: $(PAWPAW_PREFIX)/bin/Qt5%.dll
	ln -sf $< $@

# ---------------------------------------------------------------------------------------------------------------------