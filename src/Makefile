NAME = Listi
TYPE = APP
SRCS = SevenZipBackend.cpp ArchivePasswords.cpp ArchivePasswordWindow.cpp ArchiveSettings.cpp ArchiveJobs.cpp ArchiveWindow.cpp JobSnapshot.cpp FileContextMenu.cpp NamedEntry.cpp FileDrop.cpp OpenWith.cpp OpenWithWindow.cpp FileProperties.cpp PropertiesWindow.cpp FileClipboard.cpp MessageStore.cpp SavedSearches.cpp SearchWorker.cpp FileSearch.cpp SearchWindow.cpp SavedPlaces.cpp App.cpp Pane.cpp PaneModel.cpp FileDetails.cpp FileGrid.cpp DirectoryScanner.cpp FileOperation.cpp Trash.cpp OperationQueue.cpp Preferences.cpp SelfTest.cpp VirtualFile.cpp ArchiveFile.cpp LibarchiveBackend.cpp NetworkFile.cpp VirtualTest.cpp VirtualOperation.cpp ConnectionView.cpp InlineAction.cpp ImagePreview.cpp FolderTree.cpp
LIBS = shared translation be columnlistview tracker $(STDCPPLIBS)
RDEFS = Lister.rdef
OPTIMIZE := FULL
WARNINGS = ALL
COMPILER_FLAGS = -std=c++17 -Wno-multichar
SYSTEM_INCLUDE_PATHS = /boot/system/develop/headers/samba-4.0 /boot/system/develop/headers/private/interface /boot/system/develop/headers/private/shared
DEVEL_DIRECTORY := $(shell findpaths -r makefile_engine B_FIND_PATH_DEVELOP_DIRECTORY)
include $(DEVEL_DIRECTORY)/etc/makefile-engine
