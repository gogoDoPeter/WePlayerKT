//
// Created by Peter Liu on 2022/6/18 0018.
//

#ifndef WEPLAYKT_PLAYERENGINE_H
#define WEPLAYKT_PLAYERENGINE_H

#include <cstring>
#include <pthread.h>

extern "C" {
#include <libavformat/avformat.h>
};

#include "VideoChannel.h"
#include "AudioChannel.h"
#include "JNICallbackHelper.h"
#include <util.h>
#include <LogUtils.h>

class PlayerEngine {
private:
    char *data_source = 0;
    pthread_t pid_prepare;
    AVFormatContext *formatContext = 0;
    AudioChannel *audio_channel = 0;
    VideoChannel *video_channel = 0;
    JNICallbackHelper *helper = 0;

public:
    PlayerEngine(const char *data_source, JNICallbackHelper *helper);

//    virtual ~PlayerEngine(); //加virtuel和不加定义的两种析构函数有什么区别？
    ~PlayerEngine();

    void prepare();

    void prepare_();
};


#endif //WEPLAYKT_PLAYERENGINE_H
