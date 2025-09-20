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
std::unordered_map<int, CachedImage> GDI::imageCache;
std::unordered_map<float, HFONT> GDI::fontCache;
int GDI::cameraX = 0;
int GDI::cameraY = 0;

// ---------------------------------------------------------------------------
// --- 内部辅助函数 ---
// ---------------------------------------------------------------------------
namespace
{
    // 32位像素Alpha混合 (源为预乘Alpha)
    inline uint32_t AlphaBlendPixel_32bpp_Premultiplied(uint32_t dest, uint32_t src)
    {
        uint32_t src_a = src >> 24;
        if (src_a == 255)
            return src; // 源不透明，直接覆盖
        if (src_a == 0)
            return dest; // 源全透明，无变化

        uint32_t inv_a = 255 - src_a;

        uint32_t dest_rb = dest & 0x00FF00FF;
        uint32_t dest_g = (dest >> 8) & 0x000000FF;

        uint32_t out_rb = (dest_rb * inv_a) / 255;
        uint32_t out_g = (dest_g * inv_a) / 255;

        out_rb &= 0x00FF00FF;
        out_g &= 0x000000FF;

        return src + out_rb + (out_g << 8);
    }
}

// ---------------------------------------------------------------------------
// --- GDI 核心生命周期函数 ---
// ---------------------------------------------------------------------------

void GDI::init(HWND h)
{
    hwnd = h;
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    RECT rc;
    GetClientRect(hwnd, &rc);
    createBackBuffer(rc.right - rc.left, rc.bottom - rc.top);

    // 设置DC的默认属性
    if (hBackDC)
    {
        SetBkMode(hBackDC, TRANSPARENT);
    }
}

void GDI::end()
{
    for (auto const &p : fontCache)
    {
        DeleteObject(p.second);
    }
    fontCache.clear();
    imageCache.clear();
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
        // 使用 memset 快速清空后备缓冲区为黑色 (0)
        memset(backPixels, 0, (size_t)backWidth * (size_t)backHeight * 4);
    }
}

