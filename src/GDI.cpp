// ---------------------------------------------------------------------------
// --- 预处理器和头文件包含顺序 ---
// 这是解决编译错误的关键部分
// ---------------------------------------------------------------------------

// 1. 定义 NOMINMAX，防止 Windows.h 定义 min/max 宏，避免与 std::min/max 冲突。
#define NOMINMAX

// 2. 包含核心的 Windows 头文件和 C++ 标准库。
#include <windows.h>
#include <algorithm> // 包含 std::min 和 std::max
#include <cmath>
#include <shlwapi.h> // 包含 SHCreateMemStream

// 3. 为 GDI+ "手动" 提供它需要的 min 和 max。
//    由于 NOMINMAX 已经生效，GDI+ 头文件找不到 min/max 宏。
//    我们通过 using 声明，将 std::min 和 std::max 引入全局命名空间，
//    这样 GDI+ 头文件就能找到它们了。
using std::max;
using std::min;

// 4. 现在可以安全地包含我们自己的头文件，它会间接包含 GDI+ 头文件。
#include "GDI.h"

// 5. 链接必要的库
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
std::unique_ptr<Gdiplus::Graphics> GDI::memGraphics = nullptr;
std::unordered_map<int, CachedImage> GDI::imageCache;
std::unordered_map<float, HFONT> GDI::fontCache;
std::vector<GDI::Command> GDI::commands;

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

        uint32_t inv_a = 256 - src_a;
        uint32_t dest_rb = dest & 0x00FF00FF;
        uint32_t dest_g = dest & 0x0000FF00;
        dest_rb = (dest_rb * inv_a) >> 8;
        dest_g = (dest_g * inv_a) >> 8;
        dest_rb &= 0x00FF00FF;
        dest_g &= 0x0000FF00;
        return src + dest_rb + dest_g;
    }
}

// ---------------------------------------------------------------------------
// --- GDI 类成员函数实现 ---
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
    backInfo.bmiHeader.biHeight = -h;
    backInfo.bmiHeader.biPlanes = 1;
    backInfo.bmiHeader.biBitCount = 32;
    backInfo.bmiHeader.biCompression = BI_RGB;

    HDC hdc = GetDC(hwnd);
    hBackBitmap = CreateDIBSection(hdc, &backInfo, DIB_RGB_COLORS, (void **)&backPixels, NULL, 0);
    if (!hBackBitmap)
    {
        ReleaseDC(hwnd, hdc);
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
    if (!createBackBuffer(rc.right - rc.left, rc.bottom - rc.top))
    {
        memGraphics = std::make_unique<Gdiplus::Graphics>(h);
    }

    commands.reserve(1024);
}

void GDI::end()
{
    for (auto const &[key, val] : fontCache)
    {
        DeleteObject(val);
    }
    fontCache.clear();

    imageCache.clear();
    commands.clear();
    destroyBackBuffer();

    memGraphics.reset();
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
        memset(backPixels, 0, backWidth * backHeight * 4);
    }
    else if (memGraphics)
    {
        Gdiplus::Color bg(0, 0, 0, 0);
        memGraphics->Clear(bg);
    }
}

