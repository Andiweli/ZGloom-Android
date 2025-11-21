#include "RendererHooks.h"
#include <SDL2/SDL.h>
#include <vector>
#include <cmath>
#include "../config.h"
#include "../ConfigOverlays.h"
#include "VignetteOverlay.h"
#include "FilmGrainOverlay.h"
#include "ScanlineOverlay.h"
#include "FrameLimiter.h"
// ---------------- LUTs: discrete 0..5 -> normalized ----------------
static inline float map_strength_05(int v){ static const float lut[6]={0.00f,0.08f,0.16f,0.26f,0.36f,0.52f}; if(v<0)v=0; if(v>5)v=5; return lut[v]; }
static inline float map_radius_05 (int v){ static const float lut[6]={0.45f,0.55f,0.65f,0.75f,0.85f,0.90f}; if(v<0)v=0; if(v>5)v=5; return lut[v]; }
static inline float map_soft_05   (int v){ static const float lut[6]={0.15f,0.23f,0.32f,0.44f,0.57f,0.80f}; if(v<0)v=0; if(v>5)v=5; return lut[v]; }
static inline float map_grainI_05 (int v){ static const float lut[6]={0.00f,0.08f,0.16f,0.24f,0.31f,0.38f}; if(v<0)v=0; if(v>5)v=5; return lut[v]; }
static inline float map_scanI_05  (int v){ static const float lut[6]={0.00f,0.20f,0.40f,0.60f,0.80f,1.00f}; if(v<0)v=0; if(v>5)v=5; return lut[v]; }

// ---------------- Globals ----------------
static SDL_Renderer*    gRenderer = nullptr;
static int              gScreenW  = 0;
static int              gScreenH  = 0;
static int              gRenderW  = 0;
static int              gRenderH  = 0;

static VignetteOverlay  gVignette;
static FilmGrainOverlay gGrain;
static ScanlineOverlay  gScan;
static FrameLimiter     gLimiter;
static int              gTargetFps = 60;
static SDL_Texture*   gHudTexThisFrame = nullptr; // HUD layer for this frame (RGBA)


// ---- World-space dust (internal) ---------------------------------
struct WDustParticle {
    float x, y, z;   // camera space (right, up, forward), z>0 in front
    float size;      // base size
    float alpha;     // 0..255
    float seed;
};

static std::vector<WDustParticle> gWDust;
static Uint32 gWDustLastTicks = 0;
static float  gWDustDensity = 0.25f;   // 0..1
static float  gWDustNearZ   = 70.0f;   // near plane-ish (in arbitrary units)
static float  gWDustFarZ    = 420.0f;  // far plane-ish
static float  gWDustFocal   = 0.9f;    // focal as fraction of screenW
static float  gWDustSizeK   = 0.16f;  // size scaling constant

static inline float frand01() { return (float)(rand() & 0x7FFF) / 32767.0f; }
static inline float frand(float a, float b) { return a + (b - a) * frand01(); }

static void WDustRespawn(WDustParticle& p, int screenW, int screenH, bool spawnFar=true) {
    // Spawn inside a forward frustum wedge
    float halfW = screenW * 0.6f;
    float halfH = screenH * 0.45f;
    p.x = frand(-halfW, halfW);
    p.y = frand(-halfH, halfH);
    p.z = spawnFar ? frand(gWDustFarZ*0.7f, gWDustFarZ) : frand(gWDustNearZ, gWDustFarZ);
    p.size  = frand(3.0f, 7.0f);
    p.alpha = 180.0f + frand(0.0f, 60.0f);
    p.seed  = frand01();
}

static void WDustEnsureCount(int screenW, int screenH) {
    // Normalize to 320x256 baseline
    const float baseArea = 320.0f * 256.0f;
    int target = (int)(gWDustDensity * ((screenW * screenH) / baseArea) * 73.0f);
    if (target < 12)  target = 12;
    if (target > 300) target = 300;
    while ((int)gWDust.size() < target) {
        WDustParticle p{};
        WDustRespawn(p, screenW, screenH, /*spawnFar=*/false);
        gWDust.push_back(p);
    }
    while ((int)gWDust.size() > target) gWDust.pop_back();
}

