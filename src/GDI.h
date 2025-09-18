#pragma once
#include <Windows.h>
#include <gdiplus.h>
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include <cstdint> // for uint32_t

// 新增：直接像素访问的图像缓存结构
struct CachedImage
{
    int width = 0;
    int height = 0;
    std::unique_ptr<uint32_t[]> pixels; // 预乘Alpha的BGRA格式
    bool isOpaque = true;               // 优化：标记图像是否完全不透明
};

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
    static void text(const std::wstring &txt, int x, int y, float size = 12.0f, Gdiplus::Color color = Gdiplus::Color::White);

    // 新增扩展接口
    static void imageEx(int resId, int x, int y, int w, int h,
                        bool flip = false, int srcX = 0, int srcY = 0,
                        int srcW = 0, int srcH = 0);

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
        // 新增裁剪参数
        int srcX = 0, srcY = 0, srcW = 0, srcH = 0;
        bool hasSrcRect = false;
        // DrawText params
        std::wstring text;
        float fontSize = 12.0f;
        Gdiplus::Color color = Gdiplus::Color::White;

        // constructors
        static Command MakeImage(int resId_, int x_, int y_, int w_, int h_, bool flip_ = false,
                                 int srcX_ = 0, int srcY_ = 0, int srcW_ = 0, int srcH_ = 0)
        {
            Command c;
            c.type = Type::DrawImage;
            c.resId = resId_;
            c.x = x_;
            c.y = y_;
            c.w = w_;
            c.h = h_;
            c.flip = flip_;
            c.srcX = srcX_;
            c.srcY = srcY_;
            c.srcW = srcW_;
            c.srcH = srcH_;
            c.hasSrcRect = (srcW_ > 0 && srcH_ > 0);
            return c;
        }
        static Command MakeText(const std::wstring &txt, int x_, int y_, float size_ = 12.0f, Gdiplus::Color color_ = Gdiplus::Color::White)
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

private:
    // push commands (轻量接口)
    static void pushImage(int resId, int x, int y, int w, int h, bool flip = false)
    {
        commands.emplace_back(Command::MakeImage(resId, x, y, w, h, flip));
    }

    // 新增扩展push命令
    static void pushImageEx(int resId, int x, int y, int w, int h,
                            bool flip, int srcX, int srcY, int srcW, int srcH)
    {
        commands.emplace_back(Command::MakeImage(resId, x, y, w, h, flip, srcX, srcY, srcW, srcH));
    }

    static void pushText(const std::wstring &txt, int x, int y, float size = 12.0f, Gdiplus::Color color = Gdiplus::Color::White)
    {
        commands.emplace_back(Command::MakeText(txt, x, y, size, color));
    }

    static HWND hwnd;
    static ULONG_PTR gdiplusToken;

    // DIBSection backing buffer
    static uint32_t *backPixels; // 直接指向像素数据 (BGRA)
    static HBITMAP hBackBitmap;
    static HDC hBackDC;
    static BITMAPINFO backInfo;
    static int backWidth;
    static int backHeight;

    // GDI+ 仅用于加载和字体
    static std::unique_ptr<Gdiplus::Graphics> memGraphics; // 用于GDI+回退模式

    // 缓存
    static std::unordered_map<int, CachedImage> imageCache;
    static std::unordered_map<float, HFONT> fontCache;

    static std::vector<Command> commands;

    // 辅助函数
    static bool createBackBuffer(int w, int h);
    static void destroyBackBuffer();
    static CachedImage *loadImage(int resId);
    static HFONT getFont(float size);

    // 优化的绘制函数
    static void drawImageFast(const Command &cmd);
};