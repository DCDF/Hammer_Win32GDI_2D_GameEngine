// Audio.cpp
// Dependencies: stb_vorbis.c (put this source file in your src/ and make sure it's included only here)
// Link: winmm.lib
#pragma comment(lib, "winmm.lib")

#include "Audio.h"

#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <cstdlib> // free
#include <cstring> // memcpy
#include <chrono>

// include stb_vorbis implementation here (single-file). Make sure stb_vorbis.c is NOT compiled elsewhere.
#include "stb_vorbis.c"

// ---------------------------
// Config: device format & buffer sizes
// ---------------------------
static const int DEVICE_SAMPLE_RATE = 44100;
static const int DEVICE_CHANNELS = 2; // stereo
static const int DEVICE_BITS = 16;
static int MIX_BUFFER_FRAMES = 2048; // frames per buffer (stereo frames)
static int MIX_BUFFER_SAMPLES = MIX_BUFFER_FRAMES * DEVICE_CHANNELS;

// ---------------------------
// Utility
// ---------------------------
static inline short clamp16(int v)
{
    if (v > 32767)
        return 32767;
    if (v < -32768)
        return -32768;
    return (short)v;
}

// ---------------------------
// Internal types
// ---------------------------
struct SFXData
{
    std::vector<short> pcm; // interleaved stereo int16 (device format)
};

struct Voice
{
    const short *data;   // pointer into SFXData.pcm
    size_t totalSamples; // total short samples
    size_t pos;          // current sample index
    float volume;
    bool loop;
};

struct BGMData
{
    std::vector<short> pcm; // interleaved stereo int16 (device format)
    size_t totalSamples = 0;
    size_t pos = 0;
    float volume = 1.0f;
    bool playing = false;
};

// ---------------------------
// Globals (protected by g_lock)
static std::mutex g_lock;
static std::map<int, SFXData> g_sfx_cache;
static std::vector<Voice> g_voices;

static BGMData g_bgm;

// mixer thread & control
static std::atomic<bool> g_running{false};
static std::thread g_mixerThread;
static std::condition_variable g_cv;
static std::mutex g_cv_mutex;

// waveOut
static HWAVEOUT g_hWaveOut = nullptr;

// forward declarations
static bool audioOpenDevice();
static void audioCloseDevice();
static void mixerThreadFunc();
static std::vector<short> resample_and_channels(const short *src, size_t srcSamples, int srcChannels, int srcRate, int dstRate, int dstChannels);

// ---------------------------
// Helpers: load RC RCDATA into memory pointer
static bool LoadResourceToMemory(int resourceId, const unsigned char **outPtr, size_t *outSize)
{
    if (!outPtr || !outSize)
        return false;
    HMODULE hMod = GetModuleHandleW(NULL);
    HRSRC hrsrc = FindResource(hMod, MAKEINTRESOURCE(resourceId), RT_RCDATA);
    if (!hrsrc)
        return false;
    HGLOBAL hgl = LoadResource(hMod, hrsrc);
    if (!hgl)
        return false;
    void *p = LockResource(hgl);
    DWORD sz = SizeofResource(hMod, hrsrc);
    if (!p || sz == 0)
        return false;
    *outPtr = (const unsigned char *)p;
    *outSize = (size_t)sz;
    return true;
}

// ---------------------------
// Simple nearest-neighbor resample + channel conversion
static std::vector<short> resample_and_channels(const short *src, size_t srcSamples, int srcChannels, int srcRate, int dstRate, int dstChannels)
{
    if (srcSamples == 0)
        return {};
    size_t srcFrames = srcSamples / srcChannels;
    double ratio = double(dstRate) / double(srcRate);
    size_t dstFrames = (size_t)(srcFrames * ratio + 0.5);
    if (dstFrames == 0)
        dstFrames = 1;
    std::vector<short> out(dstFrames * dstChannels);
    for (size_t f = 0; f < dstFrames; ++f)
    {
        size_t srcF = (size_t)(f / ratio);
        if (srcF >= srcFrames)
            srcF = srcFrames - 1;
        if (srcChannels == 1)
        {
            short s = src[srcF];
            if (dstChannels == 1)
                out[f] = s;
            else
            {
                out[f * 2 + 0] = s;
                out[f * 2 + 1] = s;
            }
        }
        else
        {
            const short *ps = src + srcF * srcChannels;
            short left = ps[0];
            short right = (srcChannels > 1 ? ps[1] : ps[0]);
            if (dstChannels == 1)
            {
                int mixed = (int(left) + int(right)) / 2;
                out[f] = (short)mixed;
            }
            else
            {
                out[f * 2 + 0] = left;
                out[f * 2 + 1] = right;
            }
        }
    }
    return out;
}

