LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := golly

LOCAL_SRC_FILES := jnicalls.cpp

# add .cpp files in gui-common
GUI_COMMON := $(wildcard $(LOCAL_PATH)/../../../gui-common/*.cpp)
LOCAL_SRC_FILES += $(GUI_COMMON:$(LOCAL_PATH)/%=%)

# add .c files in gui-common/MiniZip
MINIZIP := $(wildcard $(LOCAL_PATH)/../../../gui-common/MiniZip/*.c)
LOCAL_SRC_FILES += $(MINIZIP:$(LOCAL_PATH)/%=%)

# add .cpp files in gollybase
GOLLYBASE := $(wildcard $(LOCAL_PATH)/../../../gollybase/*.cpp)
LOCAL_SRC_FILES += $(GOLLYBASE:$(LOCAL_PATH)/%=%)

# include .h files in gui-common
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../../gui-common

# include .h files in gui-common/MiniZip
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../../gui-common/MiniZip

# include .h files in gollybase
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../../gollybase

LOCAL_LDLIBS := -lGLESv1_CM -llog -lz -ljnigraphics

include $(BUILD_SHARED_LIBRARY)
