#pragma once
#include <Windows.h>
#include <gdiplus.h>
#include <memory>
#include <unordered_map>
class GDI
{
public:
    GDI(HWND h);
    ~GDI() = default;

    void init();

    void tick();

    void image(int resId, int x, int y, int w, int h);

    void end();

private:
    HWND hwnd;

    // GDI+释放对象
    ULONG_PTR gdiplusToken;
    // 双缓冲的内存位图
    std::unique_ptr<Gdiplus::Bitmap> memBitmap;
    // 内存双缓冲
    std::unique_ptr<Gdiplus::Graphics> memGraphics;
    // 笔缓存
    std::unique_ptr<Gdiplus::Pen> pen;

    // 图片缓存
    std::unordered_map<int, std::unique_ptr<Gdiplus::Bitmap>> bitmapCache;
    // rcdata图片资源
    Gdiplus::Bitmap *LoadBitmapFromRCDATA(int resId);
};