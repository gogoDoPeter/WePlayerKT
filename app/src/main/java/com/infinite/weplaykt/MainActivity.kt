package com.infinite.weplaykt

import android.Manifest
import android.content.pm.PackageManager
import android.graphics.Color
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.os.Environment
import android.util.Log
import android.view.SurfaceView
import android.view.View
import android.widget.SeekBar
import android.widget.TextView
import android.widget.Toast
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.lifecycle.Lifecycle
import com.infinite.weplaykt.databinding.ActivityMainBinding
import java.io.File

class MainActivity : AppCompatActivity(), SeekBar.OnSeekBarChangeListener {
    private var TAG: String = "MainActivity"
    private lateinit var binding: ActivityMainBinding
    private var tvState: TextView? = null
    private var player: PlayerEngine? = null
    private var surfaceView: SurfaceView? = null

    private var seekBar: SeekBar? = null
    private var tvTime: TextView? = null
    private var isTouch = false
    private var duration = 0

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)
        tvState = binding.tvState
        surfaceView = binding.surfaceView

        tvTime = binding.tvTime
        seekBar = binding.seekBar
        seekBar?.setOnSeekBarChangeListener(this)

        player = PlayerEngine()
        lifecycle.addObserver(player!!)// MainActivity做为被观察者，与PlayerEngine观察者建立绑定关系

        player?.setSurfaceView(surfaceView!!)
        player?.setDataSource(File(Environment.getExternalStorageDirectory(),
            "demo.mp4").absolutePath)

//        player?.setDataSource(File(Environment.getExternalStorageDirectory(),
//            "aabb/memory.mkv").absolutePath)

        // 准备成功的回调处,有C++子线程调用的
        player!!.setOnPreparedListener(object : PlayerEngine.OnPreparedListener {
            override fun onPrepared() {
                //得到视频总时长 duration=0是直播，有其他值表示本地视频
                duration = player!!.duration
                runOnUiThread {
                    if (duration != 0) {
                        //显示所有托动条控件UI View， 非直播视频，也就是本地视频
                        tvTime!!.text = "00:00/" + getMinutes(duration) + ":" + getSeconds(duration)
                        tvTime?.visibility = View.VISIBLE //显示
                        seekBar!!.visibility = View.VISIBLE //显示
                    }
//                    Toast.makeText(this@MainActivity, "准备成功，开始播放", Toast.LENGTH_LONG).show()
                    tvState?.setTextColor(Color.GREEN)
                    tvState!!.text = "init success"
                }
                player!!.start()
            }

        })

        player!!.setOnErrorListener(object : PlayerEngine.OnErrorListener {
            override fun onError(errorMsg: String?) {
                runOnUiThread {
                    tvState?.setTextColor(Color.RED)
                    tvState!!.text = errorMsg
                }
            }

        })

        player!!.setOnProgressListener(object : PlayerEngine.OnProgressListener {
            override fun onProgress(playProgress: Int) {
                if (!isTouch) { //如果没有托动时，实时显示播放进度
                    runOnUiThread { //C++层是异步线程调用上来的，所以要这样写，小心UI
                        if (duration != 0) {
                            //playProgress 是C++层中ffmpeg获取的当前播放时间，单位是秒
                            tvTime!!.text =
                                (getMinutes(playProgress)) + ":" + getSeconds(playProgress) + "/" +
                                        getMinutes(duration) + ":" + getSeconds(duration)
                            //获取托动条seekBar当前进度相对于总时长的百分比
                            seekBar!!.progress = playProgress * 100 / duration
                        }
                    }
                }
            }

        })
        checkPermission()
    }

    private fun getMinutes(duration: Int): String {
        val minutes = duration / 60
        return if (minutes <= 9) {
            "0$minutes"
        } else "" + minutes
    }

    private fun getSeconds(duration: Int): String {
        val seconds = duration % 60
        return if (seconds <= 9) {
            "0$seconds"
        } else "" + seconds
    }

    /**
     * 当前托动条进度发生了改变回调此函数
     * @param seekBar SeekBar 控件
     * @param progress Int 1~100
     * @param fromUser Boolean 是否用户拖拽导致的改变
     */
    override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
        if (fromUser) {
            tvTime!!.text = (getMinutes(progress * duration / 100) + ":" +
                    getSeconds(progress * duration / 100) + "/" +
                    getMinutes(duration) + ":" + getSeconds(duration))
        }
    }

    //手按下去回调此函数
    override fun onStartTrackingTouch(seekBar: SeekBar?) {
        isTouch = true
    }

    //手抬起回调此函数
    override fun onStopTrackingTouch(seekBar: SeekBar) {
        isTouch = false
        //获取当前seekBar进度
        val currentProgress = seekBar.progress
        val playProgress = currentProgress * duration / 100
        player!!.seek(playProgress)
    }

    private var permissions = arrayOf<String>(Manifest.permission.WRITE_EXTERNAL_STORAGE)
    var mPermissionList: MutableList<String> = ArrayList()
    private val PERMISSION_REQUEST = 1

    private fun checkPermission() {
        mPermissionList.clear()
        for (permission in permissions) {
            if (ContextCompat.checkSelfPermission(this,
                    permission) != PackageManager.PERMISSION_GRANTED
            ) {
                mPermissionList.add(permission)
            }
        }
        if (!mPermissionList.isEmpty()) {
            val reqPermissions = mPermissionList.toTypedArray()//将List转为数组
            ActivityCompat.requestPermissions(this, reqPermissions, PERMISSION_REQUEST)
        }
    }

    /**
     * 响应授权
     * 这里不管用户是否拒绝，都进入首页，不再重复申请权限
     */
    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<out String>,
        grantResults: IntArray,
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        when (requestCode) {
            PERMISSION_REQUEST -> {}
            else -> super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        }
    }

    override fun onResume() {
        super.onResume()
        Log.d(TAG, "onResume")
    }

    override fun onStop() {
        super.onStop()
        Log.d(TAG, "onStop")
    }

    override fun onDestroy() {
        super.onDestroy()
        Log.d(TAG, "onDestroy")
    }

}