//
// Created by Peter Liu on 2022/6/18 0018.
//

#ifndef WEPLAYKT_AUDIOCHANNEL_H
#define WEPLAYKT_AUDIOCHANNEL_H

#include "BaseChannel.h"
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

extern "C" {
//#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/time.h>
};

class AudioChannel : public BaseChannel {
private:
    pthread_t pid_audio_decode;
    pthread_t pid_audio_play;

public:
    int out_channels;
    int out_sample_size;
    int out_sample_rate;
    int out_buffers_size;
    uint8_t *out_buffers = 0;
    SwrContext *swr_ctx = 0;

    SLObjectItf engineObject = 0; // 引擎
    SLEngineItf engineInterface = 0; // 引擎接口
    SLObjectItf outputMixObject = 0; // 混音器
    SLObjectItf bqPlayerObject=0; // 播放器
    SLPlayItf bqPlayerPlay = 0; // 播放器接口
    SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue=0; // 播放器队列接口

public:
    AudioChannel(int streamIndex, AVCodecContext *pCodecContext);

    virtual ~AudioChannel();

    void start();

    void stop();

    void audio_decode();

    void audio_play();

    int getPcmAndSize();
};


#endif //WEPLAYKT_AUDIOCHANNEL_H
