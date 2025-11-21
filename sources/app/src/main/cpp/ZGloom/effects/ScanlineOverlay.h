#pragma once
#include <SDL2/SDL.h>

class ScanlineOverlay {
public:
    ScanlineOverlay() = default;
    ~ScanlineOverlay() { destroy(); }

    bool init(SDL_Renderer* renderer, int screenW, int screenH);
    void destroy();

    void setParams(float darkness, int spacing) { m_darkness = darkness; m_spacing = (spacing < 1 ? 1 : spacing); rebuildNeeded = true; }
    void setEnabled(bool e) { m_enabled = e; }
    bool isEnabled() const { return m_enabled; }

    void render(SDL_Renderer* renderer);

private:
    SDL_Texture* m_tex = nullptr;
    int m_w = 0, m_h = 0;
    float m_darkness = 0.2f;
    int m_spacing = 2;
    bool m_enabled = false;
    bool rebuildNeeded = false;

    bool rebuild(SDL_Renderer* renderer);
};
