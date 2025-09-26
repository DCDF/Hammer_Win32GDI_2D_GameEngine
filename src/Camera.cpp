#include "Camera.h"

extern int GAME_WIDTH;
extern int GAME_HEIGHT;
extern int WORLD_LEFT;
extern int WORLD_RIGHT;

int Camera::x = 0;
int Camera::y = 0;
Role *Camera::m_target = nullptr;

void Camera::setTarget(Role *target)
{
    m_target = target;
}

int Camera::getOffsetX()
{
    if (m_target == nullptr)
        return 0;
    double tx = m_target->x - GAME_WIDTH / 2;

    if (tx < 0)
    {
        tx = 0;
    }
    else if (tx > WORLD_RIGHT - GAME_WIDTH)
    {
        tx = WORLD_RIGHT - GAME_WIDTH;
    }
    return static_cast<int>(tx);
} 

int Camera::getOffsetY()
{ 
    if (m_target == nullptr)
        return 0;

    return 0;
}
