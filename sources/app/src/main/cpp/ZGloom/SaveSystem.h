#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace SaveSystem
{
    struct SaveData
    {
        int formatVersion = 2;

        std::string levelPath;
        int         flatIndex = -1;

        int camX = 0;
        int camY = 0;
        int camZ = 0;
        int camRot = 0;

        int hp        = 0;
        int lives     = 3;
        int weapon    = 0;
        int reload    = 0;
        int reloadcnt = 0;

        // V2 stores the persistent event history in the same save file.
        // V1 saves leave this empty and use the legacy last.events sidecar.
        std::vector<uint32_t> eventHistory;
    };

    bool HasSave();
    bool LoadFromDisk(SaveData& outData);
    bool SaveToDisk(const SaveData& inData);

    void SetCurrentLevelPath(const std::string& levelPath);
    const std::string& GetCurrentLevelPath();

    void SetCurrentFlat(int flatIndex);
    int  GetCurrentFlat();
}
