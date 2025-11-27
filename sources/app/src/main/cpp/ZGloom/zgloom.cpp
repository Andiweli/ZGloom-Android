// zgloom.cpp : Defines the entry point for the console application.
//

#include <sdl2/SDL.h>
#include <sdl2/SDL_mixer.h>
#ifdef __ANDROID__
#include <sdl2/SDL_system.h>
#include <unistd.h>
#include <errno.h>
#endif
#include "xmp/include/xmp.h"


// Global XMP context to avoid scope issues across code paths
static xmp_context g_xmp = nullptr;
#include "config.h"
#include "gloommap.h"
#include "script.h"
#include "crmfile.h"
#include "iffhandler.h"
#include "renderer.h"
#include "effects/RendererHooks.h"
#include "audio/EmbeddedBGM.h"
#include "audio/AtmosphereVolume.h"
#include "objectgraphics.h"
#include <iostream>
#include "SaveSystem.h"
#include "EventReplay.h"

#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <cstring>

#ifndef _WIN32
#include <dirent.h>
#include <sys/stat.h>
#endif

#include "assets/launcher_bg_4_3_embed.h"
#include "assets/launcher_bg_16_9_embed.h"

#include "gamelogic.h"
#include "soundhandler.h"
#include "font.h"
#include "titlescreen.h"
#include "menuscreen.h"
#include "hud.h"
#include "effects/MuzzleFlashFX.h"

#ifdef __ANDROID__
static void ConfigureAndroidDataRoot()
{
    // Preferred: app-specific external files dir used by Java installer:
    //   getExternalFilesDir(null)/ZGloom
    const char* ext = SDL_AndroidGetExternalStoragePath();
    if (ext && *ext) {
        std::string root = std::string(ext);
        if (!root.empty() && root.back() != '/' && root.back() != '\\') {
            root += "/";
        }
        root += "ZGloom/";

        Config::SetDataRoot(root);
        SDL_Log("ZGloom: Android DataRoot set to '%s'", root.c_str());

        if (chdir(root.c_str()) != 0) {
            SDL_Log("ZGloom: chdir('%s') failed (errno=%d)", root.c_str(), errno);
        } else {
            SDL_Log("ZGloom: chdir to DataRoot OK");
        }
    } else {
        SDL_Log("ZGloom: SDL_AndroidGetExternalStoragePath() returned null; keeping default DataRoot");
    }
}
#endif



// ==================== ZHUD NO-INCLUDE GLUE BLOCK (drop-in) ====================
// Paste this block *after* your existing #includes in zgloom.cpp (no extra headers).
// It provides hudTex/hudLayer32 + helpers, and forward-declares the RendererHooks
// functions so you don't need to include any new headers here.

// (removed) // #include <sdl2/SDL.h>

// RendererHooks forward declarations removed (using included header)
// Global HUD resources (stay internal to this TU)
static SDL_Texture* g_ZHudTex = nullptr;
static SDL_Surface* g_ZHudLayer32 = nullptr;

// Create HUD texture/surface with ARGB8888 if missing
static inline void ZHUD_EnsureCreated(SDL_Renderer* ren, int w, int h) {
    if (!g_ZHudTex) {
        g_ZHudTex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
        SDL_SetTextureBlendMode(g_ZHudTex, SDL_BLENDMODE_BLEND);
    }
    if (!g_ZHudLayer32) {
        g_ZHudLayer32 = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    }
}

// Transparent clear at frame begin
static inline void ZHUD_Clear() {
    if (g_ZHudLayer32) SDL_FillRect(g_ZHudLayer32, nullptr, 0x00000000);
}

// Submit HUD for this frame: upload & register for on-top composition
static inline void ZHUD_Submit(SDL_Renderer* ren) {
    if (!g_ZHudTex || !g_ZHudLayer32) return;
    SDL_UpdateTexture(g_ZHudTex, nullptr, g_ZHudLayer32->pixels, g_ZHudLayer32->pitch);
    RendererHooks::SetHudTexture(g_ZHudTex);
}

// Optional cleanup
static inline void ZHUD_Destroy() {
    if (g_ZHudLayer32) { SDL_FreeSurface(g_ZHudLayer32); g_ZHudLayer32 = nullptr; }
    if (g_ZHudTex) { SDL_DestroyTexture(g_ZHudTex); g_ZHudTex = nullptr; }
}

// Ensure HUD resources are valid for this frame (handle context loss / size change)
static inline void ZHUD_EnsureAlive(SDL_Renderer* ren, int w, int h) {
    // If nothing yet, just create fresh resources
    if (!g_ZHudTex || !g_ZHudLayer32) {
        ZHUD_EnsureCreated(ren, w, h);
        return;
    }

    // Verify texture is still valid and matches expected size
    Uint32 fmt = 0;
    int access = 0, tw = 0, th = 0;
    if (SDL_QueryTexture(g_ZHudTex, &fmt, &access, &tw, &th) != 0 || tw != w || th != h) {
        // Renderer may have been reset or size changed; recreate HUD resources
        ZHUD_Destroy();
        ZHUD_EnsureCreated(ren, w, h);
    }
}

// Convenience accessors if your code used 'hudTex' / 'hudLayer32' names before:
#define hudTex      g_ZHudTex
#define hudLayer32  g_ZHudLayer32

// ================== END ZHUD NO-INCLUDE GLUE BLOCK (drop-in) ===================


Uint32 my_callbackfunc(Uint32 interval, void *param)
{
	SDL_Event event;
	SDL_UserEvent userevent;

	/* In this example, our callback pushes an SDL_USEREVENT event
	into the queue, and causes our callback to be called again at the
	same interval: */

	userevent.type = SDL_USEREVENT;
	userevent.code = 0;
	userevent.data1 = NULL;
	userevent.data2 = NULL;

	event.type = SDL_USEREVENT;
	event.user = userevent;

	SDL_PushEvent(&event);
	return(interval);
}

static void fill_audio(void *udata, Uint8 *stream, int len)
{
	auto res = xmp_play_buffer((xmp_context)udata, stream, len, 0);
}

