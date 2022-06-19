//
// Created by Peter Liu on 2022/6/18 0018.
//

#ifndef WEPLAYKT_VIDEOCHANNEL_H
#define WEPLAYKT_VIDEOCHANNEL_H

#include "BaseChannel.h"

extern "C" {
    #include <libavcodec/avcodec.h>
};

class VideoChannel : public BaseChannel{
public:
    VideoChannel(int streamIndex, AVCodecContext *pCodecContext);

    virtual ~VideoChannel();

};

#endif //WEPLAYKT_VIDEOCHANNEL_H
