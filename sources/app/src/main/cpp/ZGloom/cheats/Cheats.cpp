#include "cheats/Cheats.h"

#include <cstdio>
#include <string>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace
{
    std::string s_path = "cheats.txt";

    bool s_loaded = false;

    bool s_god = false;
    bool s_onehit = false;
    int  s_startWeapon = 5; // DEFAULT
    bool s_thermo = false;
    bool s_bouncy = false;
    bool s_invis  = false;

    inline std::string trim(const std::string& s) {
        size_t a = 0, b = s.size();
        while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
        while (b > a && std::isspace(static_cast<unsigned char>(s[b-1]))) --b;
        return s.substr(a, b - a);
    }

    inline std::string upper(const std::string& s) {
        std::string t = s;
        std::transform(t.begin(), t.end(), t.begin(),
            [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
        return t;
    }

    inline int clamp01(int v) { return v <= 0 ? 0 : (v >= 1 ? 1 : v); }
    inline int clampWeapon(int v) { return (v < 0) ? 0 : (v > 5 ? 5 : v); }

    inline bool parse_bool(const std::string& v, bool defVal)
    {
        std::string t = upper(trim(v));
        if (t == "1" || t == "TRUE" || t == "ON" || t == "YES") return true;
        if (t == "0" || t == "FALSE" || t == "OFF" || t == "NO") return false;
        return defVal;
    }

    static void kv_set(const std::string& k, const std::string& v)
    {
        std::string key = upper(trim(k));
        std::string val = trim(v);

        if (key == "GOD" || key == "GODMODE") {
            s_god = parse_bool(val, false);
        } else if (key == "ONEHIT" || key == "ONEHITKILL") {
            s_onehit = parse_bool(val, false);
        } else if (key == "STARTWEAPON" || key == "WEAPON") {
            s_startWeapon = clampWeapon(std::atoi(val.c_str()));
        } else if (key == "THERMO" || key == "THERMO_GOGGLES" || key == "THERMOGOGGLES") {
            s_thermo = parse_bool(val, false);
        } else if (key == "BOUNCY" || key == "BOUNCY_BULLETS" || key == "BOUNCYBULLETS") {
            s_bouncy = parse_bool(val, false);
        } else if (key == "INVIS" || key == "INVISIBLE" || key == "INVISIBILITY") {
            s_invis = parse_bool(val, false);
        }
    }

    static void loadKV()
    {
        std::FILE* f = std::fopen(s_path.c_str(), "rb");
        if (!f) { s_loaded = true; return; }

        char linebuf[1024];
        while (std::fgets(linebuf, sizeof(linebuf), f))
        {
            std::string s(linebuf);

            // strip comments ; or #
            size_t cpos = s.find_first_of(";#");
            if (cpos != std::string::npos) s.erase(cpos);

            // trim outer whitespace
            s = trim(s);
            if (s.empty()) continue;

            // support KEY=VAL / KEY:VAL and also KEY VAL
            std::string k, v;
            size_t p = s.find_first_of("=:");
            if (p == std::string::npos) {
                // split by first whitespace
                size_t sp = s.find_first_of(" \t");
                if (sp == std::string::npos) continue;
                k = trim(s.substr(0, sp));
                v = trim(s.substr(sp + 1));
            } else {
                k = trim(s.substr(0, p));
                v = trim(s.substr(p + 1));
            }
            if (!k.empty()) kv_set(k, v);
        }
        std::fclose(f);
        s_loaded = true;
    }

    static void ensureLoaded()
    {
        if (!s_loaded) {
            loadKV();
            // clamp after load for safety
            s_startWeapon = clampWeapon(s_startWeapon);
            s_god   = !!s_god;
            s_onehit= !!s_onehit;
            s_thermo= !!s_thermo;
            s_bouncy= !!s_bouncy;
            s_invis = !!s_invis;
        }
    }

    static bool saveKV()
    {
        std::FILE* f = std::fopen(s_path.c_str(), "wb");
        if (!f) return false;

        // Header as requested
        std::fputs(";ZGloom Cheats Configuration\n\n", f);

        auto put = [&](const char* k, int v){ std::fprintf(f, "%s %d\n", k, v); };

        put("godmode",       s_god ? 1 : 0);
        put("onehitkill",    s_onehit ? 1 : 0);
        put("weapon",        s_startWeapon);
        put("thermogoggles", s_thermo ? 1 : 0);
        put("bouncybullets", s_bouncy ? 1 : 0);
        put("invisible",     s_invis ? 1 : 0);

        std::fclose(f);
        return true;
    }
} // anon

namespace Cheats
{
    void Load()
    {
        // explicit load for callers that do it on startup
        loadKV();
    }

    bool Save()
    {
        ensureLoaded();
        return saveKV();
    }

    bool GetGodMode()                    { ensureLoaded(); return s_god; }
    void SetGodMode(bool on)             { ensureLoaded(); s_god = on; }

    bool GetOneHitKill()                 { ensureLoaded(); return s_onehit; }
    void SetOneHitKill(bool on)          { ensureLoaded(); s_onehit = on; }

    int  GetStartWeapon()                { ensureLoaded(); return s_startWeapon; }
    void SetStartWeapon(int idx)         { ensureLoaded(); s_startWeapon = clampWeapon(idx); }

    bool GetThermoGoggles()              { ensureLoaded(); return s_thermo; }
    void SetThermoGoggles(bool on)       { ensureLoaded(); s_thermo = on; }

    bool GetBouncyBullets()              { ensureLoaded(); return s_bouncy; }
    void SetBouncyBullets(bool on)       { ensureLoaded(); s_bouncy = on; }

    bool GetInvisibility()               { ensureLoaded(); return s_invis; }
    void SetInvisibility(bool on)        { ensureLoaded(); s_invis = on; }

    // Compatibility helpers
    int AmplifyPlayerOutgoingDamage(int baseDamage)
    {
        ensureLoaded();
        if (s_onehit) {
            const int maxd = 32767;
            return maxd;
        }
        return baseDamage;
    }

    int FilterDamageToPlayer(int incomingDamage)
    {
        ensureLoaded();
        if (s_god) return 0;
        return incomingDamage;
    }

    int GetCheatReloadForWeapon(int /*weaponIdx*/, int defaultReload)
    {
        ensureLoaded();
        return defaultReload;
    }
}
