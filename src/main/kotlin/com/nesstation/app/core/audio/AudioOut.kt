package com.nesstation.app.core.audio

import javax.sound.sampled.AudioFormat
import javax.sound.sampled.AudioSystem
import javax.sound.sampled.SourceDataLine

/**
 * 音频输出（对应 Android 版 AudioTrack 路径的 PC 等价物）。
 *
 * 管线：libretro 核心（原生采样率）→ 环形缓冲 → javax.sound SourceDataLine。
 *  - 核心请求的采样率与当前线路不一致时自动重开线路（无重采样直通，
 *    与原版「核心速率直通」策略一致）。
 *  - 缓冲满时丢弃最旧数据（防卡顿爆音，模拟不掉帧优先）。
 */
class AudioOut {

    @Volatile private var sampleRate = 0
    private var line: SourceDataLine? = null
    private val lock = Any()

    // 环形缓冲（字节）
    private var ring: ByteArray = ByteArray(1 shl 16)
    private var readPos = 0
    private var writePos = 0
    private var buffered = 0

    @Volatile private var running = false
    private var thread: Thread? = null

    fun start(rate: Int) {
        synchronized(lock) {
            if (rate <= 0) return
            if (line != null && sampleRate == rate) return
            stopLocked()
            sampleRate = rate
            line = try {
                val fmt = AudioFormat(rate.toFloat(), 16, 2, true, false)
                val l = AudioSystem.getSourceDataLine(fmt)
                l.open(fmt, bufferBytes(rate))
                l.start()
                l
            } catch (_: Exception) { null }
            if (line == null) { sampleRate = 0; return }
            running = true
            thread = Thread({ drainLoop() }, "nesstation-audio").apply { isDaemon = true; start() }
        }
    }

    /** libretro 音频回调写入（s16 交错立体声）。 */
    fun write(samples: ShortArray, frames: Int) {
        if (frames <= 0 || !running) return
        val n = frames * 4
        val tmp = if (scratch.size < n) ByteArray(n).also { scratch = it } else scratch
        var o = 0
        for (i in 0 until frames * 2) {
            val v = samples[i].toInt()
            tmp[o++] = (v and 0xFF).toByte()
            tmp[o++] = ((v shr 8) and 0xFF).toByte()
        }
        writeBytes(tmp, n)
    }

    private var scratch = ByteArray(8192)

    /** 直接写字节流（短路径）。 */
    fun writeBytes(b: ByteArray, len: Int) {
        if (len <= 0 || !running) return
        synchronized(lock) {
            var i = 0
            while (i < len && buffered < ring.size) {
                ring[writePos] = b[i]
                writePos = (writePos + 1) % ring.size
                buffered++
                i++
            }
        }
    }

    private fun drainLoop() {
        val chunk = ByteArray(4096)
        while (running) {
            var n = 0
            synchronized(lock) {
                val avail = buffered
                val take = minOf(chunk.size, avail)
                for (i in 0 until take) {
                    chunk[i] = ring[readPos]
                    readPos = (readPos + 1) % ring.size
                }
                buffered -= take
                n = take
            }
            if (n > 0) {
                try { line?.write(chunk, 0, n) } catch (_: Exception) { }
            } else {
                Thread.sleep(4)
            }
        }
    }

    private fun bufferBytes(rate: Int): Int {
        val ms = com.nesstation.app.core.storage.SettingsStore.load().audioBufferMs.coerceIn(16, 512)
        return ((rate * ms / 1000) * 4).coerceAtLeast(2048)
    }

    fun flush() {
        synchronized(lock) { buffered = 0; readPos = 0; writePos = 0 }
    }

    fun stop() { synchronized(lock) { stopLocked() } }

    private fun stopLocked() {
        running = false
        try { thread?.join(300) } catch (_: InterruptedException) { }
        thread = null
        try { line?.stop() } catch (_: Exception) { }
        try { line?.close() } catch (_: Exception) { }
        line = null
        buffered = 0; readPos = 0; writePos = 0
    }
}
