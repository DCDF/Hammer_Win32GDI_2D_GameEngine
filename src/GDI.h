#pragma once
#include <Windows.h>
#include <gdiplus.h>
#include <memory>
#include <unordered_map>
#include <string>
#include <cstdint>
#include <immintrin.h> // For SIMD Intrinsics (SSE2)

// 图像缓存结构 (保持不变)
struct CachedImage
{
    int width = 0;
    int height = 0;
    std::unique_ptr<uint32_t[]> pixels; // 预乘Alpha的BGRA格式
    bool isOpaque = true;
};

class GDI
{
public:
    GDI() = delete;
    ~GDI() = default;

    // --- 核心生命周期接口 ---
    static void init(HWND h);
    static void begin([[maybe_unused]] float dt)
    { // 清屏
        if (backPixels)
        {
            memset(backPixels, 0, (size_t)backWidth * (size_t)backHeight * 4);
        }
    }
    // tick 函数为空，仅为保持API兼容性。所有绘制都是立即执行的。
    static void tick([[maybe_unused]] float dt) {}
    static void flush([[maybe_unused]] float dt);
    static void end();

    // --- 绘图接口 (定义移至头文件以强制内联) ---
    static void image(int resId, int x, int y, int w, int h, bool flip = false)
    {
        if (!backPixels)
            return;
        drawImageFast(resId, x - cameraX, y - cameraY, w, h, flip, false, 0, 0, 0, 0);
    }

    static void imageEx(int resId, int x, int y, int w, int h,
                        bool flip, int srcX, int srcY,
                        int srcW, int srcH)
    {
        if (!backPixels)
            return;
        drawImageFast(resId, x - cameraX, y - cameraY, w, h, flip, true, srcX, srcY, srcW, srcH);
    }

    // 用于UI等不受相机影响的静态图
    static void imageStatic(int resId, int x, int y)
    {
        if (!backPixels)
            return;
        drawImageStaticFast(resId, x, y);
    }

    // 新增：用于绘制世界背景等受相机影响、但不缩放的静态图
    static void imageWorld(int resId, int x, int y)
    {
        if (!backPixels)
            return;
        // 直接复用最高效的静态图绘制函数，并传入经过相机转换的坐标
        drawImageStaticFast(resId, x - cameraX, y - cameraY);
    }

    static void rect(int x, int y, int w, int h, Gdiplus::Color color = Gdiplus::Color::Green)
    {
        if (!backPixels || w <= 0 || h <= 0)
            return;
        drawRectFast(x - cameraX, y - cameraY, w, h, color);
    }

    static void text(const std::wstring &txt, int x, int y, float size = 12.0f, Gdiplus::Color color = Gdiplus::Color::White);

    // --- 相机接口 (内联) ---
    static void setCamera(int x, int y)
    {
        cameraX = x;
        cameraY = y;
    }

private:
    // --- 核心成员 ---
    static HWND hwnd;
    static ULONG_PTR gdiplusToken;
    static uint32_t *backPixels;
    static HBITMAP hBackBitmap;
    static HDC hBackDC;
    static BITMAPINFO backInfo;
    static int backWidth;
    static int backHeight;
    static int cameraX;
    static int cameraY;

    // --- 缓存 ---
    static std::unordered_map<int, CachedImage> imageCache;
    static std::unordered_map<float, HFONT> fontCache;

    // --- 内部辅助函数 ---
    static bool createBackBuffer(int w, int h);
    static void destroyBackBuffer();
    static CachedImage *loadImage(int resId);
    static HFONT getFont(float size);

    // --- 底层绘制实现 (性能关键) ---
    static void drawImageFast(int resId, int x, int y, int w, int h,
                              bool flip, bool hasSrcRect, int srcX,
                              int srcY, int srcW, int srcH);
    static void drawRectFast(int x, int y, int w, int h, Gdiplus::Color color);
    static void drawImageStaticFast(int resId, int x, int y);
};