# makefile-gtk includes local-gtk.mk, so create a copy of this file
# and call it local-gtk.mk, then make any desired changes.

# Change the next lines to specify which wx-config program to use.
# This allows to switch to different versions of wxGTK.
WX_CONFIG = wx-config

# Change the next line depending on where you installed Python:
PYTHON = python2

# Uncomment the next line to enable Golly to run perl scripts.
# ENABLE_PERL = 1

# Uncomment the next line to allow Golly to play sounds:
# ENABLE_SOUND = 1
# Change the next line to specify where you installed IrrKLang
IRRKLANGDIR = /home/chris/irrKlang-64bit-1.6.0

# Add any extra CXX flags here
EXTRACXXFLAGS = 
