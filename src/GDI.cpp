#include "GDI.h"
#include <objidl.h>
#include <memory>
using namespace Gdiplus;
#pragma comment(lib, "Gdiplus.lib")

HWND GDI::hwnd;
using Command = std::function<void()>;
ULONG_PTR GDI::gdiplusToken;
std::unique_ptr<Gdiplus::Bitmap> GDI::memBitmap;
std::unique_ptr<Gdiplus::Graphics> GDI::memGraphics;
std::unique_ptr<Gdiplus::Pen> GDI::pen;
std::unordered_map<int, std::unique_ptr<Gdiplus::Bitmap>> GDI::bitmapCache;
std::vector<Command> GDI::commands;

void GDI::init(HWND h)
{
    hwnd = h;
    // Initialize GDI+.
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    RECT rc;
    GetClientRect(hwnd, &rc);
    int clientW = rc.right - rc.left;
    int clientH = rc.bottom - rc.top;

    //  内存位图
    memBitmap = std::make_unique<Gdiplus::Bitmap>(clientW, clientH, PixelFormat32bppPARGB);
    // 内存位图绘制上下文
    memGraphics = std::make_unique<Gdiplus::Graphics>(memBitmap.get());

    // 画笔,红色宽2
    pen = std::make_unique<Gdiplus::Pen>(Gdiplus::Color(255, 0, 0, 255), 2.0f);

    // gdi+高质量渲染
    memGraphics->SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
    // 文本抗锯齿
    memGraphics->SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
}

void GDI::end()
{
    bitmapCache.clear();
    pen.reset();
    memGraphics.reset();
    memBitmap.reset();

    if (gdiplusToken)
    {
        GdiplusShutdown(gdiplusToken);
        gdiplusToken = 0;
    }
}
void GDI::tick()
{
    // 清空内存位图,白
    Gdiplus::Color backgroundColor(255, 255, 255, 255);
    memGraphics->Clear(backgroundColor);

    // 内存位图上的绘制,类似离线canvas
    memGraphics->DrawLine(pen.get(), 0, 0, 640, 160);

    for (auto cmd : commands)
    {
        cmd();
    }

    // 绘制内存位图,创建一个 Graphics(hdc) 对象开销几乎可以忽略（远比 DrawImage 轻），最安全的做法就是每帧临时建一个
    HDC hdc = GetDC(hwnd);
    if (hdc)
    {
        Gdiplus::Graphics g(hdc); // 局部对象，作用域结束时析构
        g.DrawImage(memBitmap.get(), 0, 0);
        ReleaseDC(hwnd, hdc);
    }
}
void GDI::image(int resId, int x, int y, int w, int h, bool flip)
{
    //todo flip
    auto image = GDI::LoadBitmapFromRCDATA(resId);
    if (image)
    {
        memGraphics->DrawImage(image, x, y, w, h);
    }
}

#include <objidl.h> // IStream
#include <gdiplus.h>

Bitmap *GDI::LoadBitmapFromRCDATA(int resId)
{
    auto cache = bitmapCache.find(resId);
    if (cache != bitmapCache.end())
    {
        return cache->second.get();
    }
    HMODULE hMod = GetModuleHandleW(NULL); // 当前 EXE（静态链接时资源在这里）
    HRSRC hrs = FindResource(hMod, MAKEINTRESOURCE(resId), RT_RCDATA);
    if (!hrs)
        return nullptr;
    HGLOBAL hRes = LoadResource(hMod, hrs);
    if (!hRes)
        return nullptr;
    DWORD cb = SizeofResource(hMod, hrs);
    void *p = LockResource(hRes);
    if (!p || cb == 0)
        return nullptr;

    // 复制到可移动内存并用 IStream 包装（CreateStreamOnHGlobal）
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, cb);
    if (!hMem)
        return nullptr;
    void *dst = GlobalLock(hMem);
    memcpy(dst, p, cb);
    GlobalUnlock(hMem);

    IStream *stream = nullptr;
    if (CreateStreamOnHGlobal(hMem, TRUE /*释放stream时释放hMem*/, &stream) != S_OK)
    {
        GlobalFree(hMem);
        return nullptr;
    }

    Bitmap *bmp = Bitmap::FromStream(stream);
    stream->Release(); // 若 FromStream 成功，GDI+ 内部会复制所需数据
    if (!bmp || bmp->GetLastStatus() != Ok)
    {
        delete bmp;
        return nullptr;
    }
    bitmapCache[resId] = std::unique_ptr<Bitmap>(bmp);
    return bmp;
}