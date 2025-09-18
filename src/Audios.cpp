// Audios.cpp
// 编译/链接示例 (MSVC):
// cl /EHsc Audios.cpp /link mfplat.lib mfreadwrite.lib shlwapi.lib xaudio2.lib ole32.lib

#include "Audios.h"
#include <windows.h>
#include <mfapi.h>
#include <mfobjects.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mmreg.h>
#include <shlwapi.h>
#include <xaudio2.h>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <iostream>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "ole32.lib")

using byte = unsigned char;

/////////////////////////
// 内部数据结构
struct SoundData
{
    std::vector<byte> pcm; // PCM bytes (interleaved)
    WAVEFORMATEX wfx{};    // PCM format
};

/////////////////////////
// 简单 AudioEngine（内部使用）
class AudioEngine
{
public:
    AudioEngine() : xaudio2(nullptr), masteringVoice(nullptr), musicVoice(nullptr),
                    bgVolume(0.5f), sfxVolume(1.0f), initialized(false) {}

    ~AudioEngine()
    {
        Shutdown();
    }

    HRESULT Init()
    {
        std::lock_guard<std::mutex> lk(initMutex);
        if (initialized)
            return S_OK;

        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(hr))
            return hr;

        hr = MFStartup(MF_VERSION);
        if (FAILED(hr))
        {
            CoUninitialize();
            return hr;
        }

        hr = XAudio2Create(&xaudio2, 0);
        if (FAILED(hr))
        {
            MFShutdown();
            CoUninitialize();
            return hr;
        }

        hr = xaudio2->CreateMasteringVoice(&masteringVoice);
        if (FAILED(hr))
        {
            xaudio2->Release();
            xaudio2 = nullptr;
            MFShutdown();
            CoUninitialize();
            return hr;
        }

