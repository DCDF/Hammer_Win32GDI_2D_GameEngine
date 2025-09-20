#include "GDI.h"
#define NOMINMAX

#include <algorithm> // for std::max/min
#include <cmath>     // for lround
#include <shlwapi.h> // for SHCreateMemStream

#pragma comment(lib, "Gdiplus.lib")
#pragma comment(lib, "Msimg32.lib")
#pragma comment(lib, "shlwapi.lib")

// 使用 std 命名空间
using std::max;
using std::min;

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
int GDI::cameraX = 0;
int GDI::cameraY = 0;
std::unordered_map<int, CachedImage> GDI::imageCache;
std::unordered_map<float, HFONT> GDI::fontCache;

// ---------------------------------------------------------------------------
// --- 内部辅助函数 ---
// ---------------------------------------------------------------------------
namespace
{
    // 高效的32位预乘Alpha混合 (整数运算)
    inline uint32_t AlphaBlendPixel_Premultiplied(uint32_t dest, uint32_t src)
    {
        uint32_t src_a = src >> 24;
        if (src_a == 255)
            return src;
        if (src_a == 0)
            return dest;

        uint32_t inv_a = 255 - src_a;

        // 分离 dest 的 R,B 和 G 通道
        uint32_t dest_rb = dest & 0x00FF00FF;
        uint32_t dest_g = dest & 0x0000FF00;

        // dest = dest * (1 - src_a)
        dest_rb = (dest_rb * inv_a) / 255;
        dest_g = (dest_g * inv_a) / 255;

        // 清理溢出位
        dest_rb &= 0x00FF00FF;
        dest_g &= 0x0000FF00;

        // out = src(premultiplied) + dest(scaled)
        return src + dest_rb + dest_g;
    }
}

// ---------------------------------------------------------------------------
// --- GDI 核心生命周期与底层实现 ---
// ---------------------------------------------------------------------------

void GDI::init(HWND h)
{
    hwnd = h;
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    RECT rc;
    GetClientRect(hwnd, &rc);
    if (createBackBuffer(rc.right - rc.left, rc.bottom - rc.top))
    {
        SetBkMode(hBackDC, TRANSPARENT);
    }
}

