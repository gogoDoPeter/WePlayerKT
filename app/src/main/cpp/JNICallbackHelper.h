//
// Created by Peter Liu on 2022/6/18 0018.
//
#ifndef WEPLAYKT_JNICALLBACKHELPER_H
#define WEPLAYKT_JNICALLBACKHELPER_H

#include <jni.h>
#include <util.h>
#include <LogUtils.h>

class JNICallbackHelper {
private:
    JNIEnv *env = 0;
    JavaVM *vm = 0;// 只有它才能 跨越线程
    jobject job;// 为了更好的寻找到 PlayerEngine.kt实例
    jmethodID jmd_prepared;
    jmethodID jmd_onError;
    jmethodID jmd_onProgress;
    int last_time=0;
    int current_time;
    jclass playerEngineKTClass;

public:
    JNICallbackHelper(JavaVM *vm,JNIEnv *env,jobject job);

    virtual ~JNICallbackHelper();

    void onPrepared(int thread_mode);
    void onError(int thread_mode, int errCode, char *errMsg);
    void onProgress(int thread_mode, double audio_time);
};


#endif //WEPLAYKT_JNICALLBACKHELPER_H
