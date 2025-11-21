#pragma once
namespace BGM {
    void Init();
    void Shutdown();
    void PlayLooping();     // idempotent
    void Stop();            // idempotent
    void SetVolume9(int v); // 0..9
}
