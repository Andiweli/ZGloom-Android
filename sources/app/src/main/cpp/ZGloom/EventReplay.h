#pragma once

#include <cstdint>
#include <vector>

class GloomMap;

// Persistent event history used by SAVE POSITION.
// Event 1 is the map-initialisation event and is deliberately never stored.
// Replaying uses GloomMap's persistent-only mode, so teleports, sounds and
// monster spawning are not triggered a second time after loading.
namespace EventReplay
{
    void Clear();
    void Record(uint32_t ev);

    void SetEvents(const std::vector<uint32_t>& events);
    const std::vector<uint32_t>& GetEvents();

    bool HasReplay();
    bool LoadFromDisk();
    bool SaveToDisk();

    void ReplayAll(GloomMap& map);
}
