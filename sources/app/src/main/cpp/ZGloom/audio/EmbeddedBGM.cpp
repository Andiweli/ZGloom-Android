#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include "xmp/include/xmp.h"
#include <atomic>
#include <cstdint>
#include <algorithm>

#include "audio/EmbeddedBGM.h"
#include "audio/AtmosphereVolume.h"
#include "audio/atmosphere_bgm.h"

namespace {
    xmp_context           s_ctx = nullptr;
    std::atomic<bool>     s_playing{false};
    std::atomic<bool>     s_hookInstalled{false};
    std::atomic<int>      s_vol9{6}; // 0..9 current ambience volume
    constexpr int         kSampleRate = 22050;
    inline int Vol9ToSDL(int v) { return (v<=0)?0:((v>=9)?128:(v*14)); }

    void mix_scale_i16(int16_t* pcm, int samples, int volSDL /*0..128*/) {
        if (volSDL >= 128) return;
        if (volSDL <= 0) { SDL_memset(pcm, 0, samples * sizeof(int16_t)); return; }
        for (int i = 0; i < samples; ++i) {
            int v = pcm[i];
            v = (v * volSDL + 64) >> 7; // /128
            if (v > 32767) v = 32767;
            if (v < -32768) v = -32768;
            pcm[i] = (int16_t)v;
        }
    }

    void SDLCALL MusicHook(void* /*udata*/, Uint8* stream, int len) {
        if (!s_ctx || !s_playing) {
            SDL_memset(stream, 0, len);
            return;
        }
        int rc = xmp_play_buffer(s_ctx, stream, len, 0);
        if (rc != 0) {
            SDL_memset(stream, 0, len);
            s_playing = false;
            return;
        }
        int volSDL = Vol9ToSDL(s_vol9.load());
        mix_scale_i16(reinterpret_cast<int16_t*>(stream), len / 2, volSDL);
    }
}

namespace BGM {

void Init() {
    if (s_ctx) return;
    s_ctx = xmp_create_context();
    if (!s_ctx) return;
    if (xmp_load_module_from_memory(
            s_ctx,
            const_cast<void*>(static_cast<const void*>(g_bgm_mod)),
            static_cast<long>(sizeof(g_bgm_mod))) != 0) {
        xmp_free_context(s_ctx);
        s_ctx = nullptr;
        return;
    }
    xmp_start_player(s_ctx, kSampleRate, 0);
    xmp_end_player(s_ctx);
    s_vol9.store(EmbeddedBGMVolume::Get());
}

void Shutdown() {
    if (!s_ctx) return;
    if (s_hookInstalled) {
        Mix_HookMusic(nullptr, nullptr);
        s_hookInstalled = false;
    }
    if (s_playing) {
        xmp_end_player(s_ctx);
        s_playing = false;
    }
    xmp_release_module(s_ctx);
    xmp_free_context(s_ctx);
    s_ctx = nullptr;
}

void PlayLooping() {
    if (!s_ctx) Init();
    if (!s_ctx) return;

    // Always (re)install our music hook here. Other systems (title/intermission/ingame music)
    // may override this via Mix_HookMusic(fill_audio, ...), but whenever ambience is (re)started
    // for a level we want to ensure our hook is active again.
    Mix_HookMusic(MusicHook, nullptr);
    s_hookInstalled = true;

    if (!s_playing) {
        xmp_start_player(s_ctx, kSampleRate, 0);
        s_playing = true;
    }
    s_vol9.store(EmbeddedBGMVolume::Get());
}

void Stop() {
    if (!s_ctx) return;
    if (s_playing) {
        xmp_end_player(s_ctx);
        s_playing = false;
    }
}

void SetVolume9(int v) {
    if (v < 0) v = 0;
    if (v > 9) v = 9;
    s_vol9.store(v);
}

} // namespace BGM
