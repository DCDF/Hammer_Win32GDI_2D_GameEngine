#pragma once
class QuadTreeRect;

class QuadTreeCollisionInfo
{
public:
    QuadTreeRect *from;
    QuadTreeRect *to;
    int dir;
};