# Makefile for creating web app version of Golly using Emscripten.
# Type "emmake make" to build golly.html and golly.js.

SOURCES = main.cpp webcalls.cpp \
    ../gollybase/bigint.cpp \
    ../gollybase/generationsalgo.cpp \
    ../gollybase/ghashbase.cpp \
    ../gollybase/ghashdraw.cpp \
    ../gollybase/hlifealgo.cpp \
    ../gollybase/hlifedraw.cpp \
    ../gollybase/jvnalgo.cpp \
    ../gollybase/lifealgo.cpp \
    ../gollybase/lifepoll.cpp \
    ../gollybase/liferender.cpp \
    ../gollybase/liferules.cpp \
    ../gollybase/qlifealgo.cpp \
    ../gollybase/qlifedraw.cpp \
    ../gollybase/readpattern.cpp \
    ../gollybase/ruleloaderalgo.cpp \
    ../gollybase/ruletable_algo.cpp \
    ../gollybase/ruletreealgo.cpp \
    ../gollybase/util.cpp \
    ../gollybase/viewport.cpp \
    ../gollybase/writepattern.cpp \
    ../gui-common/algos.cpp \
    ../gui-common/control.cpp \
    ../gui-common/file.cpp \
    ../gui-common/layer.cpp \
    ../gui-common/prefs.cpp \
    ../gui-common/render.cpp \
    ../gui-common/select.cpp \
    ../gui-common/status.cpp \
    ../gui-common/undo.cpp \
    ../gui-common/utils.cpp \
    ../gui-common/view.cpp \
    ../gui-common/MiniZip/ioapi.c \
    ../gui-common/MiniZip/unzip.c \
    ../gui-common/MiniZip/zip.c

OBJECTS = main.o webcalls.o \
    ../gollybase/bigint.o \
    ../gollybase/generationsalgo.o \
    ../gollybase/ghashbase.o \
    ../gollybase/ghashdraw.o \
    ../gollybase/hlifealgo.o \
    ../gollybase/hlifedraw.o \
    ../gollybase/jvnalgo.o \
    ../gollybase/lifealgo.o \
    ../gollybase/lifepoll.o \
    ../gollybase/liferender.o \
    ../gollybase/liferules.o \
    ../gollybase/qlifealgo.o \
    ../gollybase/qlifedraw.o \
    ../gollybase/readpattern.o \
    ../gollybase/ruleloaderalgo.o \
    ../gollybase/ruletable_algo.o \
    ../gollybase/ruletreealgo.o \
    ../gollybase/util.o \
    ../gollybase/viewport.o \
    ../gollybase/writepattern.o \
    ../gui-common/algos.o \
    ../gui-common/control.o \
    ../gui-common/file.o \
    ../gui-common/layer.o \
    ../gui-common/prefs.o \
    ../gui-common/render.o \
    ../gui-common/select.o \
    ../gui-common/status.o \
    ../gui-common/undo.o \
    ../gui-common/utils.o \
    ../gui-common/view.o \
    ../gui-common/MiniZip/ioapi.o \
    ../gui-common/MiniZip/unzip.o \
    ../gui-common/MiniZip/zip.o

# note that using -O2 results in a much smaller golly.js and no .map file
CXXFLAGS = -I. -I../gollybase -I../gui-common -I../gui-common/MiniZip -DWEB_GUI \
    -Wall -Wextra -Wno-ignored-qualifiers -Wno-unused-parameter -O2

TARGET = golly.html

all: $(TARGET)

$(TARGET): $(OBJECTS) shell.html
	$(CXX) $(CXXFLAGS) --shell-file shell.html -o $@ $(LDFLAGS) $(OBJECTS)

clean:
	$(RM) $(OBJECTS) $(TARGET) golly.js

depend:
	@$(CXX) $(CXXFLAGS) -MM $(SOURCES)

# type "emmake make depend" to generate these dependencies:

