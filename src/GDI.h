#pragma once
#include <Windows.h>
#include <gdiplus.h>
#include <memory>
class GDI
{
public:
    GDI(HWND h);
    ~GDI() = default;

    void init();

    void tick();

    void end();

private:
    HWND hwnd;

    // GDI+释放对象
    ULONG_PTR gdiplusToken;
    // 双缓冲的内存位图
    std::unique_ptr<Gdiplus::Bitmap> memBitmap;
    // 内存双缓冲
    std::unique_ptr<Gdiplus::Graphics> memGraphics;
    // 实际绘制上下文
    std::unique_ptr<Gdiplus::Graphics> trueGraphics;
    // 笔缓存
    std::unique_ptr<Gdiplus::Pen> pen;
};