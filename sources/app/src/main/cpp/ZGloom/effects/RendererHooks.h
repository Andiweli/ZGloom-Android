#pragma once
#include <SDL2/SDL.h>

// Queue a lens flare using RENDER-SPACE coords (x,y in offscreen)
// NOTE: Lens flare removed; API kept as no-op for compatibility.
void EnqueueLensFlareScreen(int x, int y, float intensity, float scale);

namespace RendererHooks {

bool init(SDL_Renderer* renderer, int screenW, int screenH);
void shutdown();

// Inform hooks about offscreen render size (renderwidth, renderheight)
void setRenderSize(int w, int h);

// Optional: FPS limit control
void setTargetFps(int fps);

// Camera motion (screen-space deltas per second) → parallax dust
void setCameraMotion(float dx, float dy, float yawRate);

// Per-frame hooks
void beginFrame();
void endFramePresent();

// Menu-driven helpers (0..5)
void setVignetteLevel(int lvl);
void setScanlineLevel(int lvl);
void setFilmGrainLevel(int lvl);

// NEW: notify that the 3D world (objects) was drawn in this frame.
// Call once per frame from renderer.cpp (e.g., within DrawObjects).
void markWorldFrame();

    // Particle Dust toggle (0 = off, 1 = on)
    int  GetParticleDustEnabled();
    void SetParticleDustEnabled(int onOff);


void SetHudTexture(SDL_Texture* hudTex);
} // namespace RendererHooks