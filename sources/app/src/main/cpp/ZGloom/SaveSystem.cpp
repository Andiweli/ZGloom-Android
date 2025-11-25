#include "SaveSystem.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "config.h"

namespace
{
    std::string g_currentLevelPath;
    int         g_currentFlat = -1;

    const char* kSaveFileName = "savepos.txt";
    const char* kMagic        = "ZGLOOM_SAVE_V1";

    std::string BuildSavePath()
    {
        std::string path = Config::GetDataRoot();
        if (!path.empty())
        {
            char last = path[path.size() - 1];
            if (last != '/' && last != '\\')
                path += "/";
        }
        path += kSaveFileName;
        return path;
    }
}

namespace SaveSystem
{
    bool HasSave()
    {
        std::string path = BuildSavePath();
        FILE* f = std::fopen(path.c_str(), "rb");
        if (!f)
        {
            return false;
        }

        char magic[64] = {0};
        if (!std::fgets(magic, sizeof(magic), f))
        {
            std::fclose(f);
            return false;
        }
        std::fclose(f);

        // strip newline
        size_t len = std::strlen(magic);
        while (len && (magic[len - 1] == '\n' || magic[len - 1] == '\r'))
        {
            magic[len - 1] = '\0';
            --len;
        }

        bool ok = (std::strcmp(magic, kMagic) == 0);
        return ok;
    }

    bool LoadFromDisk(SaveData& outData)
    {
        std::string path = BuildSavePath();
        FILE* f = std::fopen(path.c_str(), "rb");
        if (!f)
            return false;

        char line[256];

        if (!std::fgets(line, sizeof(line), f))
        {
            std::fclose(f);
            return false;
        }

        // magic
        {
            size_t len = std::strlen(line);
            while (len && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            {
                line[len - 1] = '\0';
                --len;
            }
            if (std::strcmp(line, kMagic) != 0)
            {
                std::fclose(f);
                return false;
            }
        }

        SaveData tmp;

        while (std::fgets(line, sizeof(line), f))
        {
            // strip newline
            size_t len = std::strlen(line);
            while (len && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            {
                line[len - 1] = '\0';
                --len;
            }

            if (std::strncmp(line, "LEVEL=", 6) == 0)
            {
                tmp.levelPath = line + 6;
            }
            else if (std::strncmp(line, "FLAT=", 5) == 0)
            {
                tmp.flatIndex = std::atoi(line + 5);
            }
            else if (std::strncmp(line, "CAMX=", 5) == 0)
            {
                tmp.camX = std::atoi(line + 5);
            }
            else if (std::strncmp(line, "CAMY=", 5) == 0)
            {
                tmp.camY = std::atoi(line + 5);
            }
            else if (std::strncmp(line, "CAMZ=", 5) == 0)
            {
                tmp.camZ = std::atoi(line + 5);
            }
            else if (std::strncmp(line, "CAMROT=", 7) == 0)
            {
                tmp.camRot = std::atoi(line + 7);
            }
            else if (std::strncmp(line, "HP=", 3) == 0)
            {
                tmp.hp = std::atoi(line + 3);
            }
            else if (std::strncmp(line, "WEAPON=", 7) == 0)
            {
                tmp.weapon = std::atoi(line + 7);
            }
            else if (std::strncmp(line, "RELOAD=", 7) == 0)
            {
                tmp.reload = std::atoi(line + 7);
            }
            else if (std::strncmp(line, "RELOADCNT=", 10) == 0)
            {
                tmp.reloadcnt = std::atoi(line + 10);
            }
        }

        std::fclose(f);

        if (tmp.levelPath.empty())
            return false;

        outData = tmp;
        return true;
    }

    bool SaveToDisk(const SaveData& data)
    {
        std::string path = BuildSavePath();

        FILE* f = std::fopen(path.c_str(), "wb");
        if (!f)
            return false;

        std::fprintf(f, "%s\n", kMagic);
        std::fprintf(f, "LEVEL=%s\n", data.levelPath.c_str());
        std::fprintf(f, "FLAT=%d\n", data.flatIndex);
        std::fprintf(f, "CAMX=%d\n", data.camX);
        std::fprintf(f, "CAMY=%d\n", data.camY);
        std::fprintf(f, "CAMZ=%d\n", data.camZ);
        std::fprintf(f, "CAMROT=%d\n", data.camRot);
        std::fprintf(f, "HP=%d\n", data.hp);
        std::fprintf(f, "WEAPON=%d\n", data.weapon);
        std::fprintf(f, "RELOAD=%d\n", data.reload);
        std::fprintf(f, "RELOADCNT=%d\n", data.reloadcnt);

        std::fclose(f);
        return true;
    }

    void SetCurrentLevelPath(const std::string& levelPath)
    {
        g_currentLevelPath = levelPath;
    }

    const std::string& GetCurrentLevelPath()
    {
        return g_currentLevelPath;
    }

    void SetCurrentFlat(int flatIndex)
    {
        g_currentFlat = flatIndex;
    }

    int GetCurrentFlat()
    {
        return g_currentFlat;
    }
}
