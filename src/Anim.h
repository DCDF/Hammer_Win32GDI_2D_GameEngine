#pragma once
#include <map>
#include "Animation.h"
#include <memory>
#include <string>

class Anim
{
private:
    std::map<std::string, std::unique_ptr<Animation>> animations;
    Animation *curAnimation;
    std::string curName;

public:
    Animation *create(const std::string &name)
    {
        auto animation = std::make_unique<Animation>();
        auto *ptr = animation.get();
        animations[name] = std::move(animation);
        return ptr;
    }

    Animation *cur()
    {
        return curAnimation;
    }

    Track *curTrack()
    {
        auto a = cur();
        if (a == nullptr)
        {
            return nullptr;
        }
        return a->curTrack;
    }

    Animation *get(const std::string &name)
    {
        auto i = animations.find(name);
        if (i == animations.end())
        {
            return nullptr;
        }
        return i->second.get();
    }
    void remove(const std::string &name)
    {
        animations.erase(name);
    }

    void play(const std::string &name, bool force = true)
    {

        auto animation = get(name);
        if (animation == nullptr)
        {
            return;
        }
        if (!force && curName == name)
            return;
        curName = name;
        curAnimation = animation;
        curAnimation->start();
    }

    void tick(double deltaTime)
    {
        if (curAnimation == nullptr)
        {
            return;
        }
        if (curAnimation->tick(deltaTime))
        {
            finishAnimation();
            return;
        }
    }

    void finishAnimation()
    {
        curAnimation = nullptr;
    }
};

class GridCreator
{
public:
    GridCreator(int w, int h, int c, int r) : width(w), height(h), col(c), row(r)
    {
    }
    int width;
    int height;
    int col;
    int row;
};