void LoadPic(std::string name, SDL_Surface* render8)
{
	std::vector<uint8_t> pic;
	CrmFile picfile;
	CrmFile palfile;

	bool ok_pic = picfile.Load(name.c_str());
	bool ok_pal = palfile.Load((name + ".pal").c_str());

	SDL_FillRect(render8, nullptr, 0);

	if (!ok_pic || !ok_pal || picfile.data == nullptr || palfile.data == nullptr || palfile.size == 0)
	{
		SDL_Log("ZGloom: LoadPic('%s') failed (missing .crm or .pal), leaving surface black", name.c_str());
		return;
	}

	// is this some sort of weird AGA/ECS backwards compatible palette encoding? 4 MSBs, then LSBs?
	// Update: Yes, yes it is. 
	for (uint32_t c = 0; c < palfile.size / 4; c++)
	{
		SDL_Color col;
		col.a = 0xFF;
		col.r = palfile.data[c * 4 + 0] & 0xf;
		col.g = palfile.data[c * 4 + 1] >> 4;
		col.b = palfile.data[c * 4 + 1] & 0xF;

		col.r <<= 4;
		col.g <<= 4;
		col.b <<= 4;

		col.r |= palfile.data[c * 4 + 2] & 0xf;
		col.g |= palfile.data[c * 4 + 3] >> 4;
		col.b |= palfile.data[c * 4 + 3] & 0xF;

		SDL_SetPaletteColors(render8->format->palette, &col, c, 1);
	}

	uint32_t width = 0;

	IffHandler::DecodeIff(picfile.data, pic, width);

	if (width == render8->w)
	{
		if (pic.size() > (size_t)(render8->w * render8->h))
		{
			pic.resize(render8->w * render8->h);
		}
		std::copy(pic.begin(), pic.begin() + pic.size(), (uint8_t*)(render8->pixels));
	}
	else
	{
		// gloom 3 has some odd-sized intermission pictures. Do a line-by-line copy.

		uint32_t p = 0;
		uint32_t y = 0;

		if (pic.size() > (width * render8->h))
		{
			pic.resize(width * render8->h);
		}

		while (p < pic.size())
		{
			std::copy(pic.begin() + p, pic.begin() + p + render8->w, (uint8_t*)(render8->pixels) + y*render8->pitch);

			p += width;
			y++;
		}
	}
}

enum GameState
{
	STATE_PLAYING,
	STATE_PARSING,
	STATE_SPOOLING,
	STATE_WAITING,
	STATE_MENU,
	STATE_TITLE
};


bool g_RequestSavePosition = false;
bool g_RequestTitleContinue = false;


// ------------------------- Multi-game launcher support -------------------------

struct GameInstall
{
    std::string baseDir; // "" = current folder, otherwise subdirectory
    std::string label;   // human-readable name (e.g., "Gloom Deluxe")
    bool isZM;           // true if pure Zombie Massacre layout (stuf/stages without misc/script)
};

static inline bool GL_FileExistsIn(const std::string& baseDir, const char* relPath)
{
    std::string full;
    if (!baseDir.empty())
    {
        full = baseDir;
        if (!full.empty() && full.back() != '/' && full.back() != '\\')
            full.push_back('/');
        full += relPath;
    }
    else
    {
        full = relPath;
    }

    FILE* f = fopen(full.c_str(), "rb");
    if (f)
    {
        fclose(f);
        return true;
    }
    return false;
}

static std::string GL_ToLower(const std::string& sIn)
{
    std::string s = sIn;
    for (size_t i = 0; i < s.size(); ++i)
        s[i] = (char)std::tolower((unsigned char)s[i]);
    return s;
}

static std::string GL_TitleCaseFromDir(const std::string& dirName)
{
    std::string s = dirName;
    bool newWord = true;
    for (size_t i = 0; i < s.size(); ++i)
    {
        unsigned char c = (unsigned char)s[i];
        if (std::isspace(c) || c == '_' || c == '-' || c == '.')
        {
            if (c == '_' || c == '-' || c == '.')
                s[i] = ' ';
            newWord = true;
        }
        else
        {
            if (newWord)
                s[i] = (char)std::toupper(c);
            else
                s[i] = (char)std::tolower(c);
            newWord = false;
        }
    }
    return s;
}

static std::string GL_MakeInstallLabel(const std::string& dirName, bool hasGloom, bool hasZM)
{
    // Current folder: prefer semantic names
    if (dirName.empty())
    {
        if (hasZM && !hasGloom)  return "Zombie Massacre";
        if (hasGloom && !hasZM)  return "Gloom";
        if (hasGloom && hasZM)   return "Gloom / Zombie Massacre";
        return "Current folder";
    }

    std::string lower = GL_ToLower(dirName);

    // Explicit special cases
    if (lower.find("8bitkiller") != std::string::npos || lower.find("8bit_killer") != std::string::npos)
        return "8Bit Killer";

    if (lower.find("deathmask") != std::string::npos || lower.find("death_mask") != std::string::npos)
        return "Death Mask";

    if (hasZM && !hasGloom)
        return "Zombie Massacre";

    if (lower.find("gloom3") != std::string::npos || lower.find("gloom 3") != std::string::npos || lower.find("gloom_3") != std::string::npos)
        return "Gloom 3";

    if (lower.find("deluxe") != std::string::npos)
        return "Gloom Deluxe";

    if (lower.find("classic") != std::string::npos)
        return "Gloom Classic";

    if (lower.find("gloom") != std::string::npos)
        return "Gloom";

    if (lower.find("zombie") != std::string::npos || lower.find("massacre") != std::string::npos)
        return "Zombie Massacre";

    if (hasGloom)
        return GL_TitleCaseFromDir(dirName);

    if (hasZM)
        return "Zombie Massacre";

    return GL_TitleCaseFromDir(dirName);
}

static void GL_TryAddInstall(const std::string& baseDir, std::vector<GameInstall>& out)
{
    bool hasGloom = GL_FileExistsIn(baseDir, "misc/script");
    bool hasZM    = GL_FileExistsIn(baseDir, "stuf/stages");

    if (!hasGloom && !hasZM)
        return;

    GameInstall gi;
    gi.baseDir = baseDir;
    gi.isZM    = hasZM && !hasGloom;
    gi.label   = GL_MakeInstallLabel(baseDir, hasGloom, hasZM);
    out.push_back(gi);
}

