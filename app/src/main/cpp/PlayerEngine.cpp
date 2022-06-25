//
// Created by Peter Liu on 2022/6/18 0018.
//

#include "PlayerEngine.h"

PlayerEngine::PlayerEngine(const char *data_source, JNICallbackHelper *helper) {
    // this->data_source = data_source;
    // data_source如果被释放，会造成悬空指针  坑1

    this->data_source = new char[strlen(data_source) + 1];
    strcpy(this->data_source, data_source);

    this->helper = helper;
}

PlayerEngine::~PlayerEngine() {
    if (data_source) {
        delete data_source;
        data_source = nullptr;
    }
    if (helper) {
        delete helper;
        helper = nullptr;
    }
}

void *task_prepare(void *args) {
    PlayerEngine *player = static_cast<PlayerEngine *>(args);
    player->prepare_();

    return nullptr; //注意这里要返回，否则?
}

void PlayerEngine::prepare() {
    pthread_create(&pid_prepare, nullptr, task_prepare, this);
}

void PlayerEngine::prepare_() {// 属于子线程了,并且拥有PlayerEngine的实例的this
    formatContext = avformat_alloc_context();
    AVDictionary *dictionary = nullptr;
    av_dict_set(&dictionary, "timeout", "5000000", 0);//unit:微妙
    /**
    * 1，AVFormatContext *
    * 2，路径
    * 3，AVInputFormat *fmt  Mac、Windows 摄像头、麦克风， 我们目前安卓用不到
    * 4，各种设置：例如：Http 连接超时， 打开rtmp的超时  AVDictionary **options
    */
    int ret = avformat_open_input(&formatContext, data_source, nullptr, &dictionary);
    av_dict_free(&dictionary);

    if (ret < 0) { //0 == success
        LOGD("avformat_open_input fail, ret:%d, %s", ret, av_err2str(ret))
        if (helper)
            helper->onError(THREAD_TYPE_CHILD, FFMPEG_CAN_NOT_OPEN_URL, av_err2str(ret));
        return;
    }
    // 查找媒体中的音视频流的信息
    ret = avformat_find_stream_info(formatContext, nullptr);
    if (ret < 0) {
        LOGD("avformat_find_stream_info fail, ret:%d", ret)
        if (helper) {
            helper->onError(THREAD_TYPE_CHILD, FFMPEG_CAN_NOT_FIND_STREAMS, av_err2str(ret));
        }
        return;
    }
    //根据流信息，流的个数，用循环来找
    for (int stream_index = 0; stream_index < formatContext->nb_streams; ++stream_index) {
        //获取媒体流（视频，音频）
        AVStream *pStream = formatContext->streams[stream_index];
        //从上面的流中获取 编码解码的参数
        AVCodecParameters *pParameters = pStream->codecpar;
        //（根据上面的【参数】）获取编解码器
        //avcodec_find_encoder() //也可以获取编码器，只是这里暂时只用到解码器
        AVCodec *pCodec = avcodec_find_decoder(pParameters->codec_id);
        if (!pCodec) {
            LOGD("pCodec is null, ret:%d", ret)
            if (helper) {
                helper->onError(THREAD_TYPE_CHILD, FFMPEG_FIND_DECODER_FAIL, av_err2str(ret));
            }
            return;
        }
        //获取编解码器 上下文,此时pCodecContext is an AVCodecContext filled with default values
        AVCodecContext *pCodecContext = avcodec_alloc_context3(pCodec);
        if (!pCodecContext) {
            LOGD("codecContext is null, ret:%d", ret)
            if (helper) {
                helper->onError(THREAD_TYPE_CHILD, FFMPEG_ALLOC_CODEC_CONTEXT_FAIL,
                                av_err2str(ret));
            }
            return;
        }
        //将参数赋值给codecContext
        ret = avcodec_parameters_to_context(pCodecContext, pParameters);
        if (ret < 0) {
            LOGD("avcodec_parameters_to_context fail, ret:%d", ret)
            if (helper) {
                helper->onError(THREAD_TYPE_CHILD, FFMPEG_CODEC_CONTEXT_PARAMETERS_FAIL,
                                av_err2str(ret));
            }
            return;
        }
        //打开解码器
        ret = avcodec_open2(pCodecContext, pCodec, nullptr);
        if (ret) { //非0表示失败，主要是负值
            LOGD("avcodec_open2 fail, ret:%d", ret)
            if (helper) {
                helper->onError(THREAD_TYPE_CHILD, FFMPEG_OPEN_DECODER_FAIL, av_err2str(ret));
            }
            return;
        }
        // 从编解码器参数中，获取流的类型 codec_type === 音频 视频
        if (pParameters->codec_type == AVMediaType::AVMEDIA_TYPE_AUDIO) {
            audio_channel = new AudioChannel(stream_index, pCodecContext);
        } else if (pParameters->codec_type == AVMediaType::AVMEDIA_TYPE_VIDEO) {
            video_channel = new VideoChannel(stream_index, pCodecContext);
            video_channel->setRenderCallback(renderCallback);
        }
    }//for end

    //如果流中 没有 音频 也没有 视频
    if (!audio_channel && !video_channel) {
        LOGD(" params of channel fail, audio_channel:%p,video_channel:%p", audio_channel,
             video_channel)
        if (helper) {
            helper->onError(THREAD_TYPE_CHILD, FFMPEG_NOMEDIA, av_err2str(ret));
        }
        return;
    }

    if (helper) {
        LOGD(" call onPrepared ret:%d", ret)
        helper->onPrepared(THREAD_TYPE_CHILD);
    }
}

