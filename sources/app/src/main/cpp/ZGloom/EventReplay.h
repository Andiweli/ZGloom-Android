#pragma once

#include <cstdint>

class GloomMap;

// Vita-style event replay stub.
// On each ExecuteEvent() call we record the event ID into a simple list.
// This list is persisted in a small file (last.events) and can be replayed
// after a level reload to restore button/door state.
namespace EventReplay
{
    void Clear();
    void Record(uint32_t ev);

    bool HasReplay();
    bool LoadFromDisk();
    bool SaveToDisk();

    void ReplayAll(GloomMap& map);
}
