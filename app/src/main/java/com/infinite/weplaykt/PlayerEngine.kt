package com.infinite.weplaykt

import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.OnLifecycleEvent
import com.infinite.weplaykt.util.FFMPGE

class PlayerEngine : LifecycleObserver {

    companion object {
        // Used to load the 'weplaykt' library on application startup.
        init {
            System.loadLibrary("weplaykt")
        }
    }

    private var nativeObj: Long? = null // 保存PlayerEngine.cpp对象的地址
    private var onPreparedListener: OnPreparedListener? = null
    private var onErrorListener: OnErrorListener? = null
    private var TAG: String = "PlayerEngine"
    private var dataSource: String? = null

    fun setDataSource(dataSource: String?) {
        this.dataSource = dataSource
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_RESUME)
    fun prepare() {
        nativeObj = prepareNative(dataSource!!)
    }

    fun start() {
        startNative(nativeObj!!)
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_STOP)
    fun stop() {
        stopNative(nativeObj!!)
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_DESTROY)
    fun release() {
        releaseNative(nativeObj!!)
    }

    //对视频文件读取成功后给jni反射调用的
    fun onPrepared() {
        if (onPreparedListener != null) {
            onPreparedListener!!.onPrepared()
        }
    }

    fun onError(errorCode: Int, ffmpegError: String) {
        val title = "\nFFmpeg给出的错误如下:\n"
        if (null != onErrorListener) {
            var msg: String? = null
            when (errorCode) {
                FFMPGE.FFMPEG_CAN_NOT_OPEN_URL -> msg = "打不开视频$title$ffmpegError"
                FFMPGE.FFMPEG_CAN_NOT_FIND_STREAMS -> msg = "找不到流媒体$title$ffmpegError"
                FFMPGE.FFMPEG_FIND_DECODER_FAIL -> msg = "找不到解码器$title$ffmpegError"
                FFMPGE.FFMPEG_ALLOC_CODEC_CONTEXT_FAIL -> msg = "无法根据解码器创建上下文$title$ffmpegError"
                FFMPGE.FFMPEG_CODEC_CONTEXT_PARAMETERS_FAIL -> msg = "根据流信息 配置上下文参数失败$title$ffmpegError"
                FFMPGE.FFMPEG_OPEN_DECODER_FAIL -> msg = "打开解码器失败$title$ffmpegError"
                FFMPGE.FFMPEG_NOMEDIA -> msg = "没有音视频$title$ffmpegError"
            }
            onErrorListener!!.onError(msg)
        }
    }

    interface OnPreparedListener {
        fun onPrepared()
    }

    interface OnErrorListener {
        fun onError(errorMsg: String?)
    }

    fun setOnPreparedListener(onPreparedListener: OnPreparedListener?) {
        this.onPreparedListener = onPreparedListener
    }

    fun setOnErrorListener(onErrorListener: OnErrorListener?) {
        this.onErrorListener = onErrorListener
    }

    private external fun prepareNative(dataSource: String): Long
    private external fun startNative(nativeObj: Long)
    private external fun stopNative(nativeObj: Long)
    private external fun releaseNative(nativeObj: Long)
}