#include "PC.h"
#include "GDI.h"
#include "Audio.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    PC pc(hInstance, 640, 160, "Test Game");
    pc.show();

    GDI gdi(pc.window());
    gdi.init();

    Audio_bg(301);  // 示例背景音乐

    bool running = true;
    while (running) {
        if (!pc.tick()) break;  // 处理窗口消息
        gdi.tick();
    }

    gdi.end();
    Audio_shutdown();
    return 0;
}