#include "menuscreen.h"
#include "config.h"
#include "ConfigOverlays.h"
#include "MenuAdapters.h"
#include "audio/AtmosphereVolume.h"
#include "cheats/CheatSystem.h"
#include "effects/BlobShadowToggle.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <sdl2/SDL.h>

// ---- Local loader for cheats.txt (no dependency on Cheats::Load) ---------
static bool g_CheatsLoadedOnce = false;

static inline void trim(std::string& s) {
    auto notspace = [](int ch){ return !std::isspace(ch); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notspace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notspace).base(), s.end());
}
static inline std::string upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)std::toupper(c); });
    return s;
}

static void EnsureCheatsLoaded() {
    if (g_CheatsLoadedOnce) return;
    g_CheatsLoadedOnce = true;
    std::ifstream in("cheats.txt");
    if (!in) return;
    std::string line;
    while (std::getline(in, line)) {
        // strip comments
        auto sc = line.find_first_of(";#");
        if (sc != std::string::npos) line = line.substr(0, sc);
        trim(line);
        if (line.empty()) continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = upper(line.substr(0, eq));
        std::string v = upper(line.substr(eq+1));
        trim(k); trim(v);

        auto isOn = [&](const std::string& s)->bool { return (s=="1" || s=="ON" || s=="TRUE"); };
        auto toInt = [&](const std::string& s)->int { try { return std::stoi(s); } catch(...) { return 0; } };

        if (k == "GOD") { Cheats::SetGodMode(isOn(v)); }
        else if (k == "ONEHIT" || k == "ONEHITKILL") { Cheats::SetOneHitKill(isOn(v)); }
        else if (k == "THERMO" || k == "THERMOGOGGLES") { Cheats::SetThermoGoggles(isOn(v)); }
        else if (k == "BOUNCY" || k == "BOUNCYBULLETS") { Cheats::SetBouncyBullets(isOn(v)); }
        else if (k == "INVIS" || k == "INVISIBILITY") { Cheats::SetInvisibility(isOn(v)); }
        else if (k == "STARTWEAPON" || k == "WEAPON") { Cheats::SetStartWeapon(toInt(v)); }
        // silently ignore unknown keys
    }
}

// --- Cheat wrappers (persist on change) ------------------------------------
static int MenuGetCheatGodMode() { EnsureCheatsLoaded(); return Cheats::GetGodMode() ? 1 : 0; }
static void MenuSetCheatGodMode(int on) { Cheats::SetGodMode(on != 0); Cheats::Save(); }

static int MenuGetCheatOneHitKill() { EnsureCheatsLoaded(); return Cheats::GetOneHitKill() ? 1 : 0; }
static void MenuSetCheatOneHitKill(int on) { Cheats::SetOneHitKill(on != 0); Cheats::Save(); }

static int MenuGetCheatThermo() { EnsureCheatsLoaded(); return Cheats::GetThermoGoggles() ? 1 : 0; }
static void MenuSetCheatThermo(int on) { Cheats::SetThermoGoggles(on != 0); Cheats::Save(); }

static int MenuGetCheatBouncy() { EnsureCheatsLoaded(); return Cheats::GetBouncyBullets() ? 1 : 0; }
static void MenuSetCheatBouncy(int on) { Cheats::SetBouncyBullets(on != 0); Cheats::Save(); }

static int MenuGetCheatInvis() { EnsureCheatsLoaded(); return Cheats::GetInvisibility() ? 1 : 0; }
static void MenuSetCheatInvis(int on) { Cheats::SetInvisibility(on != 0); Cheats::Save(); }

// Start weapon: 0..4 fixed, 5 = DEFAULT
static int MenuGetCheatStartWeapon() { EnsureCheatsLoaded(); return Cheats::GetStartWeapon(); }
static void MenuSetCheatStartWeapon(int idx) { Cheats::SetStartWeapon(idx); Cheats::Save(); }


