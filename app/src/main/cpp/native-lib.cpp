#include <jni.h>
#include <string>
#include "PlayerEngine.h"
#include <LogUtils.h>
#include <android/native_window_jni.h> // ANativeWindow 用来渲染画面的 == Surface对象

JavaVM *vm = nullptr;
ANativeWindow *window = nullptr;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; // 静态初始化锁

jint JNI_OnLoad(JavaVM *vm_, void *args) {
    ::vm = vm_;
    return JNI_VERSION_1_6;
}

// 函数指针的实现 实现渲染画面
void renderCallback(uint8_t *src_data, int width, int height,
                    int src_linesize) {//lineSize应该就是stride
    pthread_mutex_lock(&mutex);
    if (!window) {
        pthread_mutex_unlock(&mutex);// 出现了问题后，立刻释放锁，避免出现死锁问题
    }
    // 设置窗口的大小，各个属性
    ANativeWindow_setBuffersGeometry(window, width, height, WINDOW_FORMAT_RGBA_8888);
    // 窗口自己有个缓冲区 buffer
    ANativeWindow_Buffer window_buffer;
    // 在渲染的时候要将窗口的下一帧的surface锁住，如果没有lock成功，立刻释放资源和锁，防止出现死锁
    // Lock the window's next drawing surface for writing
    if (ANativeWindow_lock(window, &window_buffer, 0)) { //return 0 is success
        ANativeWindow_release(window);
        window = nullptr;
        pthread_mutex_unlock(&mutex);
        return;
    }
    // 填充[window_buffer] 画面就出来了  ==== 目标 window_buffer 注意：做渲染时字节对齐
    // 为什么FFmpeg播放是可以的？ FFmpeg是8字节对齐    ANativeWindows是64字节对齐
    uint8_t * dst_data = static_cast<uint8_t *>(window_buffer.bits);
    int dst_linesize = window_buffer.stride * 4;
    for (int i = 0; i < window_buffer.height; ++i) {
        // memcpy(dst_data + i * 1704, src_data + i * 1704, 1704); // 不字节对齐 就会花屏
        // 花屏原因：1704 无法 64字节对齐，所以花屏

        // memcpy(dst_data + i * 1792, src_data + i * 1704, 1792); //OK, 对应媒体文件视频分辨率：426 * 240
        // memcpy(dst_data + i * 1728, src_data + i * 1704, 1728); // 还是会花屏

        // 通用的
        memcpy(dst_data + i * dst_linesize, src_data + i * src_linesize, dst_linesize);
//        LOGD("dst_linesize:%d  src_linesize:%d\n", dst_linesize, src_linesize)
    }
    // 数据刷新
    ANativeWindow_unlockAndPost(window); // 解锁后 并且刷新 window_buffer的数据显示画面

    pthread_mutex_unlock(&mutex);
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_infinite_weplaykt_PlayerEngine_prepareNative(JNIEnv *env, jobject thiz,
                                                      jstring data_source) {
    LOGD("prepareNative")
    JNICallbackHelper *helper = new JNICallbackHelper(vm, env, thiz);
    const char *data_source_ = env->GetStringUTFChars(data_source, nullptr);
    auto *player = new PlayerEngine(data_source_, helper);//申请的堆空间，注意后面要释放
    player->setRenderCallback(renderCallback);
    player->prepare();

    return reinterpret_cast<jlong>(player);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_infinite_weplaykt_PlayerEngine_startNative(JNIEnv *env, jobject thiz, jlong native_obj) {
    LOGD("startNative")
    PlayerEngine *player = reinterpret_cast<PlayerEngine *>(native_obj);
    player->start();
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

extern "C"
JNIEXPORT void JNICALL
Java_com_infinite_weplaykt_PlayerEngine_setSurfaceNative(JNIEnv *env, jobject thiz,
                                                         jobject surface,
                                                         jlong native_obj) {
    LOGD("setSurfaceNative")
    pthread_mutex_lock(&mutex);
    // 先释放之前的显示窗口
    if (window) {
        ANativeWindow_release(window);
        window = nullptr;
    }
    // 创建新的窗口用于视频显示  让surface和window建立绑定
    window = ANativeWindow_fromSurface(env, surface);
    pthread_mutex_unlock(&mutex);
}