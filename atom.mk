
LOCAL_PATH := $(call my-dir)

#################
#  MP4 library  #
#################

include $(CLEAR_VARS)
LOCAL_MODULE := libmp4
LOCAL_DESCRIPTION := MP4 file library
LOCAL_CATEGORY_PATH := libs
LOCAL_SRC_FILES := \
    src/mp4_demux.c \
    src/mp4_log.c
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_CONDITIONAL_LIBRARIES := OPTIONAL:libulog

include $(BUILD_LIBRARY)

#####################
#  Test executable  #
#####################

include $(CLEAR_VARS)
LOCAL_MODULE := mp4_demux_test
LOCAL_DESCRIPTION := MP4 file library demuxer test program
LOCAL_CATEGORY_PATH := multimedia
LOCAL_SRC_FILES := test/mp4_demux_test.c
LOCAL_LIBRARIES := libmp4 libulog
include $(BUILD_EXECUTABLE)
