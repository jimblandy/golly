# Change the next 2 lines to specify where you installed wxWidgets:
!include </wxWidgets-2.8.7-32/build/msw/config.vc>
WX_DIR = c:\wxWidgets-2.8.7-32

# Change the next line to match your wxWidgets version (first two digits):
WX_RELEASE = 28

# Change the next line depending on where you installed Python:
PYTHON_INCLUDE = -I"C:\Python25-32\include"

# Simplified output from "perl -MExtUtils::Embed -e ccopts":
PERL_INCLUDE = \
-DHAVE_DES_FCRYPT -DNO_HASH_SEED -DUSE_SITECUSTOMIZE -DPERL_IMPLICIT_CONTEXT \
-DPERL_IMPLICIT_SYS -DUSE_PERLIO -DPERL_MSVCRT_READFIX -I"C:\Perl\lib\CORE"
