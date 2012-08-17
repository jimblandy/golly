Golly
=====

This repository is a clone of the Golly Game of Life simulator, on [SourceForge][]. Golly is pretty great; for details, see [src/README][readme].

  [sourceforge]: http://sourceforge.net/projects/golly/
  [readme]: golly/tree/master/src/README

Building on Fedora 17
---------------------

I needed to install the following packages to build Golly on Fedora 17:

  $ yum install wxGTK-devel perl-ExtUtils-Embed zlib-static

However, that's probably not a complete list of everything you'd need: I
already have a lot of development packages installed, and I don't know
which of those Golly also depends on.