static void GL_DiscoverGameInstalls(std::vector<GameInstall>& out)
{
    out.clear();

    // Current folder first
    GL_TryAddInstall(std::string(), out);

    // One level of subdirectories
#ifdef _WIN32
    struct _finddata_t info;
    intptr_t handle = _findfirst("*", &info);
    if (handle != -1)
    {
        do
        {
            if (info.attrib & _A_SUBDIR)
            {
                if (std::strcmp(info.name, ".") == 0 || std::strcmp(info.name, "..") == 0)
                    continue;
                GL_TryAddInstall(info.name, out);
            }
        } while (_findnext(handle, &info) == 0);
        _findclose(handle);
    }
#else
    DIR* dir = opendir(".");
    if (dir)
    {
        struct dirent* ent;
        while ((ent = readdir(dir)) != nullptr)
        {
            const char* name = ent->d_name;
            if (!name || name[0] == '.')
                continue;

            std::string dname(name);

            struct stat st;
            if (stat(dname.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
            {
                GL_TryAddInstall(dname, out);
            }
        }
        closedir(dir);
    }
#endif
}

// ------------------------- Minimal 8x8 bitmap font for launcher ----------------

struct LauncherGlyph8 { char c; unsigned char r[8]; };

static const LauncherGlyph8 kLaunchFont8[] =
{
    {' ', {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
    {'A',{0x18,0x24,0x42,0x7E,0x42,0x42,0x42,0x00}},
    {'B',{0x7C,0x42,0x42,0x7C,0x42,0x42,0x7C,0x00}},
    {'C',{0x3C,0x42,0x40,0x40,0x40,0x42,0x3C,0x00}},
    {'D',{0x78,0x44,0x42,0x42,0x42,0x44,0x78,0x00}},
    {'E',{0x7E,0x40,0x40,0x7C,0x40,0x40,0x7E,0x00}},
    {'F',{0x7E,0x40,0x40,0x7C,0x40,0x40,0x40,0x00}},
    {'G',{0x3C,0x42,0x40,0x4E,0x42,0x42,0x3C,0x00}},
    {'H',{0x42,0x42,0x42,0x7E,0x42,0x42,0x42,0x00}},
    {'I',{0x38,0x10,0x10,0x10,0x10,0x10,0x38,0x00}},
    {'J',{0x02,0x02,0x02,0x02,0x42,0x42,0x3C,0x00}},
    {'K',{0x42,0x44,0x48,0x70,0x48,0x44,0x42,0x00}},
    {'L',{0x40,0x40,0x40,0x40,0x40,0x40,0x7E,0x00}},
    {'M',{0x42,0x66,0x5A,0x42,0x42,0x42,0x42,0x00}},
    {'N',{0x42,0x62,0x52,0x4A,0x46,0x42,0x42,0x00}},
    {'O',{0x3C,0x42,0x42,0x42,0x42,0x42,0x3C,0x00}},
    {'P',{0x7C,0x42,0x42,0x7C,0x40,0x40,0x40,0x00}},
    {'Q',{0x3C,0x42,0x42,0x42,0x4A,0x44,0x3A,0x00}},
    {'R',{0x7C,0x42,0x42,0x7C,0x48,0x44,0x42,0x00}},
    {'S',{0x3C,0x40,0x40,0x3C,0x02,0x02,0x3C,0x00}},
    {'T',{0x7C,0x10,0x10,0x10,0x10,0x10,0x10,0x00}},
    {'U',{0x42,0x42,0x42,0x42,0x42,0x42,0x3C,0x00}},
    {'V',{0x42,0x42,0x42,0x42,0x42,0x24,0x18,0x00}},
    {'W',{0x42,0x42,0x42,0x42,0x5A,0x66,0x42,0x00}},
    {'X',{0x42,0x42,0x24,0x18,0x24,0x42,0x42,0x00}},
    {'Y',{0x44,0x44,0x28,0x10,0x10,0x10,0x10,0x00}},
    {'Z',{0x7E,0x02,0x04,0x18,0x20,0x40,0x7E,0x00}},
    {'0',{0x3C,0x46,0x4A,0x52,0x62,0x46,0x3C,0x00}},
    {'1',{0x10,0x30,0x10,0x10,0x10,0x10,0x38,0x00}},
    {'2',{0x3C,0x42,0x02,0x04,0x18,0x20,0x7E,0x00}},
    {'3',{0x3C,0x42,0x02,0x1C,0x02,0x42,0x3C,0x00}},
    {'4',{0x04,0x0C,0x14,0x24,0x44,0x7E,0x04,0x00}},
    {'5',{0x7E,0x40,0x7C,0x02,0x02,0x42,0x3C,0x00}},
    {'6',{0x1C,0x20,0x40,0x7C,0x42,0x42,0x3C,0x00}},
    {'7',{0x7E,0x02,0x04,0x08,0x10,0x10,0x10,0x00}},
    {'8',{0x3C,0x42,0x42,0x3C,0x42,0x42,0x3C,0x00}},
    {'9',{0x3C,0x42,0x42,0x3E,0x02,0x04,0x38,0x00}},
    {':',{0x00,0x18,0x18,0x00,0x18,0x18,0x00,0x00}},
    {'-',{0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00}},
    {'.',{0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}},
    {',',{0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x10}},
};

static const unsigned char* GL_FontRows(char c)
{
    if (c >= 'a' && c <= 'z')
        c = (char)(c - 32);
    for (size_t i = 0; i < sizeof(kLaunchFont8) / sizeof(kLaunchFont8[0]); ++i)
    {
        if (kLaunchFont8[i].c == c)
            return kLaunchFont8[i].r;
    }
    return kLaunchFont8[0].r; // space fallback
}

static void GL_DrawGlyph8(SDL_Renderer* ren, int x, int y, char c, int scale, const SDL_Color& col)
{
    const unsigned char* rows = GL_FontRows(c);
    SDL_SetRenderDrawColor(ren, col.r, col.g, col.b, col.a);
    for (int row = 0; row < 8; ++row)
    {
        unsigned char bits = rows[row];
        for (int colx = 0; colx < 8; ++colx)
        {
            if (bits & (0x80 >> colx))
            {
                SDL_Rect r;
                r.x = x + colx * scale;
                r.y = y + row * scale;
                r.w = scale;
                r.h = scale;
                SDL_RenderFillRect(ren, &r);
            }
        }
    }
}

static int GL_TextWidth(const std::string& text, int scale)
{
    int n = 0;
    for (size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] != '\n')
            ++n;
    }
    return n * 8 * scale;
}

static void GL_DrawText(SDL_Renderer* ren, int x, int y, const std::string& text, int scale, const SDL_Color& col)
{
    int cx = x;
    int cy = y;
    for (size_t i = 0; i < text.size(); ++i)
    {
        char ch = text[i];
        if (ch == '\n')
        {
            cy += 8 * scale + 2;
            cx = x;
            continue;
        }
        GL_DrawGlyph8(ren, cx, cy, ch, scale, col);
        cx += 8 * scale;
    }
}

// Launcher window: background + big centered list with fade-out
static bool GL_RunGameLauncher(const std::vector<GameInstall>& installs, GameInstall& outSelection)
{
    if (installs.empty())
        return false;

    // Determine window size & which background to use (match actual display size)
    int winW = 960;
    int winH = 720;
    bool useWideBG = false;

    SDL_DisplayMode dm;
    if (SDL_GetCurrentDisplayMode(0, &dm) == 0 && dm.w > 0 && dm.h > 0)
    {
        float aspect = dm.w / (float)dm.h;
        useWideBG = (aspect > 1.5f); // 16:9 vs 4:3/5:4 etc.

        // Use the real display size to avoid affecting game scaling
        winW = dm.w;
        winH = dm.h;
    }

    SDL_Window* win = SDL_CreateWindow(
        "ZGloom Launcher",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        winW, winH,
        SDL_WINDOW_SHOWN);

    if (!win)
        return false;

    Uint32 renderFlags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, renderFlags);
    if (!ren)
    {
        SDL_DestroyWindow(win);
        return false;
    }

    SDL_Texture* bgTex = nullptr;
    {
        const unsigned char* bgData = useWideBG ? kLauncherBG_16_9_BMP : kLauncherBG_4_3_BMP;
        int bgSize = useWideBG ? (int)sizeof(kLauncherBG_16_9_BMP) : (int)sizeof(kLauncherBG_4_3_BMP);
        SDL_RWops* rw = SDL_RWFromConstMem(bgData, bgSize);
        SDL_Surface* bg = rw ? SDL_LoadBMP_RW(rw, 1) : nullptr;
        if (bg)
        {
            bgTex = SDL_CreateTextureFromSurface(ren, bg);
            SDL_FreeSurface(bg);
        }
    }

    const SDL_Color colTitle    = { 255, 255, 255, 255 };
    const SDL_Color colNormal   = { 255, 255, 255, 255 };
    const SDL_Color colSelected = { 255, 230, 100, 255 };

    // Font scales
    const int scaleTitle = 7; // SELECT GAME
    const int scaleList  = 7; // Spieleliste
    const int scaleHint  = 4; // etwas kleiner fuer Hint-Zeile

    // Uppercase labels for drawing
    std::vector<std::string> labelsUpper;
    labelsUpper.reserve(installs.size());
    for (size_t i = 0; i < installs.size(); ++i)
    {
        std::string s = installs[i].label;
        for (size_t j = 0; j < s.size(); ++j)
            s[j] = (char)std::toupper((unsigned char)s[j]);
        labelsUpper.push_back(s);
    }

    const std::string title = "SELECT GAME";
    const std::string hint  = "DPAD TO MOVE     A TO START     B TO EXIT";

    const int fontHTitle = 8 * scaleTitle;
    const int fontHHint  = 8 * scaleHint;
    const int fontHList  = 8 * scaleList;
    const int listGap    = fontHList / 2;

    // Titel oben, Hint direkt darunter (beide zentriert)
    const int titleY = winH / 12;
    const int hintY  = titleY + fontHTitle + fontHHint / 2;

    // Spieleliste vertikal zentriert
    const int numEntries       = (int)installs.size();
    const int listBlockHeight  = numEntries * fontHList + (numEntries - 1) * listGap;
    const int listY0           = (winH - listBlockHeight) / 2;

    bool running  = true;
    int  selected = 0;

    // Optional: erstes Gamepad oeffnen
    SDL_GameController* pad = nullptr;
    for (int i = 0; i < SDL_NumJoysticks(); ++i)
    {
        if (SDL_IsGameController(i))
        {
            pad = SDL_GameControllerOpen(i);
            if (pad)
                break;
        }
    }

    while (running)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
            {
                selected = -1;
                running = false;
                break;
            }
            else if (e.type == SDL_KEYDOWN)
            {
                switch (e.key.keysym.sym)
                {
                case SDLK_UP:
                    if (selected > 0) --selected;
                    else selected = numEntries - 1;
                    break;
                case SDLK_DOWN:
                    if (selected < numEntries - 1) ++selected;
                    else selected = 0;
                    break;
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                    running = false;
                    break;
                case SDLK_ESCAPE:
                    selected = -1;
                    running = false;
                    break;
                default:
                    break;
                }
            }
            else if (e.type == SDL_CONTROLLERBUTTONDOWN)
            {
                switch (e.cbutton.button)
                {
                case SDL_CONTROLLER_BUTTON_DPAD_UP:
                    if (selected > 0) --selected;
                    else selected = numEntries - 1;
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                    if (selected < numEntries - 1) ++selected;
                    else selected = 0;
                    break;
                case SDL_CONTROLLER_BUTTON_A:
                    running = false;
                    break;
                case SDL_CONTROLLER_BUTTON_B:
                    selected = -1;
                    running = false;
                    break;
                default:
                    break;
                }
            }
        }

        if (!running)
            break;

        if (bgTex)
        {
            SDL_RenderClear(ren);
            SDL_RenderCopy(ren, bgTex, nullptr, nullptr);
        }
        else
        {
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
            SDL_RenderClear(ren);
        }

        // Titel oben mittig
        int titleW = GL_TextWidth(title, scaleTitle);
        int titleX = (winW - titleW) / 2;
        GL_DrawText(ren, titleX, titleY, title, scaleTitle, colTitle);

        // Hint-Zeile darunter, mittig
        int hintW = GL_TextWidth(hint, scaleHint);
        int hintX = (winW - hintW) / 2;
        GL_DrawText(ren, hintX, hintY, hint, scaleHint, colNormal);

        // Spieleliste mittig
        for (int i = 0; i < numEntries; ++i)
        {
            const SDL_Color& col = (i == selected) ? colSelected : colNormal;
            int w  = GL_TextWidth(labelsUpper[i], scaleList);
            int x  = (winW - w) / 2;
            int y  = listY0 + i * (fontHList + listGap);
            GL_DrawText(ren, x, y, labelsUpper[i], scaleList, col);
        }

        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }

    int resultIndex = selected;

    // Fade-out (0.6 s) if a game was selected
    if (resultIndex >= 0)
    {
        const int fadeFrames  = 24;
        const int fadeDelayMs = 25; // 24 * 25 ms = 600 ms

        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

        for (int i = 0; i < fadeFrames; ++i)
        {
            float t = (float)(i + 1) / (float)fadeFrames;
            Uint8 alpha = (Uint8)(t * 255.0f + 0.5f);

            SDL_SetRenderDrawColor(ren, 0, 0, 0, alpha);
            SDL_RenderFillRect(ren, nullptr);
            SDL_RenderPresent(ren);
            SDL_Delay(fadeDelayMs);
        }

        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    }

    if (pad)
        SDL_GameControllerClose(pad);
    if (bgTex)
        SDL_DestroyTexture(bgTex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);

    if (resultIndex < 0)
        return false;

    outSelection = installs[resultIndex];
    return true;
}



