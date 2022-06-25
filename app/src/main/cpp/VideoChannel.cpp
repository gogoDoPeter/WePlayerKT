//
// Created by Peter Liu on 2022/6/18 0018.
//

#include "VideoChannel.h"

VideoChannel::VideoChannel(int streamIndex, AVCodecContext *pCodecContext) : BaseChannel(
        streamIndex, pCodecContext) {}

VideoChannel::~VideoChannel() {

}

//视频：取出队列的压缩包 进行解码 解码后的原始包 再push队列中去
void VideoChannel::video_decode() {
    AVPacket *pkt = nullptr;
    while (isPlaying) {
        if(isPlaying && frames.size() > 100){
            av_usleep(10*1000);
            continue;
        }

        int ret = packets.getQueueAndDel(pkt); //阻塞式函数
        if (!isPlaying) { // 如果关闭了播放
            break;
        }
        if (!ret) { // ret == 0 error, return 1 represent success
            continue; // 如果生产太慢(压缩包加入队列)，消费就等一下
        }
        // 第一步：把我们的 压缩包 AVPack发送给 FFmpeg缓存区
        ret = avcodec_send_packet(codecContext, pkt);
        // FFmpeg源码内部 缓存了一份pkt副本，所以这里用完可以及时的释放
//        releaseAVPacket(&pkt); //放后面做
        if (ret) { //ret != 0 命中
            break;
        }
        // 第二步：读取 FFmpeg缓存区里面的原始包,有可能读不到，为什么？--内部缓冲区 会运作过程比较慢
        AVFrame *frame = av_frame_alloc();
        ret = avcodec_receive_frame(codecContext, frame);
        if (ret == AVERROR(EAGAIN)) {
            continue;// B帧 B帧参考前面成功,但是参考后面失败 (可能是P帧还没有出来)，那么等一等重新再拿一次可能就拿到了
        } else if (ret != 0) { // 出错误了
            //解码视频的frame出错，马上释放，防止你在堆区开辟了空间
            if(frame){
                releaseAVFrame(&frame);
            }
            break;
        }
        //拿到 原始包了，加入队列
        frames.insertToQueue(frame);

        av_packet_unref(pkt); // ffmpeg中对pkt的引用计数减1操作，当计数为0时释放成员分配的堆区
        releaseAVPacket(&pkt); // 释放AVPacket * 本身的堆区空间
    }// while end
    av_packet_unref(pkt);// ffmpeg中对pkt的引用计数减1操作，当计数为0时释放成员分配的堆区
    releaseAVPacket(&pkt); // 释放AVPacket * 本身的堆区空间
}

void *task_video_decode(void *args) {
    VideoChannel *video_channel = static_cast<VideoChannel *>(args);
    video_channel->video_decode();
    return nullptr;
}

//视频：从队列取出原始包，播放
void VideoChannel::video_play() {
    AVFrame *frame = nullptr;
    uint8_t *dst_data[4];//RGBA
    int dst_linesize[4];//RGBA  一行数据宽度

    //给 dst_data 申请内存   如果是RGBA格式，底层其实用width * height * 4 大小的内存
    av_image_alloc(dst_data, dst_linesize,
                   codecContext->width, codecContext->height, AV_PIX_FMT_RGBA,
                   1); //the value to use for buffer size alignment
    SwsContext *sws_ctx = sws_getContext(codecContext->width, codecContext->height,
                                         codecContext->pix_fmt,// 自动获取媒体文件的像素格式  AV_PIX_FMT_YUV420P写死也行，大部分都是420P
                                         codecContext->width, codecContext->height,
                                         AV_PIX_FMT_RGBA,
                                         SWS_BILINEAR, //配置格式转换算法参数，SWS_FAST_BILINEAR ==很快,可能会模糊, SWS_BILINEAR适中的算法,兼顾速度和图片质量
                                         nullptr, nullptr,
                                         nullptr);//filter使用OpenGL来做，一般不用ffmpeg的这个模块
    while (isPlaying) {
        int ret = frames.getQueueAndDel(frame);
        if (!isPlaying) { // 如果关闭了播放
            break;
        }
        if (!ret) { //ret ==1 is success
            continue; // 如果生产太慢(原始包加入队列)，消费就等一下
        }
        // 格式转换 yuv --> rgba
        sws_scale(sws_ctx,
                  frame->data, frame->linesize,
                  0, //srcSliceY
                  codecContext->height, //srcSliceH
                  dst_data,
                  dst_linesize);
        // ANativeWindow 渲染工作 方式1，用ANativeWindow来渲染
        // SurfaceView  ---- ANativeWindow
        // 如何渲染一帧图像？ --宽，高，数据，数据大小
        if (renderCallback)
            renderCallback(dst_data[0], codecContext->width, codecContext->height, dst_linesize[0]);
        //TODO 方式2：用OpenGL来渲染
        av_frame_unref(frame);// 减1 = 0 释放成员执行的堆区
        releaseAVFrame(&frame);// 释放原始包，因为已经被渲染了，没用了
    }
    av_frame_unref(frame);// 减1 = 0 释放成员执行的堆区
    releaseAVFrame(&frame);// 出现错误时退出循环，都要释放frame;如果正常退出也不会释放两次，释放实现中做了指针是否为空的判断
    isPlaying = 0;
    av_free(&dst_data[0]);
    sws_freeContext(sws_ctx);
}

void *task_video_play(void *args) {
    VideoChannel *video_channel = static_cast<VideoChannel *>(args);
    video_channel->video_play();
    return nullptr;
}

void VideoChannel::start() {
    isPlaying = 1;
    packets.setWork(1);
    frames.setWork(1);

    // 第一个线程： 视频：取出队列的压缩包 进行解码 解码后的原始包 再push队列中去
    pthread_create(&pid_video_decode, nullptr, task_video_decode, this);
    // 第二线线程：视频：从队列取出原始包，播放
    pthread_create(&pid_video_play, nullptr, task_video_play, this);
}

void VideoChannel::stop() {

}

void VideoChannel::setRenderCallback(RenderCallback renderCallback) {
    this->renderCallback = renderCallback;
}



