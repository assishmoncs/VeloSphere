package com.hsissa.velosphere

import android.content.pm.ActivityInfo
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
import java.io.File
import java.io.FileOutputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder
import kotlin.math.PI
import kotlin.math.exp
import kotlin.math.sin

class MainActivity : GameActivity() {

    private var mediaPlayer: MediaPlayer? = null
    private var soundPool: SoundPool? = null
    private val sfxIds = IntArray(SFX_COUNT)
    private val sfxLoaded = BooleanArray(SFX_COUNT)
    private var vibrator: Vibrator? = null

    @Volatile
    private var musicEnabled: Boolean = true
    @Volatile
    private var sfxEnabled: Boolean = true
    @Volatile
    private var hapticsEnabled: Boolean = true

    companion object {
        private var instance: MainActivity? = null

        const val SFX_HIT          = 0
        const val SFX_ORB          = 1
        const val SFX_POWERUP      = 2
        const val SFX_SHIELD_SMASH = 3
        const val SFX_JUMP         = 4
        const val SFX_SLAM         = 5
        const val SFX_NEAR_MISS    = 6
        const val SFX_FEVER        = 7
        const val SFX_UI_CLICK     = 8
        const val SFX_UNLOCK       = 9
        const val SFX_MISSION_DONE = 10
        const val SFX_COUNTDOWN    = 11
        const val SFX_PHASE_DASH   = 12
        const val SFX_COUNT        = 13

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
        // Enforce strict portrait orientation at runtime as well as in AndroidManifest
        requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_PORTRAIT
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
                    if (mediaPlayer?.isPlaying == false) mediaPlayer?.start()
                } else {
                    if (mediaPlayer?.isPlaying == true) mediaPlayer?.pause()
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
            v.vibrate(VibrationEffect.createOneShot(safeDuration, safeAmplitude))
        } else {
            @Suppress("DEPRECATION")
            v.vibrate(durationMs.coerceAtLeast(1).toLong())
        }
    }

    private fun writeWavToCache(name: String, samples: ShortArray, sampleRate: Int = 22050): String {
        val file = File(cacheDir, name)
        val dataSize = samples.size * 2
        val header = ByteBuffer.allocate(44).order(ByteOrder.LITTLE_ENDIAN).apply {
            put("RIFF".toByteArray())
            putInt(36 + dataSize)
            put("WAVE".toByteArray())
            put("fmt ".toByteArray())
            putInt(16)
            putShort(1)
            putShort(1)
            putInt(sampleRate)
            putInt(sampleRate * 2)
            putShort(2)
            putShort(16)
            put("data".toByteArray())
            putInt(dataSize)
        }
        val body = ByteBuffer.allocate(dataSize).order(ByteOrder.LITTLE_ENDIAN)
        for (s in samples) body.putShort(s)

        FileOutputStream(file).use { out ->
            out.write(header.array())
            out.write(body.array())
        }
        return file.absolutePath
    }

    private fun synthSweep(
        durationSec: Double,
        fStart: Double,
        fEnd: Double,
        volume: Double = 0.45,
        harmonic: Double = 0.25,
        sampleRate: Int = 22050
    ): ShortArray {
        val count = (durationSec * sampleRate).toInt()
        val out = ShortArray(count)
        var phase = 0.0
        for (i in 0 until count) {
            val t = i.toDouble() / count
            val freq = fStart + (fEnd - fStart) * t
            phase += 2.0 * PI * freq / sampleRate
            val env = exp(-t * 4.2) * (1.0 - exp(-t * 45.0))
            val wave = sin(phase) + harmonic * sin(phase * 2.0)
            out[i] = (wave * env * volume * 32767.0).toInt().coerceIn(-32767, 32767).toShort()
        }
        return out
    }

    private fun synthChord(
        durationSec: Double,
        freqs: DoubleArray,
        volume: Double = 0.35,
        arpeggioStepSec: Double = 0.0,
        sampleRate: Int = 22050
    ): ShortArray {
        val count = (durationSec * sampleRate).toInt()
        val out = ShortArray(count)
        for (i in 0 until count) {
            val sec = i.toDouble() / sampleRate
            var sum = 0.0
            for (n in freqs.indices) {
                val start = n * arpeggioStepSec
                if (sec >= start) {
                    val localT = (sec - start) / (durationSec - start).coerceAtLeast(0.05)
                    val env = exp(-localT * 3.5) * (1.0 - exp(-localT * 50.0))
                    sum += sin(2.0 * PI * freqs[n] * (sec - start)) * env
                }
            }
            val norm = sum / freqs.size.coerceAtLeast(1)
            out[i] = (norm * volume * 32767.0).toInt().coerceIn(-32767, 32767).toShort()
        }
        return out
    }

    private fun initSoundPool() {
        try {
            val audioAttributes = AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_GAME)
                .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
                .build()

            soundPool = SoundPool.Builder()
                .setMaxStreams(8)
                .setAudioAttributes(audioAttributes)
                .build()

            soundPool?.setOnLoadCompleteListener { _, sampleId, status ->
                if (status == 0) {
                    for (i in 0 until SFX_COUNT) {
                        if (sfxIds[i] == sampleId) {
                            sfxLoaded[i] = true
                            break
                        }
                    }
                }
            }

            sfxIds[SFX_HIT] = soundPool?.load(this, R.raw.ball_hit, 1) ?: 0
            val pool = soundPool ?: return
            sfxIds[SFX_ORB]          = pool.load(writeWavToCache("sfx_orb.wav",       synthSweep(0.14, 720.0, 1480.0, 0.38, 0.30)), 1)
            sfxIds[SFX_POWERUP]      = pool.load(writeWavToCache("sfx_powerup.wav",   synthChord(0.32, doubleArrayOf(523.25, 659.25, 783.99, 1046.5), 0.48, 0.045)), 1)
            sfxIds[SFX_SHIELD_SMASH] = pool.load(writeWavToCache("sfx_smash.wav",     synthSweep(0.24, 420.0, 95.0, 0.60, 0.55)), 1)
            sfxIds[SFX_JUMP]         = pool.load(writeWavToCache("sfx_jump.wav",      synthSweep(0.15, 300.0, 820.0, 0.35, 0.20)), 1)
            sfxIds[SFX_SLAM]         = pool.load(writeWavToCache("sfx_slam.wav",      synthSweep(0.16, 260.0, 75.0, 0.50, 0.35)), 1)
            sfxIds[SFX_NEAR_MISS]    = pool.load(writeWavToCache("sfx_nearmiss.wav",  synthSweep(0.18, 920.0, 1760.0, 0.42, 0.40)), 1)
            sfxIds[SFX_FEVER]        = pool.load(writeWavToCache("sfx_fever.wav",     synthChord(0.45, doubleArrayOf(440.0, 554.37, 659.25, 880.0), 0.52, 0.055)), 1)
            sfxIds[SFX_UI_CLICK]     = pool.load(writeWavToCache("sfx_ui.wav",        synthSweep(0.06, 980.0, 1280.0, 0.28, 0.15)), 1)
            sfxIds[SFX_UNLOCK]       = pool.load(writeWavToCache("sfx_unlock.wav",    synthChord(0.38, doubleArrayOf(587.33, 739.99, 880.0, 1174.66), 0.50, 0.06)), 1)
            sfxIds[SFX_MISSION_DONE] = pool.load(writeWavToCache("sfx_mission.wav",   synthChord(0.48, doubleArrayOf(523.25, 659.25, 783.99, 1046.5, 1318.5), 0.52, 0.06)), 1)
            sfxIds[SFX_COUNTDOWN]    = pool.load(writeWavToCache("sfx_countdown.wav", synthSweep(0.11, 660.0, 660.0, 0.35, 0.15)), 1)
            sfxIds[SFX_PHASE_DASH]   = pool.load(writeWavToCache("sfx_dash.wav",      synthSweep(0.22, 1350.0, 280.0, 0.52, 0.45)), 1)
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    private fun playSfxInternal(sfxType: Int) {
        if (!sfxEnabled) return
        val idx = sfxType.coerceIn(0, SFX_COUNT - 1)
        val sid = sfxIds[idx]
        if (sid != 0 && sfxLoaded[idx]) {
            val vol = if (idx == SFX_UI_CLICK) 0.65f else 0.90f
            soundPool?.play(sid, vol, vol, 1, 0, 1.0f)
        } else if (sfxIds[SFX_HIT] != 0 && sfxLoaded[SFX_HIT]) {
            soundPool?.play(sfxIds[SFX_HIT], 0.7f, 0.7f, 1, 0, 1.25f)
        }
    }

    private fun initBackgroundMusic() {
        try {
            mediaPlayer = MediaPlayer.create(this, R.raw.velosphere_bg)?.apply {
                isLooping = true
                setVolume(0.45f, 0.45f)
                if (musicEnabled) start()
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    override fun onPause() {
        super.onPause()
        if (mediaPlayer?.isPlaying == true) mediaPlayer?.pause()
    }

    override fun onResume() {
        super.onResume()
        requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_PORTRAIT
        hideSystemUI()
        if (musicEnabled && mediaPlayer?.isPlaying == false) mediaPlayer?.start()
    }

    override fun onDestroy() {
        super.onDestroy()
        try { mediaPlayer?.stop() } catch (_: Exception) {}
        mediaPlayer?.release()
        mediaPlayer = null
        soundPool?.release()
        soundPool = null
        sfxIds.fill(0)
        sfxLoaded.fill(false)
        vibrator = null
        instance = null
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) hideSystemUI()
    }

    private fun hideSystemUI() {
        val windowInsetsController = WindowCompat.getInsetsController(window, window.decorView)
        windowInsetsController.systemBarsBehavior =
            WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        windowInsetsController.hide(WindowInsetsCompat.Type.systemBars())
    }
}
