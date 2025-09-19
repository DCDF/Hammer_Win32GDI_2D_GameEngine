// ---------------------------------------------------------------------------
// --- 预处理器和头文件包含顺序 ---
// ---------------------------------------------------------------------------

#define NOMINMAX

#include <windows.h>
#include <algorithm>
#include <cmath>
#include <shlwapi.h>
#include <unordered_map>
#include <memory>
#include <vector>
#include <cstdint>
#include <string>

using std::max;
using std::min;

#include "GDI.h"

#pragma comment(lib, "Gdiplus.lib")
#pragma comment(lib, "Msimg32.lib")
#pragma comment(lib, "shlwapi.lib")

// ---------------------------------------------------------------------------
// --- 静态成员初始化 ---
// ---------------------------------------------------------------------------
HWND GDI::hwnd = nullptr;
ULONG_PTR GDI::gdiplusToken = 0;
uint32_t *GDI::backPixels = nullptr;
HBITMAP GDI::hBackBitmap = nullptr;
HDC GDI::hBackDC = nullptr;
BITMAPINFO GDI::backInfo = {};
int GDI::backWidth = 0;
int GDI::backHeight = 0;
// removed memGraphics as per your request
std::unordered_map<int, CachedImage> GDI::imageCache;
std::unordered_map<float, HFONT> GDI::fontCache;
std::vector<GDI::Command> GDI::commands;
// In GDI.cpp, with other static member initializations
int GDI::cameraX = 0;
int GDI::cameraY = 0;
// ---------------------------------------------------------------------------
// --- 内部辅助函数 ---
// ---------------------------------------------------------------------------
namespace
{
    inline uint32_t AlphaBlendPixel_32bpp(uint32_t dest, uint32_t src)
    {
        uint32_t src_a = src >> 24;
        if (src_a == 255)
            return src;
        if (src_a == 0)
            return dest;

        // faster integer math, keep 8-bit channel values
        uint32_t inv_a = 255 - src_a;
        uint32_t src_rb = src & 0x00FF00FF;
        uint32_t src_g = src & 0x0000FF00;
        uint32_t dest_rb = dest & 0x00FF00FF;
        uint32_t dest_g = dest & 0x0000FF00;

        uint32_t out_rb = ((src_rb * src_a) + (dest_rb * inv_a)) / 255;
        uint32_t out_g = ((src_g * src_a) + (dest_g * inv_a)) / 255;

        out_rb &= 0x00FF00FF;
        out_g &= 0x0000FF00;
        return (src & 0xFF000000) | out_rb | (out_g & 0x00FF0000) | (out_g & 0x0000FF00);
    }
}

// ---------------------------------------------------------------------------
// --- GDI 类成员函数实现 （已移除备用 Gdiplus memGraphics 分支）---
// ---------------------------------------------------------------------------

bool GDI::createBackBuffer(int w, int h)
{
    destroyBackBuffer();
    if (w <= 0 || h <= 0)
        return false;

    backWidth = w;
    backHeight = h;

    ZeroMemory(&backInfo, sizeof(backInfo));
    backInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    backInfo.bmiHeader.biWidth = w;
    backInfo.bmiHeader.biHeight = -h; // top-down
    backInfo.bmiHeader.biPlanes = 1;
    backInfo.bmiHeader.biBitCount = 32;
    backInfo.bmiHeader.biCompression = BI_RGB;

    HDC hdc = GetDC(hwnd);
    hBackBitmap = CreateDIBSection(hdc, &backInfo, DIB_RGB_COLORS, (void **)&backPixels, NULL, 0);
    if (!hBackBitmap)
    {
        ReleaseDC(hwnd, hdc);
        backPixels = nullptr;
        return false;
    }

    hBackDC = CreateCompatibleDC(hdc);
    SelectObject(hBackDC, hBackBitmap);
    ReleaseDC(hwnd, hdc);

    SetBkMode(hBackDC, TRANSPARENT);
    return true;
}

