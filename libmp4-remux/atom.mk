
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libmp4-remux
LOCAL_CATEGORY_PATH := libs
LOCAL_DESCRIPTION := MP4 remuxer library
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_CFLAGS := -std=gnu99 -D_GNU_SOURCE
LOCAL_EXPORT_CXXFLAGS := -std=c++17
LOCAL_SRC_FILES := \
	src/remuxer.cpp \
	src/sample.cpp \
	src/track.cpp
LOCAL_LIBRARIES := \
	libmp4 \
	libulog

LOCAL_CONDITIONAL_LIBRARIES := \
	OPTIONAL:util-linux-ng

ifeq ("$(TARGET_OS)","windows")
  LOCAL_LDLIBS += -lws2_32
endif

include $(BUILD_LIBRARY)


include $(CLEAR_VARS)

LOCAL_MODULE := mp4-remux
LOCAL_CATEGORY_PATH := multimedia
LOCAL_DESCRIPTION := MP4 remuxer
LOCAL_CFLAGS := -std=gnu99 -D_GNU_SOURCE
LOCAL_EXPORT_CXXFLAGS := -std=c++17
LOCAL_SRC_FILES := \
	tools/mp4_remux.cpp
LOCAL_LIBRARIES := \
	libfutils \
	libmp4 \
	libmp4-remux \
	libulog

include $(BUILD_EXECUTABLE)