main.o: main.cpp ../gollybase/bigint.h ../gollybase/lifealgo.h \
  ../gollybase/viewport.h ../gollybase/liferender.h \
  ../gollybase/lifepoll.h ../gollybase/readpattern.h \
  ../gollybase/platform.h ../gollybase/qlifealgo.h \
  ../gollybase/liferules.h ../gollybase/hlifealgo.h \
  ../gollybase/jvnalgo.h ../gollybase/ghashbase.h \
  ../gollybase/generationsalgo.h ../gollybase/ruleloaderalgo.h \
  ../gollybase/ruletable_algo.h ../gollybase/ruletreealgo.h \
  ../gui-common/algos.h ../gui-common/utils.h ../gui-common/prefs.h \
  ../gui-common/layer.h ../gui-common/select.h ../gui-common/control.h \
  ../gui-common/file.h ../gollybase/writepattern.h ../gui-common/view.h \
  ../gui-common/status.h ../gui-common/render.h webcalls.h
webcalls.o: webcalls.cpp ../gui-common/utils.h ../gui-common/algos.h \
  ../gollybase/lifealgo.h ../gollybase/bigint.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h \
  ../gui-common/prefs.h ../gui-common/layer.h ../gui-common/select.h \
  ../gui-common/control.h ../gui-common/file.h \
  ../gollybase/writepattern.h ../gui-common/view.h \
  ../gui-common/status.h ../gui-common/undo.h webcalls.h
bigint.o: ../gollybase/bigint.cpp ../gollybase/bigint.h \
  ../gollybase/util.h
generationsalgo.o: ../gollybase/generationsalgo.cpp \
  ../gollybase/generationsalgo.h ../gollybase/ghashbase.h \
  ../gollybase/lifealgo.h ../gollybase/bigint.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h \
  ../gollybase/liferules.h
ghashbase.o: ../gollybase/ghashbase.cpp ../gollybase/ghashbase.h \
  ../gollybase/lifealgo.h ../gollybase/bigint.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h \
  ../gollybase/liferules.h ../gollybase/util.h
ghashdraw.o: ../gollybase/ghashdraw.cpp ../gollybase/ghashbase.h \
  ../gollybase/lifealgo.h ../gollybase/bigint.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h \
  ../gollybase/liferules.h ../gollybase/util.h
hlifealgo.o: ../gollybase/hlifealgo.cpp ../gollybase/hlifealgo.h \
  ../gollybase/lifealgo.h ../gollybase/bigint.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h \
  ../gollybase/liferules.h ../gollybase/util.h
hlifedraw.o: ../gollybase/hlifedraw.cpp ../gollybase/hlifealgo.h \
  ../gollybase/lifealgo.h ../gollybase/bigint.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h \
  ../gollybase/liferules.h
jvnalgo.o: ../gollybase/jvnalgo.cpp ../gollybase/jvnalgo.h \
  ../gollybase/ghashbase.h ../gollybase/lifealgo.h ../gollybase/bigint.h \
  ../gollybase/viewport.h ../gollybase/liferender.h \
  ../gollybase/lifepoll.h ../gollybase/readpattern.h \
  ../gollybase/platform.h ../gollybase/liferules.h
lifealgo.o: ../gollybase/lifealgo.cpp ../gollybase/lifealgo.h \
  ../gollybase/bigint.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h
lifepoll.o: ../gollybase/lifepoll.cpp ../gollybase/lifepoll.h \
  ../gollybase/util.h
liferender.o: ../gollybase/liferender.cpp ../gollybase/liferender.h
liferules.o: ../gollybase/liferules.cpp ../gollybase/liferules.h \
  ../gollybase/lifealgo.h ../gollybase/bigint.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h ../gollybase/util.h
qlifealgo.o: ../gollybase/qlifealgo.cpp ../gollybase/qlifealgo.h \
  ../gollybase/lifealgo.h ../gollybase/bigint.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h \
  ../gollybase/liferules.h ../gollybase/util.h
