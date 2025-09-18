#include "GDI.h"
#include <objidl.h>
#include <cassert>
#include <algorithm>
#include <cstdint>

// 在包含 Windows 头文件之前定义这些宏以避免冲突
#define NOMINMAX
#include <Windows.h>
#undef NOMINMAX

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

// 新增：直接像素访问的图像缓存
struct CachedImage
{
    int width;
    int height;
    int stride;
    std::unique_ptr<uint32_t[]> pixels; // BGRA格式
};
static std::unordered_map<int, CachedImage> directImageCache;

static inline int AlignStride(int w) { return (w * 4 + 3) & ~3; } // 32bpp with 4-byte alignment

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
    // 降低绘制质量以提高性能 - 对于游戏这是可接受的权衡
    memGraphics->SetSmoothingMode(SmoothingModeNone);
    memGraphics->SetInterpolationMode(InterpolationModeLowQuality);
    memGraphics->SetTextRenderingHint(TextRenderingHintSystemDefault);

    return true;
}

void GDI::destroyBackBuffer()
{
    memGraphics.reset();
    memBitmap.reset();

    if (hBackBitmap)
    {
        DeleteObject(hBackBitmap);
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
        // 如果创建DIBSection失败，回退到GDI+
        memBitmap = std::make_unique<Gdiplus::Bitmap>(w, hgt, PixelFormat32bppPARGB);
        memGraphics = std::make_unique<Gdiplus::Graphics>(memBitmap.get());
        memGraphics->SetSmoothingMode(SmoothingModeNone);
        memGraphics->SetInterpolationMode(InterpolationModeLowQuality);
        memGraphics->SetTextRenderingHint(TextRenderingHintSystemDefault);
    }

    defaultFont = std::make_unique<Gdiplus::Font>(L"Arial", 12.0f, FontStyleRegular, UnitPixel);
    commands.reserve(512);
}

void GDI::end()
{
    commands.clear();
    bitmapCache.clear();
    directImageCache.clear();
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
    if (!memGraphics && backPixels)
    {
        // 使用DIBSection时，直接清空内存
        uint32_t *pixels = static_cast<uint32_t *>(backPixels);
        const int totalPixels = backHeight * (backStride / 4);
        for (int i = 0; i < totalPixels; ++i)
        {
            pixels[i] = 0xFF000000; // 黑色背景
        }
    }
    else if (memGraphics)
    {
        // 使用GDI+时，使用Clear
        Color bg(0, 0, 0, 0);
        memGraphics->Clear(bg);
    }
}

// 优化的图像绘制函数 - 直接内存操作
static void DrawImageDirect(uint32_t *destPixels, int destWidth, int destHeight, int destStride,
                            const uint32_t *srcPixels, int srcWidth, int srcHeight,
                            int destX, int destY, int drawWidth, int drawHeight)
{
    // 计算裁剪区域
    int startX = (std::max)(0, destX);
    int startY = (std::max)(0, destY);
    int endX = (std::min)(destWidth, destX + drawWidth);
    int endY = (std::min)(destHeight, destY + drawHeight);

    if (startX >= endX || startY >= endY)
        return;

    // 计算源图像起始位置
    int srcStartX = startX - destX;
    int srcStartY = startY - destY;

    // 逐行复制像素
    for (int y = startY; y < endY; ++y)
    {
        const uint32_t *srcRow = srcPixels + (srcStartY + (y - startY)) * srcWidth + srcStartX;
        uint32_t *destRow = destPixels + y * (destStride / 4) + startX;

        int copyWidth = endX - startX;
        std::copy(srcRow, srcRow + copyWidth, destRow);
    }
}

