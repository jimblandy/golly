# makefile-gtk includes local-gtk.mk, so create a copy of this file
# and call it local-gtk.mk, then make any desired changes.

# Change the next line to specify which wx-config program to use.
# This allows to switch to different versions of wxGTK.
WX_CONFIG = wx-config

# Change the next command depending on your Python installation:
PYTHON = python3

# Uncomment the next line to enable Golly to run perl scripts.
# ENABLE_PERL = 1

# Comment out the next line to disable sound play:
ENABLE_SOUND = 1

# Add any extra CXX and LD flags here
# CXXFLAGS =
# LDFLAGS =
