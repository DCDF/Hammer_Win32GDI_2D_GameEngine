#include "Animation.h"

void Animation::reset()
{
    if (tracks.size() == 0)
        return;
    double d = trueDuration();
    double s = static_cast<double>(tracks.size());
    interval = d / s;
}

double Animation::trueDuration()
{
    return duration * scale;
}

void Animation::add(int x, int y, int w, int h, std::function<void()> callback)
{
    auto track = std::make_unique<Track>();
    track->spriteX = x;
    track->spriteY = y;
    track->spriteW = w;
    track->spriteH = h;
    track->setCallback(callback);
    tracks.push_back(std::move(track));
}

bool Animation::tick(double deltaTime)
{
    pass += deltaTime;
    while (pass >= interval)
    {
        pass -= interval;
        int nextIndex = index + 1;
        if (nextIndex >= tracks.size())
        {
            if (!loop)
            {
                curTrack = nullptr;
                return true;
            }
            nextIndex = 0;
        }
        changeIndex(nextIndex);
    }
    return false;
}

void Animation::changeIndex(int i)
{
    if (i >= tracks.size())
        index = 0;
    index = i;
    curTrack = tracks[index].get();
    curTrack->executeCallback();
}

void Animation::start()
{
    changeIndex(0);
}