int main(int argc, char* argv[])
{
	SDL_Log("ZGloom: main() start (argc=%d)", argc);
#ifdef __ANDROID__
	SDL_Log("ZGloom: running on Android");
#endif

	// Initialize SDL first (for launcher + gamepad)
	if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0)
	{
		std::cout << "SDL_Init Error: " << SDL_GetError() << std::endl;
		return 1;
	}

#ifdef __ANDROID__
	// SDL needs to be inited before this to pick up gamepad and Android paths
	ConfigureAndroidDataRoot();
#endif

	// Discover compatible game installs (current DataRoot + subdirectories)
	std::vector<GameInstall> installs;
	GL_DiscoverGameInstalls(installs);

	if (installs.empty())
	{
		// Fallback: original Zombie Massacre auto-detect in current DataRoot
		if (FILE* file = fopen("stuf/stages", "rb"))
		{
			fclose(file);
			Config::SetZM(true);
		}
	}
	else
	{
		GameInstall chosen;

		if (installs.size() == 1)
		{
			chosen = installs[0];
		}
		else
		{
			if (!GL_RunGameLauncher(installs, chosen))
			{
				SDL_Quit();
				return 0;
			}
		}

#ifdef __ANDROID__
		// Refine DataRoot to chosen subfolder and chdir there
		std::string root = Config::GetDataRoot();
		if (!root.empty())
		{
			char last = root.back();
			if (last != '/' && last != '\\')
				root.push_back('/');
		}

		if (!chosen.baseDir.empty())
		{
			root += chosen.baseDir;
			if (!root.empty())
			{
				char last2 = root.back();
				if (last2 != '/' && last2 != '\\')
					root.push_back('/');
			}

			Config::SetDataRoot(root);
			if (chdir(root.c_str()) != 0)
			{
				SDL_Log("ZGloom: chdir('%s') to chosen DataRoot failed (errno=%d)", root.c_str(), errno);
			}
			else
			{
				SDL_Log("ZGloom: chdir to chosen DataRoot OK");
			}
		}
#else
		if (!chosen.baseDir.empty())
		{
#ifdef _WIN32
			_chdir(chosen.baseDir.c_str());
#else
			chdir(chosen.baseDir.c_str());
#endif
		}
#endif // __ANDROID__

		Config::SetZM(chosen.isZM);
	}

	SDL_Log("ZGloom: SDL_Init succeeded, calling Config::Init()");
	Config::Init();
	AtmosphereVolume::LoadFromConfig();
	BGM::Init();
	BGM::SetVolume9(AtmosphereVolume::Get());

	SDL_Log("ZGloom: Config and BGM initialized");

	GloomMap gmap;
	Script script;
	TitleScreen titlescreen;
	MenuScreen menuscreen;
	GameState state = STATE_TITLE;
	SDL_Log("ZGloom: core objects constructed (GloomMap, Script, TitleScreen, MenuScreen)");
