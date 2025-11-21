#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>

#include "ConfigOverlays.h"
#include "config.h"

namespace Config {

    // ---- State ----
    static int g_loaded      = 0;
    static int g_vEnabled    = 0;
    static int g_vStrength   = 0;  // 0..5
    static int g_vRadius     = 0;  // 0..5
    static int g_vSoftness   = 0;  // 0..5
    static int g_vWarmth     = 0;  // -100..100
    static int g_grain       = 0;  // 0/1
    static int g_grainInt    = 1;  // 0..5 (1 schon stark)
    static int g_scan        = 0;  // 0/1
    static int g_scanInt     = 1;  // 0..5
    static int g_muzzle      = 1;  // 0/1  (default ON)

static std::string GetOverlayConfigPath()
{
    const std::string& root = GetDataRoot();
    if (root.empty())
        return std::string("config.txt");
    return root + "config.txt";
}

    // ---- Helpers ----
    static inline int clamp(int v, int lo, int hi){
        if (v < lo) return lo; if (v > hi) return hi; return v;
    }

    // Patch or append managed key lines in-place in config.txt (non-destructive for other keys).
    static void patchKeys(const std::vector<std::pair<std::string, std::string>>& kvs)
    {
        // read whole file (if exists)
        std::vector<std::string> lines;
        {
            std::FILE* f = std::fopen(GetOverlayConfigPath().c_str(), "r");
            if (f){
                char buf[1024];
                while (std::fgets(buf, sizeof(buf), f)){
                    std::string line(buf);
                    // strip CR/LF
                    while (!line.empty() && (line.back()=='\n' || line.back()=='\r')) line.pop_back();
                    lines.push_back(line);
                }
                std::fclose(f);
            }
        }

        // map of managed keys for quick lookup
        auto starts_with = [](const std::string& s, const std::string& prefix)->bool{
            return s.size() >= prefix.size() && std::memcmp(s.data(), prefix.data(), prefix.size()) == 0;
        };

        std::vector<int> seen(kvs.size(), 0);
        for (std::string& line : lines){
            // skip comments
            if (!line.empty() && line[0] == ';') continue;
            // replace any managed key at start of line
            for (size_t i=0; i<kvs.size(); ++i){
                const std::string& key = kvs[i].first;
                if (starts_with(line, key)){
                    line = key + kvs[i].second;
                    seen[i] = 1;
                }
            }
        }
        // append missing
        for (size_t i=0; i<kvs.size(); ++i){
            if (!seen[i]){
                lines.push_back(kvs[i].first + kvs[i].second);
            }
        }

        // write back
        std::FILE* wf = std::fopen(GetOverlayConfigPath().c_str(), "w");
        if (wf){
            for (const std::string& l : lines){
                std::fwrite(l.c_str(), 1, l.size(), wf);
                std::fwrite("\n", 1, 1, wf);
            }
            std::fclose(wf);
        }
    }

    static void ensureLoaded(){
        if (g_loaded) return;
        g_loaded = 1;

        std::FILE* f = std::fopen(GetOverlayConfigPath().c_str(), "r");
        if (!f) return;

        char buf[512];
        while (std::fgets(buf, sizeof(buf), f)){
            // strip trailing newline(s)
            int n = (int)std::strlen(buf);
            while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) { buf[--n] = '\0'; }

            // find '='
            char* eq = std::strchr(buf, '=');
            if (!eq) continue;
            *eq = '\0';
            const char* key = buf;
            const char* valstr = eq + 1;
            while (*valstr == ' ' || *valstr == '\t') ++valstr;
            const int val = std::atoi(valstr);

            if      (!std::strcmp(key, "VIGNETTE"))   g_vEnabled  = (val != 0);
            else if (!std::strcmp(key, "V_STRENGTH")) g_vStrength = clamp(val, 0, 5);
            else if (!std::strcmp(key, "V_RADIUS"))   g_vRadius   = clamp(val, 0, 5);
            else if (!std::strcmp(key, "V_SOFTNESS")) g_vSoftness = clamp(val, 0, 5);
            else if (!std::strcmp(key, "V_WARMTH"))   g_vWarmth   = clamp(val, -100, 100);
            else if (!std::strcmp(key, "GRAIN"))      g_grain     = (val != 0);
            else if (!std::strcmp(key, "GRAIN_I") || !std::strcmp(key, "GRAIN_INTENSITY")) g_grainInt = clamp(val, 0, 5);
            else if (!std::strcmp(key, "SCAN"))       g_scan      = (val != 0);
            else if (!std::strcmp(key, "SCAN_I")  || !std::strcmp(key, "SCAN_INTENSITY"))  g_scanInt  = clamp(val, 0, 5);
            else if (!std::strcmp(key, "MUZZLE"))     g_muzzle    = (val != 0);
        }

