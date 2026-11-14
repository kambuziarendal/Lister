NAME = Lister
TYPE = APP
SRCS = App.cpp Pane.cpp FileOperation.cpp Trash.cpp OperationQueue.cpp Preferences.cpp SelfTest.cpp
LIBS = be columnlistview tracker $(STDCPPLIBS)
RDEFS = Lister.rdef
OPTIMIZE := FULL
WARNINGS = ALL
COMPILER_FLAGS = -std=c++17 -Wno-multichar
SYSTEM_INCLUDE_PATHS = /boot/system/develop/headers/private/interface
DEVEL_DIRECTORY := $(shell findpaths -r makefile_engine B_FIND_PATH_DEVELOP_DIRECTORY)
include $(DEVEL_DIRECTORY)/etc/makefile-engine
