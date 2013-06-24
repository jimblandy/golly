#??? APP_PLATFORM := android-17

#??? APP_ABI = all

# define ZLIB so Golly can read/write .gz files
# and set -fexceptions so we can use stdexcept
APP_CFLAGS += -DZLIB -fexceptions

# needed for std::string etc
APP_STL := stlport_shared
# also need System.loadLibrary("stlport_shared") in MainActivity.java
