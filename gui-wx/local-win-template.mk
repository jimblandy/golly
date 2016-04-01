# makefile-win includes local-win.mk, so create a copy of this file
# and call it local-win.mk, then make any desired changes.

# Change the next 2 lines to specify where you installed wxWidgets:
!include <C:/wxWidgets-2.8.12-64/build/msw/config.vc>
WX_DIR = C:\wxWidgets-2.8.12-64

# Change the next line to match your wxWidgets version (first two digits):
WX_RELEASE = 28

LUA_DEFS = -DLUA_COMPAT_5_2
# Uncomment the next line if building a 32-bit version of Golly:
# LUA_DEFS = -DLUA_COMPAT_5_2 -DLUA_32BITS


# Change the next line depending on where you installed Python:
PYTHON_INCLUDE = -I"C:\Python27-64\include"

# Uncomment the next 4 lines to allow Golly to run Perl scripts:
# PERL_INCLUDE = \
# -DENABLE_PERL \
# -DHAVE_DES_FCRYPT -DNO_HASH_SEED -DUSE_SITECUSTOMIZE -DPERL_IMPLICIT_CONTEXT \
# -DPERL_IMPLICIT_SYS -DUSE_PERLIO -DPERL_MSVCRT_READFIX -I"C:\Perl514-64\lib\CORE"
