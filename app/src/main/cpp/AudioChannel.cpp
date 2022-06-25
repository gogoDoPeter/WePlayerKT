//
// Created by Peter Liu on 2022/6/18 0018.
//

#include <LogUtils.h>
#include "AudioChannel.h"

AudioChannel::AudioChannel(int stream_index, AVCodecContext *codecContext)
        : BaseChannel(stream_index, codecContext) {
    out_channels = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO); //获取 声道数 2
    // AV_SAMPLE_FMT_S16: 位声、采用格式大小，存放大小
    out_sample_size = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16); //采样格式大小
    out_sample_rate = 44100; // 采样率,单位Hz
    // out_buffers_size = 176,400
    out_buffers_size = out_channels * out_sample_size * out_sample_rate;
    out_buffers = static_cast<uint8_t *>(malloc(out_buffers_size));

    //新建并初始化音频重采样上下文
    swr_ctx = swr_alloc_set_opts(swr_ctx, //0,
            //输出参数
                                 AV_CH_LAYOUT_STEREO, // 声道布局类型
                                 AV_SAMPLE_FMT_S16, // 采样大小
                                 out_sample_rate, // 采样率
            //输入参数
                                 codecContext->channel_layout,// 声道布局类型
                                 codecContext->sample_fmt,// 采样大小 32bit  aac
                                 codecContext->sample_rate, // 采样率  48000 or 44100?
                                 0, 0);
    swr_init(swr_ctx);
    LOGD("AudioChannel out buffers_size:%d, channels:%d, sample_size:%d, sample_rate:%d"
         " \n in channels:%d, sample_size:%d, sample_rate:%d",
         out_buffers_size, out_channels, out_sample_size, out_sample_rate,
         codecContext->channel_layout, codecContext->sample_fmt, codecContext->sample_rate)
}

AudioChannel::~AudioChannel() {

}

void *task_audio_decode(void *args) {
    AudioChannel *audio_channel = static_cast<AudioChannel *>(args);
    audio_channel->audio_decode();

    return nullptr;
}

void AudioChannel::audio_decode() {
    AVPacket *pkt = nullptr;
    while (isPlaying) {
        int ret = packets.getQueueAndDel(pkt);
        if (!isPlaying) {
            break;
        }
        if (!ret) { // 1== success, if fail
            continue; //如果生产太慢(压缩包加入队列)的情况，消费就等一下
        }
        // 第一步：把压缩包 AVPack发送给 FFmpeg缓存区
        ret = avcodec_send_packet(codecContext, pkt);
        releaseAVPacket(&pkt);//FFmpeg源码内部 缓存了一份pkt副本，所以这里可以直接释放
        if (ret) { // 0 == success, if not 0 fail
            break;
        }
        // 第二步：读取 FFmpeg缓存区 里面的原始包. 有可能读不到，为什么？ 内部缓冲区 会运作过程比较慢
        AVFrame *frame = av_frame_alloc();
        ret = avcodec_receive_frame(codecContext, frame);
        if (ret == AVERROR(EAGAIN)) {
            continue; // 有可能音频帧，也会获取失败，重新拿一次
        } else if (ret != 0) {
            break; // avcodec_receive_frame失败了，注意刚刚通过av_frame_alloc分配的frame是否要释放?
        }
        //拿到原始包，加入队列中，原始包是pcm数据
        frames.insertToQueue(frame);
    }
    releaseAVPacket(&pkt);
}