static int MenuGetAtmos() { return EmbeddedBGMVolume::Get(); }
static void MenuSetAtmos(int v) { EmbeddedBGMVolume::Set(v, true); EmbeddedBGMVolume::Save(); }
// ---- Vignette Warmth Bool Mapping (ON=WARM, OFF=COLD) ----------------------
static int MenuGetVignetteWarmthBool() {
    int v = MenuAdapters::GetWarmth01();
    return (v >= 6) ? 1 : 0; // ON -> WARM (1), OFF -> COLD (0)
}
static void MenuSetVignetteWarmthBool(int on) {
    if (on) { MenuAdapters::SetWarmth01(8); } else { MenuAdapters::SetWarmth01(3); }
}


// ---- Blob Shadow toggle glue ----------------------------------------------
static int MenuGetBlobShadows() { return ZGloomPC::gBlobShadows ? 1 : 0; }
static void MenuSetBlobShadows(int on) { ZGloomPC::gBlobShadows = (on != 0); }


// ---- Display aspect glue (4:3 vs 16:9) ------------------------------------
static int MenuGetDisplayAspect() { return Config::GetDisplayAspect(); }
static void MenuSetDisplayAspect(int a) { Config::SetDisplayAspect(a); Config::Save(); }

// ---- Field of View glue (ULTRA/HIGH/MED/LOW) ------------------------------
static int MenuGetFovPreset()
{
    // 0 = ULTRA (128), 1 = HIGH (160), 2 = MED (192), 3 = LOW (224)
    const int kFovVals[4] = { 128, 160, 192, 224 };
    int fl = (int)Config::GetFocalLength();

    int bestIdx  = 0;
    int bestDiff = fl - kFovVals[0];
    if (bestDiff < 0) bestDiff = -bestDiff;

    for (int i = 1; i < 4; ++i)
    {
        int d = fl - kFovVals[i];
        if (d < 0) d = -d;
        if (d < bestDiff)
        {
            bestDiff = d;
            bestIdx  = i;
        }
    }
    return bestIdx;
}

static void MenuSetFovPreset(int v)
{
    if (v < 0) v = 0;
    if (v > 3) v = 3;
    const int kFovVals[4] = { 128, 160, 192, 224 };
    Config::SetFocalLength(kFovVals[v]);
    Config::Save();
}