static void WDustUpdateAndRender(SDL_Renderer* r, int screenW, int screenH,
                                 float camDX, float camDY, float camYaw)
{
    if (!r) return;
    Uint32 now = SDL_GetTicks();
    if (gWDustLastTicks == 0) gWDustLastTicks = now;
    float dt = (now - gWDustLastTicks) / 1000.0f;
    if (dt <= 0.0f) dt = 0.001f;
    gWDustLastTicks = now;

    WDustEnsureCount(screenW, screenH);

    // Prepare blending
    SDL_BlendMode oldMode;
    SDL_GetRenderDrawBlendMode(r, &oldMode);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    // Convert yaw (screen units) to radians-ish small angle
    float yawRad = -camYaw * 0.05f; // tweakable

    float focal = gWDustFocal * screenW;

    for (auto& p : gWDust) {
        // Camera-induced motion: rotate opposite to camera yaw
        float s = 0.0f, c = 1.0f;
        if (fabsf(yawRad) > 0.0015f) { s = sinf(yawRad); c = cosf(yawRad); }
        float nx =  c * p.x + s * p.z;
        float nz = -s * p.x + c * p.z;
        p.x = nx; p.z = nz;
        // tiny random angular jitter to avoid coherent swirls
        float jitter = (frand01() - 0.5f) * 0.004f;
        float js = sinf(jitter), jc = cosf(jitter);
        float jx =  jc * p.x + js * p.z;
        float jz = -js * p.x + jc * p.z;
        p.x = jx; p.z = jz;
        // extra lateral drift inversely proportional to depth
        p.x += (-camYaw) * 1.2f * (1.0f - (p.z / gWDustFarZ));

        // Translate opposite to camera translation (dx right, dy forward)
        // screen dx -> right in camera space; screen dy -> forward in camera space
        p.x -= camDX * 12.0f; // strafing: move dust opposite to lateral camera motion      // scale to taste
        p.z -= camDY * 30.0f;     // forward motion brings particles closer

        // gentle wobble
        float wob = 20.0f * sinf(p.seed * 13.0f + now * 0.0017f);
        p.x += wob * 0.02f;
        float wobY = 18.0f * cosf(p.seed * 9.7f + now * 0.0013f);
        p.y += wobY * 0.015f;

        // Recycle if out of view
        if (p.z < gWDustNearZ || p.z > gWDustFarZ) {
            WDustRespawn(p, screenW, screenH, /*spawnFar=*/(p.z < gWDustNearZ));
        }

        // Project to screen
        float invz = 1.0f / p.z;
        int sx = (int)(screenW * 0.5f + (p.x * focal) * invz);
        int sy = (int)(screenH * 0.5f - (p.y * focal) * invz);

        // Size grows when approaching (1/z)
                // Near fade for walls/doors
        float nearFade = (p.z - gWDustNearZ) / (gWDustNearZ * 1.0f);
        if (nearFade < 0.0f) nearFade = 0.0f; if (nearFade > 1.0f) nearFade = 1.0f;
        if (nearFade < 0.35f) { continue; }
int sz = (int)(p.size * (gWDustSizeK * focal) * invz * (0.7f + 0.3f * nearFade));
        if (sz < 1) sz = 1; if (sz > 28) sz = 28;

        // Cull if far off-screen; otherwise draw
        if (sx < -16 || sx > screenW + 16 || sy < -16 || sy > screenH + 16) {
            if (p.z > gWDustNearZ) {
                // nudge back into frustum a bit
                p.x *= 0.9f;
            } else {
                WDustRespawn(p, screenW, screenH, /*spawnFar=*/true);
            }
            continue;
        }

        nearFade = (p.z - gWDustNearZ) / (gWDustNearZ * 1.0f);
        if (nearFade < 0.0f) nearFade = 0.0f; if (nearFade > 1.0f) nearFade = 1.0f;
        if (nearFade < 0.35f) { continue; } // render far fewer when hugging walls
        Uint8 alpha = (Uint8)(p.alpha * nearFade);
        SDL_SetRenderDrawColor(r, 245, 235, 210, alpha);
        SDL_Rect rr{ sx, sy, sz, sz };
        SDL_RenderFillRect(r, &rr);
    }

    SDL_SetRenderDrawBlendMode(r, oldMode);
}

static float           gCamDX = 0.0f, gCamDY = 0.0f, gCamYaw = 0.0f;

// world flag: set true by markWorldFrame() when the 3D scene was drawn this frame
static bool             gWorldThisFrame = false;

// ---------------- Helpers ----------------
static inline void applyVignetteFromConfig(){
const int s = Config::GetVignetteStrength();
    const int r = Config::GetVignetteRadius();
    const int f = Config::GetVignetteSoftness();
    gVignette.setStrength(map_strength_05(s));
    gVignette.setRadius  (map_radius_05(r));
    gVignette.setSoftness(map_soft_05  (f));
    // neutral warm tint (can be mapped from warmth later)
        // Map warmth (0..10) around neutral 5 to an RGB tint (cool ↔ warm)
    int w = Config::GetVignetteWarmth(); // -100..100
if (w < -100) w = -100; if (w > 100) w = 100;
int warmth01 = (w + 100) / 20; // -> 0..10
float t = warmth01 / 10.0f; // 0..1
// 0..1
float u = t * t * (3.0f - 2.0f * t);
int coolR=210, coolG=232, coolB=255;
int warmR=255, warmG=220, warmB=170;
int r8 = (int)(coolR + (warmR - coolR) * u);
int g8 = (int)(coolG + (warmG - coolG) * u);
int b8 = (int)(coolB + (warmB - coolB) * u);
gVignette.setTint(r8, g8, b8);
gVignette.setEnabled(Config::GetVignetteEnabled()!=0);
}
static inline void applyGrainFromConfig(){
    gGrain.setParams(map_grainI_05(Config::GetFilmGrainIntensity()), 2);
    gGrain.setEnabled(Config::GetFilmGrain()!=0);
}
static inline void applyScanFromConfig(){
    gScan.setParams(map_scanI_05(Config::GetScanlineIntensity()), 2);
    gScan.setEnabled(Config::GetScanlines()!=0);
}

