package com.hsissa.velosphere

import android.media.AudioAttributes
import android.media.MediaPlayer
import android.media.SoundPool
import android.os.Bundle
import androidx.core.splashscreen.SplashScreen.Companion.installSplashScreen
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import com.google.androidgamesdk.GameActivity

class MainActivity : GameActivity() {

    private var mediaPlayer: MediaPlayer? = null
    private var hitSoundPool: SoundPool? = null
    private var hitSoundId: Int = 0
    private var hitSoundLoaded = false

    companion object {
        private var instance: MainActivity? = null

        init {
            System.loadLibrary("native-lib")
        }

        @JvmStatic
        fun playHitSound() {
            instance?.playHitSoundInternal()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        installSplashScreen()
        super.onCreate(savedInstanceState)
        instance = this
        hideSystemUI()
        initBackgroundMusic()
        initHitSound()
    }

    private fun initHitSound() {
        try {
            val audioAttributes = AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_GAME)
                .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
                .build()

            hitSoundPool = SoundPool.Builder()
                .setMaxStreams(2)
                .setAudioAttributes(audioAttributes)
                .build()

            hitSoundPool?.setOnLoadCompleteListener { _, sampleId, status ->
                if (sampleId == hitSoundId && status == 0) {
                    hitSoundLoaded = true
                }
            }

            hitSoundId = hitSoundPool?.load(this, R.raw.ball_hit, 1) ?: 0
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    private fun playHitSoundInternal() {
        if (hitSoundLoaded && hitSoundId != 0) {
            hitSoundPool?.play(hitSoundId, 1.0f, 1.0f, 1, 0, 1.0f)
        }
    }

    private fun initBackgroundMusic() {
        try {
            mediaPlayer = MediaPlayer.create(this, R.raw.velosphere_bg).apply {
                isLooping = true
                setVolume(0.5f, 0.5f)
                start()
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    override fun onPause() {
        super.onPause()
        if (mediaPlayer?.isPlaying == true) {
            mediaPlayer?.pause()
        }
    }

    override fun onResume() {
        super.onResume()
        hideSystemUI()
        if (mediaPlayer?.isPlaying == false) {
            mediaPlayer?.start()
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        mediaPlayer?.stop()
        mediaPlayer?.release()
        mediaPlayer = null

        hitSoundPool?.release()
        hitSoundPool = null
        hitSoundId = 0
        hitSoundLoaded = false
        instance = null
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) {
            hideSystemUI()
        }
    }

    private fun hideSystemUI() {
        val windowInsetsController =
            WindowCompat.getInsetsController(window, window.decorView)
        windowInsetsController.systemBarsBehavior =
            WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        windowInsetsController.hide(WindowInsetsCompat.Type.systemBars())
    }
}