void MenuScreen::Render(SDL_Surface* src, SDL_Surface* dest, Font& font)
{
    SDL_BlitSurface(src, nullptr, dest, nullptr);
    bool flash = (timer / 5) & 1;

    int scale = dest->h / 256;
    if (scale < 1) scale = 1;

    if (status == MENUSTATUS_MAIN)
    {
        DisplayStandardMenu(mainmenu, flash, scale, dest, font);
    }
    else if (status == MENUSTATUS_SOUNDOPTIONS)
    {
        DisplayStandardMenu(soundmenu, flash, scale, dest, font);
    }
    else if (status == MENUSTATUS_CONTROLOPTIONS)
    {
        DisplayStandardMenu(controlmenu, flash, scale, dest, font);
    }
    else if (status == MENUSTATUS_DISPLAYOPTIONS)
    {
        DisplayStandardMenu(displaymenu, flash, scale, dest, font);
    }
    
    else if (status == MENUSTATUS_EFFECTSOPTIONS)
    {
        static std::vector<MenuEntry> effectsmenu;
        effectsmenu.clear();
        effectsmenu.push_back(MenuEntry("EFFECTS OPTIONS", ACTION_LABEL, 0, nullptr, nullptr));
        effectsmenu.push_back(MenuEntry("RETURN", ACTION_SWITCHMENU, MENUSTATUS_MAIN, nullptr, nullptr));
        effectsmenu.push_back(MenuEntry("BLOOD SIZE: ", ACTION_INT, 5, Config::GetBlood, Config::SetBlood));
        effectsmenu.push_back(MenuEntry("MUZZLE FLASH AND REFLECTION: ", ACTION_BOOL, 0, Config::GetMuzzleFlash, Config::SetMuzzleFlash));
        effectsmenu.push_back(MenuEntry("BLOB SHADOWS: ", ACTION_BOOL, 0, MenuGetBlobShadows, MenuSetBlobShadows));
        effectsmenu.push_back(MenuEntry("ATMOSPHERIC VIGNETTE: ", ACTION_BOOL, 0, Config::GetVignetteEnabled, Config::SetVignetteEnabled));
        effectsmenu.push_back(MenuEntry("VIGNETTE STRENGTH: ", ACTION_INT, 6, Config::GetVignetteStrength, Config::SetVignetteStrength));
        effectsmenu.push_back(MenuEntry("VIGNETTE RADIUS: ", ACTION_INT, 6, Config::GetVignetteRadius, Config::SetVignetteRadius));
        effectsmenu.push_back(MenuEntry("VIGNETTE SOFTNESS: ", ACTION_INT, 6, Config::GetVignetteSoftness, Config::SetVignetteSoftness));
        effectsmenu.push_back(MenuEntry("VIGNETTE WARMTH: ", ACTION_BOOL, 0, MenuGetVignetteWarmthBool, MenuSetVignetteWarmthBool));
        effectsmenu.push_back(MenuEntry("FILM GRAIN: ", ACTION_BOOL, 0, Config::GetFilmGrain, Config::SetFilmGrain));
        effectsmenu.push_back(MenuEntry("FILM GRAIN INTENSITY: ", ACTION_INT, 6, Config::GetFilmGrainIntensity, Config::SetFilmGrainIntensity));
        effectsmenu.push_back(MenuEntry("SCANLINES: ", ACTION_BOOL, 0, Config::GetScanlines, Config::SetScanlines));
        effectsmenu.push_back(MenuEntry("SCANLINE INTENSITY: ", ACTION_INT, 6, Config::GetScanlineIntensity, Config::SetScanlineIntensity));
        DisplayStandardMenu(effectsmenu, flash, scale, dest, font);
    }
    else if (status == MENUSTATUS_CHEATOPTIONS)
    {
        static std::vector<MenuEntry> cheatmenu;
        cheatmenu.clear();
        cheatmenu.push_back(MenuEntry("CHEAT OPTIONS", ACTION_LABEL, 0, nullptr, nullptr));
        cheatmenu.push_back(MenuEntry("RETURN", ACTION_SWITCHMENU, MENUSTATUS_MAIN, nullptr, nullptr));
        cheatmenu.push_back(MenuEntry("GOD MODE: ", ACTION_BOOL, 0, MenuGetCheatGodMode, MenuSetCheatGodMode));
        cheatmenu.push_back(MenuEntry("ONE HIT KILL: ", ACTION_BOOL, 0, MenuGetCheatOneHitKill, MenuSetCheatOneHitKill));
        cheatmenu.push_back(MenuEntry("BOUNCY BULLETS: ", ACTION_BOOL, 0, MenuGetCheatBouncy, MenuSetCheatBouncy));
        cheatmenu.push_back(MenuEntry("THERMO GOGGLES: ", ACTION_BOOL, 0, MenuGetCheatThermo, MenuSetCheatThermo));
        cheatmenu.push_back(MenuEntry("INVISIBILITY: ", ACTION_BOOL, 0, MenuGetCheatInvis, MenuSetCheatInvis));
        cheatmenu.push_back(MenuEntry("WEAPON: ", ACTION_INT, 6, MenuGetCheatStartWeapon, MenuSetCheatStartWeapon));
        DisplayStandardMenu(cheatmenu, flash, scale, dest, font);
    }
    else if (status == MENUSTATUS_KEYCONFIG)
    {
        switch (selection)
        {
            case Config::KEY_UP:
                font.PrintMessage("PRESS KEY FOR FORWARD", 120 * scale, dest, scale);
                break;
            case Config::KEY_DOWN:
                font.PrintMessage("PRESS KEY FOR BACK", 120 * scale, dest, scale);
                break;
            case Config::KEY_LEFT:
                font.PrintMessage("PRESS KEY FOR ROTATE LEFT", 120 * scale, dest, scale);
                break;
            case Config::KEY_RIGHT:
                font.PrintMessage("PRESS KEY FOR ROTATE RIGHT", 120 * scale, dest, scale);
                break;
            case Config::KEY_SLEFT:
                font.PrintMessage("PRESS KEY FOR STRAFE LEFT", 120 * scale, dest, scale);
                break;
            case Config::KEY_SRIGHT:
                font.PrintMessage("PRESS KEY FOR STRAFE RIGHT", 120 * scale, dest, scale);
                break;
            case Config::KEY_STRAFEMOD:
                font.PrintMessage("PRESS KEY FOR STRAFE MODIFIER", 120 * scale, dest, scale);
                break;
            case Config::KEY_RUN:
                font.PrintMessage("PRESS KEY FOR RUN", 120 * scale, dest, scale);
                break;
            case Config::KEY_SHOOT:
                font.PrintMessage("PRESS KEY FOR SHOOT", 120 * scale, dest, scale);
                break;
        }
    }
}

