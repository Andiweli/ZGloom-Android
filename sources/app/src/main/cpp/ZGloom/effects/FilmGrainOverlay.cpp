#include "FilmGrainOverlay.h"
#include <cstdlib>

bool FilmGrainOverlay::init(SDL_Renderer* renderer, int screenW, int screenH) {
    destroy();
    m_w = screenW; m_h = screenH;
    m_texW = screenW / 3;
    m_texH = screenH / 3;
    if (m_texW < 64) m_texW = 64;
    if (m_texH < 64) m_texH = 64;

    m_noise = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, m_texW, m_texH);
    if (!m_noise) return false;
    SDL_SetTextureBlendMode(m_noise, SDL_BLENDMODE_BLEND);
    regenerate(renderer, 0);
    return true;
}

void FilmGrainOverlay::destroy() {
    if (m_noise) { SDL_DestroyTexture(m_noise); m_noise = nullptr; }
}

void FilmGrainOverlay::regenerate(SDL_Renderer* /*renderer*/, int seed) {
    if (!m_noise) return;
    void* pixels = nullptr; int pitch = 0;
    if (SDL_LockTexture(m_noise, nullptr, &pixels, &pitch) != 0) return;

    unsigned int s = 1664525u * (seed + 1) + 1013904223u;
    for (int y = 0; y < m_texH; ++y) {
        Uint32* row = reinterpret_cast<Uint32*>(static_cast<Uint8*>(pixels) + y * pitch);
        for (int x = 0; x < m_texW; ++x) {
            s = 1664525u * s + 1013904223u;
            Uint8 n = static_cast<Uint8>((s >> 24) & 0xFF);
            Uint8 c = 128 + (int(n) - 128) / 2;
            Uint8 a = static_cast<Uint8>(m_intensity * 255);
            row[x] = (Uint32(a) << 24) | (Uint32(c) << 16) | (Uint32(c) << 8) | Uint32(c);
        }
    }
    SDL_UnlockTexture(m_noise);
}

void FilmGrainOverlay::render(SDL_Renderer* renderer, int frameIndex) {
    if (!m_enabled || !renderer || !m_noise) return;
    if (frameIndex % m_updateRate == 0) regenerate(renderer, frameIndex);
    SDL_Rect dst{0,0,m_w,m_h};
    SDL_RenderCopy(renderer, m_noise, nullptr, &dst);
}
