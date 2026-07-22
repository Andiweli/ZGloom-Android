#include "SaveSystem.h"

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "config.h"

namespace
{
    std::string g_currentLevelPath;
    int         g_currentFlat = -1;

    const char* kSaveFileName = "savepos.txt";
    const char* kMagicV1      = "ZGLOOM_SAVE_V1";
    const char* kMagicV2      = "ZGLOOM_SAVE_V2";

    std::string BuildSavePath()
    {
        std::string path = Config::GetDataRoot();
        if (!path.empty())
        {
            const char last = path[path.size() - 1];
            if (last != '/' && last != '\\')
                path += "/";
        }
        path += kSaveFileName;
        return path;
    }

    void StripLineEnd(char* line)
    {
        size_t len = std::strlen(line);
        while (len && (line[len - 1] == '\n' || line[len - 1] == '\r'))
        {
            line[len - 1] = '\0';
            --len;
        }
    }

    bool ParseInt(const char* text, int& value)
    {
        if (!text || !*text)
            return false;

        errno = 0;
        char* end = nullptr;
        const long parsed = std::strtol(text, &end, 10);
        if (errno != 0 || end == text || *end != '\0' || parsed < INT_MIN || parsed > INT_MAX)
            return false;

        value = static_cast<int>(parsed);
        return true;
    }

    bool ParseUInt32(const char* text, uint32_t& value)
    {
        if (!text || !*text || *text == '-')
            return false;

        errno = 0;
        char* end = nullptr;
        const unsigned long parsed = std::strtoul(text, &end, 10);
        if (errno != 0 || end == text || *end != '\0' || parsed > 0xFFFFFFFFUL)
            return false;

        value = static_cast<uint32_t>(parsed);
        return true;
    }

    bool CommitTemporarySave(const std::string& temporaryPath, const std::string& finalPath)
    {
        // POSIX/Android rename replaces the destination atomically.
        if (std::rename(temporaryPath.c_str(), finalPath.c_str()) == 0)
            return true;

        // Some desktop C runtimes do not replace an existing destination.
        // Preserve the old save as a backup while replacing it.
        const std::string backupPath = finalPath + ".bak";
        std::remove(backupPath.c_str());

        const bool hadOldSave = (std::rename(finalPath.c_str(), backupPath.c_str()) == 0);
        if (std::rename(temporaryPath.c_str(), finalPath.c_str()) == 0)
        {
            if (hadOldSave)
                std::remove(backupPath.c_str());
            return true;
        }

        if (hadOldSave)
            std::rename(backupPath.c_str(), finalPath.c_str());

        std::remove(temporaryPath.c_str());
        return false;
    }
}

namespace SaveSystem
{
    bool HasSave()
    {
        const std::string path = BuildSavePath();
        FILE* f = std::fopen(path.c_str(), "rb");
        if (!f)
            return false;

        char magic[64] = {0};
        const bool readOk = (std::fgets(magic, sizeof(magic), f) != nullptr);
        std::fclose(f);
        if (!readOk)
            return false;

        StripLineEnd(magic);
        return std::strcmp(magic, kMagicV1) == 0 || std::strcmp(magic, kMagicV2) == 0;
    }