MenuScreen::MenuScreen()
{
    status    = MENUSTATUS_MAIN;
    selection = 1;    // first selectable entry (skip label)
    timer     = 0;

    // MAIN
    mainmenu.push_back(MenuEntry("MAIN MENU", ACTION_LABEL, 0, nullptr, nullptr));
    mainmenu.push_back(MenuEntry("CONTINUE", ACTION_RETURN, MENURET_PLAY, nullptr, nullptr));
    mainmenu.push_back(MenuEntry("CONTROL OPTIONS", ACTION_SWITCHMENU, MENUSTATUS_CONTROLOPTIONS, nullptr, nullptr));
    mainmenu.push_back(MenuEntry("SOUND OPTIONS", ACTION_SWITCHMENU, MENUSTATUS_SOUNDOPTIONS, nullptr, nullptr));
    mainmenu.push_back(MenuEntry("DISPLAY OPTIONS", ACTION_SWITCHMENU, MENUSTATUS_DISPLAYOPTIONS, nullptr, nullptr));
    mainmenu.push_back(MenuEntry("EFFECTS OPTIONS", ACTION_SWITCHMENU, MENUSTATUS_EFFECTSOPTIONS, nullptr, nullptr));
    mainmenu.push_back(MenuEntry("CHEAT OPTIONS", ACTION_SWITCHMENU, MENUSTATUS_CHEATOPTIONS, nullptr, nullptr));
    mainmenu.push_back(MenuEntry("QUIT TO TITLE", ACTION_RETURN, MENURET_QUIT, nullptr, nullptr));

    // SOUND
    soundmenu.push_back(MenuEntry("SOUND OPTIONS", ACTION_LABEL, 0, nullptr, nullptr));
    soundmenu.push_back(MenuEntry("RETURN", ACTION_SWITCHMENU, MENUSTATUS_MAIN, nullptr, nullptr));
    soundmenu.push_back(MenuEntry("SFX VOLUME: ", ACTION_INT, 10, Config::GetSFXVol, Config::SetSFXVol));
    soundmenu.push_back(MenuEntry("MUSIC VOLUME: ", ACTION_INT, 10, Config::GetMusicVol, Config::SetMusicVol));

    soundmenu.push_back(MenuEntry("ATMOSPHERE VOLUME: ", ACTION_INT, 10, MenuGetAtmos, MenuSetAtmos));
    // CONTROL
    controlmenu.push_back(MenuEntry("CONTROL OPTIONS", ACTION_LABEL, 0, nullptr, nullptr));
    controlmenu.push_back(MenuEntry("RETURN", ACTION_SWITCHMENU, MENUSTATUS_MAIN, nullptr, nullptr));
    controlmenu.push_back(MenuEntry("AUTOFIRE: ", ACTION_BOOL, 0, Config::GetAutoFire, Config::SetAutoFire));
    controlmenu.push_back(MenuEntry("CONFIGURE KEYS", ACTION_SWITCHMENU, MENUSTATUS_KEYCONFIG, nullptr, nullptr));
    controlmenu.push_back(MenuEntry("MOUSE SENSITIVITY: ", ACTION_INT, 10, Config::GetMouseSens, Config::SetMouseSens));

    // DISPLAY
    displaymenu.push_back(MenuEntry("DISPLAY OPTIONS", ACTION_LABEL, 0, nullptr, nullptr));
    displaymenu.push_back(MenuEntry("RETURN", ACTION_SWITCHMENU, MENUSTATUS_MAIN, nullptr, nullptr));
        displaymenu.push_back(MenuEntry("DISPLAY ASPECT: ", ACTION_INT, 2, MenuGetDisplayAspect, MenuSetDisplayAspect));
    displaymenu.push_back(MenuEntry("FIELD OF VIEW: ", ACTION_INT, 4, MenuGetFovPreset, MenuSetFovPreset));
}

