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

// 新增：镜像图像缓存
static std::unordered_map<int, CachedImage> flippedImageCache;

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
    flippedImageCache.clear();
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
        // 使用DIBSection时，直接清空内存为透明
        uint32_t *pixels = static_cast<uint32_t *>(backPixels);
        const int totalPixels = backHeight * (backStride / 4);
        for (int i = 0; i < totalPixels; ++i)
        {
            pixels[i] = 0x00000000; // 透明背景 (ARGB: 0,0,0,0)
        }
    }
    else if (memGraphics)
    {
        // 使用GDI+时，使用Clear
        Color bg(0, 0, 0, 0); // 完全透明
        memGraphics->Clear(bg);
    }
}

// 正确的预乘 Alpha 混合函数
static inline uint32_t AlphaBlend(uint32_t dest, uint32_t src)
{
    // 提取源像素的 Alpha 通道
    uint8_t alpha = (src >> 24) & 0xFF;

    // 如果完全透明，直接返回目标
    if (alpha == 0)
        return dest;

    // 如果完全不透明，直接返回源
    if (alpha == 255)
        return src;

    // 提取源和目标颜色分量（注意：源是预乘的）
    uint8_t src_r = (src >> 16) & 0xFF;
    uint8_t src_g = (src >> 8) & 0xFF;
    uint8_t src_b = src & 0xFF;

    uint8_t dest_r = (dest >> 16) & 0xFF;
    uint8_t dest_g = (dest >> 8) & 0xFF;
    uint8_t dest_b = dest & 0xFF;

    // 计算逆 Alpha 值
    uint8_t inv_alpha = 255 - alpha;

    // 对于预乘 Alpha 图像，混合公式是：result = src + dest * (1 - alpha)
    uint8_t out_r = src_r + (dest_r * inv_alpha) / 255;
    uint8_t out_g = src_g + (dest_g * inv_alpha) / 255;
    uint8_t out_b = src_b + (dest_b * inv_alpha) / 255;

    // 组合结果（保持目标 Alpha 不变）
    return (dest & 0xFF000000) | (out_r << 16) | (out_g << 8) | out_b;
}

// 修复DrawImageFast函数
static void DrawImageFast(uint32_t *destPixels, int destWidth, int destHeight, int destStride,
                          const uint32_t *srcPixels, int srcWidth, int srcHeight,
                          int destX, int destY, int destW, int destH, bool flip)
{
    // 计算裁剪区域
    int startX = (std::max)(0, destX);
    int startY = (std::max)(0, destY);
    int endX = (std::min)(destWidth, destX + destW);
    int endY = (std::min)(destHeight, destY + destH);

    if (startX >= endX || startY >= endY)
        return;

    // 逐行复制像素
    for (int y = startY; y < endY; ++y)
    {
        // 计算源图像Y坐标
        int srcYPos = y - destY;
        if (srcYPos < 0 || srcYPos >= srcHeight)
            continue;

        if (flip)
        {
            // 水平翻转：从右向左复制
            const uint32_t *srcRow = srcPixels + srcYPos * srcWidth;
            uint32_t *destRow = destPixels + y * (destStride / 4) + startX;

            for (int x = startX; x < endX; ++x)
            {
                int srcXPos = srcWidth - 1 - (x - destX);
                if (srcXPos >= 0 && srcXPos < srcWidth)
                {
                    *destRow = AlphaBlend(*destRow, srcRow[srcXPos]);
                }
                destRow++;
            }
        }
        else
        {
            // 正常复制：从左向右复制
            const uint32_t *srcRow = srcPixels + srcYPos * srcWidth + (startX - destX);
            uint32_t *destRow = destPixels + y * (destStride / 4) + startX;

            int copyWidth = endX - startX;
            for (int i = 0; i < copyWidth; i++)
            {
                *destRow = AlphaBlend(*destRow, srcRow[i]);
                destRow++;
            }
        }
    }
}