void GDI::destroyBackBuffer()
{
    if (hBackDC)
    {
        DeleteDC(hBackDC);
        hBackDC = nullptr;
    }
    if (hBackBitmap)
    {
        DeleteObject(hBackBitmap);
        hBackBitmap = nullptr;
    }
    backPixels = nullptr;
    backWidth = backHeight = 0;
}

void GDI::init(HWND h)
{
    hwnd = h;
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    RECT rc;
    GetClientRect(hwnd, &rc);
    createBackBuffer(rc.right - rc.left, rc.bottom - rc.top);
    // no fallback to memGraphics per request

    commands.reserve(1024);
}
// In GDI.cpp
void GDI::setCamera(int x, int y)
{
    cameraX = x;
    cameraY = y;
}
void GDI::end()
{
    for (auto const &p : fontCache)
    {
        DeleteObject(p.second);
    }
    fontCache.clear();

    imageCache.clear();
    commands.clear();
    destroyBackBuffer();

    if (gdiplusToken)
    {
        Gdiplus::GdiplusShutdown(gdiplusToken);
        gdiplusToken = 0;
    }
}

void GDI::begin(float /*dt*/)
{
    if (backPixels)
    {
        // faster zeroing: use memset (already used), but size in bytes:
        memset(backPixels, 0, (size_t)backWidth * (size_t)backHeight * 4);
    }
}

CachedImage *GDI::loadImage(int resId)
{
    auto it = imageCache.find(resId);
    if (it != imageCache.end())
        return &it->second;

    HMODULE hMod = GetModuleHandleW(NULL);
    HRSRC hResInfo = FindResource(hMod, MAKEINTRESOURCE(resId), RT_RCDATA);
    if (!hResInfo)
        return nullptr;

    DWORD resSize = SizeofResource(hMod, hResInfo);
    HGLOBAL hResData = LoadResource(hMod, hResInfo);
    if (!hResData || resSize == 0)
        return nullptr;

    void *pRes = LockResource(hResData);
    if (!pRes)
        return nullptr;

    IStream *pStream = SHCreateMemStream((const BYTE *)pRes, resSize);
    if (!pStream)
        return nullptr;

    std::unique_ptr<Gdiplus::Bitmap> bmp(Gdiplus::Bitmap::FromStream(pStream));
    pStream->Release();

    if (!bmp || bmp->GetLastStatus() != Gdiplus::Ok)
        return nullptr;

    int width = bmp->GetWidth();
    int height = bmp->GetHeight();

    CachedImage cached;
    cached.width = width;
    cached.height = height;
    cached.pixels = std::make_unique<uint32_t[]>(width * height);
    cached.isOpaque = true;

    Gdiplus::BitmapData bmpData;
    Gdiplus::Rect rect(0, 0, width, height);
    if (bmp->LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppPARGB, &bmpData) != Gdiplus::Ok)
    {
        return nullptr;
    }

    uint32_t *srcPixels = (uint32_t *)bmpData.Scan0;
    const int pixelCount = width * height;
    for (int i = 0; i < pixelCount; ++i)
    {
        uint8_t alpha = (uint8_t)(srcPixels[i] >> 24);
        if (alpha < 255)
            cached.isOpaque = false;
        cached.pixels[i] = srcPixels[i];
    }

    bmp->UnlockBits(&bmpData);

    auto [iter, success] = imageCache.emplace(resId, std::move(cached));
    return &iter->second;
}

HFONT GDI::getFont(float size)
{
    auto it = fontCache.find(size);
    if (it != fontCache.end())
        return it->second;

    LOGFONTW lf = {0};
    lf.lfHeight = -lround(size);
    lf.lfWeight = FW_NORMAL;
    wcscpy_s(lf.lfFaceName, LF_FACESIZE, L"Arial");
    HFONT hf = CreateFontIndirectW(&lf);

    fontCache[size] = hf;
    return hf;
}

