#include "ConfigOverlays.h"
#include "config.h"
#include <cmath>
namespace MenuAdapters {
    int GetWarmth01() {
        int w = Config::GetVignetteWarmth(); // -100..100
        if (w < -100) w = -100; if (w > 100) w = 100;
        return (w + 100) / 20; // -> 0..10
    }
    void SetWarmth01(int v) {
        if (v < 0) v = 0; if (v > 10) v = 10;
        Config::SetVignetteWarmth(v * 20 - 100);
    }

    int GetFovPreset()
    {
        // Map current focallength to one of 4 presets:
        // 0 = ULTRA (128), 1 = HIGH (160), 2 = MED (192), 3 = LOW (224)
        const int kFovVals[4] = { 128, 160, 192, 224 };
        int fl = (int)Config::GetFocalLength();
        int bestIdx = 0;
        int bestDiff = std::abs(fl - kFovVals[0]);
        for (int i = 1; i < 4; ++i)
        {
            int d = std::abs(fl - kFovVals[i]);
            if (d < bestDiff)
            {
                bestDiff = d;
                bestIdx = i;
            }
        }
        return bestIdx;
    }

    void SetFovPreset(int v)
    {
        if (v < 0) v = 0;
        if (v > 3) v = 3;
        const int kFovVals[4] = { 128, 160, 192, 224 };
        Config::SetFocalLength(kFovVals[v]);
        Config::Save();
    }

}
