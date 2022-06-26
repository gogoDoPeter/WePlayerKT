//
// Created by Peter Liu on 2022/6/18 0018.
//

#include <chrono>
#include "PlayerEngine.h"

PlayerEngine::PlayerEngine(const char *data_source, JNICallbackHelper *helper) {
    // this->data_source = data_source;
    // data_source如果被释放，会造成悬空指针  坑1

    this->data_source = new char[strlen(data_source) + 1];
    strcpy(this->data_source, data_source);

    this->helper = helper;

    pthread_mutex_init(&seek_mutex, nullptr);
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
    pthread_mutex_destroy(&seek_mutex);
}


void *task_stop(void *args){
    auto *player= static_cast<PlayerEngine *>(args);
    player->stop_(player);
    return nullptr;
}

//在子线程中等待其他线程稳稳结束，释放资源
void PlayerEngine::stop_(PlayerEngine *pPlayer) {
    isPlaying = false; //停掉所有的while循环，然后会调用audio和videoChannel的stop函数
    pthread_join(pid_prepare, nullptr);
    pthread_join(pid_start, nullptr);
    if (formatContext) {
        avformat_close_input(&formatContext);
        avformat_free_context(formatContext);
        formatContext = nullptr;
    }
    DELETE(audio_channel);
    DELETE(video_channel);
}

void PlayerEngine::stop() {
    //只要用户点击关闭，就不能回调给java或start播放
    if (helper) {
        delete helper;
        helper = nullptr;
    }
    if (audio_channel) {
        DELETE(audio_channel->jniCallbackHelper);
    }
    if (video_channel) {
        DELETE(video_channel->jniCallbackHelper);
    }
    //注意这里是主线程调用的stop函数，不能在这里等线程结束，否则如果线程要5分钟结束，要等待5分钟会造成ANR
    //要等prepare和start两个线程停下来后，再释放相关工作，不等的话可能ANR异常
    pthread_create(&pid_stop, nullptr, task_stop, this);
}