// ---------------------------
// Decode & cache SFX (blocking)
static bool decode_and_cache_sfx(int resourceId)
{
    std::lock_guard<std::mutex> lg(g_lock);
    if (g_sfx_cache.count(resourceId))
        return true;

    const unsigned char *data = nullptr;
    size_t size = 0;
    if (!LoadResourceToMemory(resourceId, &data, &size))
        return false;

    int channels = 0;
    int sample_rate = 0;
    short *out = nullptr;
    int samples_per_channel = stb_vorbis_decode_memory(data, (int)size, &channels, &sample_rate, &out);
    if (samples_per_channel <= 0)
    {
        if (out)
            free(out);
        return false;
    }
    int totalSamples = samples_per_channel * channels;

    std::vector<short> pcm;
    if (sample_rate != DEVICE_SAMPLE_RATE || channels != DEVICE_CHANNELS)
    {
        pcm = resample_and_channels(out, totalSamples, channels, sample_rate, DEVICE_SAMPLE_RATE, DEVICE_CHANNELS);
    }
    else
    {
        pcm.assign(out, out + totalSamples);
    }
    free(out);

    SFXData sd;
    sd.pcm.swap(pcm);
    g_sfx_cache[resourceId] = std::move(sd);
    return true;
}

// ---------------------------
// Public API
void Audio_bg(int resourceId)
{
    // ensure device + mixer running
    {
        bool needStart = false;
        if (!g_running.load())
        {
            std::lock_guard<std::mutex> lg(g_lock);
            if (!g_running.load())
            {
                if (!audioOpenDevice())
                    return;
                g_running.store(true);
                needStart = true;
            }
        }
        if (needStart)
        {
            g_mixerThread = std::thread(mixerThreadFunc);
        }
    }

    // load OGG bytes
    const unsigned char *data = nullptr;
    size_t size = 0;
    if (!LoadResourceToMemory(resourceId, &data, &size))
        return;

    // decode entire OGG to PCM (blocking) to avoid runtime decode in mixer
    int channels = 0;
    int sample_rate = 0;
    short *out = nullptr;
    int samples_per_channel = stb_vorbis_decode_memory(data, (int)size, &channels, &sample_rate, &out);
    if (samples_per_channel <= 0)
    {
        if (out)
            free(out);
        return;
    }
    int totalSamples = samples_per_channel * channels;
    std::vector<short> pcm;
    if (sample_rate != DEVICE_SAMPLE_RATE || channels != DEVICE_CHANNELS)
    {
        pcm = resample_and_channels(out, totalSamples, channels, sample_rate, DEVICE_SAMPLE_RATE, DEVICE_CHANNELS);
    }
    else
    {
        pcm.assign(out, out + totalSamples);
    }
    free(out);

    // install as new BGM
    {
        std::lock_guard<std::mutex> lg(g_lock);
        g_bgm.pcm.swap(pcm);
        g_bgm.totalSamples = g_bgm.pcm.size();
        g_bgm.pos = 0;
        g_bgm.volume = 1.0f;
        g_bgm.playing = true;
        // notify mixer (in case it's waiting)
        g_cv.notify_all();
    }
}

void Audio_sfx(int resourceId)
{
    // ensure device + mixer running
    {
        bool needStart = false;
        if (!g_running.load())
        {
            std::lock_guard<std::mutex> lg(g_lock);
            if (!g_running.load())
            {
                if (!audioOpenDevice())
                    return;
                g_running.store(true);
                needStart = true;
            }
        }
        if (needStart)
        {
            g_mixerThread = std::thread(mixerThreadFunc);
        }
    }

    if (!decode_and_cache_sfx(resourceId))
        return;

    std::lock_guard<std::mutex> lg(g_lock);
    const SFXData &sd = g_sfx_cache[resourceId];
    if (sd.pcm.empty())
        return;
    Voice v;
    v.data = sd.pcm.data();
    v.totalSamples = sd.pcm.size();
    v.pos = 0;
    v.volume = 1.0f;
    v.loop = false;
    g_voices.push_back(v);
    g_cv.notify_all();
}