// 此函数会一直被 缓存队列bq 来调用，注意不是靠while循环拿队列那数据的
int AudioChannel::getPcmAndSize() {
    int pcm_data_size = 0;
    AVFrame *frame = nullptr;
    while (isPlaying) { //只执行一次
        int ret = frames.getQueueAndDel(frame);
        if (!isPlaying) {
            break;
        }
        if (!ret) {
            continue;
        }
        // 来源：10个48000   ---->  目标:44100  11个44100
        int dst_nb_samples = av_rescale_rnd(
                swr_get_delay(swr_ctx, frame->sample_rate) +
                frame->nb_samples,// 获取下一个输入样本相对于下一个输出样本将经历的延迟
                out_sample_rate,// 输出采样率
                frame->sample_rate,// 输入采样率
                AV_ROUND_UP);// 向上取 取去11个才能容纳的上
        // 单通道样本数 1024  （基于常规：44100Hz， 16bit 和 2声道的情况下）
        // 获取单通道的样本数 (计算目标样本数： ？ 10个48000 --->  48000/44100因为除不尽  11个44100)
        // 返回的结果：每个通道输出的样本数(注意：是转换后的)    如果做一个简单的重采样实验,通道基本上都是1024
        int samples_per_channel = swr_convert(swr_ctx,
                // 下面是输出区域
                                              &out_buffers,
                                              dst_nb_samples, //单通道的样本数 无法与out_buffers对应(数值不够精确)，用下面的pcm_data_size重新计算
                // 下面是输入区域
                                              (const uint8_t **) frame->data,
                                              frame->nb_samples);// 输入的样本数
        // 由于out_buffers 和 dst_nb_samples 无法对应，所以需要重新计算
        pcm_data_size = samples_per_channel * out_sample_size * out_channels;
        // in frame->sample_rate:48000, frame->nb_samples(输入的样本数):1024 using demo.mp4
//        LOGD("getPcmAndSize: pcm_data_size:%d,samples_per_channel:%d,out_sample_size:%d,out_channels:%d, out_sample_rate:%d"
//             " \n in frame->sample_rate:%d, frame->nb_samples(输入的样本数):%d",
//             pcm_data_size, samples_per_channel, out_sample_size, out_channels, out_sample_rate,
//             frame->sample_rate, frame->nb_samples)
        break;
    }
    return pcm_data_size;
}

/**
 * 4.3 回调函数
 * @param bq  队列
 * @param args  this // 给回调函数的参数
 */
void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *args) {
    auto *audio_channel = static_cast<AudioChannel *>(args);
    int pcm_size = audio_channel->getPcmAndSize();
    (*bq)->Enqueue(bq,
                   audio_channel->out_buffers,// PCM数据 拿到有重采样后的数据，就可以播放了
                   pcm_size);
}

