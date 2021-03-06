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
import android.widget.Button
import android.widget.SeekBar
import android.widget.TextView
import android.widget.Toast
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.lifecycle.Lifecycle
import com.infinite.weplaykt.databinding.ActivityMainBinding
import java.io.File

class MainActivity : AppCompatActivity(), SeekBar.OnSeekBarChangeListener, View.OnClickListener {
    private var TAG: String = "MainActivity"
    private lateinit var binding: ActivityMainBinding
    private var tvState: TextView? = null
    private var player: PlayerEngine? = null
    private var surfaceView: SurfaceView? = null

    private var seekBar: SeekBar? = null
    private var tvTime: TextView? = null
    private var isTouch = false
    private var duration = 0

    private var btStart: Button? = null
    private var btPre: Button? = null
    private var btNext: Button? = null
    private var btStop: Button? = null
    private var isStart: Boolean = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)
        tvState = binding.tvState
        surfaceView = binding.surfaceView

        tvTime = binding.tvTime
        seekBar = binding.seekBar
        seekBar?.setOnSeekBarChangeListener(this)

        btStart = binding.btStart
        btStop = binding.btStop
        btPre = binding.btPre
        btNext = binding.btNext
        btStart?.setOnClickListener(this)
        btStop?.setOnClickListener(this)
        btPre?.setOnClickListener(this)
        btNext?.setOnClickListener(this)

        player = PlayerEngine()
        lifecycle.addObserver(player!!)// MainActivity????????????????????????PlayerEngine???????????????????????????

        player?.setSurfaceView(surfaceView!!)
        player?.setDataSource(
            File(
                Environment.getExternalStorageDirectory(),
                "demo.mp4"
            ).absolutePath
        )

//        player?.setDataSource(File(Environment.getExternalStorageDirectory(),
//            "aabb/memory.mkv").absolutePath)

        // ????????????????????????,???C++??????????????????
        player!!.setOnPreparedListener(object : PlayerEngine.OnPreparedListener {
            override fun onPrepared() {
                //????????????????????? duration=0??????????????????????????????????????????
                duration = player!!.duration
                runOnUiThread {
                    if (duration != 0) {
                        //???????????????????????????UI View??? ???????????????????????????????????????
                        tvTime!!.text = "00:00/" + getMinutes(duration) + ":" + getSeconds(duration)
                        tvTime?.visibility = View.VISIBLE //??????
                        seekBar!!.visibility = View.VISIBLE //??????
                    }
//                    Toast.makeText(this@MainActivity, "???????????????????????????", Toast.LENGTH_LONG).show()
                    tvState?.setTextColor(Color.GREEN)
                    tvState!!.text = "init success"
                }
                player!!.start()
                isStart=true
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
                if (!isTouch) { //????????????????????????????????????????????????
                    runOnUiThread { //C++???????????????????????????????????????????????????????????????UI
                        if (duration != 0) {
                            //playProgress ???C++??????ffmpeg??????????????????????????????????????????
                            tvTime!!.text =
                                (getMinutes(playProgress)) + ":" + getSeconds(playProgress) + "/" +
                                        getMinutes(duration) + ":" + getSeconds(duration)
                            //???????????????seekBar??????????????????????????????????????????
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
     * ???????????????????????????????????????????????????
     * @param seekBar SeekBar ??????
     * @param progress Int 1~100
     * @param fromUser Boolean ?????????????????????????????????
     */
    override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
        if (fromUser) {
            tvTime!!.text = (getMinutes(progress * duration / 100) + ":" +
                    getSeconds(progress * duration / 100) + "/" +
                    getMinutes(duration) + ":" + getSeconds(duration))
        }
    }

    //???????????????????????????
    override fun onStartTrackingTouch(seekBar: SeekBar?) {
        isTouch = true
    }

    //????????????????????????
    override fun onStopTrackingTouch(seekBar: SeekBar) {
        isTouch = false
        //????????????seekBar??????
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
            if (ContextCompat.checkSelfPermission(
                    this,
                    permission
                ) != PackageManager.PERMISSION_GRANTED
            ) {
                mPermissionList.add(permission)
            }
        }
        if (!mPermissionList.isEmpty()) {
            val reqPermissions = mPermissionList.toTypedArray()//???List????????????
            ActivityCompat.requestPermissions(this, reqPermissions, PERMISSION_REQUEST)
        }
    }

    /**
     * ????????????
     * ???????????????????????????????????????????????????????????????????????????
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

    override fun onClick(v: View?) {
        when (v?.id) {
            R.id.bt_start -> {
                Toast.makeText(this, "?????????start", Toast.LENGTH_SHORT).show()
                playMedia()
            }
            R.id.bt_stop -> {
                Toast.makeText(this, "click stop", Toast.LENGTH_SHORT).show()
            }
            R.id.bt_pre -> {
                Toast.makeText(this, "click pre", Toast.LENGTH_SHORT).show()
            }
            R.id.bt_next -> {
                Toast.makeText(this, "click next", Toast.LENGTH_SHORT).show()
            }
        }
    }

    private fun playMedia() {
        if(isStart){
            player?.pauseMedia()
            isStart=false
        }else{
            player?.playMedia()
            isStart=true
        }
    }

}