#include "PC.h"
#include "GDI.h"
#include "Audio.h"

extern int GAME_WIDTH;
extern int GAME_HEIGHT;
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    PC pc(hInstance, GAME_WIDTH, GAME_HEIGHT, "Hammer");
    pc.show();

    GDI::init(pc.window());

    Audio_bg(301);

    GDI::addImageCommand(101, 0, 0, GAME_WIDTH, GAME_HEIGHT);

    bool running = true;
    while (running)
    {
        if (!pc.tick())
            break;
        GDI::tick();
    }

    GDI::end();
    Audio_shutdown();
    return 0;
}