//TODO release 中需要做那些工作？
void PlayerEngine::release() {

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
    /** 第一步
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
        avformat_close_input(&formatContext);
        return;
    }
    // 第二步 查找媒体中的音视频流的信息,这个是流探索，拿到媒体文件的所有信息
    ret = avformat_find_stream_info(formatContext, nullptr);
    if (ret < 0) {
        LOGD("avformat_find_stream_info fail, ret:%d", ret)
        if (helper) {
            helper->onError(THREAD_TYPE_CHILD, FFMPEG_CAN_NOT_FIND_STREAMS, av_err2str(ret));
        }
        avformat_close_input(&formatContext);
        return;
    }
    //如果放在avformat_open_input后面拿，MP4格式可以拿到，FLV格式拿不到，因为flv视频的总时长信息在流中间，需要经过流探索avformat_find_stream_info后才能获取
    this->duration = formatContext->duration / AV_TIME_BASE;
    AVCodecContext *pCodecContext = nullptr;
    // 第三步 根据流信息，流的个数，用循环来找
    for (int stream_index = 0; stream_index < formatContext->nb_streams; ++stream_index) {
        // 第四步 获取媒体流（视频，音频）
        AVStream *pStream = formatContext->streams[stream_index];
        // 第五步 从上面的流中获取 编码解码的参数
        AVCodecParameters *pParameters = pStream->codecpar;
        // 第六步 （根据上面的【参数】）获取编解码器
        //avcodec_find_encoder() //也可以获取编码器，只是这里暂时只用到解码器
        AVCodec *pCodec = avcodec_find_decoder(pParameters->codec_id);
        if (!pCodec) {
            LOGD("pCodec is null, ret:%d", ret)
            if (helper) {
                helper->onError(THREAD_TYPE_CHILD, FFMPEG_FIND_DECODER_FAIL, av_err2str(ret));
            }
            avformat_close_input(&formatContext);
            return;
        }
        // 第七步 获取编解码器 上下文,此时pCodecContext is an AVCodecContext filled with default values
        pCodecContext = avcodec_alloc_context3(pCodec);
        if (!pCodecContext) {
            LOGD("codecContext is null, ret:%d", ret)
            if (helper) {
                helper->onError(THREAD_TYPE_CHILD, FFMPEG_ALLOC_CODEC_CONTEXT_FAIL,
                                av_err2str(ret));
            }
            avcodec_free_context(&pCodecContext); //释放此上下文codecContext，它会考虑把codec指针一起释放
            avformat_close_input(&formatContext);
            return;
        }
        // 第八步 将参数（AVCodecParameters）赋值给codecContext
        ret = avcodec_parameters_to_context(pCodecContext, pParameters);
        if (ret < 0) {
            LOGD("avcodec_parameters_to_context fail, ret:%d", ret)
            if (helper) {
                helper->onError(THREAD_TYPE_CHILD, FFMPEG_CODEC_CONTEXT_PARAMETERS_FAIL,
                                av_err2str(ret));
            }
            avcodec_free_context(
                    &pCodecContext); //释放此上下文codecContext，它会考虑把codec指针和 AVCodecParameters 一起释放
            avformat_close_input(&formatContext);
            return;
        }
        // 第九步 打开解码器
        ret = avcodec_open2(pCodecContext, pCodec, nullptr);
        if (ret) { //非0表示失败，主要是负值
            LOGD("avcodec_open2 fail, ret:%d", ret)
            if (helper) {
                helper->onError(THREAD_TYPE_CHILD, FFMPEG_OPEN_DECODER_FAIL, av_err2str(ret));
            }
            avcodec_free_context(
                    &pCodecContext); //释放此上下文codecContext，它会考虑把codec指针和 AVCodecParameters 一起释放
            avformat_close_input(&formatContext);
            return;
        }

        AVRational time_base = pStream->time_base;
        // 第十步 从编解码器参数中，获取流的类型 codec_type === 音频 视频
        if (pParameters->codec_type == AVMediaType::AVMEDIA_TYPE_AUDIO) {
            audio_channel = new AudioChannel(stream_index, pCodecContext, time_base);
            if (this->duration != 0) { //只有非直播才有意义把它传递过去，用于更新当前播放进度
                audio_channel->setJNICallbackHelper(helper);
            }
        } else if (pParameters->codec_type == AVMediaType::AVMEDIA_TYPE_VIDEO) {
            // 虽然是视频类型，但是只有一帧封面，这个不应该参与 视频 解码 和 播放，而是跳过或做对应专门处理
            if (pStream->disposition & AV_DISPOSITION_ATTACHED_PIC) {
                continue; // 过滤 封面流
            }

            // 获取视频独有的fps值
            AVRational fps_rational = pStream->avg_frame_rate;
            int fps = av_q2d(fps_rational); //转成ffmpeg支持的时间基
            video_channel = new VideoChannel(stream_index, pCodecContext, time_base, fps);
            video_channel->setRenderCallback(renderCallback);

            if (this->duration != 0) { //只有非直播才有意义把它传递过去，给后续扩展功能用
                video_channel->setJNICallbackHelper(helper);
            }
        }
    } //for end

    // 第十一步 参数判断 如果流中 没有 音频 也没有 视频
    if (!audio_channel && !video_channel) {
        LOGD(" params of channel fail, audio_channel:%p,video_channel:%p", audio_channel,
             video_channel)
        if (helper) {
            helper->onError(THREAD_TYPE_CHILD, FFMPEG_NOMEDIA, av_err2str(ret));
        }
        if (pCodecContext) {
            avcodec_free_context(&pCodecContext);
        }
        avformat_close_input(&formatContext);
        return;
    }
    // 第十二步 准备成功，媒体文件的解封装成功完成，通知上层
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
    isPlaying = true;

    // 视频：1.解码    2.播放
    // 1.把队列里面的压缩包(AVPacket *)取出来，然后解码成（AVFrame * ）原始包 ----> 保存队列
    // 2.把队列里面的原始包(AVFrame *)取出来， 视频播放
    if (video_channel) {
        video_channel->setAudioChannel(audio_channel);
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
    } //end while
    isPlaying = false;
    //一旦停止循环，将isPlaying置为false，调用stop
    if (video_channel)
        video_channel->stop();
    if (audio_channel)
        audio_channel->stop();
}

void PlayerEngine::setRenderCallback(RenderCallback renderCallback) {
    this->renderCallback = renderCallback;
}

int PlayerEngine::getDuration() {
    return duration;
}

void PlayerEngine::seek(int progress) {
    //输入参数判断
    if (progress < 0 || progress > duration) {
        //TODO 打印错误并给java回调
        return;
    }
    if (!audio_channel && !video_channel) {
        //TODO 打印错误并给java回调
        return;
    }
    if (!formatContext) {
        //TODO 打印错误并给java回调
        return;
    }
    //formatContext 多线程用到，在prepare_()解封装的12步中会用到formatContext，av_seek_frame内部也会多个线程用到formatContext，要考虑多线程安全的问题
    // av_seek_frame内部会开几个线程对我们的formatContext上下文的成员做处理来完成seek功能，使用互斥锁，保证多线程情况下安全
    pthread_mutex_lock(&seek_mutex);
    int ret = av_seek_frame(formatContext, -1, progress * AV_TIME_BASE,
                            AVSEEK_FLAG_FRAME); //按关键帧来定位(非常不准确，可能会跳的太多)
    if (ret < 0) {
        //TODO 打印错误并给java回调
        pthread_mutex_unlock(&seek_mutex);
        return;
    }

    //如果音视频正在播放，用户做seek，应该停掉播放的数据，包括：音频 1frames， 1packets 和 视频 1frames， 1packets
    //seek时让四个队列停下来，seek完后重新播放
    if (audio_channel) {
        audio_channel->packets.setWork(0);
        audio_channel->frames.setWork(0);
        audio_channel->packets.clear();
        audio_channel->frames.clear();
        audio_channel->packets.setWork(1);
        audio_channel->frames.setWork(1);
    }
    if (video_channel) {
        video_channel->packets.setWork(0);
        video_channel->frames.setWork(0);
        video_channel->packets.clear();
        video_channel->frames.clear();
        video_channel->packets.setWork(1);
        video_channel->frames.setWork(1);
    }

    pthread_mutex_unlock(&seek_mutex);
}