        initialized = true;
        return S_OK;
    }

    void Shutdown()
    {
        std::lock_guard<std::mutex> lk(initMutex);
        if (!initialized)
            return;

        // Stop and destroy music voice
        if (musicVoice)
        {
            musicVoice->Stop(0);
            musicVoice->FlushSourceBuffers();
            musicVoice->DestroyVoice();
            musicVoice = nullptr;
        }

        if (masteringVoice)
        {
            masteringVoice->DestroyVoice();
            masteringVoice = nullptr;
        }
        if (xaudio2)
        {
            xaudio2->Release();
            xaudio2 = nullptr;
        }
        MFShutdown();
        CoUninitialize();
        initialized = false;
    }

    // Decode compressed bytes (mp3/aac/wma supported by platform) into PCM (16-bit)
    HRESULT DecodeCompressedMemory(const byte *data, size_t size, std::shared_ptr<SoundData> &outSound)
    {
        if (!data || size == 0)
            return E_INVALIDARG;
        if (FAILED(Init()))
            return E_FAIL;

        // create IStream from memory
        IStream *pStream = SHCreateMemStream(data, (UINT)size);
        if (!pStream)
            return E_FAIL;

        IMFByteStream *pByteStream = nullptr;
        HRESULT hr = MFCreateMFByteStreamOnStream(pStream, &pByteStream);
        pStream->Release();
        if (FAILED(hr))
            return hr;

        IMFSourceReader *pReader = nullptr;
        hr = MFCreateSourceReaderFromByteStream(pByteStream, nullptr, &pReader);
        pByteStream->Release();
        if (FAILED(hr))
            return hr;

        // Request PCM 16-bit output. Let MF pick sample rate / channels (or convert if needed).
        IMFMediaType *pOutType = nullptr;
        hr = MFCreateMediaType(&pOutType);
        if (FAILED(hr))
        {
            pReader->Release();
            return hr;
        }
        hr = pOutType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        if (FAILED(hr))
        {
            pOutType->Release();
            pReader->Release();
            return hr;
        }
        hr = pOutType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
        if (FAILED(hr))
        {
            pOutType->Release();
            pReader->Release();
            return hr;
        }
        hr = pOutType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
        if (FAILED(hr))
        {
            pOutType->Release();
            pReader->Release();
            return hr;
        }

        hr = pReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pOutType);
        pOutType->Release();
        if (FAILED(hr))
        {
            pReader->Release();
            return hr;
        }

        IMFMediaType *actualType = nullptr;
        hr = pReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &actualType);
        if (FAILED(hr))
        {
            pReader->Release();
            return hr;
        }

        // Query format info
        UINT32 channels = 0, samplesPerSec = 44100, bits = 16;
        (void)actualType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels);
        (void)actualType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &samplesPerSec);
        (void)actualType->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bits);
        if (channels == 0)
            channels = 2;
        if (samplesPerSec == 0)
            samplesPerSec = 44100;
        if (bits == 0)
            bits = 16;

        // Construct WAVEFORMATEX
        WAVEFORMATEX wfx = {};
        wfx.wFormatTag = WAVE_FORMAT_PCM;
        wfx.nChannels = (WORD)channels;
        wfx.nSamplesPerSec = samplesPerSec;
        wfx.wBitsPerSample = (WORD)bits;
        wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample) / 8;
        wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
        wfx.cbSize = 0;

        // Read decoded samples
        std::vector<byte> pcm;
        for (;;)
        {
            IMFSample *pSample = nullptr;
            DWORD streamIndex = 0, flags = 0;
            LONGLONG llTime = 0;
            hr = pReader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, &streamIndex, &flags, &llTime, &pSample);
            if (FAILED(hr))
            {
                if (pSample)
                    pSample->Release();
                actualType->Release();
                pReader->Release();
                return hr;
            }
            if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
            {
                if (pSample)
                {
                    pSample->Release();
                    pSample = nullptr;
                }
                break;
            }
            if (pSample)
            {
                IMFMediaBuffer *pBuffer = nullptr;
                hr = pSample->ConvertToContiguousBuffer(&pBuffer);
                if (SUCCEEDED(hr) && pBuffer)
                {
                    BYTE *pData = nullptr;
                    DWORD maxLen = 0, currLen = 0;
                    hr = pBuffer->Lock(&pData, &maxLen, &currLen);
                    if (SUCCEEDED(hr))
                    {
                        pcm.insert(pcm.end(), pData, pData + currLen);
                        pBuffer->Unlock();
                    }
                    pBuffer->Release();
                }
                pSample->Release();
            }
            // continue until EOF
        }

        actualType->Release();
        pReader->Release();

        // Fill out sound object
        std::shared_ptr<SoundData> sd = std::make_shared<SoundData>();
        sd->pcm.swap(pcm);
        sd->wfx = wfx;
        outSound = sd;
        return S_OK;
    }

    // 播放一次性音效（不会阻塞）
    HRESULT PlayOneShot(const std::shared_ptr<SoundData> &sound, float volume = 1.0f)
    {
        if (!sound || sound->pcm.empty() || !xaudio2)
            return E_FAIL;
        IXAudio2SourceVoice *src = nullptr;
        HRESULT hr = xaudio2->CreateSourceVoice(&src, (WAVEFORMATEX *)&sound->wfx);
        if (FAILED(hr))
            return hr;
        src->SetVolume(volume * sfxVolume);
        XAUDIO2_BUFFER buf = {};
        buf.pAudioData = sound->pcm.data();

        buf.AudioBytes = (UINT32)sound->pcm.size();
        buf.Flags = XAUDIO2_END_OF_STREAM;
        buf.LoopCount = 0;
        hr = src->SubmitSourceBuffer(&buf);
        if (FAILED(hr))
        {
            src->DestroyVoice();
            return hr;
        }
        hr = src->Start(0);
        if (FAILED(hr))
        {
            src->DestroyVoice();
            return hr;
        }

        // spawn thread to clean the voice after playback (简单实现)
        double seconds = 1.0;
        if (sound->wfx.nAvgBytesPerSec > 0)
        {
            seconds = double(sound->pcm.size()) / double(sound->wfx.nAvgBytesPerSec);
        }
        std::thread([src, seconds]()
                    {
            // 等待播放结束并销毁 voice
            Sleep((DWORD)(seconds * 1000.0 + 200));
            src->Stop(0);
            src->FlushSourceBuffers();
            src->DestroyVoice(); })
            .detach();

        return S_OK;
    }

    // 播放背景音乐（循环）
    HRESULT PlayLoop(const std::shared_ptr<SoundData> &sound, float volume = 0.5f)
    {
        if (!sound || sound->pcm.empty() || !xaudio2)
            return E_FAIL;

        std::lock_guard<std::mutex> lk(musicMutex);
        // stop previous
        if (musicVoice)
        {
            musicVoice->Stop(0);
            musicVoice->FlushSourceBuffers();
            musicVoice->DestroyVoice();
            musicVoice = nullptr;
        }

        HRESULT hr = xaudio2->CreateSourceVoice(&musicVoice, (WAVEFORMATEX *)&sound->wfx);
        if (FAILED(hr))
        {
            musicVoice = nullptr;
            return hr;
        }
        musicVoice->SetVolume(volume * bgVolume);

        XAUDIO2_BUFFER buf = {};
        buf.pAudioData = sound->pcm.data();

        buf.AudioBytes = (UINT32)sound->pcm.size();
        buf.LoopCount = XAUDIO2_LOOP_INFINITE;
        buf.Flags = XAUDIO2_END_OF_STREAM;
        hr = musicVoice->SubmitSourceBuffer(&buf);
        if (FAILED(hr))
        {
            musicVoice->DestroyVoice();
            musicVoice = nullptr;
            return hr;
        }
        hr = musicVoice->Start(0);
        if (FAILED(hr))
        {
            musicVoice->DestroyVoice();
            musicVoice = nullptr;
            return hr;
        }
        currentMusic = sound;
        return S_OK;
    }

    void StopLoop()
    {
        std::lock_guard<std::mutex> lk(musicMutex);
        if (musicVoice)
        {
            musicVoice->Stop(0);
            musicVoice->FlushSourceBuffers();
            musicVoice->DestroyVoice();
            musicVoice = nullptr;
            currentMusic.reset();
        }
    }

    void SetBgVolume(float v)
    {
        std::lock_guard<std::mutex> lk(musicMutex);
        bgVolume = v;
        if (musicVoice)
            musicVoice->SetVolume(bgVolume);
    }

    void SetSfxVolume(float v)
    {
        sfxVolume = v;
    }

