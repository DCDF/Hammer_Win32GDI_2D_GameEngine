#pragma once
#include <Windows.h>
#include <gdiplus.h>
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>

class GDI
{
public:
    GDI() = delete;
    ~GDI() = default;

    static void init(HWND h);    // 初始化（创建DIBSection + GDI+）
    static void begin(float dt); // 每帧开始（清屏）
    static void tick(float dt);  // 执行命令列表（绘制到内存buffer）
    static void flush(float dt); // 将内存buffer刷到窗口（快速Blit）
    static void end();           // 释放资源

    // 向后兼容接口（旧代码可能直接调用）
    static void image(int resId, int x, int y, int w, int h, bool flip = false);
    static void text(const std::wstring &txt, int x, int y, float size = 12.0f, Gdiplus::Color color = Gdiplus::Color::Black);

    // 轻量命令类型（避免 std::function 分配）
    enum class Type : uint8_t
    {
        DrawImage,
        DrawText
    };

    struct Command
    {
        Type type;
        // DrawImage params
        int resId = 0;
        int x = 0, y = 0, w = 0, h = 0;
        bool flip = false;
        // DrawText params
        std::wstring text;
        float fontSize = 12.0f;
        Gdiplus::Color color = Gdiplus::Color::Black;

        // constructors
        static Command MakeImage(int resId_, int x_, int y_, int w_, int h_, bool flip_ = false)
        {
            Command c;
            c.type = Type::DrawImage;
            c.resId = resId_;
            c.x = x_;
            c.y = y_;
            c.w = w_;
            c.h = h_;
            c.flip = flip_;
            return c;
        }
        static Command MakeText(const std::wstring &txt, int x_, int y_, float size_ = 12.0f, Gdiplus::Color color_ = Gdiplus::Color::Black)
        {
            Command c;
            c.type = Type::DrawText;
            c.x = x_;
            c.y = y_;
            c.text = txt;
            c.fontSize = size_;
            c.color = color_;
            return c;
        }
    };

    // push commands (轻量接口)
    static void pushImage(int resId, int x, int y, int w, int h, bool flip = false)
    {
        commands.emplace_back(Command::MakeImage(resId, x, y, w, h, flip));
    }
    static void pushText(const std::wstring &txt, int x, int y, float size = 12.0f, Gdiplus::Color color = Gdiplus::Color::Black)
    {
        commands.emplace_back(Command::MakeText(txt, x, y, size, color));
    }

private:
    static HWND hwnd;

    // GDI+ token
    static ULONG_PTR gdiplusToken;

    // DIBSection backing buffer
    static void *backPixels; // pointer to pixel data (top-down BGRA/premult)
    static HBITMAP hBackBitmap;
    static BITMAPINFO backInfo;
    static int backWidth;
    static int backHeight;
    static int backStride;

    // GDI+ objects that wrap the buffer
    static std::unique_ptr<Gdiplus::Bitmap> memBitmap;     // wrapper around backPixels (zero-copy)
    static std::unique_ptr<Gdiplus::Graphics> memGraphics; // draws into memBitmap

    // caches
    static std::unique_ptr<Gdiplus::Font> defaultFont;
    static std::unordered_map<int, std::unique_ptr<Gdiplus::Bitmap>> bitmapCache;

    // lightweight command list
    static std::vector<Command> commands;

    // helpers
    static bool createBackBuffer(int w, int h);
    static void destroyBackBuffer();
    static Gdiplus::Bitmap *LoadBitmapFromRCDATA(int resId);
};
