//
// Created by Peter Liu on 2022/6/18 0018.
//

#ifndef WEPLAYKT_VIDEOCHANNEL_H
#define WEPLAYKT_VIDEOCHANNEL_H

#include "BaseChannel.h"
#include "AudioChannel.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
};

// 函数指针声明定义 用作回调函数
typedef void(*RenderCallback)(uint8_t *, int, int, int);

class VideoChannel : public BaseChannel {
private:
    pthread_t pid_video_decode;
    pthread_t pid_video_play;
    RenderCallback renderCallback;

    int fps;
    AudioChannel *audio_channel = 0;

public:
    VideoChannel(int streamIndex, AVCodecContext *pCodecContext, AVRational time_base, int fps);

    virtual ~VideoChannel();

    void start();

    void stop();

    void video_decode();

    void video_play();

    void setRenderCallback(RenderCallback renderCallback);

    void setAudioChannel(AudioChannel *audio_channel);

    void pause();
};

#endif //WEPLAYKT_VIDEOCHANNEL_H