        std::fclose(f);
    }

    static void SaveOverlays(){
        ensureLoaded();
        std::vector<std::pair<std::string, std::string>> kvs;
        kvs.push_back({"VIGNETTE=",    std::to_string(g_vEnabled)});
        kvs.push_back({"V_STRENGTH=",  std::to_string(g_vStrength)});
        kvs.push_back({"V_RADIUS=",    std::to_string(g_vRadius)});
        kvs.push_back({"V_SOFTNESS=",  std::to_string(g_vSoftness)});
        kvs.push_back({"V_WARMTH=",    std::to_string(g_vWarmth)});
        kvs.push_back({"GRAIN=",       std::to_string(g_grain)});
        kvs.push_back({"GRAIN_I=",     std::to_string(g_grainInt)});
        kvs.push_back({"SCAN=",        std::to_string(g_scan)});
        kvs.push_back({"SCAN_I=",      std::to_string(g_scanInt)});
        kvs.push_back({"MUZZLE=",      std::to_string(g_muzzle)});
        patchKeys(kvs);
    }

    void EffectsConfigInit(){ ensureLoaded(); }
    void EffectsConfigSaveOverlays(){ SaveOverlays(); }

    // ---- Vignette ----
    int  GetVignetteEnabled(){ ensureLoaded(); return g_vEnabled ? 1 : 0; }
    void SetVignetteEnabled(int s){ ensureLoaded(); g_vEnabled = (s!=0); SaveOverlays(); }

    int  GetVignetteStrength(){ ensureLoaded(); return g_vStrength; }
    void SetVignetteStrength(int s){ ensureLoaded(); g_vStrength = clamp(s,0,5); SaveOverlays(); }

    int  GetVignetteRadius(){ ensureLoaded(); return g_vRadius; }
    void SetVignetteRadius(int s){ ensureLoaded(); g_vRadius = clamp(s,0,5); SaveOverlays(); }

    int  GetVignetteSoftness(){ ensureLoaded(); return g_vSoftness; }
    void SetVignetteSoftness(int s){ ensureLoaded(); g_vSoftness = clamp(s,0,5); SaveOverlays(); }

    int  GetVignetteWarmth(){ ensureLoaded(); return g_vWarmth; }
    void SetVignetteWarmth(int s){ ensureLoaded(); g_vWarmth = clamp(s,-100,100); SaveOverlays(); }

    // ---- Film grain ----
    int  GetFilmGrain(){ ensureLoaded(); return g_grain ? 1 : 0; }
    void SetFilmGrain(int s){ ensureLoaded(); g_grain = (s!=0); SaveOverlays(); }

    int  GetFilmGrainIntensity(){ ensureLoaded(); return g_grainInt; }
    void SetFilmGrainIntensity(int s){ ensureLoaded(); g_grainInt = clamp(s,0,5); SaveOverlays(); }

    // ---- Scanlines ----
    int  GetScanlines(){ ensureLoaded(); return g_scan ? 1 : 0; }
    void SetScanlines(int s){ ensureLoaded(); g_scan = (s!=0); SaveOverlays(); }

    int  GetScanlineIntensity(){ ensureLoaded(); return g_scanInt; }
    void SetScanlineIntensity(int s){ ensureLoaded(); g_scanInt = clamp(s,0,5); SaveOverlays(); }

    // ---- Muzzle flash ----
    int  GetMuzzleFlash(){ ensureLoaded(); return g_muzzle ? 1 : 0; }
    void SetMuzzleFlash(int s){ ensureLoaded(); g_muzzle = (s!=0); SaveOverlays(); }

} // namespace Config