CachedImage *GDI::loadImage(int resId)
{
    auto it = imageCache.find(resId);
    if (it != imageCache.end())
    {
        return &it->second;
    }

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
    {
        return nullptr;
    }

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
    for (int i = 0; i < width * height; ++i)
    {
        uint8_t alpha = srcPixels[i] >> 24;
        if (alpha < 255)
        {
            cached.isOpaque = false;
        }
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
    {
        return it->second;
    }

    LOGFONTW lf = {0};
    lf.lfHeight = -lround(size);
    lf.lfWeight = FW_NORMAL;
    wcscpy_s(lf.lfFaceName, LF_FACESIZE, L"Arial");
    HFONT hf = CreateFontIndirectW(&lf);

    fontCache[size] = hf;
    return hf;
}

void GDI::drawImageFast(const Command &cmd)
{
    CachedImage *img = loadImage(cmd.resId);
    if (!img)
        return;

    int srcW = cmd.hasSrcRect ? cmd.srcW : img->width;
    int srcH = cmd.hasSrcRect ? cmd.srcH : img->height;
    int srcX = cmd.hasSrcRect ? cmd.srcX : 0;
    int srcY = cmd.hasSrcRect ? cmd.srcY : 0;

    if (srcW <= 0 || srcH <= 0)
        return;

    int destX1 = cmd.x;
    int destY1 = cmd.y;
    int destX2 = cmd.x + cmd.w;
    int destY2 = cmd.y + cmd.h;

    // NOMINMAX 已定义，可以直接使用 std::min/max，无需括号
    int clipX1 = std::max(0, destX1);
    int clipY1 = std::max(0, destY1);
    int clipX2 = std::min(backWidth, destX2);
    int clipY2 = std::min(backHeight, destY2);

    if (clipX1 >= clipX2 || clipY1 >= clipY2)
        return;

    const int FRACT_BITS = 16;
    const int FRACT_UNIT = 1 << FRACT_BITS;

    uint64_t stepX_fixed = (uint64_t)srcW * FRACT_UNIT / cmd.w;
    uint64_t stepY_fixed = (uint64_t)srcH * FRACT_UNIT / cmd.h;

    uint64_t srcX_fixed_start = (uint64_t)(clipX1 - destX1) * stepX_fixed;
    uint64_t srcY_fixed_start = (uint64_t)(clipY1 - destY1) * stepY_fixed;

    uint32_t *pDestRow = backPixels + clipY1 * backWidth + clipX1;
    uint64_t srcY_fixed = srcY_fixed_start;

    for (int y = clipY1; y < clipY2; ++y)
    {
        int current_srcY = srcY + static_cast<int>(srcY_fixed >> FRACT_BITS);
        const uint32_t *pSrcRow = img->pixels.get() + current_srcY * img->width;
        uint64_t srcX_fixed = srcX_fixed_start;

        if (cmd.flip)
        {
            if (img->isOpaque)
            {
                for (int x = clipX1; x < clipX2; ++x)
                {
                    int current_srcX = srcX + srcW - 1 - static_cast<int>(srcX_fixed >> FRACT_BITS);
                    pDestRow[x - clipX1] = pSrcRow[current_srcX];
                    srcX_fixed += stepX_fixed;
                }
            }
            else
            {
                for (int x = clipX1; x < clipX2; ++x)
                {
                    int current_srcX = srcX + srcW - 1 - static_cast<int>(srcX_fixed >> FRACT_BITS);
                    uint32_t srcPixel = pSrcRow[current_srcX];
                    pDestRow[x - clipX1] = AlphaBlendPixel_32bpp(pDestRow[x - clipX1], srcPixel);
                    srcX_fixed += stepX_fixed;
                }
            }
        }
        else
        {
            if (img->isOpaque)
            {
                if (cmd.w == srcW && cmd.h == srcH)
                {
                    memcpy(pDestRow, pSrcRow + srcX + static_cast<int>(srcX_fixed_start >> FRACT_BITS), (clipX2 - clipX1) * 4);
                }
                else
                {
                    for (int x = clipX1; x < clipX2; ++x)
                    {
                        int current_srcX = srcX + static_cast<int>(srcX_fixed >> FRACT_BITS);
                        pDestRow[x - clipX1] = pSrcRow[current_srcX];
                        srcX_fixed += stepX_fixed;
                    }
                }
            }
            else
            {
                for (int x = clipX1; x < clipX2; ++x)
                {
                    int current_srcX = srcX + static_cast<int>(srcX_fixed >> FRACT_BITS);
                    uint32_t srcPixel = pSrcRow[current_srcX];
                    pDestRow[x - clipX1] = AlphaBlendPixel_32bpp(pDestRow[x - clipX1], srcPixel);
                    srcX_fixed += stepX_fixed;
                }
            }
        }
        pDestRow += backWidth;
        srcY_fixed += stepY_fixed;
    }
}

void GDI::tick(float /*dt*/)
{
    if (backPixels)
    {
        for (const auto &c : commands)
        {
            if (c.type == Type::DrawImage)
            {
                drawImageFast(c);
            }
        }
        for (const auto &c : commands)
        {
            if (c.type == Type::DrawText)
            {
                HFONT hf = getFont(c.fontSize);
                SelectObject(hBackDC, hf);
                Gdiplus::Color gdiColor = c.color;
                SetTextColor(hBackDC, RGB(gdiColor.GetR(), gdiColor.GetG(), gdiColor.GetB()));
                TextOutW(hBackDC, c.x, c.y, c.text.c_str(), (int)c.text.length());
            }
        }
    }
    else if (memGraphics)
    {
        Gdiplus::Font font(L"Arial", 12.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        for (const auto &c : commands)
        {
            if (c.type == Type::DrawImage)
            {
                CachedImage *imgData = loadImage(c.resId);
                if (imgData)
                {
                    Gdiplus::Bitmap bmp(imgData->width, imgData->height, imgData->width * 4, PixelFormat32bppPARGB, (BYTE *)imgData->pixels.get());
                    if (c.hasSrcRect)
                    {
                        memGraphics->DrawImage(&bmp, Gdiplus::Rect(c.x, c.y, c.w, c.h), c.srcX, c.srcY, c.srcW, c.srcH, Gdiplus::UnitPixel);
                    }
                    else
                    {
                        memGraphics->DrawImage(&bmp, c.x, c.y, c.w, c.h);
                    }
                }
            }
            else if (c.type == Type::DrawText)
            {
                Gdiplus::SolidBrush brush(c.color);
                Gdiplus::PointF pt((Gdiplus::REAL)c.x, (Gdiplus::REAL)c.y);
                memGraphics->DrawString(c.text.c_str(), -1, &font, pt, &brush);
            }
        }
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

    if (backPixels)
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