qlifedraw.o: ../gollybase/qlifedraw.cpp ../gollybase/qlifealgo.h \
  ../gollybase/lifealgo.h ../gollybase/bigint.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h \
  ../gollybase/liferules.h ../gollybase/util.h
readpattern.o: ../gollybase/readpattern.cpp ../gollybase/readpattern.h \
  ../gollybase/bigint.h ../gollybase/lifealgo.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/platform.h ../gollybase/liferules.h ../gollybase/util.h
ruleloaderalgo.o: ../gollybase/ruleloaderalgo.cpp \
  ../gollybase/ruleloaderalgo.h ../gollybase/ghashbase.h \
  ../gollybase/lifealgo.h ../gollybase/bigint.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h \
  ../gollybase/liferules.h ../gollybase/ruletable_algo.h \
  ../gollybase/ruletreealgo.h ../gollybase/util.h
ruletable_algo.o: ../gollybase/ruletable_algo.cpp \
  ../gollybase/ruletable_algo.h ../gollybase/ghashbase.h \
  ../gollybase/lifealgo.h ../gollybase/bigint.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h \
  ../gollybase/liferules.h ../gollybase/util.h
ruletreealgo.o: ../gollybase/ruletreealgo.cpp ../gollybase/ruletreealgo.h \
  ../gollybase/ghashbase.h ../gollybase/lifealgo.h ../gollybase/bigint.h \
  ../gollybase/viewport.h ../gollybase/liferender.h \
  ../gollybase/lifepoll.h ../gollybase/readpattern.h \
  ../gollybase/platform.h ../gollybase/liferules.h ../gollybase/util.h
util.o: ../gollybase/util.cpp ../gollybase/util.h
viewport.o: ../gollybase/viewport.cpp ../gollybase/viewport.h \
  ../gollybase/bigint.h ../gollybase/lifealgo.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h
writepattern.o: ../gollybase/writepattern.cpp ../gollybase/writepattern.h \
  ../gollybase/lifealgo.h ../gollybase/bigint.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h ../gollybase/util.h
algos.o: ../gui-common/algos.cpp ../gollybase/bigint.h \
  ../gollybase/lifealgo.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h \
  ../gollybase/qlifealgo.h ../gollybase/liferules.h \
  ../gollybase/hlifealgo.h ../gollybase/jvnalgo.h \
  ../gollybase/ghashbase.h ../gollybase/generationsalgo.h \
  ../gollybase/ruleloaderalgo.h ../gollybase/ruletable_algo.h \
  ../gollybase/ruletreealgo.h ../gui-common/utils.h \
  ../gui-common/prefs.h ../gui-common/layer.h ../gui-common/algos.h \
  ../gui-common/select.h
control.o: ../gui-common/control.cpp ../gollybase/bigint.h \
  ../gollybase/lifealgo.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h \
  ../gollybase/qlifealgo.h ../gollybase/liferules.h \
  ../gollybase/hlifealgo.h ../gollybase/writepattern.h \
  ../gollybase/util.h ../gui-common/utils.h ../gui-common/prefs.h \
  ../gui-common/status.h ../gui-common/file.h ../gui-common/algos.h \
  ../gui-common/layer.h ../gui-common/select.h ../gui-common/view.h \
  ../gui-common/undo.h ../gui-common/control.h webcalls.h
file.o: ../gui-common/file.cpp webcalls.h ../gui-common/MiniZip/zip.h \
  ../gui-common/MiniZip/ioapi.h ../gui-common/MiniZip/unzip.h \
  ../gollybase/bigint.h ../gollybase/lifealgo.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h \
  ../gollybase/qlifealgo.h ../gollybase/liferules.h \
  ../gollybase/hlifealgo.h ../gollybase/writepattern.h \
  ../gui-common/utils.h ../gui-common/prefs.h ../gui-common/status.h \
  ../gui-common/algos.h ../gui-common/layer.h ../gui-common/select.h \
  ../gui-common/control.h ../gui-common/view.h ../gui-common/undo.h \
  ../gui-common/file.h