// Highly-optimized blit for the main path (backPixels != nullptr)
// Minimized per-pixel overhead, pointer-based loops, fewer divisions.
void GDI::drawImageFast(const Command &cmd)
{
    CachedImage *img = loadImage(cmd.resId);
    if (!img)
        return;

    const int imgW = img->width;
    const int imgH = img->height;

    int srcW = cmd.hasSrcRect ? cmd.srcW : imgW;
    int srcH = cmd.hasSrcRect ? cmd.srcH : imgH;
    int srcX = cmd.hasSrcRect ? cmd.srcX : 0;
    int srcY = cmd.hasSrcRect ? cmd.srcY : 0;

    if (srcW <= 0 || srcH <= 0 || cmd.w <= 0 || cmd.h <= 0)
        return;

    int destX1 = cmd.x;
    int destY1 = cmd.y;
    int destX2 = cmd.x + cmd.w;
    int destY2 = cmd.y + cmd.h;

    int clipX1 = max(0, destX1);
    int clipY1 = max(0, destY1);
    int clipX2 = min(backWidth, destX2);
    int clipY2 = min(backHeight, destY2);

    if (clipX1 >= clipX2 || clipY1 >= clipY2)
        return;

    const int FRACT_BITS = 16;
    const uint32_t FRACT_UNIT = 1u << FRACT_BITS;

    // fixed-point step
    uint64_t stepX_fixed = ((uint64_t)srcW * FRACT_UNIT) / (uint64_t)cmd.w;
    uint64_t stepY_fixed = ((uint64_t)srcH * FRACT_UNIT) / (uint64_t)cmd.h;

    uint64_t srcX_fixed_start = (uint64_t)(clipX1 - destX1) * stepX_fixed;
    uint64_t srcY_fixed_start = (uint64_t)(clipY1 - destY1) * stepY_fixed;

    // start row pointer in dest
    uint32_t *destRow = backPixels + (clipY1 * backWidth) + clipX1;
    uint64_t srcY_fixed = srcY_fixed_start;

    for (int y = clipY1; y < clipY2; ++y)
    {
        int current_srcY = srcY + (int)(srcY_fixed >> FRACT_BITS);
        if (current_srcY < 0)
            current_srcY = 0;
        if (current_srcY >= imgH)
            current_srcY = imgH - 1;

        const uint32_t *srcRowBase = img->pixels.get() + (size_t)current_srcY * (size_t)imgW;

        uint64_t srcX_fixed = srcX_fixed_start;
        uint32_t *destPtr = destRow;

        if (cmd.flip)
        {
            if (img->isOpaque)
            {
                for (int x = clipX1; x < clipX2; ++x)
                {
                    int sx = srcX + srcW - 1 - (int)(srcX_fixed >> FRACT_BITS);
                    // bound check (cheap)
                    if (sx < 0)
                        sx = 0;
                    if (sx >= imgW)
                        sx = imgW - 1;
                    *destPtr++ = srcRowBase[sx];
                    srcX_fixed += stepX_fixed;
                }
            }
            else
            {
                for (int x = clipX1; x < clipX2; ++x)
                {
                    int sx = srcX + srcW - 1 - (int)(srcX_fixed >> FRACT_BITS);
                    if (sx < 0)
                        sx = 0;
                    if (sx >= imgW)
                        sx = imgW - 1;
                    uint32_t srcPixel = srcRowBase[sx];
                    *destPtr = AlphaBlendPixel_32bpp(*destPtr, srcPixel);
                    ++destPtr;
                    srcX_fixed += stepX_fixed;
                }
            }
        }
        else
        {
            if (img->isOpaque)
            {
                // fast path: if there's no scaling in X and srcX_fixed aligns to integer, do memcpy
                if ((cmd.w == srcW) && (cmd.h == srcH) && ((srcX_fixed_start & (FRACT_UNIT - 1)) == 0))
                {
                    int srcStart = srcX + (int)(srcX_fixed_start >> FRACT_BITS);
                    if (srcStart < 0)
                        srcStart = 0;
                    if (srcStart + (clipX2 - clipX1) > imgW)
                        srcStart = imgW - (clipX2 - clipX1);
                    const void *srcBytes = srcRowBase + srcStart;
                    memcpy(destPtr, srcBytes, (size_t)(clipX2 - clipX1) * 4);
                }
                else
                {
                    for (int x = clipX1; x < clipX2; ++x)
                    {
                        int sx = srcX + (int)(srcX_fixed >> FRACT_BITS);
                        if (sx < 0)
                            sx = 0;
                        if (sx >= imgW)
                            sx = imgW - 1;
                        *destPtr++ = srcRowBase[sx];
                        srcX_fixed += stepX_fixed;
                    }
                }
            }
            else
            {
                for (int x = clipX1; x < clipX2; ++x)
                {
                    int sx = srcX + (int)(srcX_fixed >> FRACT_BITS);
                    if (sx < 0)
                        sx = 0;
                    if (sx >= imgW)
                        sx = imgW - 1;
                    uint32_t srcPixel = srcRowBase[sx];
                    *destPtr = AlphaBlendPixel_32bpp(*destPtr, srcPixel);
                    ++destPtr;
                    srcX_fixed += stepX_fixed;
                }
            }
        }

        // next dest row
        destRow += backWidth;
        srcY_fixed += stepY_fixed;
    }
}