void GDI::end()
{
    for (auto const &[size, font] : fontCache)
    {
        DeleteObject(font);
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

void GDI::flush([[maybe_unused]] float dt)
{
    if (!hwnd)
        return;
    HDC hdc = GetDC(hwnd);
    if (!hdc)
        return;

    if (hBackDC)
    {
        BitBlt(hdc, 0, 0, backWidth, backHeight, hBackDC, 0, 0, SRCCOPY);
    }

    ReleaseDC(hwnd, hdc);
}

// 文本绘制：这是一个性能权衡。
// 立即模式下，每次调用都改变DC状态（字体/颜色）开销较大。
// 为了极致性能，上层应用应自行将相同字体/颜色的文本绘制集中调用。
// 此处为保证正确性，每次都完整设置并恢复状态。
void GDI::text(const std::wstring &txt, int x, int y, float size, Gdiplus::Color color)
{
    if (!hBackDC || txt.empty())
        return;

    HFONT hf = getFont(size);
    HFONT oldFont = (HFONT)SelectObject(hBackDC, hf);

    COLORREF col = RGB(color.GetR(), color.GetG(), color.GetB());
    COLORREF oldColor = SetTextColor(hBackDC, col);

    ExtTextOutW(hBackDC, x - cameraX, y - cameraY, 0, nullptr, txt.c_str(), (int)txt.length(), nullptr);

    SelectObject(hBackDC, oldFont);
    SetTextColor(hBackDC, oldColor);
}

// 极致优化：使用SSE2指令集绘制矩形
void GDI::drawRectFast(int x, int y, int w, int h, Gdiplus::Color color)
{
    // 1. 裁剪
    int clipX1 = max(0, x);
    int clipY1 = max(0, y);
    int clipX2 = min(backWidth, x + w);
    int clipY2 = min(backHeight, y + h);

    int clippedW = clipX2 - clipX1;
    if (clippedW <= 0 || clipY1 >= clipY2)
        return;

    // 2. 颜色转换 Gdiplus(ARGB) -> Buffer(BGRA)
    uint32_t argb = color.GetValue();
    uint8_t a = (argb >> 24);
    uint8_t r = (argb >> 16);
    uint8_t g = (argb >> 8);
    uint8_t b = argb;
    uint32_t bgraColor = (a << 24) | (b << 16) | (g << 8) | r;

    uint32_t *destRowStart = backPixels + (clipY1 * backWidth) + clipX1;

    // 3. 根据透明度选择不同路径
    if (a == 255)
    { // --- 不透明路径 (SSE2优化) ---
        const __m128i color_s = _mm_set1_epi32(bgraColor);
        for (int j = clipY1; j < clipY2; ++j)
        {
            uint32_t *p = destRowStart;
            int count = clippedW;

            // 内存对齐处理：先处理行首未对齐的几个像素
            while ((reinterpret_cast<uintptr_t>(p) & 15) && count > 0)
            {
                *p++ = bgraColor;
                count--;
            }

            // SSE2核心循环：一次处理4个像素
            __m128i *p_s = reinterpret_cast<__m128i *>(p);
            while (count >= 4)
            {
                _mm_store_si128(p_s++, color_s);
                count -= 4;
            }

            // 处理行尾剩下的不足4个的像素
            p = reinterpret_cast<uint32_t *>(p_s);
            while (count > 0)
            {
                *p++ = bgraColor;
                count--;
            }
            destRowStart += backWidth;
        }
    }
    else if (a > 0)
    { // --- Alpha混合路径 (逐像素) ---
        // SIMD优化Alpha混合非常复杂，对于通用情况，逐像素仍是可靠方案
        uint32_t pr = (r * a) / 255;
        uint32_t pg = (g * a) / 255;
        uint32_t pb = (b * a) / 255;
        uint32_t srcPixel = (a << 24) | (pb << 16) | (pg << 8) | pr;

        for (int j = clipY1; j < clipY2; ++j)
        {
            uint32_t *destPtr = destRowStart;
            for (int i = 0; i < clippedW; ++i)
            {
                *destPtr = AlphaBlendPixel_Premultiplied(*destPtr, srcPixel);
                destPtr++;
            }
            destRowStart += backWidth;
        }
    }
}

// 图像绘制 (基本保持原样，其瓶颈在于内存带宽和像素计算，已使用高效的定点数算法)
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

    int clipX1 = max(0, x);
    int clipY1 = max(0, y);
    int clipX2 = min(backWidth, x + w);
    int clipY2 = min(backHeight, y + h);

    if (clipX1 >= clipX2 || clipY1 >= clipY2)
        return;

    const int FRACT_BITS = 16;
    const uint64_t FRACT_UNIT = 1ULL << FRACT_BITS;

    uint64_t stepX_fixed = (static_cast<uint64_t>(srcW) * FRACT_UNIT) / w;
    uint64_t stepY_fixed = (static_cast<uint64_t>(srcH) * FRACT_UNIT) / h;

    uint64_t srcX_fixed_start = static_cast<uint64_t>(clipX1 - x) * stepX_fixed;
    uint64_t srcY_fixed_start = static_cast<uint64_t>(clipY1 - y) * stepY_fixed;

    uint32_t *destRow = backPixels + (clipY1 * backWidth) + clipX1;
    uint64_t srcY_fixed = srcY_fixed_start;

    for (int j = clipY1; j < clipY2; ++j)
    {
        int current_srcY = srcY + (srcY_fixed >> FRACT_BITS);

        const uint32_t *srcRowBase = img->pixels.get() + min(imgH - 1, max(0, current_srcY)) * (size_t)imgW;
        uint64_t srcX_fixed = srcX_fixed_start;
        uint32_t *destPtr = destRow;

        bool isOpaque = img->isOpaque;

        if (isOpaque)
        { // --- 不透明路径 ---
            if (!flip)
            {
                // 优化: 1:1无缩放绘制，直接 memcpy
                if (w == srcW && h == srcH)
                {
                    int srcStart = srcX + (srcX_fixed_start >> FRACT_BITS);
                    memcpy(destPtr, srcRowBase + srcStart, (size_t)(clipX2 - clipX1) * 4);
                }
                else
                {
                    for (int i = clipX1; i < clipX2; ++i)
                    {
                        int sx = srcX + (srcX_fixed >> FRACT_BITS);
                        *destPtr++ = srcRowBase[min(imgW - 1, max(0, sx))];
                        srcX_fixed += stepX_fixed;
                    }
                }
            }
            else
            { // flip
                for (int i = clipX1; i < clipX2; ++i)
                {
                    int sx = srcX + srcW - 1 - (srcX_fixed >> FRACT_BITS);
                    *destPtr++ = srcRowBase[min(imgW - 1, max(0, sx))];
                    srcX_fixed += stepX_fixed;
                }
            }
        }
        else
        { // --- Alpha混合路径 ---
            if (!flip)
            {
                for (int i = clipX1; i < clipX2; ++i)
                {
                    int sx = srcX + (srcX_fixed >> FRACT_BITS);
                    *destPtr = AlphaBlendPixel_Premultiplied(*destPtr, srcRowBase[min(imgW - 1, max(0, sx))]);
                    destPtr++;
                    srcX_fixed += stepX_fixed;
                }
            }
            else
            { // flip
                for (int i = clipX1; i < clipX2; ++i)
                {
                    int sx = srcX + srcW - 1 - (srcX_fixed >> FRACT_BITS);
                    *destPtr = AlphaBlendPixel_Premultiplied(*destPtr, srcRowBase[min(imgW - 1, max(0, sx))]);
                    destPtr++;
                    srcX_fixed += stepX_fixed;
                }
            }
        }
        destRow += backWidth;
        srcY_fixed += stepY_fixed;
    }
}

// (createBackBuffer, destroyBackBuffer, loadImage, getFont 函数与上一版基本相同，为简洁省略)
// ... 粘贴上一版中这四个函数的实现即可 ...
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

    hBackDC = CreateCompatibleDC(nullptr);
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

    const uint32_t *srcPixels = (const uint32_t *)bmpData.Scan0;
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

    // 更精确地将点(Point)大小转换为逻辑像素大小
    LOGFONTW lf = {0};
    HDC hdc = GetDC(hwnd);
    lf.lfHeight = -MulDiv(static_cast<int>(round(size)), GetDeviceCaps(hdc, LOGPIXELSY), 72);
    ReleaseDC(hwnd, hdc);

    lf.lfWeight = FW_NORMAL;
    wcscpy_s(lf.lfFaceName, LF_FACESIZE, L"Arial");
    HFONT hf = CreateFontIndirectW(&lf);

    fontCache[size] = hf;
    return hf;
}