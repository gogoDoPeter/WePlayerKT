//
// Created by Peter Liu on 2022/6/19 0019.
//

#ifndef WEPLAYKT_UTIL_H
#define WEPLAYKT_UTIL_H

#define THREAD_TYPE_MAIN 1 // 主线程
#define THREAD_TYPE_CHILD 2 // 子线程

// 打不开视频
#define FFMPEG_CAN_NOT_OPEN_URL 1
// 找不到流媒体
#define FFMPEG_CAN_NOT_FIND_STREAMS 2
// 找不到解码器
#define FFMPEG_FIND_DECODER_FAIL 3
// 无法根据解码器创建上下文
#define FFMPEG_ALLOC_CODEC_CONTEXT_FAIL 4
// 根据流信息 配置上下文参数失败
#define FFMPEG_CODEC_CONTEXT_PARAMETERS_FAIL 6
// 打开解码器失败
#define FFMPEG_OPEN_DECODER_FAIL 7
// 没有音视频
#define FFMPEG_NOMEDIA 8

#endif //WEPLAYKT_UTIL_H
