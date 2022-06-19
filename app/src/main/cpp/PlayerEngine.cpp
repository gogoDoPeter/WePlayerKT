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
    }
    if (helper) {
        delete this->helper;
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
    int ret = avformat_open_input(&formatContext, this->data_source, nullptr, &dictionary);
    av_dict_free(&dictionary);

    if (ret < 0) { //0 == success
        LOGD("avformat_open_input fail, ret:%d, %s",ret, av_err2str(ret))
        if(helper)
            helper->onError(THREAD_TYPE_CHILD, FFMPEG_CAN_NOT_OPEN_URL, av_err2str(ret));
        return;
    }
    // 查找媒体中的音视频流的信息
    ret = avformat_find_stream_info(formatContext, nullptr);
    if (ret < 0) {
        LOGD("avformat_find_stream_info fail, ret:%d",ret)
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
            LOGD("pCodec is null, ret:%d",ret)
            if (helper) {
                helper->onError(THREAD_TYPE_CHILD, FFMPEG_FIND_DECODER_FAIL, av_err2str(ret));
            }
            return;
        }
        //获取编解码器 上下文,此时pCodecContext is an AVCodecContext filled with default values
        AVCodecContext *pCodecContext = avcodec_alloc_context3(pCodec);
        if (!pCodecContext) {
            LOGD("pCodecContext is null, ret:%d",ret)
            if (helper) {
                helper->onError(THREAD_TYPE_CHILD, FFMPEG_ALLOC_CODEC_CONTEXT_FAIL, av_err2str(ret));
            }
            return;
        }
        //将参数赋值给codecContext
        ret = avcodec_parameters_to_context(pCodecContext, pParameters);
        if (ret < 0) {
            LOGD("avcodec_parameters_to_context fail, ret:%d",ret)
            if (helper) {
                helper->onError(THREAD_TYPE_CHILD, FFMPEG_CODEC_CONTEXT_PARAMETERS_FAIL, av_err2str(ret));
            }
            return;
        }
        //打开解码器
        ret = avcodec_open2(pCodecContext, pCodec, nullptr);
        if (ret) { //非0表示失败，主要是负值
            LOGD("avcodec_open2 fail, ret:%d",ret)
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
        }
    }//for end

    //如果流中 没有 音频 也没有 视频
    if (!audio_channel && !video_channel) {
        LOGD(" params of channel fail, audio_channel:%p,video_channel:%p",audio_channel,video_channel)
        if (helper) {
            helper->onError(THREAD_TYPE_CHILD, FFMPEG_NOMEDIA, av_err2str(ret));
        }
        return;
    }

    if(helper){
        LOGD(" call onPrepared ret:%d",ret)
        helper->onPrepared(THREAD_TYPE_CHILD);
    }
}
