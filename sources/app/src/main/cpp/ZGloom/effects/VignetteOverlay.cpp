#include "VignetteOverlay.h"
#include <algorithm>
#include <cmath>

static inline float clamp01(float x){ return x < 0.f ? 0.f : (x > 1.f ? 1.f : x); }

VignetteOverlay::VignetteOverlay() {}
VignetteOverlay::~VignetteOverlay() { destroy(); }

bool VignetteOverlay::init(SDL_Renderer* renderer, int w, int h,
                           float strength, float radius, float softness,
                           Uint8 tintR, Uint8 tintG, Uint8 tintB)
{
    destroy();
    screenW = w; screenH = h;
    texW = std::max(8, w / 2);
    texH = std::max(8, h / 2);
    m_strength = clamp01(strength);
    m_radius   = clamp01(radius);
    m_softness = clamp01(softness);
    m_tintR = tintR; m_tintG = tintG; m_tintB = tintB;
    enabled = true;
    dirty = true;
    return buildTexture(renderer);
}

void VignetteOverlay::destroy()
{
    if (tex) { SDL_DestroyTexture(tex); tex = nullptr; }
}

bool VignetteOverlay::buildTexture(SDL_Renderer* renderer)
{
    if (!renderer || texW <= 0 || texH <= 0) return false;

    if (tex) { SDL_DestroyTexture(tex); tex = nullptr; }
    tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, texW, texH);
    if (!tex) return false;
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_MOD);

    void* pixels = nullptr; int pitch = 0;
    if (SDL_LockTexture(tex, nullptr, &pixels, &pitch) != 0) {
        SDL_DestroyTexture(tex); tex = nullptr; return false;
    }

    const float cx = (texW - 1) * 0.5f;
    const float cy = (texH - 1) * 0.5f;
    const float minHalf = std::min(texW, texH) * 0.5f;
    const float inner = m_radius * minHalf;
    const float softPx  = std::max(1.0f, m_softness * minHalf);
    const float invSoft = 1.0f / softPx;

    for (int y = 0; y < texH; ++y) {
        Uint32* row = reinterpret_cast<Uint32*>(static_cast<Uint8*>(pixels) + y * pitch);
        for (int x = 0; x < texW; ++x) {
            const float dx = x - cx;
            const float dy = y - cy;
            const float r  = std::sqrt(dx*dx + dy*dy);

            float t = (r - inner) * invSoft;
            t = clamp01(t);
            t = t * t * (3.f - 2.f * t); // smoothstep

            // Soft, linear scaling: center ~ 1.0, edges darken by up to ~0.45 at level 4.
            float f = clamp01(m_strength * t);
            const Uint8 grey = (Uint8)(255.0f * (1.0f - f) + 0.5f);

            row[x] = (255u << 24) | (Uint32(grey) << 16) | (Uint32(grey) << 8) | Uint32(grey);
        }
    }

    SDL_UnlockTexture(tex);
    dirty = false;
    return true;
}

void VignetteOverlay::render(SDL_Renderer* renderer)
{
    if (!enabled || !renderer) return;
    if (dirty || !tex) {
        if (!buildTexture(renderer)) return;
    }
    SDL_Rect dst{0,0,screenW,screenH};
    SDL_SetTextureColorMod(tex, m_tintR, m_tintG, m_tintB);
    SDL_RenderCopy(renderer, tex, nullptr, &dst);
}

void VignetteOverlay::setEnabled(bool on){ if (enabled != on){ enabled = on; } }
bool VignetteOverlay::isEnabled() const { return enabled; }

void VignetteOverlay::setStrength(float s){ s = clamp01(s); if (m_strength != s){ m_strength = s; dirty = true; } }
void VignetteOverlay::setRadius(float r){ r = clamp01(r); if (m_radius   != r){ m_radius   = r; dirty = true; } }
void VignetteOverlay::setSoftness(float s){ s = clamp01(s); if (m_softness != s){ m_softness = s; dirty = true; } }
void VignetteOverlay::setTint(Uint8 r, Uint8 g, Uint8 b){
    if (m_tintR!=r || m_tintG!=g || m_tintB!=b){ m_tintR=r; m_tintG=g; m_tintB=b; /* no rebuild needed for color mod */ }
}
