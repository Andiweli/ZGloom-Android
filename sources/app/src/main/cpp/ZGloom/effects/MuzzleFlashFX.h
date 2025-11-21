#pragma once
#include <stdint.h>
#include <math.h>
#include <SDL2/SDL.h>
#include "config.h"
#include "ConfigOverlays.h"

// Simple one-frame additive muzzle flash applied on the 32-bit HUD/backbuffer surface.
class MuzzleFlashFX {
public:
    MuzzleFlashFX();
    static MuzzleFlashFX& Get() { static MuzzleFlashFX s; return s; }

    void SetTint(float r, float g, float b);
    void GetTint(float& r, float& g, float& b) const { r = tintR_; g = tintG_; b = tintB_; }
    void SetScale(float s);
    void Trigger(float intensity = 1.0f, float duration_ms = 1.0f);
    void Update(float dt_ms);
    void ApplyToSurface(SDL_Surface* s);
    inline void Disarm() { frames_left_ -= 1; if (frames_left_ <= 0) { target_ = 0.0f; armed_ = 0; } }

private:
    float target_;
    int   frames_left_;
    float tintR_, tintG_, tintB_;
    float scale_;
    int   armed_;
};

inline MuzzleFlashFX::MuzzleFlashFX()
: target_(0.0f), frames_left_(0),
  tintR_(1.00f), tintG_(0.97f), tintB_(0.94f),
  scale_(1.0f),
  armed_(0) {}

inline void MuzzleFlashFX::SetTint(float r, float g, float b) {
    tintR_ = r; tintG_ = g; tintB_ = b;
}

inline void MuzzleFlashFX::SetScale(float s) {
    if (s < 0.25f) s = 0.25f;
    if (s > 3.0f) s = 3.0f;
    scale_ = s;
}

inline void MuzzleFlashFX::Trigger(float intensity, float /*duration_ms*/) {
        if (Config::GetMuzzleFlash() == 0) return;
    if (armed_) return;
    if (intensity < 0.0f) intensity = 0.0f;
    if (intensity > 1.0f) intensity = 1.0f;
    target_ = intensity;
    frames_left_ = 2; // ~50% longer total: 2 frames (second at 0.5x)
    armed_ = 1;
}

inline void MuzzleFlashFX::Update(float /*dt_ms*/) {}

inline void MuzzleFlashFX::ApplyToSurface(SDL_Surface* s) {
    if (frames_left_ <= 0) { armed_ = 0; return; }
    float strength = target_;
    // Extend ~50%: second frame at half intensity
    if (frames_left_ <= 1) strength *= 0.5f;
    // 10% less transparent (more intense)
    strength *= 1.10f;
    if (strength > 1.0f) strength = 1.0f;
    if (strength <= 0.001f || !s) { frames_left_ -= 1; if (frames_left_ <= 0) { target_ = 0.0f; armed_ = 0; } return; }
    if (s->format->BytesPerPixel != 4) { frames_left_ -= 1; if (frames_left_ <= 0) { target_ = 0.0f; armed_ = 0; } return; }

    const bool need_lock = (SDL_MUSTLOCK(s) != 0);
    if (need_lock) SDL_LockSurface(s);

    const int w = s->w, h = s->h;
    const int cx = w >> 1;
    const int cy = (int)(h * 0.80f);
    const int rx = (int)(w * 0.253f * scale_);
    const int ry = (int)(h * 0.334f * scale_);

    const int x0 = (cx - rx < 0) ? 0 : (cx - rx);
    const int x1 = (cx + rx > w) ? w : (cx + rx);
    const int y0 = (cy - ry < 0) ? 0 : (cy - ry);
    const int y1 = (cy + ry > h) ? h : (cy + ry);

    const float invRx2 = 1.0f / (float)(rx * rx);
    const float invRy2 = 1.0f / (float)(ry * ry);
    const int addMax = (int)(37.0f * strength);

    // --- Gentle gamma lift for tint to make flash crisper without clipping ---
    // Gamma exponent < 1.0 brightens mid-tones slightly. 0.75 is a mild lift.
    float gexp = 0.75f;
    float tR = powf(tintR_, gexp);
    float tG = powf(tintG_, gexp);
    float tB = powf(tintB_, gexp);
    float tm = tR; if (tG > tm) tm = tG; if (tB > tm) tm = tB;
    if (tm < 1e-4f) { tR = 1.0f; tG = 1.0f; tB = 1.0f; tm = 1.0f; }
    tR /= tm; tG /= tm; tB /= tm;
    // -------------------------------------------------------------------------

    uint8_t* row = (uint8_t*)s->pixels + y0 * s->pitch;
    for (int y = y0; y < y1; ++y) {
        uint32_t* px = (uint32_t*)row;
        const float dy = (float)(y - cy);
        const float dy2n = (dy * dy) * invRy2;
        for (int x = x0; x < x1; ++x) {
            const float dx = (float)(x - cx);
            const float dx2n = (dx * dx) * invRx2;
            float r = 1.0f - (dx2n + dy2n);
            if (r > 0.0f) {
                float r2 = r * r;
                float r3 = r2 * r;
                float wgt = (6.0f * r3 * r2) - (15.0f * r2 * r2) + (10.0f * r3);
                int add = (int)(addMax * wgt);
                if (add > 0) {
                    uint32_t c = px[x];
                    uint32_t Rmask = s->format->Rmask, Gmask = s->format->Gmask, Bmask = s->format->Bmask, Amask = s->format->Amask;
                    int Rshift = s->format->Rshift, Gshift = s->format->Gshift, Bshift = s->format->Bshift, Ashift = s->format->Ashift;
                    uint8_t r8 = (uint8_t)((c & Rmask) >> Rshift);
                    uint8_t g8 = (uint8_t)((c & Gmask) >> Gshift);
                    uint8_t b8 = (uint8_t)((c & Bmask) >> Bshift);
                    uint32_t a = (uint32_t)((c & Amask) >> Ashift);
                    int addR = (int)(add * tR);
                    int addG = (int)(add * tG);
                    int addB = (int)(add * tB);
                    int nr = r8 + addR; if (nr > 255) nr = 255;
                    int ng = g8 + addG; if (ng > 255) ng = 255;
                    int nb = b8 + addB; if (nb > 255) nb = 255;
                    uint32_t out = 0;
                    out |= ((uint32_t)nr << Rshift) & Rmask;
                    out |= ((uint32_t)ng << Gshift) & Gmask;
                    out |= ((uint32_t)nb << Bshift) & Bmask;
                    out |= ((uint32_t)a  << Ashift) & Amask;
                    px[x] = out;
                }
            }
        }
        row += s->pitch;
    }

    if (need_lock) SDL_UnlockSurface(s);
    frames_left_ -= 1; if (frames_left_ <= 0) { target_ = 0.0f; armed_ = 0; }
}