void GDI::tick(float /*dt*/)
{
    if (!memGraphics && !backPixels)
    {
        commands.clear();
        return;
    }

    // 如果有DIBSection，使用直接内存操作处理图像
    if (backPixels)
    {
        uint32_t *destPixels = static_cast<uint32_t *>(backPixels);

        // 处理图像命令
        for (const auto &c : commands)
        {
            if (c.type == Type::DrawImage)
            {
                // 检查是否有直接像素访问的缓存
                auto it = directImageCache.find(c.resId);
                if (it != directImageCache.end())
                {
                    const CachedImage &cached = it->second;
                    DrawImageDirect(destPixels, backWidth, backHeight, backStride,
                                    cached.pixels.get(), cached.width, cached.height,
                                    c.x, c.y, c.w, c.h);
                }
                else
                {
                    // 回退到GDI+绘制
                    Bitmap *img = LoadBitmapFromRCDATA(c.resId);
                    if (img && memGraphics)
                    {
                        memGraphics->DrawImage(img, c.x, c.y, c.w, c.h);
                    }
                }
            }
        }
    }
    else if (memGraphics)
    {
        // 没有DIBSection，使用GDI+绘制图像
        for (const auto &c : commands)
        {
            if (c.type == Type::DrawImage)
            {
                Bitmap *img = LoadBitmapFromRCDATA(c.resId);
                if (img)
                {
                    memGraphics->DrawImage(img, c.x, c.y, c.w, c.h);
                }
            }
        }
    }

    // 处理文本命令 - 无论是否有DIBSection，都使用GDI+绘制文本
    // 注意：文本应该在所有图像之后绘制，以确保文本显示在最上面
    for (const auto &c : commands)
    {
        if (c.type == Type::DrawText && memGraphics)
        {
            // 文本绘制使用GDI+
            SolidBrush brush(c.color);
            PointF pt(static_cast<REAL>(c.x), static_cast<REAL>(c.y));
            memGraphics->DrawString(c.text.c_str(), -1, defaultFont.get(), pt, &brush);
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

    if (backPixels && backWidth > 0 && backHeight > 0)
    {
        // 使用最快的位块传输方式
        SetStretchBltMode(hdc, COLORONCOLOR);
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
        g.SetInterpolationMode(InterpolationModeLowQuality);
        g.DrawImage(memBitmap.get(), 0, 0, memBitmap->GetWidth(), memBitmap->GetHeight());
    }

    ReleaseDC(hwnd, hdc);
}

// 预加载图像到直接像素缓存
static bool PreloadImageToCache(int resId)
{
    if (directImageCache.find(resId) != directImageCache.end())
        return true;

    // 由于LoadBitmapFromRCDATA是私有的，我们需要复制其实现到这里
    HMODULE hMod = GetModuleHandleW(NULL);
    HRSRC hrs = FindResource(hMod, MAKEINTRESOURCE(resId), RT_RCDATA);
    if (!hrs)
        return false;
    HGLOBAL hRes = LoadResource(hMod, hrs);
    if (!hRes)
        return false;
    DWORD cb = SizeofResource(hMod, hrs);
    void *p = LockResource(hRes);
    if (!p || cb == 0)
        return false;

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, cb);
    if (!hMem)
        return false;
    void *dst = GlobalLock(hMem);
    memcpy(dst, p, cb);
    GlobalUnlock(hMem);

    IStream *stream = nullptr;
    if (CreateStreamOnHGlobal(hMem, TRUE, &stream) != S_OK)
    {
        GlobalFree(hMem);
        return false;
    }

    Bitmap *bmp = Bitmap::FromStream(stream);
    stream->Release();
    if (!bmp || bmp->GetLastStatus() != Ok)
    {
        delete bmp;
        return false;
    }

    // 获取图像尺寸
    int width = bmp->GetWidth();
    int height = bmp->GetHeight();

    // 创建缓存
    CachedImage cached;
    cached.width = width;
    cached.height = height;
    cached.stride = width * 4;
    cached.pixels = std::make_unique<uint32_t[]>(width * height);

    // 锁定位图并复制像素数据
    BitmapData bmpData;
    Rect rect(0, 0, width, height);

    if (bmp->LockBits(&rect, ImageLockModeRead, PixelFormat32bppPARGB, &bmpData) == Ok)
    {
        uint32_t *srcPixels = static_cast<uint32_t *>(bmpData.Scan0);
        std::copy(srcPixels, srcPixels + width * height, cached.pixels.get());
        bmp->UnlockBits(&bmpData);

        directImageCache[resId] = std::move(cached);
        delete bmp; // 清理临时位图
        return true;
    }

    delete bmp; // 清理临时位图
    return false;
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

    // 尝试预加载到直接像素缓存
    PreloadImageToCache(resId);

    bitmapCache[resId] = std::unique_ptr<Bitmap>(bmp);
    return bmp;
}

// backward-compatible wrappers
void GDI::image(int resId, int x, int y, int w, int h, bool flip)
{
    // 预加载图像到缓存（如果尚未加载）
    if (directImageCache.find(resId) == directImageCache.end())
    {
        PreloadImageToCache(resId);
    }

    pushImage(resId, x, y, w, h, flip);
}

void GDI::text(const std::wstring &txt, int x, int y, float size, Gdiplus::Color color)
{
    // 确保文本颜色不是黑色（如果背景是黑色）
    // 修复：直接比较ARGB值而不是调用GetValue()
    if (color.GetValue() == 0xFF000000) // 黑色
    {
        // 默认使用白色文本
        color = Color(255, 255, 255);
    }

    pushText(txt, x, y, size, color);
}