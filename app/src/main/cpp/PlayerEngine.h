//
// Created by Peter Liu on 2022/6/18 0018.
//

#ifndef WEPLAYKT_PLAYERENGINE_H
#define WEPLAYKT_PLAYERENGINE_H

#include <cstring>
#include <pthread.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/time.h>
};

#include "VideoChannel.h"
#include "AudioChannel.h"
#include "JNICallbackHelper.h"
#include <util.h>
#include <LogUtils.h>

class PlayerEngine {
private:
    char *data_source = 0; // 指针 * 请赋初始值
    pthread_t pid_prepare;
    pthread_t pid_start;
    pthread_t pid_stop;

    AVFormatContext *formatContext = 0; // 媒体上下文 封装格式
    AudioChannel *audio_channel = 0;
    VideoChannel *video_channel = 0;
    JNICallbackHelper *helper = 0;
    bool isPlaying=0; // 是否播放
    RenderCallback renderCallback;

    int duration; //视频总时长
    pthread_mutex_t seek_mutex;

public:
    PlayerEngine(const char *data_source, JNICallbackHelper *helper);

//    virtual ~PlayerEngine(); //加virtuel和不加定义的两种析构函数有什么区别？
    ~PlayerEngine();

    void prepare();
    void prepare_();

    void start();
    void start_();
    void stop();
    void release();

    void setRenderCallback(RenderCallback renderCallback);

    int getDuration();

    void seek(int progress);

    void stop_(PlayerEngine *pEngine);

    void pause();
};


#endif //WEPLAYKT_PLAYERENGINE_H
