#pragma once
#include <SDL2/SDL.h>

// Fast vignette + tint overlay (CPU), optimized for Vita.
// - Generates a HALF-RES texture and scales to screen -> ~4x fewer pixels to compute.
// - Live param setters only rebuild when values actually change (no per-frame stall).
class VignetteOverlay {
public:
    VignetteOverlay();
    ~VignetteOverlay();

    // strength/radius/softness in 0..1; tint RGB 0..255
    bool init(SDL_Renderer* renderer, int screenW, int screenH,
              float strength = 0.45f, float radius = 0.65f, float softness = 0.35f,
              Uint8 tintR = 255, Uint8 tintG = 240, Uint8 tintB = 230);

    void destroy();

    // Enable/disable drawing
    void setEnabled(bool on);
    bool isEnabled() const;

    // Change parameters (rebuilds the half-res texture on change)
    void setStrength(float s);
    void setRadius(float r);
    void setSoftness(float s);
    void setTint(Uint8 r, Uint8 g, Uint8 b);

    // NEW: control whether the vignette darkens the center (classic "spotlight")
    // or the edges (classic photo-style vignette). Default = center-darker to
    // preserve legacy behaviour; set true to darken the edges.
    void setEdgesDark(bool on);
    bool getEdgesDark() const;

    // (Re)render the overlay
    void render(SDL_Renderer* renderer);

private:
    SDL_Texture* tex = nullptr;
    int screenW = 0, screenH = 0;
    int texW = 0, texH = 0;   // half-res texture
    float m_strength = 0.45f;
    float m_radius   = 0.65f;
    float m_softness = 0.35f;
    Uint8 m_tintR = 255, m_tintG = 240, m_tintB = 230;
    bool m_edgesDark = false;     // default keeps previous "center darker" look
    bool enabled = true;
    bool dirty = true;

    bool buildTexture(SDL_Renderer* renderer);
};
