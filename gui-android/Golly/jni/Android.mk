LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := golly

LOCAL_SRC_FILES := \
    algos.cpp \
    control.cpp \
    file.cpp \
    layer.cpp \
    prefs.cpp \
    render.cpp \
    select.cpp \
    status.cpp \
    undo.cpp \
    utils.cpp \
    view.cpp \
    jnicalls.cpp

# add .cpp files in gollybase
GOLLYBASE_FILES := $(wildcard $(LOCAL_PATH)/../../../gollybase/*.cpp)
LOCAL_SRC_FILES += $(GOLLYBASE_FILES:$(LOCAL_PATH)/%=%)

# find .h files in gollybase
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../../gollybase

LOCAL_LDLIBS := -lGLESv1_CM -llog -lz -ljnigraphics

include $(BUILD_SHARED_LIBRARY)
