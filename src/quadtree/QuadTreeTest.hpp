#pragma once
#include <iostream>
#include <random>
#include <chrono>
#include <vector>
#include <conio.h>
#include "QuadTree.h"

using namespace std;
using namespace std::chrono;

// 生成随机浮点数
float randomFloat(float min, float max)
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(min, max);
    return dis(gen);
}

int test()
{
    // 四叉树边界 (0,0) 到 (1000,1000)
    QuadTreeRect bound(0, 0, 1000, 1000);
    QuadTree tree(bound, 4); // 容量4，最大深度8

    const int OBJECT_COUNT = 1000; // 物体数量
    const int FRAMES = 1000;       // 测试帧数

    // 创建物体
    vector<QuadTreeRect *> objects;
    for (int i = 0; i < OBJECT_COUNT; ++i)
    {
        float x = randomFloat(0, 900);
        float y = randomFloat(0, 900);
        float w = randomFloat(10, 50);
        float h = randomFloat(10, 50);

        auto *obj = new QuadTreeRect(x, y, w, h, i);
        tree.insert(obj);
        objects.push_back(obj);
    }

    // 为每个物体设置随机速度
    vector<pair<float, float>> velocities;
    for (int i = 0; i < OBJECT_COUNT; ++i)
    {
        velocities.emplace_back(randomFloat(-5, 5), randomFloat(-5, 5));
    }

    // 开始性能测试
    auto start = high_resolution_clock::now();

    for (int frame = 0; frame < FRAMES; ++frame)
    {
        // 更新所有物体位置
        for (int i = 0; i < OBJECT_COUNT; ++i)
        {
            auto *obj = objects[i];
            auto &vel = velocities[i];

            // 移动物体
            obj->x += vel.first;
            obj->y += vel.second;

            // 边界反弹
            if (obj->x < 0 || obj->x + obj->w > 1000)
            {
                vel.first = -vel.first;
                obj->x += vel.first * 2; // 防止卡在边界
            }
            if (obj->y < 0 || obj->y + obj->h > 1000)
            {
                vel.second = -vel.second;
                obj->y += vel.second * 2;
            }

            // 更新四叉树
            tree.update(obj);
        }

        // 执行碰撞检测
        tree.tick(1.0 / 60.0); // 假设60FPS

        for (auto obj : objects)
        {
            // if (obj->collisioning.size() > 0 || obj->collisionDir.size() > 0)
            // {
            //     cout << "obj info: " << obj->id << " collisioning size: " << obj->collisioning.size() << " dir size: " << obj->collisionDir.size() << endl;
            // }
            // if (obj->collisionDir.size() != obj->collisioning.size())
            // {
            //     cout << "err obj info: " << obj->id << " collisioning size: " << obj->collisioning.size() << " dir size: " << obj->collisionDir.size() << endl;
            // }
        }
    }

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start);

    // 输出性能结果
    cout << "test suc!" << endl;
    cout << "num: " << OBJECT_COUNT << endl;
    cout << "fps: " << FRAMES << endl;
    cout << "time: " << duration.count() << "ms" << endl;
    cout << "each tick time: " << duration.count() / (double)FRAMES << "ms" << endl;
    cout << "each fps: " << FRAMES / (duration.count() / 1000.0) << endl;

    cout << "each fps: " << FRAMES / (duration.count() / 1000.0) << endl;
    // 清理内存
    for (auto obj : objects)
    {
        // cout << "obj info: " << obj->id << " collisioning size: " << obj->collisioning.size() << " dir size: " << obj->collisionDir.size() << endl;
        delete obj;
    }
    return 0;
}

void test1()
{
    // 四叉树边界 (0,0) 到 (1000,1000)
    QuadTreeRect bound(0, 0, 1000, 1000);
    QuadTree tree(bound, 4); // 容量4，最大深度8

    QuadTreeRect *rect1 = new QuadTreeRect(0, 100, 10, 10, 1);
    QuadTreeRect *rect2 = new QuadTreeRect(100, 100, 100, 100, 2);

    // 插入矩形到四叉树
    tree.insert(rect1);
    tree.insert(rect2);

    cout << "QuadTree Debug Test" << endl;
    cout << "Commands: w (up), s (down), a (left), d (right), q (quit)" << endl;

    string command;
    while (true)
    {
        cout << "Enter command: ";
        getline(cin, command);

        if (command.empty())
            continue;

        char key = tolower(command[0]);
        float moveAmount = 10.0f;

        switch (key)
        {
        case 'w': // 上移
            rect1->y -= moveAmount;
            break;
        case 's': // 下移
            rect1->y += moveAmount;
            break;
        case 'a': // 左移
            rect1->x -= moveAmount;
            break;
        case 'd': // 右移
            rect1->x += moveAmount;
            break;
        case 'q': // 退出
            goto exit_loop;
        default:
            cout << "Invalid command. Use w, a, s, d, or q." << endl;
            continue;
        }

        // 边界检查
        if (rect1->x < 0)
            rect1->x = 0;
        if (rect1->x + rect1->w > 1000)
            rect1->x = 1000 - rect1->w;
        if (rect1->y < 0)
            rect1->y = 0;
        if (rect1->y + rect1->h > 1000)
            rect1->y = 1000 - rect1->h;

        // 更新四叉树中的rect1位置
        tree.update(rect1);

        // 执行碰撞检测
        tree.tick(1.0 / 60.0);

        // 显示当前位置
        cout << "Rect1 position: (" << rect1->x << ", " << rect1->y << ")" << endl;

        // 查询与rect1碰撞的所有对象
        auto collisions = tree.query(rect1);
        cout << "Collisions with Rect1: " << collisions.size() << endl;
        for (auto obj : collisions)
        {
            if (obj != rect1) // 跳过自己
                cout << "  - Rect" << obj->id << endl;
        }
    }
exit_loop:

    // 清理内存
    delete rect1;
    delete rect2;
}