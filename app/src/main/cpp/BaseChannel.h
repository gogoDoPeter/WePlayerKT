//
// Created by Peter Liu on 2022/6/18 0018.
//

#ifndef WEPLAYKT_BASECHANNEL_H
#define WEPLAYKT_BASECHANNEL_H

extern "C" {
#include <libavcodec/avcodec.h>
};

#include "util/SafeQueue.h"
#include "JNICallbackHelper.h"
#include <LogUtils.h>

class BaseChannel {
public:
    int stream_index;
    SafeQueue<AVPacket *> packets;
    SafeQueue<AVFrame *> frames;
    bool isPlaying;
    bool isPause = false;
    AVCodecContext *codecContext = 0;

    AVRational time_base; //AudioChannel VideoChannel 都需要用到 时间基）ffmpeg中统计时间的单位而已
    JNICallbackHelper *jniCallbackHelper = 0;

    BaseChannel(int stream_index, AVCodecContext *codecContext, AVRational time_base)
            : stream_index(stream_index),
              codecContext(codecContext),
              time_base(time_base) {
        // 给队列设置Callback，Callback释放队列里面的数据
        packets.setReleaseCallback(releaseAVPacket);
        frames.setReleaseCallback(releaseAVFrame);
    }

    virtual ~BaseChannel() {
        packets.clear();
        frames.clear();
    }

    void setJNICallbackHelper(JNICallbackHelper *jniCallbackHelper) {
        this->jniCallbackHelper = jniCallbackHelper; //使用前要判断下
    }

    /**
     * 释放队列中所有的 AVPacket *
     * @param packet
     */
    // typedef void (*ReleaseCallback)(T *);
    static void releaseAVPacket(AVPacket **p) { //思考题：回调函数定义中参数是AVPacket类型二级指针，为什么调用用的一级指针?
        if (p) {
            av_packet_free(p); // 释放队列里面的 T == AVPacket
            *p = 0;
        }
    }
    /**
     * 释放队列中所有的 AVFrame *
     * @param packet
     */
    // typedef void (*ReleaseCallback)(T *);
    static void releaseAVFrame(AVFrame **f) {
        if (f) {
            av_frame_free(f); // 释放队列里面的 T == AVFrame
            *f = 0;
        }
    }
};

#endif //WEPLAYKT_BASECHANNEL_H
