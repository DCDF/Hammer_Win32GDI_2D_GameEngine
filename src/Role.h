#pragma once

#include <memory>
#include <string>
#include <vector>
#include "Damage.h"
#include <unordered_map>
#include "PropType.h"
namespace Gdiplus
{
    class Bitmap;
} // forward-declare GDI+ Bitmap
class Anim;
class KV;

class Role
{
public:
    Role(int resId, int x, int y, int imgW, int imgH, int row,int col, int w, int h);
    virtual ~Role() = default;

    bool idle = true;
    bool ground = true;
    bool face;

    int imgW;
    int imgH;
    int w;
    int h;

    int resId;
    // sprite sheet layout
    int imgCol;
    int imgRow;
    // optional animation offset
    int animOffsetX;
    int animOffsetY;
    int nameXOffset = -10;
    int nameYOffset = -50;
    // scale (used to mirror)
    float scaleX;
    float scaleY;

    // position & frame size
    double x;
    double y;
    // gravity
    double gravity = 800;
    double upSpeed = 0;
    double downSpeed = 0;
    // movement / input vectors (KV is your small struct)
    std::unique_ptr<KV> handVec;
    std::unique_ptr<KV> otherVec;
    std::unique_ptr<KV> totalVec;
    std::unique_ptr<KV> preVec;

    std::wstring name;
    // pointer to image wrapper (owns the texture elsewhere)
    std::unique_ptr<Anim> anim;

    // prop
    std::unordered_map<PropType, double> props;
    void changeProp(PropType type, double value);
    double getProp(PropType type);
    void onPropZero(PropType type);
    void initProp(std::unordered_map<PropType, double> &&p);

    // control
    void setFace(bool right);

    // lifecycle
    virtual void tick(double deltaTime);
    virtual void render();

    // animation helpers
    virtual void addAnimation(const std::string &name, int start, int num, bool loop, std::vector<int> hitIndex);
    virtual void play(const std::string &name, bool force = true);

    // HIT
    virtual void hurt(Damage *dmg);
};
