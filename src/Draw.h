// Draw.h
#pragma once
#include "PC.h"
#include <map>
#include <wincodec.h>
#include <string>
#include <chrono>

struct BitmapCache
{
    HBITMAP bmp;
    HDC memDC;
    HDC flippedMemDC; // 水平翻转缓存
    int width;
    int height;
};

class Draw
{
public:
    Draw(PC &pc);
    ~Draw();

    void begin();
    void tick();
    void end();

    // 普通图片绘制
    void image(int resourceId, int dx, int dy, bool flipX = false);
    void image(int resourceId, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh, bool flipX = false);

    // 网格 Sprite 支持
    struct GridSprite
    {
        int resourceId;
        int frameWidth;
        int frameHeight;
        int cols;
        int rows;
        int xOffset;
        int yOffset;
    };
    void drawGridSprite(const GridSprite &sprite, int frameIndex, int dx, int dy, bool flipX = false);

    // 简单文字绘制
    void text(const std::wstring &str, int x, int y,
              COLORREF color = RGB(255, 0, 0),
              int fontSize = 16,
              UINT align = DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // 矩形绘制
    void rect(int x, int y, int w, int h,
              COLORREF color = RGB(0, 255, 0),
              bool fill = true);

    void showFPS(bool enable = true) { showFPS_ = enable; }

private:
    PC &pc_;
    HDC backDC_;
    std::map<int, BitmapCache> bmpCache_;

    BitmapCache loadPNGResource(int resourceId);
    HDC createFlippedDC(BitmapCache &bc);

    // FPS
    bool showFPS_ = false;
    int fps_ = 0;
    int frameCount_ = 0;
    std::chrono::steady_clock::time_point lastTime_;
};