// tick 函数现在是空的，为了API兼容性而保留。
// 所有绘图都已在 image/text/rect 调用时立即执行。
void GDI::tick(float /*dt*/)
{
    // No-op
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

void GDI::setCamera(int x, int y)
{
    cameraX = x;
    cameraY = y;
}

// ---------------------------------------------------------------------------
// --- 立即模式绘图接口 ---
// ---------------------------------------------------------------------------

void GDI::image(int resId, int x, int y, int w, int h, bool flip)
{
    if (!backPixels)
        return;
    drawImageFast(resId, x - cameraX, y - cameraY, w, h, flip, false, 0, 0, 0, 0);
}

void GDI::imageEx(int resId, int x, int y, int w, int h,
                  bool flip, int srcX, int srcY, int srcW, int srcH)
{
    if (!backPixels)
        return;
    drawImageFast(resId, x - cameraX, y - cameraY, w, h, flip, true, srcX, srcY, srcW, srcH);
}

void GDI::rect(int x, int y, int w, int h, Gdiplus::Color color)
{
    if (!backPixels || w <= 0 || h <= 0)
        return;
    drawRectFast(x - cameraX, y - cameraY, w, h, color);
}

void GDI::text(const std::wstring &txt, int x, int y, float size, Gdiplus::Color color)
{
    if (!hBackDC || txt.empty())
        return;

    // 获取并选择字体
    HFONT hf = getFont(size);
    HFONT oldFont = (HFONT)SelectObject(hBackDC, hf);

    // 设置文本颜色
    COLORREF col = RGB(color.GetR(), color.GetG(), color.GetB());
    COLORREF oldColor = SetTextColor(hBackDC, col);

    // 绘制文本
    int finalX = x - cameraX;
    int finalY = y - cameraY;
    ExtTextOutW(hBackDC, finalX, finalY, 0, nullptr, txt.c_str(), (int)txt.length(), nullptr);

    // 恢复之前的DC状态
    SetTextColor(hBackDC, oldColor);
    SelectObject(hBackDC, oldFont);
}

// ---------------------------------------------------------------------------
// --- 内部绘制与资源管理实现 ---
// ---------------------------------------------------------------------------

void GDI::drawRectFast(int x, int y, int w, int h, Gdiplus::Color color)
{
    // 裁剪
    int destX1 = x;
    int destY1 = y;
    int destX2 = x + w;
    int destY2 = y + h;

    int clipX1 = max(0, destX1);
    int clipY1 = max(0, destY1);
    int clipX2 = min(backWidth, destX2);
    int clipY2 = min(backHeight, destY2);

    int clippedW = clipX2 - clipX1;
    if (clippedW <= 0 || clipY1 >= clipY2)
    {
        return;
    }

    // 转换颜色格式 Gdiplus(ARGB) -> Buffer(BGRA)
    uint32_t argb = color.GetValue();
    uint32_t a = (argb >> 24) & 0xFF;
    uint32_t r = (argb >> 16) & 0xFF;
    uint32_t g = (argb >> 8) & 0xFF;
    uint32_t b = argb & 0xFF;
    uint32_t bgraColor = (a << 24) | (b << 16) | (g << 8) | r;

    uint32_t *destRow = backPixels + (clipY1 * backWidth) + clipX1;

    if (a == 255)
    { // 不透明，直接填充
        for (int j = clipY1; j < clipY2; ++j)
        {
            // 使用循环或std::fill_n填充一行
            for (int i = 0; i < clippedW; ++i)
            {
                destRow[i] = bgraColor;
            }
            destRow += backWidth;
        }
    }
    else if (a > 0)
    { // 半透明，需要混合
        // 预乘Alpha
        uint32_t pr = (r * a) / 255;
        uint32_t pg = (g * a) / 255;
        uint32_t pb = (b * a) / 255;
        uint32_t srcPixel = (a << 24) | (pb << 16) | (pg << 8) | pr;

        for (int j = clipY1; j < clipY2; ++j)
        {
            uint32_t *destPtr = destRow;
            for (int i = 0; i < clippedW; ++i)
            {
                *destPtr = AlphaBlendPixel_32bpp_Premultiplied(*destPtr, srcPixel);
                destPtr++;
            }
            destRow += backWidth;
        }
    }
}

// 已重构，直接接收参数而不是Command对象
void GDI::drawImageFast(int resId, int x, int y, int w, int h,
                        bool flip, bool hasSrcRect, int srcX_in,
                        int srcY_in, int srcW_in, int srcH_in)
{
    CachedImage *img = loadImage(resId);
    if (!img)
        return;

    const int imgW = img->width;
    const int imgH = img->height;

    int srcW = hasSrcRect ? srcW_in : imgW;
    int srcH = hasSrcRect ? srcH_in : imgH;
    int srcX = hasSrcRect ? srcX_in : 0;
    int srcY = hasSrcRect ? srcY_in : 0;

    if (srcW <= 0 || srcH <= 0 || w <= 0 || h <= 0)
        return;

    int destX1 = x;
    int destY1 = y;
    int destX2 = x + w;
    int destY2 = y + h;

    int clipX1 = max(0, destX1);
    int clipY1 = max(0, destY1);
    int clipX2 = min(backWidth, destX2);
    int clipY2 = min(backHeight, destY2);

    if (clipX1 >= clipX2 || clipY1 >= clipY2)
        return;

    const int FRACT_BITS = 16;
    const uint32_t FRACT_UNIT = 1u << FRACT_BITS;

    uint64_t stepX_fixed = ((uint64_t)srcW * FRACT_UNIT) / (uint64_t)w;
    uint64_t stepY_fixed = ((uint64_t)srcH * FRACT_UNIT) / (uint64_t)h;

    uint64_t srcX_fixed_start = (uint64_t)(clipX1 - destX1) * stepX_fixed;
    uint64_t srcY_fixed_start = (uint64_t)(clipY1 - destY1) * stepY_fixed;

    uint32_t *destRow = backPixels + (clipY1 * backWidth) + clipX1;
    uint64_t srcY_fixed = srcY_fixed_start;

    bool isOpaque = img->isOpaque;

    for (int j = clipY1; j < clipY2; ++j)
    {
        int current_srcY = srcY + (int)(srcY_fixed >> FRACT_BITS);
        if (current_srcY < 0)
            current_srcY = 0;
        if (current_srcY >= imgH)
            current_srcY = imgH - 1;

        const uint32_t *srcRowBase = img->pixels.get() + (size_t)current_srcY * (size_t)imgW;
        uint64_t srcX_fixed = srcX_fixed_start;
        uint32_t *destPtr = destRow;

        if (flip)
        {
            if (isOpaque)
            {
                for (int i = clipX1; i < clipX2; ++i)
                {
                    int sx = srcX + srcW - 1 - (int)(srcX_fixed >> FRACT_BITS);
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
                for (int i = clipX1; i < clipX2; ++i)
                {
                    int sx = srcX + srcW - 1 - (int)(srcX_fixed >> FRACT_BITS);
                    if (sx < 0)
                        sx = 0;
                    if (sx >= imgW)
                        sx = imgW - 1;
                    *destPtr = AlphaBlendPixel_32bpp_Premultiplied(*destPtr, srcRowBase[sx]);
                    destPtr++;
                    srcX_fixed += stepX_fixed;
                }
            }
        }
        else // no flip
        {
            if (isOpaque)
            {
                // 优化：对于1:1绘制，使用memcpy
                if (w == srcW && h == srcH && (srcX_fixed_start & (FRACT_UNIT - 1)) == 0)
                {
                    int srcStart = srcX + (int)(srcX_fixed_start >> FRACT_BITS);
                    const void *srcBytes = srcRowBase + srcStart;
                    memcpy(destPtr, srcBytes, (size_t)(clipX2 - clipX1) * 4);
                }
                else
                {
                    for (int i = clipX1; i < clipX2; ++i)
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
            { // has alpha
                for (int i = clipX1; i < clipX2; ++i)
                {
                    int sx = srcX + (int)(srcX_fixed >> FRACT_BITS);
                    if (sx < 0)
                        sx = 0;
                    if (sx >= imgW)
                        sx = imgW - 1;
                    *destPtr = AlphaBlendPixel_32bpp_Premultiplied(*destPtr, srcRowBase[sx]);
                    destPtr++;
                    srcX_fixed += stepX_fixed;
                }
            }
        }

        destRow += backWidth;
        srcY_fixed += stepY_fixed;
    }
}

// 其他辅助函数 (createBackBuffer, destroyBackBuffer, loadImage, getFont) 保持不变...
// (为简洁起见，此处省略了与之前版本完全相同的函数代码)

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
    ReleaseDC(hwnd, hdc);

    if (!hBackBitmap)
    {
        backPixels = nullptr;
        return false;
    }

    hBackDC = CreateCompatibleDC(nullptr); // 使用NULL而不是窗口DC
    SelectObject(hBackDC, hBackBitmap);

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
    uint32_t *destPixels = cached.pixels.get();
    for (int i = 0; i < pixelCount; ++i)
    {
        uint32_t pixel = srcPixels[i];
        if ((pixel >> 24) < 255)
        {
            cached.isOpaque = false;
        }
        destPixels[i] = pixel;
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
    lf.lfHeight = -lround(size * GetDeviceCaps(GetDC(hwnd), LOGPIXELSY) / 72.0f); // 更精确的高度
    lf.lfWeight = FW_NORMAL;
    wcscpy_s(lf.lfFaceName, LF_FACESIZE, L"Arial");
    HFONT hf = CreateFontIndirectW(&lf);

    fontCache[size] = hf;
    return hf;
}