void Audio_shutdown()
{
    // stop mixer thread
    {
        std::lock_guard<std::mutex> lg(g_lock);
        g_running.store(false);
    }
    g_cv.notify_all();
    if (g_mixerThread.joinable())
        g_mixerThread.join();

    // free cached resources
    {
        std::lock_guard<std::mutex> lg(g_lock);
        g_voices.clear();
        g_sfx_cache.clear();
        g_bgm.pcm.clear();
        g_bgm.totalSamples = 0;
        g_bgm.pos = 0;
        g_bgm.playing = false;
    }
    audioCloseDevice();
}

// ---------------------------
// audio device open / close
static bool audioOpenDevice()
{
    WAVEFORMATEX wf = {};
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = DEVICE_CHANNELS;
    wf.nSamplesPerSec = DEVICE_SAMPLE_RATE;
    wf.wBitsPerSample = DEVICE_BITS;
    wf.nBlockAlign = wf.nChannels * (wf.wBitsPerSample / 8);
    wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;

    MMRESULT res = waveOutOpen(&g_hWaveOut, WAVE_MAPPER, &wf, 0, 0, CALLBACK_NULL);
    return (res == MMSYSERR_NOERROR);
}

static void audioCloseDevice()
{
    if (g_hWaveOut)
    {
        waveOutReset(g_hWaveOut);
        waveOutClose(g_hWaveOut);
        g_hWaveOut = nullptr;
    }
}

