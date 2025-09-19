#include "Camera.h"

extern int GAME_WIDTH;
extern int GAME_HEIGHT;

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
    return tx;
}

int Camera::getOffsetY()
{
    if (m_target == nullptr)
        return 0;

    return 0;
}
