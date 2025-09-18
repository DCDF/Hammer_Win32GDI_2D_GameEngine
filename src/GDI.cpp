#include "GDI.h"
#include <objidl.h>
#include <cassert>
using namespace Gdiplus;
#pragma comment(lib, "Gdiplus.lib")

HWND GDI::hwnd = nullptr;
ULONG_PTR GDI::gdiplusToken = 0;

void *GDI::backPixels = nullptr;
HBITMAP GDI::hBackBitmap = nullptr;
BITMAPINFO GDI::backInfo = {};
int GDI::backWidth = 0;
int GDI::backHeight = 0;
int GDI::backStride = 0;

std::unique_ptr<Gdiplus::Bitmap> GDI::memBitmap = nullptr;
std::unique_ptr<Gdiplus::Graphics> GDI::memGraphics = nullptr;
std::unique_ptr<Gdiplus::Font> GDI::defaultFont = nullptr;
std::unordered_map<int, std::unique_ptr<Gdiplus::Bitmap>> GDI::bitmapCache;
std::vector<GDI::Command> GDI::commands;

static inline int AlignStride(int w) { return w * 4; } // 32bpp

bool GDI::createBackBuffer(int w, int h)
{
    destroyBackBuffer();
    if (w <= 0 || h <= 0)
        return false;

    backWidth = w;
    backHeight = h;
    backStride = AlignStride(backWidth);

    ZeroMemory(&backInfo, sizeof(backInfo));
    backInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    backInfo.bmiHeader.biWidth = backWidth;
    backInfo.bmiHeader.biHeight = -backHeight; // top-down DIB
    backInfo.bmiHeader.biPlanes = 1;
    backInfo.bmiHeader.biBitCount = 32;
    backInfo.bmiHeader.biCompression = BI_RGB;
    backInfo.bmiHeader.biSizeImage = backStride * backHeight;

    HDC hdc = GetDC(hwnd);
    void *pv = nullptr;
    hBackBitmap = CreateDIBSection(hdc, &backInfo, DIB_RGB_COLORS, &pv, NULL, 0);
    ReleaseDC(hwnd, hdc);

    if (!hBackBitmap || !pv)
    {
        destroyBackBuffer();
        return false;
    }

    backPixels = pv;

    // Wrap the pixel buffer into a Gdiplus::Bitmap (zero-copy)
    memBitmap = std::make_unique<Gdiplus::Bitmap>(backWidth, backHeight, backStride, PixelFormat32bppPARGB, reinterpret_cast<BYTE *>(backPixels));
    if (!memBitmap || memBitmap->GetLastStatus() != Ok)
    {
        destroyBackBuffer();
        return false;
    }

    memGraphics = std::make_unique<Gdiplus::Graphics>(memBitmap.get());
    memGraphics->SetSmoothingMode(SmoothingModeHighQuality);
    memGraphics->SetTextRenderingHint(TextRenderingHintAntiAlias);

    return true;
}

void GDI::destroyBackBuffer()
{
    memGraphics.reset();
    memBitmap.reset();

    if (hBackBitmap)
    {
        DeleteObject(hBackBitmap);
        hBackBitmap = nullptr;
    }
    backPixels = nullptr;
    backWidth = backHeight = backStride = 0;
    ZeroMemory(&backInfo, sizeof(backInfo));
}

void GDI::init(HWND h)
{
    hwnd = h;
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    RECT rc;
    GetClientRect(hwnd, &rc);
    int w = rc.right - rc.left;
    int hgt = rc.bottom - rc.top;

    if (!createBackBuffer(w, hgt))
    {
        // fallback: try to make GDI+ bitmap only (should be rare)
        memBitmap = std::make_unique<Gdiplus::Bitmap>(w, hgt, PixelFormat32bppPARGB);
        memGraphics = std::make_unique<Gdiplus::Graphics>(memBitmap.get());
        memGraphics->SetSmoothingMode(SmoothingModeHighQuality);
        memGraphics->SetTextRenderingHint(TextRenderingHintAntiAlias);
    }

    defaultFont = std::make_unique<Gdiplus::Font>(L"Arial", 12.0f, FontStyleRegular, UnitPixel);

    commands.reserve(512);
}

void GDI::end()
{
    commands.clear();
    bitmapCache.clear();
    defaultFont.reset();

    destroyBackBuffer();

    if (gdiplusToken)
    {
        GdiplusShutdown(gdiplusToken);
        gdiplusToken = 0;
    }
}

void GDI::begin(float /*dt*/)
{
    if (!memGraphics)
        return;
    Color bg(255, 255, 255, 255);
    memGraphics->Clear(bg);
}

void GDI::tick(float /*dt*/)
{
    if (!memGraphics)
    {
        commands.clear();
        return;
    }

    // execute lightweight commands
    for (const auto &c : commands)
    {
        if (c.type == GDI::Type::DrawImage)
        {
            Bitmap *img = LoadBitmapFromRCDATA(c.resId);
            if (img)
            {
                memGraphics->DrawImage(img, c.x, c.y, c.w, c.h);
            }
        }
        else if (c.type == GDI::Type::DrawText)
        {
            SolidBrush brush(c.color);
            PointF pt(static_cast<REAL>(c.x), static_cast<REAL>(c.y));
            memGraphics->DrawString(c.text.c_str(), -1, defaultFont.get(), pt, &brush);
        }
    }

    // after applying commands clear for next frame
    commands.clear();
}

void GDI::flush(float /*dt*/)
{
    if (!hwnd)
        return;
    HDC hdc = GetDC(hwnd);
    if (!hdc)
        return;

    if (backPixels && backWidth > 0 && backHeight > 0)
    {
        StretchDIBits(
            hdc,
            0, 0, backWidth, backHeight,
            0, 0, backWidth, backHeight,
            backPixels,
            &backInfo,
            DIB_RGB_COLORS,
            SRCCOPY);
    }
    else if (memBitmap)
    {
        Graphics g(hdc);
        g.DrawImage(memBitmap.get(), 0, 0, memBitmap->GetWidth(), memBitmap->GetHeight());
    }

    ReleaseDC(hwnd, hdc);
}

Bitmap *GDI::LoadBitmapFromRCDATA(int resId)
{
    auto it = bitmapCache.find(resId);
    if (it != bitmapCache.end())
        return it->second.get();

    HMODULE hMod = GetModuleHandleW(NULL);
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

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, cb);
    if (!hMem)
        return nullptr;
    void *dst = GlobalLock(hMem);
    memcpy(dst, p, cb);
    GlobalUnlock(hMem);

    IStream *stream = nullptr;
    if (CreateStreamOnHGlobal(hMem, TRUE, &stream) != S_OK)
    {
        GlobalFree(hMem);
        return nullptr;
    }

    Bitmap *bmp = Bitmap::FromStream(stream);
    stream->Release();
    if (!bmp || bmp->GetLastStatus() != Ok)
    {
        delete bmp;
        return nullptr;
    }
    bitmapCache[resId] = std::unique_ptr<Bitmap>(bmp);
    return bmp;
}

// backward-compatible wrappers
void GDI::image(int resId, int x, int y, int w, int h, bool flip)
{
    pushImage(resId, x, y, w, h, flip);
}
void GDI::text(const std::wstring &txt, int x, int y, float size, Gdiplus::Color color)
{
    pushText(txt, x, y, size, color);
}