// ---------------------------
// Mixer loop: double-buffer style
static void mixerThreadFunc()
{
    MIX_BUFFER_FRAMES = MIX_BUFFER_FRAMES; // keep variable to allow tweak if needed
    MIX_BUFFER_SAMPLES = MIX_BUFFER_FRAMES * DEVICE_CHANNELS;

    std::vector<short> buf0(MIX_BUFFER_SAMPLES);
    std::vector<short> buf1(MIX_BUFFER_SAMPLES);
    WAVEHDR hdr0 = {};
    WAVEHDR hdr1 = {};

    hdr0.lpData = (LPSTR)buf0.data();
    hdr0.dwBufferLength = (DWORD)(MIX_BUFFER_SAMPLES * sizeof(short));
    hdr1.lpData = (LPSTR)buf1.data();
    hdr1.dwBufferLength = (DWORD)(MIX_BUFFER_SAMPLES * sizeof(short));

    // prime both buffers
    {
        // produce initial audio
        // call mixing directly to fill
        // we'll mix silence if nothing
    }

// prepare and write first two buffers
mix_and_write:
{
    // fill buf0
    {
        // accumulate into int32 temp
        int samples = MIX_BUFFER_SAMPLES;
        std::vector<int> acc(samples);
        // lock and mix
        {
            std::lock_guard<std::mutex> lg(g_lock);
            // mix SFX voices
            for (auto it = g_voices.begin(); it != g_voices.end();)
            {
                Voice &v = *it;
                for (int i = 0; i < MIX_BUFFER_FRAMES; ++i)
                {
                    for (int ch = 0; ch < DEVICE_CHANNELS; ++ch)
                    {
                        size_t idx = v.pos + (size_t)i * DEVICE_CHANNELS + ch;
                        short s = 0;
                        if (idx < v.totalSamples)
                            s = v.data[idx];
                        acc[i * DEVICE_CHANNELS + ch] += (int)(s * v.volume);
                    }
                }
                v.pos += (size_t)MIX_BUFFER_FRAMES * DEVICE_CHANNELS;
                if (v.pos >= v.totalSamples)
                {
                    if (v.loop)
                    {
                        v.pos = v.pos % v.totalSamples;
                        ++it;
                    }
                    else
                    {
                        it = g_voices.erase(it);
                    }
                }
                else
                    ++it;
            }

            // mix BGM
            if (g_bgm.playing && g_bgm.totalSamples > 0)
            {
                for (int i = 0; i < MIX_BUFFER_FRAMES; ++i)
                {
                    for (int ch = 0; ch < DEVICE_CHANNELS; ++ch)
                    {
                        size_t idx = g_bgm.pos + (size_t)i * DEVICE_CHANNELS + ch;
                        short s = 0;
                        if (idx < g_bgm.totalSamples)
                            s = g_bgm.pcm[idx];
                        acc[i * DEVICE_CHANNELS + ch] += (int)(s * g_bgm.volume);
                    }
                }
                g_bgm.pos += (size_t)MIX_BUFFER_FRAMES * DEVICE_CHANNELS;
                if (g_bgm.pos >= g_bgm.totalSamples)
                {
                    // loop back
                    if (g_bgm.totalSamples > 0)
                        g_bgm.pos %= g_bgm.totalSamples;
                }
            }
        } // unlock

        // write to buf0 (with clamp)
        for (int i = 0; i < samples; ++i)
            buf0[i] = clamp16(acc[i]);
    }

    // write buffer 0
    if (waveOutPrepareHeader(g_hWaveOut, &hdr0, sizeof(hdr0)) == MMSYSERR_NOERROR)
        waveOutWrite(g_hWaveOut, &hdr0, sizeof(hdr0));
}

    // fill and queue second buffer similarly
    {
        int samples = MIX_BUFFER_SAMPLES;
        std::vector<int> acc(samples);
        {
            std::lock_guard<std::mutex> lg(g_lock);
            // mix voices
            for (auto it = g_voices.begin(); it != g_voices.end();)
            {
                Voice &v = *it;
                for (int i = 0; i < MIX_BUFFER_FRAMES; ++i)
                {
                    for (int ch = 0; ch < DEVICE_CHANNELS; ++ch)
                    {
                        size_t idx = v.pos + (size_t)i * DEVICE_CHANNELS + ch;
                        short s = 0;
                        if (idx < v.totalSamples)
                            s = v.data[idx];
                        acc[i * DEVICE_CHANNELS + ch] += (int)(s * v.volume);
                    }
                }
                v.pos += (size_t)MIX_BUFFER_FRAMES * DEVICE_CHANNELS;
                if (v.pos >= v.totalSamples)
                {
                    if (v.loop)
                    {
                        v.pos = v.pos % v.totalSamples;
                        ++it;
                    }
                    else
                    {
                        it = g_voices.erase(it);
                    }
                }
                else
                    ++it;
            }

            // mix BGM
            if (g_bgm.playing && g_bgm.totalSamples > 0)
            {
                for (int i = 0; i < MIX_BUFFER_FRAMES; ++i)
                {
                    for (int ch = 0; ch < DEVICE_CHANNELS; ++ch)
                    {
                        size_t idx = g_bgm.pos + (size_t)i * DEVICE_CHANNELS + ch;
                        short s = 0;
                        if (idx < g_bgm.totalSamples)
                            s = g_bgm.pcm[idx];
                        acc[i * DEVICE_CHANNELS + ch] += (int)(s * g_bgm.volume);
                    }
                }
                g_bgm.pos += (size_t)MIX_BUFFER_FRAMES * DEVICE_CHANNELS;
                if (g_bgm.pos >= g_bgm.totalSamples)
                {
                    if (g_bgm.totalSamples > 0)
                        g_bgm.pos %= g_bgm.totalSamples;
                }
            }
        } // unlock

        for (int i = 0; i < samples; ++i)
            buf1[i] = clamp16(acc[i]);

        if (waveOutPrepareHeader(g_hWaveOut, &hdr1, sizeof(hdr1)) == MMSYSERR_NOERROR)
            waveOutWrite(g_hWaveOut, &hdr1, sizeof(hdr1));
    }

    // main loop: poll for finished buffers and refill
    while (g_running.load())
    {
        // check hdr0
        if (hdr0.dwFlags & WHDR_DONE)
        {
            waveOutUnprepareHeader(g_hWaveOut, &hdr0, sizeof(hdr0));
            // refill buf0
            int samples = MIX_BUFFER_SAMPLES;
            std::vector<int> acc(samples);
            {
                std::lock_guard<std::mutex> lg(g_lock);
                // mix voices
                for (auto it = g_voices.begin(); it != g_voices.end();)
                {
                    Voice &v = *it;
                    for (int i = 0; i < MIX_BUFFER_FRAMES; ++i)
                    {
                        for (int ch = 0; ch < DEVICE_CHANNELS; ++ch)
                        {
                            size_t idx = v.pos + (size_t)i * DEVICE_CHANNELS + ch;
                            short s = 0;
                            if (idx < v.totalSamples)
                                s = v.data[idx];
                            acc[i * DEVICE_CHANNELS + ch] += (int)(s * v.volume);
                        }
                    }
                    v.pos += (size_t)MIX_BUFFER_FRAMES * DEVICE_CHANNELS;
                    if (v.pos >= v.totalSamples)
                    {
                        if (v.loop)
                            v.pos = v.pos % v.totalSamples, ++it;
                        else
                            it = g_voices.erase(it);
                    }
                    else
                        ++it;
                }
                // mix BGM
                if (g_bgm.playing && g_bgm.totalSamples > 0)
                {
                    for (int i = 0; i < MIX_BUFFER_FRAMES; ++i)
                    {
                        for (int ch = 0; ch < DEVICE_CHANNELS; ++ch)
                        {
                            size_t idx = g_bgm.pos + (size_t)i * DEVICE_CHANNELS + ch;
                            short s = 0;
                            if (idx < g_bgm.totalSamples)
                                s = g_bgm.pcm[idx];
                            acc[i * DEVICE_CHANNELS + ch] += (int)(s * g_bgm.volume);
                        }
                    }
                    g_bgm.pos += (size_t)MIX_BUFFER_FRAMES * DEVICE_CHANNELS;
                    if (g_bgm.pos >= g_bgm.totalSamples)
                    {
                        if (g_bgm.totalSamples > 0)
                            g_bgm.pos %= g_bgm.totalSamples;
                    }
                }
            } // unlock

            for (int i = 0; i < samples; ++i)
                buf0[i] = clamp16(acc[i]);
            // requeue
            if (waveOutPrepareHeader(g_hWaveOut, &hdr0, sizeof(hdr0)) == MMSYSERR_NOERROR)
                waveOutWrite(g_hWaveOut, &hdr0, sizeof(hdr0));
        }

        if (hdr1.dwFlags & WHDR_DONE)
        {
            waveOutUnprepareHeader(g_hWaveOut, &hdr1, sizeof(hdr1));
            // refill buf1
            int samples = MIX_BUFFER_SAMPLES;
            std::vector<int> acc(samples);
            {
                std::lock_guard<std::mutex> lg(g_lock);
                // mix voices
                for (auto it = g_voices.begin(); it != g_voices.end();)
                {
                    Voice &v = *it;
                    for (int i = 0; i < MIX_BUFFER_FRAMES; ++i)
                    {
                        for (int ch = 0; ch < DEVICE_CHANNELS; ++ch)
                        {
                            size_t idx = v.pos + (size_t)i * DEVICE_CHANNELS + ch;
                            short s = 0;
                            if (idx < v.totalSamples)
                                s = v.data[idx];
                            acc[i * DEVICE_CHANNELS + ch] += (int)(s * v.volume);
                        }
                    }
                    v.pos += (size_t)MIX_BUFFER_FRAMES * DEVICE_CHANNELS;
                    if (v.pos >= v.totalSamples)
                    {
                        if (v.loop)
                            v.pos = v.pos % v.totalSamples, ++it;
                        else
                            it = g_voices.erase(it);
                    }
                    else
                        ++it;
                }
                // mix BGM
                if (g_bgm.playing && g_bgm.totalSamples > 0)
                {
                    for (int i = 0; i < MIX_BUFFER_FRAMES; ++i)
                    {
                        for (int ch = 0; ch < DEVICE_CHANNELS; ++ch)
                        {
                            size_t idx = g_bgm.pos + (size_t)i * DEVICE_CHANNELS + ch;
                            short s = 0;
                            if (idx < g_bgm.totalSamples)
                                s = g_bgm.pcm[idx];
                            acc[i * DEVICE_CHANNELS + ch] += (int)(s * g_bgm.volume);
                        }
                    }
                    g_bgm.pos += (size_t)MIX_BUFFER_FRAMES * DEVICE_CHANNELS;
                    if (g_bgm.pos >= g_bgm.totalSamples)
                    {
                        if (g_bgm.totalSamples > 0)
                            g_bgm.pos %= g_bgm.totalSamples;
                    }
                }
            } // unlock

            for (int i = 0; i < samples; ++i)
                buf1[i] = clamp16(acc[i]);
            // requeue
            if (waveOutPrepareHeader(g_hWaveOut, &hdr1, sizeof(hdr1)) == MMSYSERR_NOERROR)
                waveOutWrite(g_hWaveOut, &hdr1, sizeof(hdr1));
        }

        // wait a bit or until notified
        std::unique_lock<std::mutex> lk(g_cv_mutex);
        g_cv.wait_for(lk, std::chrono::milliseconds(6));
    }

    // cleanup: stop and unprepare any queued buffers
    waveOutReset(g_hWaveOut);
    if (hdr0.dwFlags & WHDR_PREPARED)
        waveOutUnprepareHeader(g_hWaveOut, &hdr0, sizeof(hdr0));
    if (hdr1.dwFlags & WHDR_PREPARED)
        waveOutUnprepareHeader(g_hWaveOut, &hdr1, sizeof(hdr1));
}
