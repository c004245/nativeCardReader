LOCAL_PATH := $(call my-dir) # 현재 파일의 설정된 경로.

include $(CLEAR_VARS) # LOCAL 관련 변수를 clear

LOCAL_MODULE := libnativecardreader # 모듈 이름, 생성되는 파일 이름을 결정.
LOCAL_CFLAGS += -std=c++14 # 컴파일 플래그. 여기서는 c++14 사용.
LOCAL_SRC_FILES := native_cardreader.cpp # 컴파일 되는 파일 리스트.
LOCAL_LDLIBS := -L$(SYSROOT)/usr/lib -llog

include $(BUILD_SHARED_LIBRARY) # 동적 라이브러리로 사용.