    bool LoadFromDisk(SaveData& outData)
    {
        const std::string path = BuildSavePath();
        FILE* f = std::fopen(path.c_str(), "rb");
        if (!f)
            return false;

        char line[512];
        if (!std::fgets(line, sizeof(line), f))
        {
            std::fclose(f);
            return false;
        }

        StripLineEnd(line);

        SaveData tmp;
        if (std::strcmp(line, kMagicV2) == 0)
        {
            tmp.formatVersion = 2;
        }
        else if (std::strcmp(line, kMagicV1) == 0)
        {
            tmp.formatVersion = 1;
        }
        else
        {
            std::fclose(f);
            return false;
        }

        int expectedEventCount = -1;
        bool parseError = false;

        while (std::fgets(line, sizeof(line), f))
        {
            StripLineEnd(line);

            if (std::strncmp(line, "LEVEL=", 6) == 0)
            {
                tmp.levelPath = line + 6;
            }
            else if (std::strncmp(line, "FLAT=", 5) == 0)
            {
                parseError |= !ParseInt(line + 5, tmp.flatIndex);
            }
            else if (std::strncmp(line, "CAMX=", 5) == 0)
            {
                parseError |= !ParseInt(line + 5, tmp.camX);
            }
            else if (std::strncmp(line, "CAMY=", 5) == 0)
            {
                parseError |= !ParseInt(line + 5, tmp.camY);
            }
            else if (std::strncmp(line, "CAMZ=", 5) == 0)
            {
                parseError |= !ParseInt(line + 5, tmp.camZ);
            }
            else if (std::strncmp(line, "CAMROT=", 7) == 0)
            {
                parseError |= !ParseInt(line + 7, tmp.camRot);
            }
            else if (std::strncmp(line, "HP=", 3) == 0)
            {
                parseError |= !ParseInt(line + 3, tmp.hp);
            }
            else if (std::strncmp(line, "LIVES=", 6) == 0)
            {
                parseError |= !ParseInt(line + 6, tmp.lives);
                if (tmp.lives < 0 || tmp.lives > 5)
                    parseError = true;
            }
            else if (std::strncmp(line, "WEAPON=", 7) == 0)
            {
                parseError |= !ParseInt(line + 7, tmp.weapon);
            }
            else if (std::strncmp(line, "RELOAD=", 7) == 0)
            {
                parseError |= !ParseInt(line + 7, tmp.reload);
            }
            else if (std::strncmp(line, "RELOADCNT=", 10) == 0)
            {
                parseError |= !ParseInt(line + 10, tmp.reloadcnt);
            }
            else if (tmp.formatVersion >= 2 && std::strncmp(line, "EVENTCOUNT=", 11) == 0)
            {
                parseError |= !ParseInt(line + 11, expectedEventCount);
                if (expectedEventCount < 0 || expectedEventCount > 1024)
                    parseError = true;
            }
            else if (tmp.formatVersion >= 2 && std::strncmp(line, "EVENT=", 6) == 0)
            {
                uint32_t eventId = 0;
                if (ParseUInt32(line + 6, eventId))
                    tmp.eventHistory.push_back(eventId);
                else
                    parseError = true;
            }
        }

        const bool ioError = (std::ferror(f) != 0);
        std::fclose(f);

        if (ioError || parseError || tmp.levelPath.empty())
            return false;

        if (tmp.formatVersion >= 2)
        {
            if (expectedEventCount < 0 ||
                static_cast<size_t>(expectedEventCount) != tmp.eventHistory.size())
            {
                return false;
            }
        }

        outData = tmp;
        return true;
    }

    bool SaveToDisk(const SaveData& data)
    {
        if (data.levelPath.empty())
            return false;

        const std::string path = BuildSavePath();
        const std::string temporaryPath = path + ".tmp";

        FILE* f = std::fopen(temporaryPath.c_str(), "wb");
        if (!f)
            return false;

        bool ok = true;
        ok &= std::fprintf(f, "%s\n", kMagicV2) >= 0;
        ok &= std::fprintf(f, "LEVEL=%s\n", data.levelPath.c_str()) >= 0;
        ok &= std::fprintf(f, "FLAT=%d\n", data.flatIndex) >= 0;
        ok &= std::fprintf(f, "CAMX=%d\n", data.camX) >= 0;
        ok &= std::fprintf(f, "CAMY=%d\n", data.camY) >= 0;
        ok &= std::fprintf(f, "CAMZ=%d\n", data.camZ) >= 0;
        ok &= std::fprintf(f, "CAMROT=%d\n", data.camRot) >= 0;
        ok &= std::fprintf(f, "HP=%d\n", data.hp) >= 0;
        ok &= std::fprintf(f, "LIVES=%d\n", std::max(0, std::min(5, data.lives))) >= 0;
        ok &= std::fprintf(f, "WEAPON=%d\n", data.weapon) >= 0;
        ok &= std::fprintf(f, "RELOAD=%d\n", data.reload) >= 0;
        ok &= std::fprintf(f, "RELOADCNT=%d\n", data.reloadcnt) >= 0;
        ok &= std::fprintf(f, "EVENTCOUNT=%lu\n",
                           static_cast<unsigned long>(data.eventHistory.size())) >= 0;

        for (const uint32_t eventId : data.eventHistory)
            ok &= std::fprintf(f, "EVENT=%lu\n", static_cast<unsigned long>(eventId)) >= 0;

        ok &= (std::fflush(f) == 0);
        ok &= (std::ferror(f) == 0);
        ok &= (std::fclose(f) == 0);

        if (!ok)
        {
            std::remove(temporaryPath.c_str());
            return false;
        }

        return CommitTemporarySave(temporaryPath, path);
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
