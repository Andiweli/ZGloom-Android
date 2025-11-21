#pragma once
#include <stdint.h>
#include <SDL2/SDL.h>

// Simple 60 FPS limiter using SDL high-resolution timer.
// Call FrameLimiter::beginFrame() at the start of your frame and
// FrameLimiter::endFrameAndSleep(targetFps) at the end.
class FrameLimiter {
public:
    FrameLimiter()
    : perfFreq(SDL_GetPerformanceFrequency()),
      lastTicks(SDL_GetPerformanceCounter()) {}

    inline void beginFrame() {
        frameStart = SDL_GetPerformanceCounter();
    }

    inline void endFrameAndSleep(int targetFps = 60) {
        if (targetFps <= 0) return;
        const uint64_t now = SDL_GetPerformanceCounter();
        const double frameElapsed = (double)(now - frameStart) / (double)perfFreq;
        const double target = 1.0 / (double)targetFps;
        if (frameElapsed < target) {
            const double sleepSec = target - frameElapsed;
            const uint32_t sleepMs = (uint32_t)(sleepSec * 1000.0);
            if (sleepMs > 0) SDL_Delay(sleepMs);
            // Busy-wait the remaining few hundred microseconds to improve stability
            uint64_t busyStart = SDL_GetPerformanceCounter();
            while (((double)(SDL_GetPerformanceCounter() - busyStart) / (double)perfFreq) < (sleepSec - sleepMs / 1000.0)) {}
        }
        lastTicks = SDL_GetPerformanceCounter();
    }

private:
    uint64_t perfFreq;
    uint64_t lastTicks;
    uint64_t frameStart;
};
