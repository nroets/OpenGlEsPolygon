LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := openglespolygon

LOCAL_CFLAGS := -DANDROID_NDK 

LOCAL_SRC_FILES := openglespolygon.cpp

LOCAL_LDLIBS := -lGLESv1_CM -ldl -llog

include $(BUILD_SHARED_LIBRARY)
