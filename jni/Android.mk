LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := esp4399
LOCAL_SRC_FILES := main.cpp hook.cpp
LOCAL_CFLAGS := -std=c++11 -Wall -Wextra -O2 -fvisibility=hidden
LOCAL_LDLIBS := -lGLESv1_CM -llog -landroid
LOCAL_ARM_MODE := arm

include $(BUILD_SHARED_LIBRARY)
