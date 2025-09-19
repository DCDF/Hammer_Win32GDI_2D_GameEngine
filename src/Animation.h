#pragma once
#include "Track.h"
#include <vector>
#include <memory>
#include <functional>

class Animation
{

private:
    std::vector<std::unique_ptr<Track>> tracks;
    double trueDuration();

public:
    Track *curTrack;
    int index;
    bool loop;
    double scale = 1.0;
    double duration = 1.0;
    double interval;
    double pass;
    void reset();
    void add(int x, int y, int w, int h, std::function<void()> callback = nullptr);

    void start();
    bool tick(double deltaTime);

    void changeIndex(int index);
};