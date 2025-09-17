#include "GDI.h"
#include <objidl.h>
#include <memory>
using namespace Gdiplus;
#pragma comment(lib, "Gdiplus.lib")
GDI::GDI(HWND h) : hwnd(h)
{
}

void GDI::init()
{
    // Initialize GDI+.
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    //  内存位图
    memBitmap = std::make_unique<Gdiplus::Bitmap>(640, 146, PixelFormat32bppPARGB);
    // 内存位图绘制上下文
    memGraphics = std::make_unique<Gdiplus::Graphics>(memBitmap.get());

    // 画笔,红色宽2
    pen = std::make_unique<Gdiplus::Pen>(Gdiplus::Color(255, 0, 0, 255), 2.0f);

    // gdi+高质量渲染
    memGraphics->SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
    // 文本抗锯齿
    memGraphics->SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);

    // 窗口绘制对象
    trueGraphics = std::make_unique<Gdiplus::Graphics>(hwnd);
}

void GDI::end()
{
    GdiplusShutdown(gdiplusToken);
}
void GDI::tick()
{
    // 清空内存位图,白
    Gdiplus::Color backgroundColor(255, 255, 255, 255);
    memGraphics->Clear(backgroundColor);

    // 内存位图上的绘制,类似离线canvas
    memGraphics->DrawLine(pen.get(), 0, 0, 640, 146);

    // 绘制内存位图
    trueGraphics->DrawImage(memBitmap.get(), 0, 0);
}