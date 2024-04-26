# makefile-win includes local-win.mk, so create a copy of this file
# and call it local-win.mk, then make any desired changes.

# Change the next 2 lines to specify where you installed wxWidgets:
!include <C:/wxWidgets-3.1.5/build/msw/config.vc>
WX_DIR = C:\wxWidgets-3.1.5

# Change the next line to match your wxWidgets version (first two digits):
WX_RELEASE = 31

# Change the next line depending on where you installed Python:
PYTHON_INCLUDE = -I"C:\Python39-64\include"

# Comment out the next line to disable sound play:
ENABLE_SOUND = 1

# Add any extra CXX flags here
EXTRACXXFLAGS =
