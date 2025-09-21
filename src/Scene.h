#pragma once
#include <memory>
#include <unordered_map>
#include <string>
#include "GDI.h"
#include "QTree.h"

class Scene
{
protected:
    // std::unordered_map<std::string, std::unique_ptr<Resource>> m_uniqueRes;
    // std::unordered_map<std::string, std::shared_ptr<Resource>> m_sharedRes;

public:
    static std::unique_ptr<Scene> curScene;

    static void change(std::unique_ptr<Scene> newScene)
    {
        if (curScene)
        {
            curScene->exit();
        }
        QTree::clear();
        newScene->beforeEnter();
        newScene->enter();

        // 重置 curScene：释放当前管理的对象，接管新对象的所有权。
        curScene = std::move(newScene);
    }

    // 所有权隐藏在m_uniqueRes,外部不用关心生命周期
    template <typename R>
    R *getSceneRes(const std::string &name)
    {
        // auto res = ResMgr::get<R>(name);
        // R *raw = res.get();
        // m_uniqueRes[name] = std::move(res);
        return nullptr;
    }

    // 与get统一,方便外部使用
    template <typename R>
    R *loadSceneRes(const std::string &name)
    {
        // auto res = ResMgr::load<R>(name);
        // R *raw = res.get();
        // m_sharedRes[name] = res;
        return nullptr;
    }

    virtual void onKeyDown(int key){}
    virtual void onKeyUp(int key){}

    virtual void beforeEnter() {}

    virtual void enter() {}

    virtual void tick(double deltaTime) {}

    virtual void render() {}
    virtual void renderGlobal() {}

    virtual void exit() {}

    virtual ~Scene() = default;
};
