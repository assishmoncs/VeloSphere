package com.hsissa.velosphere

import android.media.AudioAttributes
import android.media.MediaPlayer
import android.media.SoundPool
import android.os.Build
import android.os.Bundle
import android.os.VibrationEffect
import android.os.Vibrator
import android.os.VibratorManager
import androidx.core.splashscreen.SplashScreen.Companion.installSplashScreen
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import com.google.androidgamesdk.GameActivity

class MainActivity : GameActivity() {

    private var mediaPlayer: MediaPlayer? = null
    private var soundPool: SoundPool? = null
    private var hitSoundId: Int = 0
    private var soundLoaded = false
    private var vibrator: Vibrator? = null

    @Volatile
    private var musicEnabled: Boolean = true
    @Volatile
    private var sfxEnabled: Boolean = true
    @Volatile
    private var hapticsEnabled: Boolean = true

    companion object {
        private var instance: MainActivity? = null

        // SFX IDs shared with native engine
        const val SFX_HIT = 0
        const val SFX_ORB = 1
        const val SFX_POWERUP = 2
        const val SFX_SHIELD_SMASH = 3
        const val SFX_JUMP = 4

        init {
            System.loadLibrary("native-lib")
        }

        @JvmStatic
        fun playHitSound() {
            instance?.playSfxInternal(SFX_HIT)
        }

        @JvmStatic
        fun playSfx(sfxType: Int) {
            instance?.playSfxInternal(sfxType)
        }

        @JvmStatic
        fun vibrate(durationMs: Int, amplitude: Int) {
            instance?.vibrateInternal(durationMs, amplitude)
        }

        @JvmStatic
        fun updateSettings(musicOn: Boolean, sfxOn: Boolean, hapticsOn: Boolean) {
            instance?.applySettingsInternal(musicOn, sfxOn, hapticsOn)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        installSplashScreen()
        super.onCreate(savedInstanceState)
        instance = this
        hideSystemUI()
        initVibrator()
        initBackgroundMusic()
        initSoundPool()
    }

    private fun initVibrator() {
        vibrator = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            val vm = getSystemService(VIBRATOR_MANAGER_SERVICE) as? VibratorManager
            vm?.defaultVibrator
        } else {
            @Suppress("DEPRECATION")
            getSystemService(VIBRATOR_SERVICE) as? Vibrator
        }
    }

    private fun applySettingsInternal(musicOn: Boolean, sfxOn: Boolean, hapticsOn: Boolean) {
        musicEnabled = musicOn
        sfxEnabled = sfxOn
        hapticsEnabled = hapticsOn

        runOnUiThread {
            try {
                if (musicEnabled) {
                    if (mediaPlayer?.isPlaying == false) {
                        mediaPlayer?.start()
                    }
                } else {
                    if (mediaPlayer?.isPlaying == true) {
                        mediaPlayer?.pause()
                    }
                }
            } catch (e: Exception) {
                e.printStackTrace()
            }
        }
    }

    private fun vibrateInternal(durationMs: Int, amplitude: Int) {
        if (!hapticsEnabled) return
        val v = vibrator ?: return
        if (!v.hasVibrator()) return

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val safeDuration = durationMs.coerceAtLeast(1).toLong()
            val safeAmplitude = amplitude.coerceIn(1, 255)
            v.vibrate(
                VibrationEffect.createOneShot(safeDuration, safeAmplitude)
            )
        } else {
            @Suppress("DEPRECATION")
            v.vibrate(durationMs.coerceAtLeast(1).toLong())
        }
    }

    private fun initSoundPool() {
        try {
            val audioAttributes = AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_GAME)
                .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
                .build()

            soundPool = SoundPool.Builder()
                .setMaxStreams(6)
                .setAudioAttributes(audioAttributes)
                .build()

            soundPool?.setOnLoadCompleteListener { _, sampleId, status ->
                if (sampleId == hitSoundId && status == 0) {
                    soundLoaded = true
                }
            }

            hitSoundId = soundPool?.load(this, R.raw.ball_hit, 1) ?: 0
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    private fun playSfxInternal(sfxType: Int) {
        if (!sfxEnabled || !soundLoaded || hitSoundId == 0) return

        // Pitch-modulated playback for distinct arcade events
        when (sfxType) {
            SFX_HIT -> soundPool?.play(hitSoundId, 1.0f, 1.0f, 2, 0, 1.0f)
            SFX_ORB -> soundPool?.play(hitSoundId, 0.55f, 0.55f, 1, 0, 1.65f)
            SFX_POWERUP -> soundPool?.play(hitSoundId, 0.80f, 0.80f, 1, 0, 1.30f)
            SFX_SHIELD_SMASH -> soundPool?.play(hitSoundId, 0.95f, 0.95f, 2, 0, 0.78f)
            SFX_JUMP -> soundPool?.play(hitSoundId, 0.40f, 0.40f, 1, 0, 1.85f)
            else -> soundPool?.play(hitSoundId, 0.8f, 0.8f, 1, 0, 1.0f)
        }
    }

    private fun initBackgroundMusic() {
        try {
            mediaPlayer = MediaPlayer.create(this, R.raw.velosphere_bg)?.apply {
                isLooping = true
                setVolume(0.5f, 0.5f)
                if (musicEnabled) {
                    start()
                }
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
        if (musicEnabled && mediaPlayer?.isPlaying == false) {
            mediaPlayer?.start()
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        try {
            mediaPlayer?.stop()
        } catch (_: Exception) {
        }
        mediaPlayer?.release()
        mediaPlayer = null

        soundPool?.release()
        soundPool = null
        hitSoundId = 0
        soundLoaded = false
        vibrator = null
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
