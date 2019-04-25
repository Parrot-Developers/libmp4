
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libmp4
LOCAL_CATEGORY_PATH := libs
LOCAL_DESCRIPTION := MP4 file library
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_CFLAGS := -DMP4_API_EXPORTS -fvisibility=hidden -std=gnu99
LOCAL_SRC_FILES := \
	src/mp4.c \
	src/mp4_box.c \
	src/mp4_demux.c \
	src/mp4_track.c
LOCAL_LIBRARIES := \
	libfutils \
	libulog

ifeq ("$(TARGET_OS)","windows")
  LOCAL_LDLIBS += -lws2_32
endif

include $(BUILD_LIBRARY)


include $(CLEAR_VARS)

LOCAL_MODULE := mp4-demux-test
LOCAL_DESCRIPTION := MP4 file library demuxer test program
LOCAL_CATEGORY_PATH := multimedia
LOCAL_SRC_FILES := tests/mp4_demux_test.c
LOCAL_LIBRARIES := \
	libfutils \
	libmp4 \
	libulog

include $(BUILD_EXECUTABLE)
