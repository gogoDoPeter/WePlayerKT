#include <jni.h>
#include <string>
#include "PlayerEngine.h"
#include <LogUtils.h>

JavaVM *vm = nullptr;

jint JNI_OnLoad(JavaVM *vm_, void *args) {
    ::vm = vm_;
    return JNI_VERSION_1_6;
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_infinite_weplaykt_PlayerEngine_prepareNative(JNIEnv *env, jobject thiz,
                                                      jstring data_source) {
    LOGD("prepareNative")
    JNICallbackHelper *helper = new JNICallbackHelper(vm, env, thiz);
    const char *data_source_ = env->GetStringUTFChars(data_source, nullptr);
    auto *player = new PlayerEngine(data_source_, helper);//申请的堆空间，注意后面要释放
    player->prepare();

    return reinterpret_cast<jlong>(player);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_infinite_weplaykt_PlayerEngine_startNative(JNIEnv *env, jobject thiz, jlong native_obj) {
    LOGD("startNative")
}

extern "C"
JNIEXPORT void JNICALL
Java_com_infinite_weplaykt_PlayerEngine_stopNative(JNIEnv *env, jobject thiz, jlong native_obj) {
    LOGD("stopNative")
}

extern "C"
JNIEXPORT void JNICALL
Java_com_infinite_weplaykt_PlayerEngine_releaseNative(JNIEnv *env, jobject thiz, jlong native_obj) {
    LOGD("releaseNative")
}