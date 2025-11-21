// zgloom.cpp : Defines the entry point for the console application.
//

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#ifdef __ANDROID__
#include <SDL2/SDL_system.h>
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
    // 1) Preferred: fixed OUYA-style path /sdcard/ZGloom/
    const char* fixedRoot = "/sdcard/ZGloom/";
    if (chdir(fixedRoot) == 0) {
        Config::SetDataRoot(fixedRoot);
        SDL_Log("ZGloom: Android DataRoot set to fixed '%s' (chdir OK)", fixedRoot);
        return;
    } else {
        SDL_Log("ZGloom: chdir('%s') failed (errno=%d), falling back to SDL_AndroidGetExternalStoragePath()", fixedRoot, errno);
    }

    // 2) Fallback: SDL external storage path + "/ZGloom/"
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

// (removed) // #include <SDL.h>

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


int main(int argc, char* argv[])
{
	SDL_Log("ZGloom: main() start (argc=%d)", argc);
#ifdef __ANDROID__
	SDL_Log("ZGloom: running on Android");
#endif

	/* AUTODETECT ZM FIRST!*/
	if (FILE* file = fopen("stuf/stages", "r"))
	{
		fclose(file);
		Config::SetZM(true);
	}

	if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0)
	{
		std::cout << "SDL_Init Error: " << SDL_GetError() << std::endl;
		return 1;
	}
	// SDL needs to be inited before this to pick up gamepad
#ifdef __ANDROID__
	ConfigureAndroidDataRoot();
#endif
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
		RendererHooks::beginFrame();
		SDL_FillRect(hudLayer32, NULL, 0x00000000);
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
					gmap.SetFlat(scriptstring[0] - '0');
					break;
				}
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

						cam.x.SetInt(0);
						cam.y = 120;
						cam.z.SetInt(0);
						cam.rotquick.SetInt(0);
						scriptstring.insert(0, Config::GetLevelDir());
						gmap.Load(scriptstring.c_str(), &objgraphics);
						//gmap.Load("maps/map1_4", &objgraphics);
						renderer.Init(render32, &gmap, &objgraphics);
						logic.InitLevel(&gmap, &cam, &objgraphics);
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
					switch (menuscreen.Update(sEvent))
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
			SDL_RenderCopy(ren, rendertex, NULL, NULL);
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