APP_PLATFORM := android-17

#??? APP_ABI = all

# define ANDROID_GUI used in the shared C++ code in gui-common;
# define ZLIB so Golly can read/write .gz files;
# set -fexceptions so we can use stdexcept
APP_CFLAGS += -DANDROID_GUI -DZLIB -fexceptions

# needed for std::string etc
APP_STL := stlport_shared
# also need System.loadLibrary("stlport_shared") in MainActivity.java
