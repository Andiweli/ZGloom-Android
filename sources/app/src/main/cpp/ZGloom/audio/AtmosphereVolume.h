#pragma once
#include <string>

namespace EmbeddedBGMVolume {
void InitOnce();
int  Get();
void Set(int level, bool save = true);
void Reload();
void Save();
void SetConfigPath(const std::string& path);
}

namespace AtmosphereVolume {
inline void LoadFromConfig() { EmbeddedBGMVolume::InitOnce(); }
inline int  Get() { return EmbeddedBGMVolume::Get(); }
inline int  ToSDLVolume(int v) { return (v<=0)?0:((v>=9)?128:(v*14)); }
}
