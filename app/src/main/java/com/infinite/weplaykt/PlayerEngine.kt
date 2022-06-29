package com.infinite.weplaykt

import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.OnLifecycleEvent
import com.infinite.weplaykt.util.FFMPGE

class PlayerEngine : LifecycleObserver, SurfaceHolder.Callback {

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
    private var surfaceHolder: SurfaceHolder? = null

    //获取总时长  这个duration是属性，有隐式的get函数
    val duration: Int get() = getDurationNative(nativeObj!!)

//    private fun getDurationNative(nativeObj: Long): Int {
//        TODO("Not yet implemented")
//    }

    fun seek(playProgress: Int) {
        seekNative(playProgress, nativeObj!!)
    }

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

    /**
     * Pause media play
     */
    fun pauseMedia() {
        pauseNative(nativeObj!!)
    }

    /**
     * Start media play again
     */
    fun playMedia() {
        playNative(nativeObj!!)
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
                FFMPGE.FFMPEG_CODEC_CONTEXT_PARAMETERS_FAIL -> msg =
                    "根据流信息 配置上下文参数失败$title$ffmpegError"
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

    fun onProgress(progress: Int) {
        if (onProgressListener != null) {
            onProgressListener!!.onProgress(progress)
        }
    }

    private var onProgressListener: OnProgressListener? = null

    interface OnProgressListener {
        fun onProgress(progress: Int)
    }

    fun setOnProgressListener(onProgressListener: OnProgressListener?) {
        this.onProgressListener = onProgressListener
    }

    /**
     * set setSurfaceView
     * @param surfaceView
     */
    fun setSurfaceView(surfaceView: SurfaceView) {
        if (surfaceHolder != null) {
            surfaceHolder?.removeCallback(this) // 清除上一次的
        }
        surfaceHolder = surfaceView.holder
        surfaceHolder?.addCallback(this) // 监听
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        // 之前开发，就是写这里来调用 setSurfaceNative，注意在surfaceChanged中调用来设置更好
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        setSurfaceNative(holder.surface, nativeObj!!)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
    }

    private external fun prepareNative(dataSource: String): Long
    private external fun startNative(nativeObj: Long)
    private external fun stopNative(nativeObj: Long)
    private external fun releaseNative(nativeObj: Long)
    private external fun setSurfaceNative(surface: Surface, nativeObj: Long)

    private external fun getDurationNative(nativeObj: Long): Int
    private external fun seekNative(playValue: Int, nativeObj: Long)

    private external fun pauseNative(nativeObj: Long)
    private external fun playNative(nativeObj: Long)
}