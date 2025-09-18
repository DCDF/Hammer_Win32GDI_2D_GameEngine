// Audios.h
#pragma once
#include <cstdint>

namespace Audios
{
    // 开始循环播放资源 ID 指向的压缩音频（RCDATA）
    // 首次调用会解码并缓存（线程安全）。
    // 使用示例: Audios::bg(301);
    void bg(unsigned int resourceId);

    // 播放一次性音效（并发可多路叠加）
    // 首次调用会解码并缓存。
    // 使用示例: Audios::sound(101);
    void sound(unsigned int resourceId);

    // 停止当前后台音乐
    void stopBg();

    // 可选：设置背景音乐音量（0.0f - 1.0f）
    void setBgVolume(float vol);

    // 可选：设置音效默认音量（0.0f - 1.0f）
    void setSfxVolume(float vol);
}