void AudioChannel::audio_play() {
    SLresult result;
    //1.创建引擎对象并获取【引擎接口】
    //1.1 创建引擎对象：SLObjectItf engineObject
    result = slCreateEngine(&engineObject, 0, 0, 0, 0, 0);
    if (SL_RESULT_SUCCESS != result) {
        LOGE("create engine error")
        return;
    }
    // 1.2 初始化引擎
    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);// SL_BOOLEAN_FALSE:延时等待你创建成功
    if (SL_RESULT_SUCCESS != result) {
        LOGE("Realize engine error")
        return;
    }
    // 1.3 获取引擎接口
    result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineInterface);
    if (SL_RESULT_SUCCESS != result) {
        LOGE("engine getInterface error")
        return;
    }
    // 1.4 是否获取成功判断
    if (engineInterface) {
        LOGD("Get engine Interface success")
    } else {
        LOGE("Get engine Interface error")
        return;
    }
    //2.设置混音器 // 2.1 创建混音器
    result = (*engineInterface)->CreateOutputMix(engineInterface, &outputMixObject, 0, 0, 0);
    if (SL_RESULT_SUCCESS != result) {
        LOGE("CreateOutputMix error")
        return;
    }// 2.2 初始化混音器
    result = (*outputMixObject)->Realize(outputMixObject,
                                         SL_BOOLEAN_FALSE);    // SL_BOOLEAN_FALSE:延时等待你创建成功
    if (SL_RESULT_SUCCESS != result) {
        LOGE("outputMixObject realize error")
        return;
    }

    // 不启用混响可以不用获取混音器接口 【声音的效果】
    // 2.3 获得混音器接口
    /*
    result = (*outputMixObject)->GetInterface(outputMixObject, SL_IID_ENVIRONMENTALREVERB,
                                             &outputMixEnvironmentalReverb);
    if (SL_RESULT_SUCCESS == result) {
    // 设置混响 ： 默认。
    SL_I3DL2_ENVIRONMENT_PRESET_ROOM: 室内
    SL_I3DL2_ENVIRONMENT_PRESET_AUDITORIUM : 礼堂 等
    const SLEnvironmentalReverbSettings settings = SL_I3DL2_ENVIRONMENT_PRESET_DEFAULT;
    (*outputMixEnvironmentalReverb)->SetEnvironmentalReverbProperties(
           outputMixEnvironmentalReverb, &settings);
    }
    */
    LOGD("Create outputMixObject success")

    //3.创建播放器 // 3.1 创建buffer缓存类型的队列  参数2表示队列大小
    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 10};
    // PCM数据格式 == PCM是不能直接播放，mp3可以直接播放(头文件中有音频参数集，所以可以播放),这里就是配置音频参数集
    SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM,
                                   2,
                                   SL_SAMPLINGRATE_44_1,
                                   SL_PCMSAMPLEFORMAT_FIXED_16,// 每秒采样样本存放大小 16bit
                                   SL_PCMSAMPLEFORMAT_FIXED_16,// 每个样本位数 16bit
                                   SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
                                   SL_BYTEORDER_LITTLEENDIAN};// 字节序(小端) 一般用小端字节序

    // 数据源 将上述配置信息放到这个数据源中
    // audioSrc最终配置音频信息的成果，给后面代码使用
    SLDataSource audioSrc = {&loc_bufq, &format_pcm};

    // 3.2 配置音轨（输出）
    // 设置混音器
    SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX,
                                          outputMixObject};// SL_DATALOCATOR_OUTPUTMIX:输出混音器类型
    SLDataSink audioSink = {&loc_outmix, NULL};// outmix最终混音器的成果，给后面代码使用
    // 需要的接口 操作队列的接口
    const SLInterfaceID ids[1] = {SL_IID_BUFFERQUEUE};
    const SLboolean req[1] = {SL_BOOLEAN_TRUE};
    // 3.3 创建播放器 SLObjectItf bqPlayerObject
    result = (*engineInterface)->CreateAudioPlayer(engineInterface, // 参数1：引擎接口
                                                   &bqPlayerObject,// 参数2：播放器
                                                   &audioSrc,// 参数3：音频配置信息
                                                   &audioSink,// 参数4：混音器
                                                   1,// 参数5：开放的参数的个数
                                                   ids,// 参数6：代表我们需要 Buff
                                                   req);//// 参数7：代表我们上面的Buff 需要开放出去
    if (SL_RESULT_SUCCESS != result) {
        LOGE("CreateAudioPlayer error")
        return;
    }
    // 3.4 初始化播放器：SLObjectItf bqPlayerObject
    result = (*bqPlayerObject)->Realize(bqPlayerObject,
                                        SL_BOOLEAN_FALSE);// SL_BOOLEAN_FALSE:延时等待你创建成功
    if (SL_RESULT_SUCCESS != result) {
        LOGE("Initialize AudioPlayer error")
        return;
    }
    // 3.5 获取播放器接口  [以后播放全部使用 播放器接口去干,这里是重点]
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_PLAY,
                                             &bqPlayerPlay);// SL_IID_PLAY:播放接口
    if (SL_RESULT_SUCCESS != result) {
        LOGE("bqPlayerObject GetInterface error")
        return;
    }
    LOGD("Create Player success")
    // 4.设置回调函数
    // 4.1 获取播放器队列接口：SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue  // 播放需要的队列
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_BUFFERQUEUE,
                                             &bqPlayerBufferQueue);
    if (SL_RESULT_SUCCESS != result) {
        LOGE("GetInterface SL_IID_BUFFERQUEUE error")
        return;
    }
    // 4.2 设置回调 void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
    (*bqPlayerBufferQueue)->RegisterCallback(bqPlayerBufferQueue,
                                             bqPlayerCallback,// 回调函数
                                             this);//传给回调函数的参数
    LOGD("RegisterCallback success")
    // 5、设置播放器状态为播放状态
    (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PLAYING);
    LOGD("set Playing Status success")
    // 6、手动激活回调函数  调用一次回调函数
    bqPlayerCallback(bqPlayerBufferQueue, this);
    LOGD("call once bqPlayerCallback success")
}

void *task_audio_play(void *args) {
    AudioChannel *audio_channel = static_cast<AudioChannel *>(args);
    audio_channel->audio_play();

    return nullptr;
}

void AudioChannel::start() {
    isPlaying = 1;

    packets.setWork(1);
    frames.setWork(1);

    // 第一个线程： 音频：取出队列的压缩包 进行解码 解码后的原始包 再push队列中去（音频：PCM数据）
    pthread_create(&pid_audio_decode, nullptr, task_audio_decode, this);
    // 第二线线程：音频：从队列取出原始包，播放
    pthread_create(&pid_audio_play, nullptr, task_audio_play, this);
}

void AudioChannel::stop() {

}