void MenuScreen::HandleKeyMenu(SDL_Keycode sym)
{
    Config::SetKey((Config::keyenum)selection, SDL_GetScancodeFromKey(sym));
    selection++;
    if (selection == Config::KEY_END)
    {
        selection = 1;            // back to first selectable main entry
        status    = MENUSTATUS_MAIN;
    }
}

MenuScreen::MenuReturn MenuScreen::HandleStandardMenu(SDL_Keycode sym, std::vector<MenuEntry>& menu)
{
    auto isLabel = [&](int idx) -> bool {
        return (idx >= 0 && idx < (int)menu.size() && menu[idx].action == ACTION_LABEL);
    };

    switch (sym)
    {
        case SDLK_DOWN:
            // move down, skipping labels
            do {
                selection++;
                if (selection >= (int)menu.size())
                    selection = 0;
            } while (isLabel(selection));
            break;

        case SDLK_UP:
            // move up, skipping labels
            do {
                selection--;
                if (selection < 0)
                    selection = (int)menu.size() - 1;
            } while (isLabel(selection));
            break;

        case SDLK_ESCAPE:
            // ESC: in main menu -> back to game, otherwise back to MAIN
            if (status == MENUSTATUS_MAIN)
            {
                return MENURET_PLAY;
            }
            else
            {
                status    = MENUSTATUS_MAIN;
                selection = 1; // first selectable in main menu
            }
            break;

        // RIGHT / ENTER / SPACE / CTRL -> activate
        case SDLK_RIGHT:
        case SDLK_SPACE:
        case SDLK_RETURN:
        case SDLK_LCTRL:
        {
            if (isLabel(selection))
                break; // labels are not actionable

            MenuEntry& e = menu[selection];
            switch (e.action)
            {
                case ACTION_BOOL:
                {
                    e.setval(!e.getval());
                    break;
                }
                case ACTION_INT:
                {
                    int x = e.getval() + 1;
                    if (x >= e.arg)
                        x = 0;
                    e.setval(x);
                    break;
                }
                case ACTION_SWITCHMENU:
                {
                    status = (MENUSTATUS)e.arg;
                    if (status == MENUSTATUS_KEYCONFIG)
                        selection = 0;
                    else
                        selection = 1; // skip label in new menu
                    break;
                }
                case ACTION_RETURN:
                {
                    return (MenuReturn)e.arg;
                }
                default:
                    break;
            }
            break;
        }

        // LEFT -> previous value for INT/BOOL
        case SDLK_LEFT:
        {
            if (isLabel(selection))
                break;

            MenuEntry& e = menu[selection];
            if (e.action == ACTION_INT)
            {
                int x = e.getval() - 1;
                if (x < 0)
                    x = e.arg - 1;
                e.setval(x);
            }
            else if (e.action == ACTION_BOOL)
            {
                e.setval(!e.getval());
            }
            break;
        }

        default:
            break;
    }

    return MENURET_NOTHING;
}

