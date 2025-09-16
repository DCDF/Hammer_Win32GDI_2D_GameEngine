// Draw.cpp
#include "Draw.h"
#include <windows.h>
#include <wincodec.h>
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "Msimg32.lib")

Draw::Draw(PC &pc) : pc_(pc)
{
    backDC_ = pc_.dc();
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
}

Draw::~Draw()
{
    for (auto &p : bmpCache_)
    {
        if (p.second.memDC)
            DeleteDC(p.second.memDC);
        if (p.second.flippedMemDC)
            DeleteDC(p.second.flippedMemDC);
        if (p.second.bmp)
            DeleteObject(p.second.bmp);
    }
    bmpCache_.clear();
}

void Draw::begin()
{
    RECT rc;
    GetClientRect(pc_.window(), &rc);
    FillRect(backDC_, &rc, (HBRUSH)(COLOR_WINDOW + 1));

    frameCount_++;
    auto now = std::chrono::steady_clock::now();
    if (lastTime_.time_since_epoch().count() == 0)
        lastTime_ = now;
    double delta = std::chrono::duration<double>(now - lastTime_).count();
    if (delta >= 1.0)
    {
        fps_ = frameCount_;
        frameCount_ = 0;
        lastTime_ = now;
    }
}

void Draw::tick() {}

void Draw::end()
{
    if (showFPS_)
    {
        std::wstring fpsText = L"FPS: " + std::to_wstring(fps_);
        text(fpsText, 10, 10, RGB(255, 0, 0), 16, DT_LEFT | DT_TOP);
    }

    HDC hdc = GetDC(pc_.window());
    RECT rc;
    GetClientRect(pc_.window(), &rc);
    BitBlt(hdc, 0, 0, rc.right, rc.bottom, backDC_, 0, 0, SRCCOPY);
    ReleaseDC(pc_.window(), hdc);
}

// 内部加载 PNG 并缓存
BitmapCache Draw::loadPNGResource(int resourceId)
{
    if (bmpCache_.count(resourceId))
        return bmpCache_[resourceId];

    BitmapCache bc = {};
    HRSRC hrsrc = FindResource(nullptr, MAKEINTRESOURCE(resourceId), RT_RCDATA);
    if (!hrsrc)
        return bc;
    HGLOBAL hglb = LoadResource(nullptr, hrsrc);
    if (!hglb)
        return bc;
    void *data = LockResource(hglb);
    DWORD size = SizeofResource(nullptr, hrsrc);

    IWICImagingFactory *pFactory = nullptr;
    CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                     IID_PPV_ARGS(&pFactory));

    IWICStream *pStream = nullptr;
    pFactory->CreateStream(&pStream);
    pStream->InitializeFromMemory((BYTE *)data, size);

    IWICBitmapDecoder *pDecoder = nullptr;
    pFactory->CreateDecoderFromStream(pStream, nullptr, WICDecodeMetadataCacheOnLoad, &pDecoder);

    IWICBitmapFrameDecode *pFrame = nullptr;
    pDecoder->GetFrame(0, &pFrame);

    IWICFormatConverter *pConverter = nullptr;
    pFactory->CreateFormatConverter(&pConverter);
    pConverter->Initialize(pFrame, GUID_WICPixelFormat32bppPBGRA,
                           WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);

    UINT width, height;
    pFrame->GetSize(&width, &height);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -(LONG)height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void *pBits = nullptr;
    HDC hdc = GetDC(pc_.window());
    bc.bmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
    pConverter->CopyPixels(nullptr, width * 4, width * height * 4, (BYTE *)pBits);

    bc.memDC = CreateCompatibleDC(backDC_);
    SelectObject(bc.memDC, bc.bmp);
    bc.width = width;
    bc.height = height;

    bc.flippedMemDC = createFlippedDC(bc); // 创建水平翻转缓存

    ReleaseDC(pc_.window(), hdc);
    pConverter->Release();
    pFrame->Release();
    pDecoder->Release();
    pStream->Release();
    pFactory->Release();

    bmpCache_[resourceId] = bc;
    return bc;
}

// 创建水平翻转缓存
HDC Draw::createFlippedDC(BitmapCache &bc)
{
    HDC hdc = CreateCompatibleDC(backDC_);
    HBITMAP hFlippedBmp = CreateCompatibleBitmap(backDC_, bc.width, bc.height);
    SelectObject(hdc, hFlippedBmp);

    // 水平翻转
    StretchBlt(hdc, bc.width, 0, -bc.width, bc.height, bc.memDC, 0, 0, bc.width, bc.height, SRCCOPY);

    return hdc;
}

// 普通图片绘制
void Draw::image(int resourceId, int dx, int dy, bool flipX)
{
    BitmapCache bc = loadPNGResource(resourceId);
    if (!bc.bmp || !bc.memDC)
        return;

    HDC srcDC = flipX ? bc.flippedMemDC : bc.memDC;
    BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    AlphaBlend(backDC_, dx, dy, bc.width, bc.height, srcDC, 0, 0, bc.width, bc.height, bf);
}

void Draw::image(int resourceId, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh, bool flipX)
{
    BitmapCache bc = loadPNGResource(resourceId);
    if (!bc.bmp || !bc.memDC)
        return;

    HDC srcDC = flipX ? bc.flippedMemDC : bc.memDC;
    BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    AlphaBlend(backDC_, dx, dy, dw, dh, srcDC, sx, sy, sw, sh, bf);
}

// 网格 Sprite 绘制
void Draw::drawGridSprite(const GridSprite &sprite, int frameIndex, int dx, int dy, bool flipX)
{
    BitmapCache bc = loadPNGResource(sprite.resourceId);
    if (!bc.bmp || !bc.memDC)
        return;

    int col = frameIndex % sprite.cols;
    int row = frameIndex / sprite.cols;

    int sx = flipX ? (sprite.cols - 1 - col) * sprite.frameWidth : col * sprite.frameWidth;
    int sy = row * sprite.frameHeight;
    int sw = sprite.frameWidth;
    int sh = sprite.frameHeight;

    HDC srcDC = flipX ? bc.flippedMemDC : bc.memDC;

    BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    AlphaBlend(backDC_, dx, dy, sw, sh, srcDC, sx, sy, sw, sh, bf);
}

// 简单文字绘制
void Draw::text(const std::wstring &str, int x, int y, COLORREF color, int fontSize, UINT align)
{
    HFONT hFont = CreateFontW(fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Consolas");
    HGDIOBJ oldFont = SelectObject(backDC_, hFont);
    SetTextColor(backDC_, color);
    SetBkMode(backDC_, TRANSPARENT);

    RECT rc = {x, y, x + 2000, y + 2000};
    DrawTextW(backDC_, str.c_str(), -1, &rc, align);

    SelectObject(backDC_, oldFont);
    DeleteObject(hFont);
}

// 矩形绘制
void Draw::rect(int x, int y, int w, int h, COLORREF color, bool fill)
{
    HBRUSH hBrush = CreateSolidBrush(color);
    HBRUSH oldBrush = (HBRUSH)SelectObject(backDC_, hBrush);
    HPEN hPen = CreatePen(PS_SOLID, 1, color);
    HPEN oldPen = (HPEN)SelectObject(backDC_, hPen);

    if (fill)
        Rectangle(backDC_, x, y, x + w, y + h);
    else
    {
        SelectObject(backDC_, GetStockObject(NULL_BRUSH));
        Rectangle(backDC_, x, y, x + w, y + h);
    }

    SelectObject(backDC_, oldBrush);
    SelectObject(backDC_, oldPen);
    DeleteObject(hBrush);
    DeleteObject(hPen);
}
