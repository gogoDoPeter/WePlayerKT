//
// Created by Peter Liu on 2022/6/18 0018.
//

#include "JNICallbackHelper.h"

JNICallbackHelper::JNICallbackHelper(JavaVM *vm, JNIEnv *env, jobject job) {
    this->vm = vm;
    this->env = env;

    // this->job = job; //注意： jobject不能跨越线程，不能跨越函数，必须全局引用
    this->job = env->NewGlobalRef(job);//使用全局引用
    playerEngineKTClass = env->GetObjectClass(job);
    jmd_prepared = env->GetMethodID(playerEngineKTClass, "onPrepared", "()V");
    jmd_onError = env->GetMethodID(playerEngineKTClass, "onError", "(ILjava/lang/String;)V");
    //播放音频的时间戳回调
    jmd_onProgress = env->GetMethodID(playerEngineKTClass, "onProgress", "(I)V");

}

JNICallbackHelper::~JNICallbackHelper() {
    vm = nullptr;
    env->DeleteGlobalRef(job);// 释放全局引用
    job = nullptr;

    if(playerEngineKTClass){
        env->DeleteLocalRef(playerEngineKTClass);
        playerEngineKTClass = nullptr;
    }

    env = nullptr;
}

void JNICallbackHelper::onPrepared(int thread_mode) {
    if (thread_mode == THREAD_TYPE_MAIN) {
        env->CallVoidMethod(job, jmd_prepared);
    } else if (thread_mode) {
        // 子线程 env也不可以跨线程?--对的,要使用全新的env,子线程必须用 JavaVM 子线程中附加出来的新env,即为子线程专用env
        JNIEnv *env_child;
        vm->AttachCurrentThread(&env_child, nullptr);
        env_child->CallVoidMethod(job, jmd_prepared);
        vm->DetachCurrentThread();
    }
}

void JNICallbackHelper::onError(int thread_mode, int errCode, char *errMsg) {
    if (thread_mode == THREAD_TYPE_MAIN) {
        jstring ffmpegError = env->NewStringUTF(errMsg); //这里不需要释放？
        env->CallVoidMethod(job, jmd_onError, errCode, ffmpegError);
    } else if (thread_mode) {
        // 子线程 env也不可以跨线程?--对的,要使用全新的env,子线程必须用 JavaVM 子线程中附加出来的新env,即为子线程专用env
        JNIEnv *env_child;
        vm->AttachCurrentThread(&env_child, nullptr);
        jstring ffmpegError = env_child->NewStringUTF(errMsg); //TODO 这里不需要释放？
        env_child->CallVoidMethod(job, jmd_onError, errCode, ffmpegError);
        vm->DetachCurrentThread();
    }
}

void JNICallbackHelper::onProgress(int thread_mode, double audio_time) {
    current_time = (int) audio_time;
    int diff_time = current_time - last_time;
    LOGD("audio_time2 :%d, diff_time:%d", (int) audio_time, diff_time)
    if (diff_time >= 1) {
        if (thread_mode == THREAD_TYPE_MAIN) {
            env->CallVoidMethod(job, jmd_onProgress, (int) audio_time);
        } else if (thread_mode) {
            // 子线程 env也不可以跨线程?--对的,要使用全新的env,子线程必须用 JavaVM 子线程中附加出来的新env,即为子线程专用env
            JNIEnv *env_child;
            vm->AttachCurrentThread(&env_child, nullptr);
            LOGD("audio_time2 :%d jni call java onProgress", (int) audio_time)
            env_child->CallVoidMethod(job, jmd_onProgress, (int) audio_time);
            vm->DetachCurrentThread();
        }
        last_time = current_time;
    }
}