// 修复DrawImageWithCrop函数中的镜像处理
static void DrawImageWithCrop(uint32_t *destPixels, int destWidth, int destHeight, int destStride,
                              const uint32_t *srcPixels, int srcWidth, int srcHeight,
                              int destX, int destY, int destW, int destH,
                              int srcX, int srcY, int srcW, int srcH, bool flip)
{
    // 如果没有指定源矩形，使用整个图像
    if (srcW <= 0 || srcH <= 0)
    {
        srcX = 0;
        srcY = 0;
        srcW = srcWidth;
        srcH = srcHeight;
    }

    // 计算缩放比例
    float scaleX = static_cast<float>(destW) / srcW;
    float scaleY = static_cast<float>(destH) / srcH;

    // 计算目标区域
    int startX = (std::max)(0, destX);
    int startY = (std::max)(0, destY);
    int endX = (std::min)(destWidth, destX + destW);
    int endY = (std::min)(destHeight, destY + destH);

    if (startX >= endX || startY >= endY)
        return;

    // 逐行复制像素（支持镜像和裁剪）
    for (int y = startY; y < endY; ++y)
    {
        // 计算源图像Y坐标
        int srcYPos = srcY + static_cast<int>((y - destY) / scaleY);
        if (srcYPos < srcY || srcYPos >= srcY + srcH)
            continue;

        uint32_t *destRow = destPixels + y * (destStride / 4) + startX;

        for (int x = startX; x < endX; ++x)
        {
            // 计算源图像X坐标（处理镜像）
            int srcXPos;
            if (flip)
            {
                // 修复：正确计算镜像坐标
                srcXPos = srcX + srcW - 1 - static_cast<int>((x - destX) / scaleX);
            }
            else
            {
                srcXPos = srcX + static_cast<int>((x - destX) / scaleX);
            }

            if (srcXPos >= srcX && srcXPos < srcX + srcW &&
                srcYPos >= srcY && srcYPos < srcY + srcH)
            {
                // 复制像素
                *destRow = AlphaBlend(*destRow, srcPixels[srcYPos * srcWidth + srcXPos]);
            }

            destRow++;
        }
    }
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

    // 确保使用 PARGB 格式
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

// 预计算镜像图像
static bool PreloadFlippedImageToCache(int resId)
{
    int flippedId = -resId; // 使用负ID表示镜像版本

    if (flippedImageCache.find(flippedId) != flippedImageCache.end())
        return true;

    // 获取原始图像
    auto it = directImageCache.find(resId);
    if (it == directImageCache.end())
        return false;

    const CachedImage &original = it->second;

    // 创建镜像版本
    CachedImage flipped;
    flipped.width = original.width;
    flipped.height = original.height;
    flipped.stride = original.stride;
    flipped.pixels = std::make_unique<uint32_t[]>(original.width * original.height);

    // 水平翻转图像
    for (int y = 0; y < original.height; ++y)
    {
        for (int x = 0; x < original.width; ++x)
        {
            int srcIndex = y * original.width + x;
            int dstIndex = y * original.width + (original.width - 1 - x);
            flipped.pixels[dstIndex] = original.pixels[srcIndex];
        }
    }

    flippedImageCache[flippedId] = std::move(flipped);
    return true;
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
                if (c.flip)
                {
                    // 检查镜像缓存
                    int flippedId = -c.resId;
                    auto it = flippedImageCache.find(flippedId);
                    if (it != flippedImageCache.end())
                    {
                        const CachedImage &cached = it->second;
                        if (c.hasSrcRect)
                        {
                            // 计算在预计算镜像中的源矩形：水平翻转后，源X坐标需要调整
                            int newSrcX = cached.width - c.srcX - c.srcW;
                            int newSrcY = c.srcY;
                            // 使用DrawImageWithCrop绘制镜像图像（注意：这里flip参数为false，因为图像已经预翻转了）
                            DrawImageWithCrop(destPixels, backWidth, backHeight, backStride,
                                              cached.pixels.get(), cached.width, cached.height,
                                              c.x, c.y, c.w, c.h,
                                              newSrcX, newSrcY, c.srcW, c.srcH, false);
                        }
                        else
                        {
                            // 没有源矩形，使用快速路径（无裁剪）
                            DrawImageFast(destPixels, backWidth, backHeight, backStride,
                                          cached.pixels.get(), cached.width, cached.height,
                                          c.x, c.y, c.w, c.h, false); // 注意：这里传递false，因为图像已经预翻转了
                        }
                        continue;
                    }
                }

                // 检查普通缓存
                auto it = directImageCache.find(c.resId);
                if (it != directImageCache.end())
                {
                    const CachedImage &cached = it->second;

                    // 根据是否需要裁剪选择不同的绘制函数
                    if (c.hasSrcRect)
                    {
                        // 使用支持裁剪的较慢路径
                        DrawImageWithCrop(destPixels, backWidth, backHeight, backStride,
                                          cached.pixels.get(), cached.width, cached.height,
                                          c.x, c.y, c.w, c.h,
                                          c.srcX, c.srcY, c.srcW, c.srcH, c.flip);
                    }
                    else
                    {
                        // 使用快速路径（无裁剪）
                        DrawImageFast(destPixels, backWidth, backHeight, backStride,
                                      cached.pixels.get(), cached.width, cached.height,
                                      c.x, c.y, c.w, c.h, c.flip);
                    }
                }
                else
                {
                    // 回退到GDI+绘制
                    Bitmap *img = LoadBitmapFromRCDATA(c.resId);
                    if (img && memGraphics)
                    {
                        if (c.hasSrcRect)
                        {
                            // 绘制裁剪区域
                            Rect destRect(c.x, c.y, c.w, c.h);
                            Rect srcRect(c.srcX, c.srcY, c.srcW, c.srcH);

                            if (c.flip)
                            {
                                // 镜像翻转：先水平翻转整个图像，然后绘制裁剪区域
                                memGraphics->DrawImage(img, destRect,
                                                       srcRect.X, srcRect.Y, srcRect.Width, srcRect.Height,
                                                       UnitPixel, nullptr, nullptr, nullptr);
                            }
                            else
                            {
                                memGraphics->DrawImage(img, destRect,
                                                       srcRect.X, srcRect.Y, srcRect.Width, srcRect.Height,
                                                       UnitPixel);
                            }
                        }
                        else
                        {
                            if (c.flip)
                            {
                                // 镜像翻转整个图像
                                memGraphics->DrawImage(img,
                                                       Rect(c.x + c.w, c.y, -c.w, c.h),
                                                       0, 0, img->GetWidth(), img->GetHeight(),
                                                       UnitPixel);
                            }
                            else
                            {
                                memGraphics->DrawImage(img, c.x, c.y, c.w, c.h);
                            }
                        }
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
                    if (c.hasSrcRect)
                    {
                        // 绘制裁剪区域
                        Rect destRect(c.x, c.y, c.w, c.h);
                        Rect srcRect(c.srcX, c.srcY, c.srcW, c.srcH);

                        if (c.flip)
                        {
                            // 镜像翻转：先水平翻转整个图像，然后绘制裁剪区域
                            memGraphics->DrawImage(img, destRect,
                                                   srcRect.X, srcRect.Y, srcRect.Width, srcRect.Height,
                                                   UnitPixel, nullptr, nullptr, nullptr);
                        }
                        else
                        {
                            memGraphics->DrawImage(img, destRect,
                                                   srcRect.X, srcRect.Y, srcRect.Width, srcRect.Height,
                                                   UnitPixel);
                        }
                    }
                    else
                    {
                        if (c.flip)
                        {
                            // 镜像翻转整个图像
                            memGraphics->DrawImage(img,
                                                   Rect(c.x + c.w, c.y, -c.w, c.h),
                                                   0, 0, img->GetWidth(), img->GetHeight(),
                                                   UnitPixel);
                        }
                        else
                        {
                            memGraphics->DrawImage(img, c.x, c.y, c.w, c.h);
                        }
                    }
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

    // 如果需要镜像，预加载镜像版本
    if (flip && flippedImageCache.find(-resId) == flippedImageCache.end())
    {
        PreloadFlippedImageToCache(resId);
    }

    pushImage(resId, x, y, w, h, flip);
}

void GDI::imageEx(int resId, int x, int y, int w, int h,
                  bool flip, int srcX, int srcY, int srcW, int srcH)
{
    // 预加载图像到缓存（如果尚未加载）
    if (directImageCache.find(resId) == directImageCache.end())
    {
        PreloadImageToCache(resId);
    }

    // 如果需要镜像，预加载镜像版本
    if (flip && flippedImageCache.find(-resId) == flippedImageCache.end())
    {
        PreloadFlippedImageToCache(resId);
    }

    pushImageEx(resId, x, y, w, h, flip, srcX, srcY, srcW, srcH);
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