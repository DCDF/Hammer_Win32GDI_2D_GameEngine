#pragma once
#include <windows.h>

// 简洁接口：只需这两个调用
// 播放/替换 BGM（资源 id, RC 使用 RCDATA）
void Audio_bg(int resourceId);

// 播放一次性 SFX（异步播放并自动回收）
void Audio_sfx(int resourceId);

// 可选：程序退出时调用（否则会自动清理）
void Audio_shutdown();
