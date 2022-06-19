//
// Created by Peter Liu on 2022/6/18 0018.
//

#ifndef WEPLAYKT_AUDIOCHANNEL_H
#define WEPLAYKT_AUDIOCHANNEL_H

#include "BaseChannel.h"

extern "C" {
    #include <libavcodec/avcodec.h>
};

class AudioChannel : public BaseChannel{

public:
    AudioChannel(int streamIndex, AVCodecContext *pCodecContext);

    virtual ~AudioChannel();

};


#endif //WEPLAYKT_AUDIOCHANNEL_H