/* xmp_context g_xmp;  // replaced by global g_xmp */
	g_xmp = xmp_create_context();
	Config::RegisterMusContext(g_xmp);

	int renderwidth, renderheight, windowwidth, windowheight;

	Config::GetRenderSizes(renderwidth, renderheight, windowwidth, windowheight);

	// Apply aspect preset: 0 = 4:3, 1 = 16:9
	int aspect = Config::GetDisplayAspect();
	if (aspect == 0)
	{
		// Original 4:3
		renderwidth  = 320;
		renderheight = 256;
		windowwidth  = 960;
		windowheight = 768;
	}
	else if (aspect == 1)
	{
		// 16:9 widescreen: keep vertical res, widen horizontally (455x256 -> 1365x768)
		renderwidth  = 455;
		renderheight = 256;
		windowwidth  = 1365;
		windowheight = 768;
	}

	CrmFile titlemusic;
	CrmFile intermissionmusic;
	CrmFile ingamemusic;
	CrmFile titlepic;

	titlemusic.Load(Config::GetMusicFilename(0).c_str());
	intermissionmusic.Load(Config::GetMusicFilename(1).c_str());

	SoundHandler::Init();

	SDL_Window* win = SDL_CreateWindow("ZGloom", 100, 100, windowwidth, windowheight, SDL_WINDOW_SHOWN | (Config::GetFullscreen()?SDL_WINDOW_FULLSCREEN:0) );
	if (win == nullptr)
	{
		std::cout << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
		return 1;
	}

	SDL_Log("ZGloom: SDL_CreateWindow OK (%dx%d)", windowwidth, windowheight);

	Config::RegisterWin(win);

	SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | (Config::GetVSync()?SDL_RENDERER_PRESENTVSYNC:0));
	if (ren == nullptr)
	{
		std::cout << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
		return 1;
	}
	SDL_Log("ZGloom: SDL_CreateRenderer OK");
	RendererHooks::init(ren, windowwidth, windowheight);

	SDL_Texture* rendertex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, renderwidth, renderheight);
	if (rendertex == nullptr)
	{
		std::cout << "SDL_CreateTexture Error: " << SDL_GetError() << std::endl;
		return 1;
	}

	SDL_Log("ZGloom: SDL_CreateTexture OK (%dx%d)", renderwidth, renderheight);

	SDL_ShowCursor(SDL_DISABLE);

	SDL_Surface* render8 = SDL_CreateRGBSurface(0, 320, 256, 8, 0, 0, 0, 0);
	SDL_Surface* intermissionscreen = SDL_CreateRGBSurface(0, 320, 256, 8, 0, 0, 0, 0);
	SDL_Surface* titlebitmap = SDL_CreateRGBSurface(0, 320, 256, 8, 0, 0, 0, 0);
	SDL_Surface* render32 = SDL_CreateRGBSurface(0, renderwidth, renderheight, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
	SDL_Surface* screen32 = SDL_CreateRGBSurface(0, 320, 256, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);


	SDL_Log("ZGloom: Surfaces created (render8/intermission/title/render32/screen32)");

	ZHUD_EnsureCreated(ren, renderwidth, renderheight);
    // HUD resources are created via ZHUD_EnsureCreated below; removed local redeclarations here.
	ObjectGraphics objgraphics;
	Renderer renderer;
	GameLogic logic;
	Camera cam;
	Hud hud;

	SDL_Log("ZGloom: before logic.Init");
	logic.Init(&objgraphics);
	SDL_Log("ZGloom: after logic.Init");
	SDL_AddTimer(1000 / 25, my_callbackfunc, NULL);
	SDL_Log("ZGloom: SDL_AddTimer installed");

	SDL_Event sEvent;

	bool notdone = true;

#if 1
	Font smallfont, bigfont;
	CrmFile fontfile;
	SDL_Log("ZGloom: loading fonts (bigfont2/smallfont/bigfont)");
	fontfile.Load((Config::GetMiscDir() + "bigfont2.bin").c_str());
	if (fontfile.data)
	{
		bigfont.Load2(fontfile);
		smallfont.Load2(fontfile);
	}
	else
	{
		fontfile.Load((Config::GetMiscDir() + "smallfont.bin").c_str());
		if (fontfile.data)smallfont.Load(fontfile);
		fontfile.Load((Config::GetMiscDir() + "bigfont.bin").c_str());
		if (fontfile.data)bigfont.Load(fontfile);
	}
	SDL_Log("ZGloom: fonts loaded");
#endif

	SDL_Log("ZGloom: loading title picture");
	titlepic.Load((Config::GetPicsDir() + "title").c_str());

	if (titlepic.data)
	{
		SDL_Log("ZGloom: title picture found, calling LoadPic(title)");
		LoadPic(Config::GetPicsDir() + "title", titlebitmap);
	}
	else
	{
		SDL_Log("ZGloom: title picture missing, calling LoadPic(blackmagic)");
		LoadPic(Config::GetPicsDir() + "blackmagic", titlebitmap);
	}

	if (titlemusic.data)
	{
		if (xmp_load_module_from_memory(g_xmp, titlemusic.data, titlemusic.size))
		{
			std::cout << "music error";
		}

		if (xmp_start_player(g_xmp, 22050, 0))
		{
			std::cout << "music error";
		}
		Mix_HookMusic(fill_audio, g_xmp);
		Config::SetMusicVol(Config::GetMusicVol());
	}

	std::string intermissiontext;

	bool intermissionmusplaying = false;
	bool haveingamemusic = false;
	bool printscreen = false;
	int screennum = 0;
	uint32_t fps = 0;
	uint32_t fpscounter = 0;

	Mix_Volume(-1, Config::GetSFXVol()*12);
	Mix_VolumeMusic(Config::GetMusicVol() * 12);

	//try and blit title etc into the middle of the screen
	SDL_Rect blitrect;

	int screenscale = renderheight / 256;
	blitrect.w = 320 * screenscale;
	blitrect.h = 256 * screenscale;
	blitrect.x = (renderwidth - 320 * screenscale) / 2;
	blitrect.y = (renderheight - 256 * screenscale) / 2;

	SDL_SetRelativeMouseMode(SDL_TRUE);
	
	//set up the level select

	std::vector<std::string> levelnames;
	script.GetLevelNames(levelnames);
	titlescreen.SetLevels(levelnames);
	int levelselect = 0;

	SDL_Log("ZGloom: entering main loop");

	while (notdone)
	{
		ZHUD_EnsureAlive(ren, renderwidth, renderheight);
		RendererHooks::beginFrame();
		ZHUD_Clear();
		if ((state == STATE_PARSING) || (state == STATE_SPOOLING))
		{
			std::string scriptstring;
			Script::ScriptOp sop;

			sop = script.NextLine(scriptstring);

			switch (sop)
			{
				case Script::SOP_SETPICT:
				{
					scriptstring.insert(0, Config::GetPicsDir());
					LoadPic(scriptstring, intermissionscreen);
					SDL_SetPaletteColors(render8->format->palette, intermissionscreen->format->palette->colors, 0, 256);
					break;
				}
				case Script::SOP_SONG:
				{
					scriptstring.insert(0, Config::GetMusicDir());
					ingamemusic.Load(scriptstring.c_str());
					haveingamemusic = (ingamemusic.data != nullptr);
					break;
				}
				case Script::SOP_LOADFLAT:
				{
					//improve this, only supports 9 flats
					int flat = scriptstring[0] - '0';
					gmap.SetFlat(flat);
					SaveSystem::SetCurrentFlat(flat);
					break;
				}
				case Script::SOP_LOADMAP:
				case Script::SOP_NOP:
					// No action needed here in intermission script
					break;
				case Script::SOP_TEXT:
				{
					 intermissiontext = scriptstring;

					 if (state == STATE_SPOOLING)
					 {
						 if (intermissiontext == levelnames[levelselect])
						 {
							 // level selector
							 if (intermissionmusic.data)
							 {
								 if (xmp_load_module_from_memory(g_xmp, intermissionmusic.data, intermissionmusic.size))
								 {
									 std::cout << "music error";
								 }

								 if (xmp_start_player(g_xmp, 22050, 0))
								 {
									 std::cout << "music error";
								 }
								 Mix_HookMusic(fill_audio, g_xmp);
								 Config::SetMusicVol(Config::GetMusicVol());
								 intermissionmusplaying = true;
							 }

							 state = STATE_PARSING;
						 }
					 }
					 break;
				}
				case Script::SOP_DRAW:
				{
					if (state == STATE_PARSING)
					{
						if (intermissionmusic.data)
						{
							if (xmp_load_module_from_memory(g_xmp, intermissionmusic.data, intermissionmusic.size))
							{
								std::cout << "music error";
							}

							if (xmp_start_player(g_xmp, 22050, 0))
							{
								std::cout << "music error";
							}
							Mix_HookMusic(fill_audio, g_xmp);
							Config::SetMusicVol(Config::GetMusicVol());
							intermissionmusplaying = true;
						}
					}
					break;
				}
				case Script::SOP_WAIT:
				{
					if (state == STATE_PARSING)
					{
						state = STATE_WAITING;

						SDL_SetPaletteColors(render8->format->palette, smallfont.GetPalette()->colors, 0, 16);
						SDL_BlitSurface(intermissionscreen, NULL, render8, NULL);
						smallfont.PrintMultiLineMessage(intermissiontext, 220, render8);
					}
					break;
				}
				case Script::SOP_PLAY:
				{
					if (state == STATE_PARSING)
					{

					// Determine which level to load: script default or saved position
					std::string levelRel = scriptstring;
					SaveSystem::SaveData s;
					bool haveSavePos = false;
					bool haveReplay  = false;

					// For each new PLAY operation we start from a clean event history
					EventReplay::Clear();

					if (g_RequestTitleContinue)
					{
						g_RequestTitleContinue = false;
						if (SaveSystem::LoadFromDisk(s))
						{
							levelRel    = s.levelPath;
							haveSavePos = true;
							SaveSystem::SetCurrentFlat(s.flatIndex);
							script.SeekAfterPlayFor(levelRel);

							// EventReplay: load stored event history for this save
							if (EventReplay::LoadFromDisk())
								haveReplay = true;
						}
					}

					// Remember level path (relative) for save/resume
					SaveSystem::SetCurrentLevelPath(levelRel);

					// Default camera for new game; may be overridden by saved data
					cam.x.SetInt(0);
					cam.y = 120;
					cam.z.SetInt(0);
					cam.rotquick.SetInt(0);

					// Build full path and load requested level
					std::string levelFull = levelRel;
					levelFull.insert(0, Config::GetLevelDir());
					gmap.Load(levelFull.c_str(), &objgraphics);
					//gmap.Load("maps/map1_4", &objgraphics);
					renderer.Init(render32, &gmap, &objgraphics);
					logic.InitLevel(&gmap, &cam, &objgraphics);

					// EventReplay: after the map is fully initialised, restore button/door state
					if (haveReplay)
					{
						EventReplay::ReplayAll(gmap);
					}

					// If we continue from a save, restore camera and player state
					if (haveSavePos)
					{
						cam.x.SetInt(s.camX);
						cam.y = s.camY;
						cam.z.SetInt(s.camZ);
						cam.rotquick.SetInt(s.camRot);

						// Restore player stats (HP, weapon, reload state)
						for (auto& o : gmap.GetMapObjects())
						{
							if (o.t == ObjectGraphics::OLT_PLAYER1)
							{
								o.data.ms.hitpoints = s.hp;
								o.data.ms.weapon    = s.weapon;
								o.data.ms.reload    = s.reload;
								o.data.ms.reloadcnt = s.reloadcnt;
								break;
							}
						}
					}


					state = STATE_PLAYING;


						// Start embedded atmosphere BGM for each level (Vita-style)
						BGM::PlayLooping();
						BGM::SetVolume9(AtmosphereVolume::Get());

						if (haveingamemusic)
						{
							if (xmp_load_module_from_memory(g_xmp, ingamemusic.data, ingamemusic.size))
							{
								std::cout << "music error";
							}

							if (xmp_start_player(g_xmp, 22050, 0))
							{
								std::cout << "music error";
							}
							Mix_HookMusic(fill_audio, g_xmp);
							Config::SetMusicVol(Config::GetMusicVol());
						}
					}
					break;
				}
				case Script::SOP_END:
				{
					BGM::Stop(); // stop Atmosphere BGM on SOP_END
					state = STATE_TITLE;
					if (intermissionmusic.data && intermissionmusplaying)
					{
						Mix_HookMusic(nullptr, nullptr);
						xmp_end_player(g_xmp);
						xmp_release_module(g_xmp);
						intermissionmusplaying = false;
					}
					if (titlemusic.data)
					{
						if (xmp_load_module_from_memory(g_xmp, titlemusic.data, titlemusic.size))
						{
							std::cout << "music error";
						}

						if (xmp_start_player(g_xmp, 22050, 0))
						{
							std::cout << "music error";
						}
						Mix_HookMusic(fill_audio, g_xmp);
						Config::SetMusicVol(Config::GetMusicVol());
					}
					break;
				}
			}
		}

		if (state == STATE_TITLE)
		{
			SDL_SetPaletteColors(render8->format->palette, titlebitmap->format->palette->colors, 0, 256);
			titlescreen.Render(titlebitmap, render8, smallfont);
		}

		while ((state!= STATE_SPOOLING) && SDL_PollEvent(&sEvent))
		{
			if (sEvent.type == SDL_WINDOWEVENT)
			{
				if (sEvent.window.event == SDL_WINDOWEVENT_CLOSE)
				{
					notdone = false;
				}
			}

			if (Config::HaveController() && (sEvent.type == SDL_CONTROLLERBUTTONDOWN))
			{
				//fake up a key event
				if ((state == STATE_TITLE) || (state == STATE_MENU) || (state == STATE_WAITING))
				{
					// Confirm / activate (menus only)
					if (Config::GetControllerConfirm())
					{
						sEvent.type = SDL_KEYDOWN;
						sEvent.key.keysym.sym = SDLK_SPACE;
					}
					// Menu navigation (DPAD)
					if (Config::GetControllerUp())
					{
						sEvent.type = SDL_KEYDOWN;
						sEvent.key.keysym.sym = SDLK_UP;
					}
					if (Config::GetControllerDown())
					{
						sEvent.type = SDL_KEYDOWN;
						sEvent.key.keysym.sym = SDLK_DOWN;
					}
					// Back one level in menus -> ESC
					if (Config::GetControllerBack())
					{
						sEvent.type = SDL_KEYDOWN;
						sEvent.key.keysym.sym = SDLK_ESCAPE;
					}
					// Decrement current option in menu (U / X / Square) -> LEFT
					if (state == STATE_MENU && Config::GetControllerMap())
					{
						sEvent.type = SDL_KEYDOWN;
						sEvent.key.keysym.sym = SDLK_LEFT;
					}
				}

				if (state == STATE_PLAYING)
				{
					// call up menu (START or GUIDE, plus Y on OUYA handled in GetControllerStart)
					if (Config::GetControllerStart())
					{
						sEvent.type = SDL_KEYDOWN;
						sEvent.key.keysym.sym = SDLK_ESCAPE;
					}
					// Map / debug overlay -> TAB (U/X/Square)
					if (Config::GetControllerMap())
					{
						sEvent.type = SDL_KEYDOWN;
						sEvent.key.keysym.sym = SDLK_TAB;
					}
				}

			}

			if ((sEvent.type == SDL_KEYDOWN) && (sEvent.key.keysym.sym == SDLK_SPACE ||
				sEvent.key.keysym.sym == SDLK_RETURN ||
			   sEvent.key.keysym.sym == SDLK_LCTRL))
			{
				if (state == STATE_WAITING)
				{
					state = STATE_PARSING;
					if (intermissionmusic.data)
					{
						Mix_HookMusic(nullptr, nullptr);
						xmp_end_player(g_xmp);
						xmp_release_module(g_xmp);
						intermissionmusplaying = false;
					}
				}
			}

			if (sEvent.type == SDL_KEYDOWN)
			{
				if (state == STATE_TITLE)
				{
					switch (titlescreen.Update(sEvent, levelselect))
					{
						case TitleScreen::TITLERET_PLAY:
							state = STATE_PARSING;
							BGM::Stop(); // ensure Atmosphere BGM is stopped on TITLERET_PLAY
							logic.Init(&objgraphics);
							if (titlemusic.data)
							{
								Mix_HookMusic(nullptr, nullptr);
								xmp_end_player(g_xmp);
								xmp_release_module(g_xmp);
							}
							break;
						case TitleScreen::TITLERET_SELECT:
							state = STATE_SPOOLING;
							logic.Init(&objgraphics);
							if (titlemusic.data)
							{
								Mix_HookMusic(nullptr, nullptr);
								xmp_end_player(g_xmp);
								xmp_release_module(g_xmp);
							}
							break;
						case TitleScreen::TITLERET_QUIT:
							notdone = false;
							break;
						default:
							break;
					}
				}
				if (state == STATE_MENU)
				{
					MenuScreen::MenuReturn mr = menuscreen.Update(sEvent);

					if (g_RequestSavePosition)
					{
						g_RequestSavePosition = false;
						SaveSystem::SaveData s;
						s.levelPath = SaveSystem::GetCurrentLevelPath();
						s.flatIndex = SaveSystem::GetCurrentFlat();
						s.camX = cam.x.GetInt();
						s.camY = cam.y;
						s.camZ = cam.z.GetInt();
						s.camRot = cam.rotquick.GetInt();

						// Capture current player state for save/resume
						MapObject player = logic.GetPlayerObj();

						s.hp        = player.data.ms.hitpoints;
						s.weapon    = player.data.ms.weapon;
						s.reload    = player.data.ms.reload;
						s.reloadcnt = player.data.ms.reloadcnt;

						SaveSystem::SaveToDisk(s);
						EventReplay::SaveToDisk();
						SDL_Log("ZGloom SaveSystem: SAVE requested (level=%s, weapon=%d, reload=%d)", s.levelPath.c_str(), s.weapon, s.reload);
					}

					switch (mr)
					{
						case MenuScreen::MENURET_PLAY:
							state = STATE_PLAYING;
							break;
						case MenuScreen::MENURET_QUIT:
							script.Reset();
							state = STATE_TITLE;
							BGM::Stop(); // ensure Atmosphere BGM is stopped on MENU->TITLE
							if (titlemusic.data)
							{
								if (xmp_load_module_from_memory(g_xmp, titlemusic.data, titlemusic.size))
								{
									std::cout << "music error";
								}

								if (xmp_start_player(g_xmp, 22050, 0))
								{
									std::cout << "music error";
								}
								Mix_HookMusic(fill_audio, g_xmp);
								Config::SetMusicVol(Config::GetMusicVol());
							}
							break;
						default:
							break;
					}
				}
				if ((state == STATE_PLAYING) && (sEvent.key.keysym.sym == SDLK_ESCAPE))
				{
					state = STATE_MENU;
				}
			}

			if ((sEvent.type == SDL_KEYDOWN) && sEvent.key.keysym.sym == SDLK_F12)
			{
				Config::SetFullscreen(!Config::GetFullscreen());
			}

			if ((sEvent.type == SDL_KEYDOWN) && sEvent.key.keysym.sym == SDLK_TAB)
			{
				Config::SetDebug(!Config::GetDebug());
			}

			if ((sEvent.type == SDL_KEYDOWN) && sEvent.key.keysym.sym == SDLK_PRINTSCREEN)
			{
				printscreen = true;
			}

			if (sEvent.type == SDL_USEREVENT)
			{
				if (state == STATE_PLAYING)
				{
					if (logic.Update(&cam))
					{
						BGM::Stop(); // stop Atmosphere BGM on level end
						if (haveingamemusic)
						{
							Mix_HookMusic(nullptr, nullptr);
							xmp_end_player(g_xmp);
							xmp_release_module(g_xmp);
							intermissionmusplaying = false;
						}
						state = STATE_PARSING;
					}
				}
				if (state == STATE_TITLE)
				{
					titlescreen.Clock();
				}
				if (state == STATE_MENU)
				{
					menuscreen.Clock();
				}

				fpscounter++;

				if (fpscounter >= 25)
				{
					Config::SetFPS(fps);
					fpscounter = 0;
					fps = 0;
				}
			}
		}

        // Atmosphere BGM is controlled explicitly on Vita-style transitions

		SDL_FillRect(render32, NULL, 0);

		if (state == STATE_PLAYING)
		{
			renderer.SetTeleEffect(logic.GetTeleEffect());
			renderer.SetPlayerHit(logic.GetPlayerHit());
			renderer.SetThermo(logic.GetThermo());

			//cam.x.SetInt(3969);
			//cam.z.SetInt(5359);
			//cam.rotquick.SetInt(254);
			renderer.Render(&cam);
			MapObject pobj = logic.GetPlayerObj();
			hud.Render(hudLayer32, pobj, smallfont);
			fps++;
		}
		if (state == STATE_MENU)
		{
			renderer.Render(&cam);
			menuscreen.Render(render32, render32, smallfont);
		}
		
		if ((state == STATE_WAITING) || (state == STATE_TITLE))
		{
			// SDL does not seem to like scaled 8->32 copy?
			SDL_BlitSurface(render8, NULL, screen32, NULL);

			int aspect = Config::GetDisplayAspect();

			// 4:3 (oder sehr schmale Renderbreite): altes Verhalten
			if (aspect == 0 || renderwidth <= 320)
			{
				SDL_BlitScaled(screen32, NULL, render32, &blitrect);
			}
			else
			{
				// 16:9: 4:3-Bild zentriert, Seiten mit gestreckten und abgedunkelten Rändern
				SDL_Rect center = blitrect;
				SDL_Rect dst;

				// Seitliche Leisten: nach Möglichkeit nur aus dem nackten Hintergrundbild erzeugen,
				// damit Schrift am Rand nicht in die 16:9-Streifen mitgestretcht wird.
				SDL_Surface* barSrc32 = screen32;   // Fallback: komplettes Bild
				SDL_Surface* tmpBars  = NULL;

				SDL_Surface* bg8 = NULL;
				if (state == STATE_TITLE)
				{
					bg8 = titlebitmap;
				}
				else if (state == STATE_WAITING)
				{
					bg8 = intermissionscreen;
				}

				if (bg8)
				{
					tmpBars = SDL_ConvertSurfaceFormat(bg8, SDL_PIXELFORMAT_ARGB8888, 0);
					if (tmpBars)
					{
						barSrc32 = tmpBars;
					}
				}

				// linke Leiste
				int leftBarW = center.x;
				if (leftBarW > 0)
				{
					SDL_Rect srcL;
					srcL.x = 0;
					srcL.y = 0;
					srcL.w = 16;           // schmaler Streifen vom linken Rand
					srcL.h = barSrc32->h;

					dst.x = 0;
					dst.y = center.y;
					dst.w = leftBarW;
					dst.h = center.h;

					// gestreckter Rand
					SDL_BlitScaled(barSrc32, &srcL, render32, &dst);

					// abdunkeln mit halbtransparentem Schwarz
					SDL_Surface* darkL = SDL_CreateRGBSurfaceWithFormat(0, dst.w, dst.h, 32, SDL_PIXELFORMAT_RGBA8888);
					if (darkL)
					{
						SDL_FillRect(darkL, NULL, SDL_MapRGBA(darkL->format, 0, 0, 0, 160));
						SDL_SetSurfaceBlendMode(darkL, SDL_BLENDMODE_BLEND);
						SDL_BlitSurface(darkL, NULL, render32, &dst);
						SDL_FreeSurface(darkL);
					}
				}

				// rechte Leiste
				int rightBarW = renderwidth - (center.x + center.w);
				if (rightBarW > 0)
				{
					SDL_Rect srcR;
					srcR.x = barSrc32->w - 16; // Streifen vom rechten Rand
					srcR.y = 0;
					srcR.w = 16;
					srcR.h = barSrc32->h;

					dst.x = center.x + center.w;
					dst.y = center.y;
					dst.w = rightBarW;
					dst.h = center.h;

					SDL_BlitScaled(barSrc32, &srcR, render32, &dst);

					SDL_Surface* darkR = SDL_CreateRGBSurfaceWithFormat(0, dst.w, dst.h, 32, SDL_PIXELFORMAT_RGBA8888);
					if (darkR)
					{
						SDL_FillRect(darkR, NULL, SDL_MapRGBA(darkR->format, 0, 0, 0, 160));
						SDL_SetSurfaceBlendMode(darkR, SDL_BLENDMODE_BLEND);
						SDL_BlitSurface(darkR, NULL, render32, &dst);
						SDL_FreeSurface(darkR);
					}
				}

				if (tmpBars)
				{
					SDL_FreeSurface(tmpBars);
				}

				// zentriertes 4:3-Hauptbild (mit Schrift etc.)
				SDL_BlitScaled(screen32, NULL, render32, &center);
			}
		}



		if (printscreen)
		{
			std::string filename("img");

			filename += std::to_string(screennum);
			filename += ".bmp";
			screennum++;

			SDL_SaveBMP(render32, filename.c_str());
			printscreen = false;
		}

		if (state != STATE_SPOOLING)
		{
			
			// --- Muzzle Flash (PC port from Vita 7.19): apply per frame ---
			{
				static uint32_t s_last = SDL_GetTicks();
				uint32_t now = SDL_GetTicks();
				uint32_t dt = now - s_last; s_last = now;
				MuzzleFlashFX::Get().ApplyToSurface(render32);
				MuzzleFlashFX::Get().Update((float)dt);
			}
			// --- end muzzle flash port ---
			
			SDL_UpdateTexture(rendertex, NULL, render32->pixels, render32->pitch);
			SDL_RenderClear(ren);

			// Compute letterboxed destination rect for world texture
			int outW = 0, outH = 0;
			if (SDL_GetRendererOutputSize(ren, &outW, &outH) != 0 || outW <= 0 || outH <= 0)
			{
				outW = windowwidth;
				outH = windowheight;
			}

			float scaleX = outW / (float)renderwidth;
			float scaleY = outH / (float)renderheight;
			float scale = (scaleX < scaleY) ? scaleX : scaleY;

			int dstW = (int)(renderwidth * scale + 0.5f);
			int dstH = (int)(renderheight * scale + 0.5f);

			SDL_Rect dst;
			dst.w = dstW;
			dst.h = dstH;
			dst.x = (outW - dstW) / 2;
			dst.y = (outH - dstH) / 2;

			SDL_RenderCopy(ren, rendertex, NULL, &dst);

			SDL_UpdateTexture(hudTex, NULL, hudLayer32->pixels, hudLayer32->pitch);
			RendererHooks::SetHudTexture(hudTex);
			RendererHooks::endFramePresent();
		}
	}

	BGM::Shutdown();
	xmp_free_context(g_xmp);

	Config::Save();

	SoundHandler::Quit();

	SDL_FreeSurface(render8);
	SDL_FreeSurface(render32);
	SDL_FreeSurface(screen32);
	SDL_FreeSurface(intermissionscreen);
	SDL_FreeSurface(titlebitmap);
	SDL_DestroyRenderer(ren);
	SDL_DestroyWindow(win);
	SDL_Quit();

	return 0;
}