MenuScreen::MenuReturn MenuScreen::Update(SDL_Event& tevent)
{
    if (tevent.type == SDL_KEYDOWN)
    {
        switch (status)
        {
            case MENUSTATUS_MAIN:
            {
                return HandleStandardMenu(tevent.key.keysym.sym, mainmenu);
                break;
            }
            case MENUSTATUS_KEYCONFIG:
            {
                HandleKeyMenu(tevent.key.keysym.sym);
                break;
            }

            case MENUSTATUS_SOUNDOPTIONS:
            {
                HandleStandardMenu(tevent.key.keysym.sym, soundmenu);
                break;
            }

            case MENUSTATUS_CONTROLOPTIONS:
            {
                HandleStandardMenu(tevent.key.keysym.sym, controlmenu);
                break;
            }

            case MENUSTATUS_DISPLAYOPTIONS:
            {
                HandleStandardMenu(tevent.key.keysym.sym, displaymenu);
                break;
            }
            
            case MENUSTATUS_EFFECTSOPTIONS:
            {
                static std::vector<MenuEntry> effectsmenu;
                effectsmenu.clear();
                effectsmenu.push_back(MenuEntry("EFFECTS OPTIONS", ACTION_LABEL, 0, nullptr, nullptr));
                effectsmenu.push_back(MenuEntry("RETURN", ACTION_SWITCHMENU, MENUSTATUS_MAIN, nullptr, nullptr));
                effectsmenu.push_back(MenuEntry("BLOOD SIZE: ", ACTION_INT, 5, Config::GetBlood, Config::SetBlood));
                effectsmenu.push_back(MenuEntry("MUZZLE FLASH AND REFLECTION: ", ACTION_BOOL, 0, Config::GetMuzzleFlash, Config::SetMuzzleFlash));
                effectsmenu.push_back(MenuEntry("BLOB SHADOWS: ", ACTION_BOOL, 0, MenuGetBlobShadows, MenuSetBlobShadows));
                // Halbzeile //
                effectsmenu.push_back(MenuEntry("ATMOSPHERIC VIGNETTE: ", ACTION_BOOL, 0, Config::GetVignetteEnabled, Config::SetVignetteEnabled));
                effectsmenu.push_back(MenuEntry("VIGNETTE STRENGTH: ", ACTION_INT, 6, Config::GetVignetteStrength, Config::SetVignetteStrength));
                effectsmenu.push_back(MenuEntry("VIGNETTE RADIUS: ", ACTION_INT, 6, Config::GetVignetteRadius, Config::SetVignetteRadius));
                effectsmenu.push_back(MenuEntry("VIGNETTE SOFTNESS: ", ACTION_INT, 6, Config::GetVignetteSoftness, Config::SetVignetteSoftness));
                effectsmenu.push_back(MenuEntry("VIGNETTE WARMTH: ", ACTION_BOOL, 0, MenuGetVignetteWarmthBool, MenuSetVignetteWarmthBool));
                // Halbzeile //
                effectsmenu.push_back(MenuEntry("FILM GRAIN: ", ACTION_BOOL, 0, Config::GetFilmGrain, Config::SetFilmGrain));
                effectsmenu.push_back(MenuEntry("FILM GRAIN INTENSITY: ", ACTION_INT, 6, Config::GetFilmGrainIntensity, Config::SetFilmGrainIntensity));
                // Halbzeile //
                effectsmenu.push_back(MenuEntry("SCANLINES: ", ACTION_BOOL, 0, Config::GetScanlines, Config::SetScanlines));
                effectsmenu.push_back(MenuEntry("SCANLINE INTENSITY: ", ACTION_INT, 6, Config::GetScanlineIntensity, Config::SetScanlineIntensity));
                HandleStandardMenu(tevent.key.keysym.sym, effectsmenu);
                break;
            }
            case MENUSTATUS_CHEATOPTIONS:
            {
                static std::vector<MenuEntry> cheatmenu;
                cheatmenu.clear();
                cheatmenu.push_back(MenuEntry("CHEAT OPTIONS", ACTION_LABEL, 0, nullptr, nullptr));
                cheatmenu.push_back(MenuEntry("RETURN", ACTION_SWITCHMENU, MENUSTATUS_MAIN, nullptr, nullptr));
                cheatmenu.push_back(MenuEntry("GOD MODE: ", ACTION_BOOL, 0, MenuGetCheatGodMode, MenuSetCheatGodMode));
                cheatmenu.push_back(MenuEntry("ONE HIT KILL: ", ACTION_BOOL, 0, MenuGetCheatOneHitKill, MenuSetCheatOneHitKill));
                cheatmenu.push_back(MenuEntry("BOUNCY BULLETS: ", ACTION_BOOL, 0, MenuGetCheatBouncy, MenuSetCheatBouncy));
                cheatmenu.push_back(MenuEntry("THERMO GOGGLES: ", ACTION_BOOL, 0, MenuGetCheatThermo, MenuSetCheatThermo));
                cheatmenu.push_back(MenuEntry("INVISIBILITY: ", ACTION_BOOL, 0, MenuGetCheatInvis, MenuSetCheatInvis));
                cheatmenu.push_back(MenuEntry("WEAPON: ", ACTION_INT, 6, MenuGetCheatStartWeapon, MenuSetCheatStartWeapon));
                HandleStandardMenu(tevent.key.keysym.sym, cheatmenu);
                break;
            }

            default:
                break;
        }
    }

    return MENURET_NOTHING;
}

