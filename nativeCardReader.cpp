/**
 * NDK_PROJECT_PATH 
 */

#ifdef __cplusplus 
extern "C" {
#endif
#include <jni.h>
#include <android/log.h>
#ifdef __cplusplus 
}
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <pthread.h>

#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <stdio.h>
#include <errno.h>

#include <iostream>
#include <list>
#include <string>
#include <fstream>
#include <algorithm>
using namespace std;

#include "hyunwook_co_kr_ndkstudyrasp_HelloNDK.h"

#define STX 0x02
#define ETX 0x03
#define ERR -1

#define ANDROID_LOG_LEV ANDROID_LOG_ERROR
#define LOG_TAG(X) "native_cardreader_"#X
#define ANDROID_LOG(TAG, format, ...) \
       __android_log_print(ANDROID_LOG_LEV, LOG_TAG(TAG), format, ## __VA_ARGS__)

#ifdef __cplusplus 
extern "C" {
#endif

jint JNI_OnLoad(JavaVM*, void*);

jint init_cardreader(JNIEnv*, jobject, jbyteArray, int);
jint run_cardreader(JNIEnv*, jobject);
jint terminate_cardreader(JNIEnv*, jobject);

#include <ctype.h>
int isspace(int);

#ifdef __cplusplus
}
#endif

int open_device(const char * const dev);
void setup_terminal(const int fd);

int _try_attach(const char * const dev, int &fd);

void* cardreader_loop(void *data);
void read_completed(char*, size_t);
jbyteArray getJavaByteArray(JNIEnv*, char*, size_t);

const int RAWCODE_MAX = 25;
const int def_buff_len = 45;

const int max_try = 100;
const long pause_time = 1500;

/** global variables */
static int thr_id;
static int end_flag;
static pthread_t p_thread[2];

/*
 * ref: https://gcc.gnu.org/onlinedocs/gcc/Designated-Inits.html
 * ref: http://sourceforge.net/p/predef/wiki/OperatingSystems/
 */
#if __gnu_linux__
    static char tty_device[def_buff_len] = {[0 ... def_buff_len-1] = 0};
#else
    static char tty_device[def_buff_len] = {0};
#endif

static list<string> errRfCode;

static int reader_fd = -1;

static JavaVM *glpVM = NULL;
static jclass jcls;
static jmethodID func_cb;

/** native_pthread.c 소스파일에서만 사용 */
static JNINativeMethod methods[] = {
    {INIT_FC, INIT_SIGNATURE, (void*)init_cardreader},
    {START_FC, START_SIGNATURE, (void*)run_cardreader},
    {ENDED_FC, ENDED_SIGNATURE, (void*)terminate_cardreader}
};

static char device_name[45] = {0};

/********************************************************************/

int _try_attach(const char * const dev, int *fd)
{
    int n = max_try;
    while (n--)
    {
        int _fd = open_device(dev);
        if (_fd > 0) {
            setup_terminal(_fd);
            ANDROID_LOG(try_attach, "succeed D/D open - %05d\n", _fd);
            *fd = _fd;
            return _fd;
        }

        ANDROID_LOG(try_attach, "D/D could not open. will sleeping during 1500 ms\n");
        usleep(pause_time);
        ANDROID_LOG(try_attach, "wake up\n");
    }

    return -1;
}

void* cardreader_loop(void *data)
{
//    int id;
//    id = *((int*) data);

    char ch;
    int i = 0;
    
#if __gnu_linux__
    /* GNU 확장 기능으로 배열을 초기화할 때 범위를 지정하여 초기화 */
    char cardNum[RAWCODE_MAX] = { [0 ... RAWCODE_MAX-1] = 0};
#else
    char cardNum[RAWCODE_MAX] = {0};
    memset(cardNum, 0, RAWCODE_MAX);
#endif

    

    if (_try_attach(tty_device, &reader_fd) < 0) {
        ANDROID_LOG(init_loop, "D/D could not open.\n");

        end_flag = 0;
        return NULL;
    }

    do
    {
        int reading = read(reader_fd, (char*)&ch, 1);
        if (reading == 1) {
            switch (ch)
            {
                case STX:
                    i = 0;
                    memset(cardNum, 0, RAWCODE_MAX);
                    break;

                case ETX:
                    if (!end_flag)
                        return NULL;

                        read_completed(cardNum, i);
                    break;

                case ERR:
                    break;

                default:
                    cardNum[i++] = ch;
            }
        }
      
    } while (end_flag);
}

/*////////////////////////////////////////////////////////////////////////////*/

int open_device(const char * const dev)
{
     // 카드리더 장치를 연다.
    int fd = open(dev, O_RDONLY | O_NOCTTY | O_NONBLOCK);
    ANDROID_LOG(init_reader, "fd value ", fd);
    if (fd < 0) {    
        ANDROID_LOG(init_reader, "device could not open");
        return -1;
    }

    return fd;
}

void setup_terminal(const int fd)
{
    // 포트 환경설정을 수행한다.
    termios tty;
    ::tcgetattr(fd, &tty);
    tty.c_iflag = IGNBRK | IGNPAR;
    tty.c_oflag = 0;
    tty.c_lflag = 0;
    tty.c_cflag = B9600 | CS7 | PARENB | CREAD | CLOCAL | HUPCL;
    tty.c_line  = 0;
    tty.c_cc[VTIME] = 0;
    tty.c_cc[VMIN]  = 1;
    tcsetattr(fd, TCSANOW, &tty);
    tcflush(fd, TCIOFLUSH);
//    usleep(10000);
}

/*////////////////////////////////////////////////////////////////////////////*/

/*
 * 시리얼 장치만 입력을 받지만 속도, 비트수 등 입력 정보를 더 받아야 함.
 */
jint init_cardreader(JNIEnv *jenv, jobject thiz, jbyteArray array, int size)
{
    jbyte *bytes = NULL;
    

    bytes = jenv->GetByteArrayElements(array, NULL); 
    if (bytes != NULL) {
    
        char *src = (char*)bytes;
        memset(tty_device, 0, def_buff_len);
        memcpy(tty_device, src, size);  
        jenv->ReleaseByteArrayElements(array, bytes, JNI_ABORT);

        char buff[100] = {0};
        memset(buff, 0, 100);
        strcpy(buff, "device driver: ");
        strcpy(buff + 15, tty_device);
        ANDROID_LOG(init_reader, "%s\n", buff);
        ANDROID_LOG(init_reader, "size: %05d\n", size);
    }
    else {
        ANDROID_LOG(init_reader, "device driver unknow");
//        __android_log_print(ANDROID_LOG_LEV, LOG_TAG("init_reader"), "device driver unknow");
        return -1;    
    }

    return 0;
}


jint run_cardreader(JNIEnv *jenv, jobject thiz)
{
#define START_PTHREAD_ERR_RESULT -1;
    end_flag = 1;
    int b = 2, dev = -1;

    string javaClass(PACKAGE);
    javaClass += "/CardReader";

//    ANDROID_LOG(run_cardreader, "Call start thread");


    jclass local_cls;
    local_cls = jenv->FindClass(javaClass.data());
    if (local_cls == NULL) {
        ANDROID_LOG(run_cardreader, "could not find the class...");
        return START_PTHREAD_ERR_RESULT;
    }

    /**
     * Even if the object itself continues to live on after the native method.
     * the reference is not vaild.
     *
     * FindClass() 함수로부터 구한 jclass reference는 일정기간 유효하지 못함
     * NewGlobalRef() 함수를 사용하여 jclass reference를 전역변수로 바꿈
     */
    jcls = static_cast<jclass>(jenv->NewGlobalRef(local_cls));
    func_cb = jenv->GetStaticMethodID(jcls, CALLBACK_FC, CALLBACK_SIGNATURE);
    if (!func_cb) {
        ANDROID_LOG(start_pthread, "could not find the method");
        jenv->DeleteGlobalRef(jcls);
        return START_PTHREAD_ERR_RESULT;
    }
//    else {
//        ANDROID_LOG(start_pthread, "Method connects success...\n");
//
//        char *testNum = "87654320";
//        jbyteArray array = getJavaByteArray(jenv, testNum, 8);
//        if (array != NULL) {
//            jenv->CallStaticVoidMethod(local_cls, func_cb, array);
//            jenv->DeleteLocalRef(array);
//        }
//    }

    thr_id = pthread_create(&p_thread[1], NULL, cardreader_loop, (void*)&b);
    if (thr_id < 0) {
        ANDROID_LOG(start_pthread, "Create thread fail.\n");
        return START_PTHREAD_ERR_RESULT;
    }

    return 0;
}

jint terminate_cardreader(JNIEnv *jenv, jobject jobj)
{
    ANDROID_LOG(terminate_cardreader, "terminated pthread");
    end_flag = 0;

    if (jenv != NULL && jcls != NULL) {
        jenv->DeleteGlobalRef(jcls);
    }

    return 0;
}

void read_completed(char *rawCode, size_t size)
{
    if (!glpVM) {
        ANDROID_LOG(ptherd_runnning, "error vm(virtual machin) not created");
        end_flag = 0;
        return;
    }
    else if (!func_cb) {
        ANDROID_LOG(pthread_running, "error pthread function not defined");
        end_flag = 0;
        return;
    }

    JNIEnv *jenv = NULL;
    glpVM->AttachCurrentThread(&jenv, NULL);
    if (jenv == NULL/* || jcls == NULL*/) {
        glpVM->DetachCurrentThread();
        ANDROID_LOG(pthread_running, "could not attach or jclass not allocated");
        end_flag = 0;
        return;
    }

    jbyteArray array = getJavaByteArray(jenv, rawCode, size);
    if (array != NULL) {
        /** 코드 복사할 때 신중하게... */
        jenv->CallStaticVoidMethod(jcls, func_cb, array); 
        jenv->DeleteLocalRef(array);
    }

    glpVM->DetachCurrentThread();

    usleep(1000);
}

jbyteArray getJavaByteArray(JNIEnv *jenv, char *data, size_t size)
{
    jbyteArray array;

    array = jenv->NewByteArray(size);
    if (array == NULL)
        return NULL;

    jbyte *fill = (jbyte*)data;
    jenv->SetByteArrayRegion(array, 0, size, fill);

    return array;
}

void getErrRfCode(char *fileName)
{
    char buff[RAWCODE_MAX] = {0,};
    fstream fs(fileName, std::fstream::in);
    locale loc;

    if (fs.is_open()) {

        do {
            fs.getline(buff, RAWCODE_MAX);
            char *p = find_if(buff, buff + RAWCODE_MAX, isspace);
            buff[p-buff] = '\0';

            if (strlen(buff))
                errRfCode.push_back(buff);

            buff[0] = '\0';
        } while (!fs.eof());
    }
}

jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
#define ONLOAD_ERR_RESULT -1
//    jint result = -1;
    JNIEnv *jenv = NULL;
    jclass jcls;

    if (vm->GetEnv((void**)&jenv, JNI_VERSION_1_6) != JNI_OK) {
        ANDROID_LOG(onload, "GetEnv failed.\n");
        return ONLOAD_ERR_RESULT;
    }

    string javaClass(PACKAGE);
    javaClass += "/CardReader";

    jcls = jenv->FindClass(javaClass.data());
    if (jcls == NULL) {
        ANDROID_LOG(onload, "Native registration unable to find class(MainActivity)\n");
        return ONLOAD_ERR_RESULT;
    }

    if (jenv->RegisterNatives(jcls, methods,
                            sizeof(methods)/sizeof(methods[0])) < 0) {
        ANDROID_LOG(onload, "RegisterNatives failed !!!\n");
        return ONLOAD_ERR_RESULT;
    }

    glpVM = vm;

    return JNI_VERSION_1_6;
}
