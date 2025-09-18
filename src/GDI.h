#pragma once
#include <Windows.h>
#include <gdiplus.h>
#include <memory>
#include <unordered_map>
#include <vector>
#include <functional>
class GDI
{
public:
    GDI() = delete;
    ~GDI() = default;

    static void init(HWND h);

    static void tick();

    static void end();
    using Command = std::function<void()>;

    static void image(int resId, int x, int y, int w, int h, bool flip = false);
    static void addImageCommand(int resId, int x, int y, int w, int h, bool flip = false)
    {
        commands.push_back([resId, x, y, w, h, flip]()
                           { image(resId, x, y, w, h, flip); });
    }

private:
    static HWND hwnd;

    // GDI+释放对象
    static ULONG_PTR gdiplusToken;
    // 双缓冲的内存位图
    static std::unique_ptr<Gdiplus::Bitmap> memBitmap;
    // 内存双缓冲
    static std::unique_ptr<Gdiplus::Graphics> memGraphics;
    // 笔缓存
    static std::unique_ptr<Gdiplus::Pen> pen;

    // 图片缓存
    static std::unordered_map<int, std::unique_ptr<Gdiplus::Bitmap>> bitmapCache;
    // rcdata图片资源
    static Gdiplus::Bitmap *LoadBitmapFromRCDATA(int resId);

    // 绘制函数指针
    static std::vector<Command> commands;
};