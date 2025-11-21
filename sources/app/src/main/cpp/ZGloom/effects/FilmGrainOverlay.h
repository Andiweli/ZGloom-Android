#pragma once
#include <SDL2/SDL.h>

class FilmGrainOverlay {
public:
    FilmGrainOverlay() = default;
    ~FilmGrainOverlay() { destroy(); }

    bool init(SDL_Renderer* renderer, int screenW, int screenH);
    void destroy();

    void setParams(float intensity, int updateRate) { m_intensity = intensity; m_updateRate = (updateRate < 1 ? 1 : updateRate); }
    void setEnabled(bool e) { m_enabled = e; }
    bool isEnabled() const { return m_enabled; }

    void render(SDL_Renderer* renderer, int frameIndex);

private:
    SDL_Texture* m_noise = nullptr;
    int m_w = 0, m_h = 0;
    int m_texW = 0, m_texH = 0;
    float m_intensity = 0.15f;
    int   m_updateRate = 2;
    bool  m_enabled = false;

    void regenerate(SDL_Renderer* renderer, int seed);
};
