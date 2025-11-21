#pragma once
// ConfigOverlays: stores display-effect options (with persistence).
// Persistence file: ux0:/data/ZGloom/config.txt
namespace Config {
    int  GetVignetteEnabled(); void SetVignetteEnabled(int);
    int  GetVignetteStrength();   void SetVignetteStrength(int);   // 0..4
    int  GetVignetteRadius();     void SetVignetteRadius(int);     // 0..4
    int  GetVignetteSoftness();   void SetVignetteSoftness(int);   // 0..4
    int  GetVignetteWarmth();     void SetVignetteWarmth(int);     // -100..100
    int  GetFilmGrain();          void SetFilmGrain(int);
    int  GetFilmGrainIntensity(); void SetFilmGrainIntensity(int); // 0..4
    int  GetScanlines();          void SetScanlines(int);
    int  GetScanlineIntensity();  void SetScanlineIntensity(int);  // 0..4
    int  GetMuzzleFlash();       void SetMuzzleFlash(int);      // 0/1

    // Optional: call once on startup; otherwise lazy-load happens on first access.
    void EffectsConfigInit();
    void EffectsConfigSave();
} // namespace Config
