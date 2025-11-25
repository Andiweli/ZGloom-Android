#pragma once

#include <string>

namespace SaveSystem
{
    struct SaveData
    {
        std::string levelPath;
        int         flatIndex = -1;

        int camX = 0;
        int camY = 0;
        int camZ = 0;
        int camRot = 0;

        int hp        = 0;
        int weapon    = 0;
        int reload    = 0;
        int reloadcnt = 0;
    };

    bool HasSave();
    bool LoadFromDisk(SaveData& outData);
    bool SaveToDisk(const SaveData& inData);

    void SetCurrentLevelPath(const std::string& levelPath);
    const std::string& GetCurrentLevelPath();

    void SetCurrentFlat(int flatIndex);
    int  GetCurrentFlat();
}
