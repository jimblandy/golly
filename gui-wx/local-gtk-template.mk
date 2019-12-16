# makefile-gtk includes local-gtk.mk, so create a copy of this file
# and call it local-gtk.mk, then make any desired changes.

# Change the next lines to specify which wx-config program to use.
# This allows to switch to different versions of wxGTK.
WX_CONFIG = wx-config

# Lua
LUA_DEFS = -DLUA_COMPAT_5_2
# Uncomment the next line if building a 32-bit version of Golly:
# LUA_DEFS = -DLUA_COMPAT_5_2 -DLUA_32BITS

# Change the next line depending on where you installed Python:
PYTHON = python2

# Uncomment the next line to enable Golly to run perl scripts.
# ENABLE_PERL = 1

# Uncomment the next line to allow Golly to play sounds:
# ENABLE_SOUND = 1
# Change the next line to specify where you installed IrrKLang
IRRKLANGDIR = /home/chris/irrKlang-64bit-1.6.0

# The following is used to deal with different license paths and
# filenames in different distributions.
# This one is for debian-based systems.
# Let's just hope this is portable somehow...
COPYLICENSECMD = \
	find /usr/share/doc/*/copyright \
	| grep  "$$( \
		for f in ../.local/lib/*; do basename $$f ; done \
		| sed -e 's|-x11||' -e 's|gmodule|glib|' -e 's|X|x|' \
		-e 's|-|-\\{0,1\\}|' -e 's|\.so\.|-\\{0,1\\}|' \
		-e 's|$$|/|' \
	)" | while read -r copyright; do \
		dest="$$( \
			echo $$copyright \
			| sed -e 's|/usr/share/doc|$(LICENSEDIR)|' \
		)"; \
		destdir="$$(dirname $$dest)"; \
		echo "copying $$copyright to\t$$dest..."; \
		mkdir -p "$$destdir" && cp "$$copyright" "$$dest"; \
	done; \
	# mkdir -p $(LICENSEDIR)/wxbase3.0-0 $(LICENSEDIR)/wxgtk3.0-0; \
	cp /usr/share/doc/wxbase3.0-0/copyright $(LICENSEDIR)/wxbase3.0-0; \
	cp /usr/share/doc/wxgtk3.0-0/copyright $(LICENSEDIR)/wxgtk3.0-0; \
	# ^^^ Delete '#' if you installed wxwidgets from a package manager, \
	# and edit the package versions appropriately.

# Add any extra CXX and LD flags here
# CXXFLAGS =
# LDFLAGS =
