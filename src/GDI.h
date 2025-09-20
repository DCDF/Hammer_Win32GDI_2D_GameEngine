#pragma once
#include <Windows.h>
#include <gdiplus.h>
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include <cstdint>

// 直接像素访问的图像缓存结构
struct CachedImage
{
    int width = 0;
    int height = 0;
    std::unique_ptr<uint32_t[]> pixels; // 预乘Alpha的BGRA格式
    bool isOpaque = true;               // 优化：标记图像是否完全不透明
};

class GDI
{
public:
    GDI() = delete;
    ~GDI() = default;

    static void init(HWND h);    // 初始化（创建DIBSection + GDI+）
    static void begin(float dt); // 每帧开始（清屏）
    static void tick(float dt);  // 现在此函数将为空，为保持API兼容性而保留
    static void flush(float dt); // 将内存buffer刷到窗口
    static void end();           // 释放资源

    // 绘图接口
    static void image(int resId, int x, int y, int w, int h, bool flip = false);
    static void imageEx(int resId, int x, int y, int w, int h,
                        bool flip, int srcX, int srcY,
                        int srcW, int srcH);
    static void text(const std::wstring &txt, int x, int y, float size = 12.0f, Gdiplus::Color color = Gdiplus::Color::White);
    // 新增：绘制填充矩形接口
    static void rect(int x, int y, int w, int h, Gdiplus::Color color);

    // 设置相机位置
    static void setCamera(int x, int y);

private:
    static HWND hwnd;
    static ULONG_PTR gdiplusToken;

    // DIBSection 后备缓冲区
    static uint32_t *backPixels; // 直接指向像素数据 (BGRA)
    static HBITMAP hBackBitmap;
    static HDC hBackDC;
    static BITMAPINFO backInfo;
    static int backWidth;
    static int backHeight;

    // 缓存
    static std::unordered_map<int, CachedImage> imageCache;
    static std::unordered_map<float, HFONT> fontCache;

    // 相机视口偏移
    static int cameraX;
    static int cameraY;

    // 辅助函数
    static bool createBackBuffer(int w, int h);
    static void destroyBackBuffer();
    static CachedImage *loadImage(int resId);
    static HFONT getFont(float size);

    // 优化的绘制函数 (签名已修改)
    static void drawImageFast(int resId, int x, int y, int w, int h,
                              bool flip, bool hasSrcRect, int srcX,
                              int srcY, int srcW, int srcH);
    static void drawRectFast(int x, int y, int w, int h, Gdiplus::Color color);
};