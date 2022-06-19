package com.infinite.weplaykt

import android.Manifest
import android.content.pm.PackageManager
import android.graphics.Color
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.os.Environment
import android.util.Log
import android.view.SurfaceView
import android.widget.TextView
import android.widget.Toast
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.lifecycle.Lifecycle
import com.infinite.weplaykt.databinding.ActivityMainBinding
import java.io.File

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding
    private var tvState: TextView? = null
    private var player: PlayerEngine? = null
    private var surfaceView: SurfaceView? = null
    private var TAG:String="MainActivity"

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)
        tvState = binding.tvState
        surfaceView = binding.surfaceView

        player = PlayerEngine()
        lifecycle.addObserver(player!!)// MainActivity做为被观察者，与PlayerEngine观察者建立绑定关系

        player?.setSurfaceView(surfaceView!!)
        player?.setDataSource(File(Environment.getExternalStorageDirectory(),
            "demo.mp4").absolutePath)

        // 准备成功的回调处,有C++子线程调用的
        player!!.setOnPreparedListener(object : PlayerEngine.OnPreparedListener {
            override fun onPrepared() {
                runOnUiThread {
//                    Toast.makeText(this@MainActivity, "准备成功，开始播放", Toast.LENGTH_LONG).show()
                    tvState?.setTextColor(Color.GREEN)
                    tvState!!.text = "init success"
                }
                player!!.start()
            }

        })

        player!!.setOnErrorListener(object:PlayerEngine.OnErrorListener{
            override fun onError(errorMsg: String?) {
                runOnUiThread{
                    tvState?.setTextColor(Color.RED)
                    tvState!!.text=errorMsg
                }
            }

        })
        checkPermission()
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
            else->super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        }
    }

    override fun onResume() {
        super.onResume()
        Log.d(TAG,"onResume")
    }

    override fun onStop() {
        super.onStop()
        Log.d(TAG,"onStop")
    }

    override fun onDestroy() {
        super.onDestroy()
        Log.d(TAG,"onDestroy")
    }
}