void MenuScreen::DisplayStandardMenu(std::vector<MenuEntry>& menu, bool flash, int scale, SDL_Surface* dest, Font& font)
{
    int starty = 40 * scale;
    int yinc   = 10 * scale;

    bool firstNonLabelPrinted = false;

    for (size_t i = 0; i < menu.size(); i++)
    {
        MenuEntry& e = menu[i];

        // Non-selectable heading with darker, more transparent yellow background
        if (e.action == ACTION_LABEL)
        {
            const int barChars  = 30;
            const int charWidth = 8;

            int barW = barChars * charWidth * scale;
            if (barW > dest->w)
                barW = dest->w;

            // Höhe: um oben und unten ca. 1/4 Zeile vergrößert (insgesamt +1/2 Zeile)
            int barH = yinc + (yinc / 2);

            // Mittelpunkt leicht nach oben verschoben (ca. 1/8 Zeile),
            // damit der Balken mittiger hinter der Überschrift sitzt.
            int centerY = starty + (yinc / 2) - (yinc / 8);

            SDL_Rect r;
            r.w = barW;
            r.h = barH;
            r.x = (dest->w - barW) / 2;
            r.y = centerY - (barH / 2);

            SDL_Surface* bar = SDL_CreateRGBSurfaceWithFormat(0, r.w, r.h, 32, SDL_PIXELFORMAT_RGBA8888);
            if (bar)
            {
                // Farbe 65,65,0 mit ~65% Transparenz
                SDL_FillRect(bar, nullptr, SDL_MapRGBA(bar->format, 65, 65, 0, 166));
                SDL_SetSurfaceBlendMode(bar, SDL_BLENDMODE_BLEND);
                SDL_BlitSurface(bar, nullptr, dest, &r);
                SDL_FreeSurface(bar);
            }

            // Heading text (immer sichtbar, kein Blinken)
            font.PrintMessage(e.name, starty, dest, scale);

            // 1 volle Leerzeile Abstand vor CONTINUE/RETURN usw.
            starty += yinc * 2;
            continue;
        }

        // Restore old half-line spacing before certain entries
        if (e.name == "QUIT TO TITLE" ||
            e.name == "ATMOSPHERIC VIGNETTE: " ||
            e.name == "FILM GRAIN: " ||
            e.name == "SCANLINES: ")
        {
            starty += yinc / 2;
        }

        std::string menustring = e.name;

        if (e.action == ACTION_INT)
        {
            if (menustring.rfind("DISPLAY ASPECT", 0) == 0)
            {
                int v = e.getval();
                if (v <= 0)
                    menustring += "4:3";
                else
                    menustring += "16:9";
            }
            else if (menustring.rfind("FIELD OF VIEW", 0) == 0)
            {
                static const char* kFovNames[4] = {
                    "ULTRA",
                    "HIGH",
                    "MED",
                    "LOW"
                };
                int v = e.getval();
                if (v < 0) v = 0;
                if (v > 3) v = 3;
                menustring += kFovNames[v];
            }
            else if (menustring.rfind("WEAPON", 0) == 0)
            {
                static const char* kWeaponNames[6] = {
                    "PLASMA CANNON",
                    "BLASTER UPGRADE",
                    "BOOSTER UPGRADE",
                    "ION UPGRADE",
                    "PHOTON UPGRADE",
                    "GAME DEFAULT"
                };
                int v = e.getval();
                if (v < 0) v = 0;
                if (v > 5) v = 5;
                menustring += kWeaponNames[v];
            }
            else
            {
                menustring += std::to_string(e.getval());
            }
        }
        else if (e.action == ACTION_BOOL)
        {
            if (menustring.rfind("VIGNETTE WARMTH", 0) == 0)
            {
                menustring += e.getval() ? "WARM" : "COLD";
            }
            else
            {
                menustring += e.getval() ? "ON" : "OFF";
            }
        }
        
        // Draw entry (respect flashing for current selection)
        if (flash || (selection != (int)i))
            font.PrintMessage(menustring, starty, dest, scale);

        // First non-label entry (CONTINUE/RETURN) gets double spacing below,
        // all further entries get normal spacing; this matches den alten Abstand.
        if (!firstNonLabelPrinted)
        {
            firstNonLabelPrinted = true;
            starty += yinc * 2;
        }
        else
        {
            starty += yinc;
        }
    }
}
