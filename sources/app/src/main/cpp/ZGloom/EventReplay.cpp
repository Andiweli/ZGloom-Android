#include "EventReplay.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "config.h"
#include "gloommap.h"

namespace
{
    std::vector<uint32_t> g_events;
    bool g_replaying = false;

    const char* kEventsFile = "last.events";

    std::string BuildEventsPath()
    {
        std::string path = Config::GetDataRoot();
        if (!path.empty())
        {
            const char last = path[path.size() - 1];
            if (last != '/' && last != '\\')
                path += "/";
        }
        path += kEventsFile;
        return path;
    }

    bool IsPersistentEvent(uint32_t ev)
    {
        // Gloom maps contain events 1..24.  Event 1 creates the initial map
        // objects and is already executed by GloomMap::Load().
        return ev >= 2 && ev <= 24;
    }

    void AddUniqueEvent(uint32_t ev)
    {
        if (!IsPersistentEvent(ev))
            return;

        if (std::find(g_events.begin(), g_events.end(), ev) == g_events.end())
            g_events.push_back(ev);
    }
}

namespace EventReplay
{
    void Clear()
    {
        g_events.clear();
        g_replaying = false;
    }

    void Record(uint32_t ev)
    {
        if (g_replaying)
            return;

        AddUniqueEvent(ev);
    }

    void SetEvents(const std::vector<uint32_t>& events)
    {
        g_events.clear();
        for (const uint32_t ev : events)
            AddUniqueEvent(ev);
    }

    const std::vector<uint32_t>& GetEvents()
    {
        return g_events;
    }

    bool HasReplay()
    {
        const std::string path = BuildEventsPath();
        FILE* f = std::fopen(path.c_str(), "rb");
        if (!f)
            return false;
        std::fclose(f);
        return true;
    }

    bool LoadFromDisk()
    {
        const std::string path = BuildEventsPath();
        FILE* f = std::fopen(path.c_str(), "rb");
        if (!f)
            return false;

        g_events.clear();

        uint32_t ev = 0;
        while (std::fread(&ev, sizeof(ev), 1, f) == 1)
            AddUniqueEvent(ev);

        const bool readError = (std::ferror(f) != 0);
        std::fclose(f);
        return !readError && !g_events.empty();
    }

    bool SaveToDisk()
    {
        const std::string path = BuildEventsPath();
        const std::string temporaryPath = path + ".tmp";

        FILE* f = std::fopen(temporaryPath.c_str(), "wb");
        if (!f)
            return false;

        bool ok = true;
        for (const uint32_t ev : g_events)
        {
            if (std::fwrite(&ev, sizeof(ev), 1, f) != 1)
            {
                ok = false;
                break;
            }
        }

        ok &= (std::fflush(f) == 0);
        ok &= (std::ferror(f) == 0);
        ok &= (std::fclose(f) == 0);

        if (!ok)
        {
            std::remove(temporaryPath.c_str());
            return false;
        }

        if (std::rename(temporaryPath.c_str(), path.c_str()) == 0)
            return true;

        // Compatibility fallback for desktop C runtimes that do not replace.
        std::remove(path.c_str());
        if (std::rename(temporaryPath.c_str(), path.c_str()) == 0)
            return true;

        std::remove(temporaryPath.c_str());
        return false;
    }

    void ReplayAll(GloomMap& map)
    {
        if (g_events.empty())
            return;

        const std::vector<uint32_t> snapshot = g_events;
        g_replaying = true;

        bool dummyTele = false;
        Teleport dummyTp;

        for (const uint32_t ev : snapshot)
        {
            map.ExecuteEvent(ev,
                             dummyTele,
                             dummyTp,
                             false,
                             EventExecutionMode::PersistentReplay);
        }

        g_replaying = false;
    }
}
