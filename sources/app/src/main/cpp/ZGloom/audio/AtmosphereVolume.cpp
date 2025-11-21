#include "audio/AtmosphereVolume.h"
#include "config.h"
#include <algorithm>

namespace BGM { void SetVolume9(int v); }

namespace {
    bool g_inited = false;
    static inline int clamp09(int v) { return std::max(0, std::min(9, v)); }

    static void apply_now_from_config() {
        const int v = clamp09(Config::GetAmbienceVol());
        BGM::SetVolume9(v);
    }
    static void ensure_init() {
        if (g_inited) return;
        g_inited = true;
        apply_now_from_config();
    }
}

namespace EmbeddedBGMVolume {

void InitOnce() { ensure_init(); }

int Get() {
    ensure_init();
    return Config::GetAmbienceVol();
}

void Set(int level, bool save) {
    ensure_init();
    const int v = clamp09(level);
    Config::SetAmbienceVol(v);
    BGM::SetVolume9(v);
    if (save) {
        Config::Save();
    }
}

void Reload() {
    g_inited = true;
    apply_now_from_config();
}

void Save() {
    Config::Save();
}

void SetConfigPath(const std::string&) {
    // No-op
}

} // namespace EmbeddedBGMVolume

// No AtmosphereVolume::* bodies here to avoid duplicate definitions (C2084). They are inline in the header.