private:
    IXAudio2 *xaudio2;
    IXAudio2MasteringVoice *masteringVoice;
    IXAudio2SourceVoice *musicVoice;
    std::shared_ptr<SoundData> currentMusic;
    std::mutex musicMutex;
    std::mutex initMutex;
    float bgVolume;
    float sfxVolume;
    bool initialized;
};

/////////////////////////
// 单例 Audio manager（负责资源缓存、读取资源、提供外部 API）
class AudioManager
{
public:
    AudioManager()
    {
        // lazy init will call engine.Init() on first decode/play
    }

    ~AudioManager()
    {
        engine.Shutdown();
    }

    // 返回已解码的 SoundData（缓存）。若缓存不存在则从资源加载并解码后缓存。
    std::shared_ptr<SoundData> GetOrDecodeResource(unsigned int resourceId)
    {
        {
            std::lock_guard<std::mutex> lk(cacheMutex);
            auto it = cache.find(resourceId);
            if (it != cache.end())
                return it->second;
        }

        // Decode and cache
        std::vector<byte> bytes;
        if (!LoadResourceBytes(resourceId, bytes))
        {
            return nullptr;
        }

        std::shared_ptr<SoundData> sd;
        HRESULT hr = engine.DecodeCompressedMemory(bytes.data(), bytes.size(), sd);
        if (FAILED(hr) || !sd)
        {
            return nullptr;
        }

        {
            std::lock_guard<std::mutex> lk(cacheMutex);
            cache[resourceId] = sd;
        }
        return sd;
    }

    void PlayBg(unsigned int resourceId)
    {
        if (!engine.Init())
        {
            // try init anyway
            if (engine.Init() != S_OK)
                return;
        }
        auto sd = GetOrDecodeResource(resourceId);
        if (!sd)
            return;
        engine.PlayLoop(sd, 1.0f);
    }

    void StopBg()
    {
        engine.StopLoop();
    }

    void PlaySound(unsigned int resourceId)
    {
        if (!engine.Init())
        {
            if (engine.Init() != S_OK)
                return;
        }
        auto sd = GetOrDecodeResource(resourceId);
        if (!sd)
            return;
        engine.PlayOneShot(sd, 1.0f);
    }

    void SetBgVolume(float v)
    {
        if (v < 0.0f)
            v = 0.0f;
        if (v > 1.0f)
            v = 1.0f;
        engine.SetBgVolume(v);
    }

    void SetSfxVolume(float v)
    {
        if (v < 0.0f)
            v = 0.0f;
        if (v > 1.0f)
            v = 1.0f;
        engine.SetSfxVolume(v);
    }

private:
    // load RCDATA resource bytes
    bool LoadResourceBytes(unsigned int resId, std::vector<byte> &out)
    {
        HMODULE hModule = GetModuleHandle(nullptr);
        if (!hModule)
            return false;
        HRSRC hRes = FindResource(hModule, MAKEINTRESOURCE(resId), RT_RCDATA);
        if (!hRes)
            return false;
        HGLOBAL hResLoad = LoadResource(hModule, hRes);
        if (!hResLoad)
            return false;
        DWORD resSize = SizeofResource(hModule, hRes);
        if (resSize == 0)
            return false;
        const void *pData = LockResource(hResLoad);
        if (!pData)
            return false;
        out.resize(resSize);
        memcpy(out.data(), pData, resSize);
        return true;
    }

    AudioEngine engine;
    std::unordered_map<unsigned int, std::shared_ptr<SoundData>> cache;
    std::mutex cacheMutex;
};

// 单实例
static AudioManager g_audioManager;

/////////////////////////
// Audios API 实现（外部调用的函数）
namespace Audios
{

    void bg(unsigned int resourceId)
    {
        g_audioManager.PlayBg(resourceId);
    }

    void sound(unsigned int resourceId)
    {
        g_audioManager.PlaySound(resourceId);
    }

    void stopBg()
    {
        g_audioManager.StopBg();
    }

    void setBgVolume(float vol)
    {
        g_audioManager.SetBgVolume(vol);
    }

    void setSfxVolume(float vol)
    {
        g_audioManager.SetSfxVolume(vol);
    }

} // namespace Audios
