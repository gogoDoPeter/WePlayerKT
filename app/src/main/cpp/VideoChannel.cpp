//
// Created by Peter Liu on 2022/6/18 0018.
//

#include "VideoChannel.h"

VideoChannel::VideoChannel(int streamIndex, AVCodecContext *pCodecContext) : BaseChannel(
        streamIndex, pCodecContext) {}

VideoChannel::~VideoChannel() {

}