// Public helper: queue flare using RENDER-space coords; scales to SCREEN-space
void EnqueueLensFlareScreen(int, int, float, float) {
    // no-op: lens flare removed
}

namespace RendererHooks {

// Particle Dust enable flag (default: ON)
static bool gDustEnabled = true;

int GetParticleDustEnabled(){ return 0; }
void SetParticleDustEnabled(int){ /* no-op */ }


bool init(SDL_Renderer* renderer, int screenW, int screenH){
    gRenderer = renderer;
    gScreenW  = screenW;
    gScreenH  = screenH;

    // Init overlays
    gVignette.init(gRenderer, gScreenW, gScreenH);
    gGrain.init(gRenderer, gScreenW, gScreenH);
    gScan.init(gRenderer, gScreenW, gScreenH);
    applyVignetteFromConfig();
    applyGrainFromConfig();
    applyScanFromConfig();

    // Init dust
    // Init lens flare system (optional)
    // lens flare removed: init no-op
return (gRenderer != nullptr);
}

void shutdown(){
    gRenderer = nullptr;
}

void setRenderSize(int w, int h){
    gRenderW = (w>0)?w:0;
    gRenderH = (h>0)?h:0;
}

void setTargetFps(int fps){
    if (fps <= 0) fps = 0;
    if (fps > 120) fps = 120;
    gTargetFps = fps;
}

void setCameraMotion(float dx, float dy, float yawRate){
    gCamDX = dx; gCamDY = dy; gCamYaw = yawRate;
}





void SetHudTexture(SDL_Texture* hudTex){
    gHudTexThisFrame = hudTex;
}

void beginFrame(){
    gLimiter.beginFrame();
    // lens flare removed: clear no-op
    gWorldThisFrame = false; // will be set from renderer.cpp when world drawn

    // Keep overlay render size in sync with current renderer output (fullscreen / resize)
    if (gRenderer) {
        int outW = 0, outH = 0;
        if (SDL_GetRendererOutputSize(gRenderer, &outW, &outH) == 0) {
            if (outW != gScreenW || outH != gScreenH) {
                gScreenW = outW;
                gScreenH = outH;

                // Re-init overlays to match new output size
                gVignette.init(gRenderer, gScreenW, gScreenH);
                gGrain.init(gRenderer, gScreenW, gScreenH);
                gScan.init(gRenderer, gScreenW, gScreenH);
            }
        }
    }

    // Re-apply overlay settings each frame so menu changes take effect
    applyVignetteFromConfig();
    applyGrainFromConfig();
    applyScanFromConfig();
}

void endFramePresent(){
    // World-space dust (3D projected) only during gameplay
// Post overlays (Dust lies visually under them)
    gGrain.render(gRenderer, SDL_GetTicks());
    gScan.render(gRenderer);
    gVignette.render(gRenderer);

    // Optional lens flares
    // lens flare removed: render no-op

    // Composite HUD layer above overlays (if provided)
    if (gHudTexThisFrame) {
        // Save & restore minimal state
        SDL_Rect vp; SDL_RenderGetViewport(gRenderer, &vp);
        SDL_RenderSetViewport(gRenderer, nullptr);
        SDL_SetRenderDrawBlendMode(gRenderer, SDL_BLENDMODE_BLEND);
        SDL_SetTextureBlendMode(gHudTexThisFrame, SDL_BLENDMODE_BLEND);
        SDL_RenderCopy(gRenderer, gHudTexThisFrame, nullptr, nullptr);
        SDL_RenderSetViewport(gRenderer, &vp);
    }
SDL_RenderPresent(gRenderer);
    gLimiter.endFrameAndSleep(gTargetFps);
}


void setVignetteLevel(int lvl){
    if (lvl<0) lvl=0; if (lvl>5) lvl=5;
    Config::SetVignetteEnabled(lvl>0?1:0);
    Config::SetVignetteStrength(lvl);
    Config::SetVignetteRadius(lvl);
    Config::SetVignetteSoftness(lvl);
    applyVignetteFromConfig();
}
void setScanlineLevel(int lvl){
    if (lvl<0) lvl=0; if (lvl>5) lvl=5;
    Config::SetScanlines(lvl>0?1:0);
    Config::SetScanlineIntensity(lvl);
    applyScanFromConfig();
}
void setFilmGrainLevel(int lvl){
    if (lvl<0) lvl=0; if (lvl>5) lvl=5;
    Config::SetFilmGrain(lvl>0?1:0);
    Config::SetFilmGrainIntensity(lvl);
    applyGrainFromConfig();
}

void markWorldFrame(){
    gWorldThisFrame = true;
}

} // namespace RendererHooks