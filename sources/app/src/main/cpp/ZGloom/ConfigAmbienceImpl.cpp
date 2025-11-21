// ConfigAmbienceImpl.cpp
#include "config.h"

namespace Config {

static int g_ambiencevol = 0;

int GetAmbienceVol()
{
    return g_ambiencevol;
}

void SetAmbienceVol(int vol)
{
    if (vol < 0) vol = 0;
    if (vol > 9) vol = 9;
    g_ambiencevol = vol;
}

} // namespace Config
