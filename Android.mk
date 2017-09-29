LOCAL_PATH := $(call my-dir)

# JNI Wrapper
include $(CLEAR_VARS)

LOCAL_CFLAGS := -g
LOCAL_MODULE := libmp4_android
LOCAL_SRC_FILES := JNI/c/libmp4_jni.c
LOCAL_LDLIBS := -llog
LOCAL_SHARED_LIBRARIES := \
	libmp4

include $(BUILD_SHARED_LIBRARY)
