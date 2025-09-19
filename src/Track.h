#pragma once
#include <functional>
#include <memory>

class Track
{
public:
    int spriteX;
    int spriteY;
    int spriteW;
    int spriteH;

    std::function<void()> onReachedCallback;

    void setCallback(std::function<void()> callback)
    {
        onReachedCallback = std::move(callback);
    }

    void executeCallback()
    {
        if (onReachedCallback)
        {
            onReachedCallback();
        }
    }
};