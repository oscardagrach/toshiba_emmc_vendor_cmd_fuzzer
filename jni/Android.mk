LOCAL_PATH := $(call my-dir)


include $(CLEAR_VARS)
LOCAL_ARM_MODE := arm
APP_ABI := armeabi-v7a
APP_PLATFORM := android-21
LOCAL_CFLAGS += -fPIE -pie -std=gnu99 -O2
LOCAL_MODULE := fuzz
LOCAL_LDFLAGS := -fPIE -static

LOCAL_SRC_FILES :=  fuzz.c
LOCAL_LDLIBS +=
include $(BUILD_EXECUTABLE)

