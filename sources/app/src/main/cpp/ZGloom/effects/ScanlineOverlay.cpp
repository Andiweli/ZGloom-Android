#include "ScanlineOverlay.h"

bool ScanlineOverlay::init(SDL_Renderer* renderer, int screenW, int screenH) {
    destroy();
    m_w = screenW; m_h = screenH;
    return rebuild(renderer);
}

void ScanlineOverlay::destroy() {
    if (m_tex) { SDL_DestroyTexture(m_tex); m_tex = nullptr; }
}

bool ScanlineOverlay::rebuild(SDL_Renderer* renderer) {
    if (!renderer || m_w <= 0 || m_h <= 0) return false;
    if (m_tex) { SDL_DestroyTexture(m_tex); m_tex = nullptr; }

    m_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, m_w, m_h);
    if (!m_tex) return false;
    SDL_SetTextureBlendMode(m_tex, SDL_BLENDMODE_MOD);

    void* pixels = nullptr; int pitch = 0;
    if (SDL_LockTexture(m_tex, nullptr, &pixels, &pitch) != 0) return false;

    for (int y = 0; y < m_h; ++y) {
        Uint32* row = reinterpret_cast<Uint32*>(static_cast<Uint8*>(pixels) + y * pitch);
        bool dark = (y % m_spacing) == 0;
        float f = dark ? (1.0f - m_darkness) : 1.0f;
        Uint8 m = static_cast<Uint8>(f * 255.0f);
        Uint32 pix = (255u << 24) | (Uint32(m) << 16) | (Uint32(m) << 8) | Uint32(m);
        for (int x = 0; x < m_w; ++x) row[x] = pix;
    }

    SDL_UnlockTexture(m_tex);
    rebuildNeeded = false;
    return true;
}

void ScanlineOverlay::render(SDL_Renderer* renderer) {
    if (!m_enabled || !renderer) return;
    if (!m_tex || rebuildNeeded) {
        if (!rebuild(renderer)) return;
    }
    SDL_Rect dst{0,0,m_w,m_h};
    SDL_RenderCopy(renderer, m_tex, nullptr, &dst);
}
