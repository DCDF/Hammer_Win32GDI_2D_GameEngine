#pragma once
#include "Role.h"
class Camera
{

public:
    static int x;
    static int y;
    static Role *m_target;

    static void setTarget(Role *target);

    static int getOffsetX();

    static int getOffsetY();
};