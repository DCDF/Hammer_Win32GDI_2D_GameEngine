#include "Role.h"

#include "KV.h"
#include "GDI.h"

#include <gdiplus.h>
#include <algorithm>
#include <thread>
#include <chrono>
#include <mutex>
#include "PropModel.h"
#include "QTree.h"

int Role::ROLE_ID = 0;

bool Role::hasCollision()
{
    return true;
}

Role::Role(int resId, int x_, int y_, int imgW_, int imgH_, int row, int col, int w_, int h_)
    : x(x_),
      y(y_),
      imgW(imgW_),
      imgH(imgH_),
      w(w_),
      h(h_),
      imgRow(row),
      imgCol(col),
      scaleX(1.0f),
      scaleY(1.0f),
      animOffsetX(0),
      animOffsetY(0),
      handVec(nullptr),
      otherVec(nullptr),
      totalVec(nullptr),
      preVec(nullptr),
      idle(true),
      face(true),
      resId(resId),
      anim(nullptr),
      name(L""),
      id(ROLE_ID++)
{
    anim = std::make_unique<Anim>();
    handVec = std::make_unique<KV>();
    otherVec = std::make_unique<KV>();
    totalVec = std::make_unique<KV>();
    preVec = std::make_unique<KV>();
    prePos = std::make_unique<KV>();
    line = GAME_LINE;
    QTree::update(id, static_cast<int>(x - w / 2), static_cast<int>(y - h), w, h, this);
}

void Role::setFace(bool right)
{
    if (right != face)
    {
        face = right;
        scaleX *= -1.0f;
    }
}

void Role::checkTickState()
{
    outSide = x < GAME_OFFSET_X - GAME_WIDTH || x > GAME_OFFSET_X + GAME_WIDTH;
}

void Role::tick(double deltaTime)
{
    checkTickState();
    if (outSide)
        return;
    line = GAME_LINE;
    if (otherLine != 0)
    {
        line = otherLine;
    }
    if (gravity > 0)
    {
        double delSpeed = gravity * deltaTime;
        if (upSpeed > 0)
        {
            otherVec->v -= upSpeed;
            upSpeed -= delSpeed;
            if (upSpeed < 0)
            {
                upSpeed = 0;
            }
            downSpeed = 0;
        }
        else
        {
            if (!ground)
            {
                downSpeed += delSpeed;
                otherVec->v += downSpeed;
            }
        }
    }

    // 合并输入向量
    totalVec->k = handVec->k + otherVec->k;
    totalVec->v = handVec->v + otherVec->v;

    if (idle)
    {
        if ((handVec->k != 0))
        {
            play("move", false);
            if ((handVec->k > 0) != face)
            {
                setFace(!face);
            }
        }
        else
        {
            play("idle", false);
        }
    }

    bool change = false;
    if (totalVec->k != 0)
    {
        x += totalVec->k * deltaTime;
        if (x < WORLD_LEFT)
        {
            x = WORLD_LEFT;
        }
        else if (x > WORLD_RIGHT)
        {
            x = WORLD_RIGHT;
        }
        change = true;
    }
    if (totalVec->v != 0)
    {
        y += totalVec->v * deltaTime;
        change = true;
    }
    if (change && hasCollision())
    {
        QTree::update(id, static_cast<int>(x - w / 2), static_cast<int>(y - h), w, h, this);
    }
    if (y >= line)
    {
        y = line;
    }
    bool tmpGround = y >= line;
    if (ground != tmpGround)
    {
        if (ground)
        {
            // todo 起飞
        }
    }
    ground = tmpGround;
    if (ground)
    {
        downSpeed = 0;
    }
    if (anim)
        anim->tick(deltaTime);

    preVec->k = totalVec->k;
    preVec->v = totalVec->v;

    handVec->clear();
    otherVec->clear();
    totalVec->clear();
    prePos->k = x;
    prePos->v = y;
}

void Role::render()
{
    if (outSide)
        return;

    // 计算绘制位置
    int drawX = static_cast<int>(x);
    int drawY = static_cast<int>(y);
    // GDI::rect(drawX, drawY, w, h);
    GDI::rect(drawX - w / 2, drawY - h, w, h);
    // GDI::text(name, static_cast<int>(x + nameXOffset), static_cast<int>(y + nameYOffset),10.5);
    if (!anim)
        return;
    auto track = anim->curTrack();
    if (track)
    {
        GDI::imageEx(resId, static_cast<int>(x - imgW / 2), static_cast<int>(y - imgH), imgW, imgH, face, track->spriteX, track->spriteY, track->spriteW, track->spriteH);
    }
}

void Role::addAnimation(const std::string &name, int start, int num, bool loop, std::vector<int> hitIndex)
{
    if (!anim)
        return;

    auto animation = anim->create(name);
    animation->loop = loop;
    for (int i = 0; i < num; i++)
    {
        int iRow = (start + i) / (imgRow ? imgRow : 1);
        int iCol = (start + i) % (imgRow ? imgRow : 1);

        auto it = std::find(hitIndex.begin(), hitIndex.end(), i);
        if (it != hitIndex.end())
        {
            animation->add(imgW * iCol, imgH * iRow, imgW, imgH, []()
                           {
                               // todo hit trigger
                           });
        }
        else
        {
            animation->add(imgW * iCol, imgH * iRow, imgW, imgH);
        }
    }
    animation->reset();
}

void Role::play(const std::string &name, bool force)
{
    if (anim)
        anim->play(name, force);
}

void Role::hurt(Damage *dmg)
{
}

void Role::changeProp(PropType type, double value)
{
    double cur = getProp(type);

    double newVal = cur + value;

    if (newVal < 0)
    {
        newVal = 0;
        if (cur > 0)
        {
            onPropZero(type);
        }
    }
    props[type] = newVal;
}

double Role::getProp(PropType type)
{
    auto it = props.find(type);
    if (it != props.end())
    {
        return it->second;
    }
    return 0.0;
}
void Role::onPropZero(PropType type)
{
}

void Role::setProps(std::unordered_map<PropType, double> &&p)
{
    props = std::move(p);
}
Role::~Role()
{
    QTree::remove(id);
}

void Role::onCollision(Role *other, DirectType dir)
{
}
void Role::onCollisioning(Role *other, DirectType dir)
{
}
void Role::onCollisionOut(Role *other)
{
}