layer.o: ../gui-common/layer.cpp ../gollybase/bigint.h \
  ../gollybase/lifealgo.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h \
  ../gollybase/qlifealgo.h ../gollybase/liferules.h \
  ../gollybase/hlifealgo.h ../gollybase/util.h ../gui-common/utils.h \
  ../gui-common/prefs.h ../gui-common/algos.h ../gui-common/control.h \
  ../gui-common/select.h ../gui-common/file.h \
  ../gollybase/writepattern.h ../gui-common/view.h ../gui-common/undo.h \
  ../gui-common/layer.h
prefs.o: ../gui-common/prefs.cpp ../gollybase/lifealgo.h \
  ../gollybase/bigint.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h ../gollybase/util.h \
  ../gui-common/utils.h ../gui-common/status.h ../gui-common/algos.h \
  ../gui-common/layer.h ../gui-common/select.h ../gui-common/prefs.h \
  webcalls.h
render.o: ../gui-common/render.cpp ../gollybase/bigint.h \
  ../gollybase/lifealgo.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h \
  ../gui-common/utils.h ../gui-common/prefs.h ../gui-common/algos.h \
  ../gui-common/layer.h ../gui-common/select.h ../gui-common/view.h \
  ../gui-common/render.h
select.o: ../gui-common/select.cpp ../gollybase/bigint.h \
  ../gollybase/lifealgo.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h \
  ../gui-common/utils.h ../gui-common/prefs.h ../gui-common/status.h \
  ../gui-common/undo.h ../gui-common/select.h ../gui-common/algos.h \
  ../gui-common/layer.h ../gui-common/view.h ../gui-common/control.h \
  ../gui-common/file.h ../gollybase/writepattern.h webcalls.h
status.o: ../gui-common/status.cpp ../gollybase/bigint.h \
  ../gollybase/lifealgo.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h \
  ../gui-common/utils.h ../gui-common/prefs.h ../gui-common/algos.h \
  ../gui-common/layer.h ../gui-common/select.h ../gui-common/view.h \
  ../gui-common/status.h webcalls.h
undo.o: ../gui-common/undo.cpp ../gollybase/bigint.h \
  ../gollybase/lifealgo.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h \
  ../gollybase/writepattern.h ../gui-common/select.h \
  ../gui-common/utils.h ../gui-common/view.h ../gui-common/algos.h \
  ../gui-common/layer.h ../gui-common/prefs.h ../gui-common/control.h \
  ../gui-common/file.h ../gui-common/undo.h
utils.o: ../gui-common/utils.cpp ../gollybase/lifepoll.h \
  ../gollybase/util.h ../gui-common/prefs.h ../gui-common/utils.h \
  webcalls.h
view.o: ../gui-common/view.cpp ../gollybase/bigint.h \
  ../gollybase/lifealgo.h ../gollybase/viewport.h \
  ../gollybase/liferender.h ../gollybase/lifepoll.h \
  ../gollybase/readpattern.h ../gollybase/platform.h \
  ../gollybase/qlifealgo.h ../gollybase/liferules.h \
  ../gollybase/hlifealgo.h ../gui-common/utils.h ../gui-common/prefs.h \
  ../gui-common/status.h ../gui-common/render.h ../gui-common/undo.h \
  ../gui-common/select.h ../gui-common/algos.h ../gui-common/layer.h \
  ../gui-common/control.h ../gui-common/file.h \
  ../gollybase/writepattern.h ../gui-common/view.h webcalls.h
ioapi.o: ../gui-common/MiniZip/ioapi.c ../gui-common/MiniZip/ioapi.h
unzip.o: ../gui-common/MiniZip/unzip.c ../gui-common/MiniZip/unzip.h \
  ../gui-common/MiniZip/ioapi.h ../gui-common/MiniZip/crypt.h
zip.o: ../gui-common/MiniZip/zip.c ../gui-common/MiniZip/zip.h \
  ../gui-common/MiniZip/ioapi.h ../gui-common/MiniZip/crypt.h