void *task_start(void *args) {
    PlayerEngine *player = static_cast<PlayerEngine *>(args);
    player->start_();

    return nullptr;
}

void PlayerEngine::start() {
    isPlaying = 1;

    // 视频：1.解码    2.播放
    // 1.把队列里面的压缩包(AVPacket *)取出来，然后解码成（AVFrame * ）原始包 ----> 保存队列
    // 2.把队列里面的原始包(AVFrame *)取出来， 视频播放
    if (video_channel) {
        video_channel->start();
    }
    // 音频：1.解码    2.播放
    // 1.把队列里面的压缩包(AVPacket *)取出来，然后解码成（AVFrame * ）原始包 ----> 保存队列
    // 2.把队列里面的原始包(AVFrame *)取出来， 音频播放
    if (audio_channel) {
        audio_channel->start();
    }
    pthread_create(&pid_start, nullptr, task_start, this);// this == PlayerEngine的实例
}

void PlayerEngine::start_() { // 子线程 把 AVPacket * 丢到 队列里面去  不区分 音频 视频
    while (isPlaying) {// AVPacket 可能是音频 也可能是视频（压缩包）
        //控制packet队列大小，等待队列中的数据被消费, 优化内存占用
        if (video_channel && video_channel->packets.size() > 100) {
            av_usleep(10 * 1000);//睡眠10毫秒 单位：microseconds 微妙
            continue;
        }
        if (audio_channel && audio_channel->packets.size() > 100) {
            av_usleep(10 * 1000);//
            continue;
        }

        AVPacket *packet = av_packet_alloc();
        int ret = av_read_frame(formatContext, packet);
        if (!ret) { //0 == success
            if (video_channel && video_channel->stream_index == packet->stream_index) {
                //注意这里传入的packet是指针变量
                video_channel->packets.insertToQueue(packet);
            } else if (audio_channel && audio_channel->stream_index == packet->stream_index) {
                audio_channel->packets.insertToQueue(packet);
            }
        } else if (ret == AVERROR_EOF) {
            // TODO 表示读完了，要考虑是否播放完成，表示读完了 并不代表播放完毕
        } else { //read error 出现了错误，结束当前循环
            break;
        }
    }//end while
    isPlaying = 0;
    if (video_channel)
        video_channel->stop();
    if (audio_channel)
        audio_channel->stop();
}

void PlayerEngine::setRenderCallback(RenderCallback renderCallback) {
    this->renderCallback = renderCallback;
}
