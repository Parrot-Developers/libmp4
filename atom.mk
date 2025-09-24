
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libmp4
LOCAL_CATEGORY_PATH := libs
LOCAL_DESCRIPTION := MP4 file library
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
# Public API headers - top level headers first
# This header list is currently used to generate a python binding
LOCAL_EXPORT_CUSTOM_VARIABLES := LIBMP4_HEADERS=$\
	$(LOCAL_PATH)/include/libmp4.h;
LOCAL_CFLAGS := -DMP4_API_EXPORTS -fvisibility=hidden -std=gnu99 -D_GNU_SOURCE

LOCAL_SRC_FILES := \
	src/mp4.c \
	src/mp4_box_reader.c \
	src/mp4_box_to_json.c \
	src/mp4_box_writer.c \
	src/mp4_demux.c \
	src/mp4_mux.c \
	src/mp4_recovery.c \
	src/mp4_recovery_reader.c \
	src/mp4_recovery_writer.c \
	src/mp4_track.c
LOCAL_LIBRARIES := \
	json \
	libfutils \
	libulog

LOCAL_CONDITIONAL_LIBRARIES := \
	OPTIONAL:util-linux-ng

ifeq ("$(TARGET_OS)","windows")
  LOCAL_LDLIBS += -lws2_32
endif

include $(BUILD_LIBRARY)


include $(CLEAR_VARS)

LOCAL_MODULE := mp4-demux
LOCAL_DESCRIPTION := MP4 file library demuxer program
LOCAL_CATEGORY_PATH := multimedia
LOCAL_SRC_FILES := tools/mp4_demux.c
LOCAL_LIBRARIES := \
	json \
	libfutils \
	libmp4 \
	libulog

include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)

LOCAL_MODULE := mp4-mux
LOCAL_DESCRIPTION := MP4 file library muxer program
LOCAL_CATEGORY_PATH := multimedia
LOCAL_SRC_FILES := tools/mp4_mux.c
LOCAL_LIBRARIES := \
	libmp4 \
	libulog

include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)

LOCAL_MODULE := larry-covery
LOCAL_DESCRIPTION := Larry Covery: MP4 file recovery CLI
LOCAL_CATEGORY_PATH := multimedia
LOCAL_SRC_FILES := tools/larry_covery.c
LOCAL_LIBRARIES := \
	libmp4 \
	libulog \
	libfutils

include $(BUILD_EXECUTABLE)


ifdef TARGET_TEST

include $(CLEAR_VARS)

LOCAL_MODULE := tst-libmp4
LOCAL_C_INCLUDES := $(LOCAL_PATH)/src
LOCAL_SRC_FILES := \
	tests/mp4_test.c \
	tests/mp4_test_demux.c \
	tests/mp4_test_mux.c \
	tests/mp4_test_recovery.c \
	tests/mp4_test_utilities.c
LOCAL_LIBRARIES := \
	json \
	libcunit \
	libfutils \
	libmp4

include $(BUILD_EXECUTABLE)

endif