void GDI::tick(float /*dt*/)
{
    if (!backPixels) // no backbuffer -> nothing to do (no memGraphics fallback)
    {
        commands.clear();
        return;
    }

    // 1) Draw images (hot path) - iterate once
    for (const auto &c : commands)
    {
        if (c.type == Type::DrawImage)
        {
            drawImageFast(c);
        }
    }

    // 2) Draw text - minimize SelectObject / SetTextColor calls
    HFONT lastFont = nullptr;
    COLORREF lastColor = 0xFFFFFFFF; // impossible initial value
    HFONT prevFont = nullptr;
    if (hBackDC)
    {
        prevFont = (HFONT)GetCurrentObject(hBackDC, OBJ_FONT);
    }

    for (const auto &c : commands)
    {
        if (c.type != Type::DrawText)
            continue;

        HFONT hf = getFont(c.fontSize);
        if (hf != lastFont)
        {
            SelectObject(hBackDC, hf);
            lastFont = hf;
        }

        // prepare color as COLORREF
        Gdiplus::Color gc = c.color;
        COLORREF col = RGB(gc.GetR(), gc.GetG(), gc.GetB());
        if (col != lastColor)
        {
            SetTextColor(hBackDC, col);
            lastColor = col;
        }

        // use ExtTextOutW for slightly better control (no background since we set TRANSPARENT earlier)
        const wchar_t *textPtr = c.text.c_str();
        int len = (int)c.text.length();
        ExtTextOutW(hBackDC, c.x, c.y, 0, nullptr, textPtr, len, nullptr);
    }

    // restore previous font if needed
    if (hBackDC && prevFont)
    {
        SelectObject(hBackDC, prevFont);
    }

    commands.clear();
}

void GDI::flush(float /*dt*/)
{
    if (!hwnd)
        return;
    HDC hdc = GetDC(hwnd);
    if (!hdc)
        return;

    if (backPixels && hBackDC)
    {
        BitBlt(hdc, 0, 0, backWidth, backHeight, hBackDC, 0, 0, SRCCOPY);
    }

    ReleaseDC(hwnd, hdc);
}

void GDI::image(int resId, int x, int y, int w, int h, bool flip)
{
    pushImage(resId, x, y, w, h, flip);
}

void GDI::imageEx(int resId, int x, int y, int w, int h,
                  bool flip, int srcX, int srcY, int srcW, int srcH)
{
    pushImageEx(resId, x, y, w, h, flip, srcX, srcY, srcW, srcH);
}

void GDI::text(const std::wstring &txt, int x, int y, float size, Gdiplus::Color color)
{
    pushText(txt, x, y, size, color);
}
