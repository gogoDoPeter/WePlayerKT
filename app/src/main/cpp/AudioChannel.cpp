//
// Created by Peter Liu on 2022/6/18 0018.
//

#include "AudioChannel.h"

AudioChannel::AudioChannel(int streamIndex, AVCodecContext *pCodecContext)
    : BaseChannel(streamIndex, pCodecContext) {

}

AudioChannel::~AudioChannel() {

}
