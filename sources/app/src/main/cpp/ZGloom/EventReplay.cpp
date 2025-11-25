#include "EventReplay.h"

#include <vector>
#include <cstdio>

#include "config.h"
#include "gloommap.h"

namespace
{
    std::vector<uint32_t> g_events;

    const char* kEventsFile = "last.events";

    std::string BuildEventsPath()
    {
        std::string path = Config::GetDataRoot();
        if (!path.empty())
        {
            char last = path[path.size() - 1];
            if (last != '/' && last != '\\')
                path += "/";
        }
        path += kEventsFile;
        return path;
    }
}

namespace EventReplay
{
    void Clear()
    {
        g_events.clear();
    }

    void Record(uint32_t ev)
    {
        g_events.push_back(ev);
    }

    bool HasReplay()
    {
        std::string path = BuildEventsPath();
        FILE* f = std::fopen(path.c_str(), "rb");
        if (!f)
            return false;
        std::fclose(f);
        return true;
    }

    bool LoadFromDisk()
    {
        std::string path = BuildEventsPath();
        FILE* f = std::fopen(path.c_str(), "rb");
        if (!f)
            return false;

        g_events.clear();
        uint32_t ev;
        while (std::fread(&ev, sizeof(ev), 1, f) == 1)
        {
            g_events.push_back(ev);
        }

        std::fclose(f);
        return !g_events.empty();
    }

    bool SaveToDisk()
    {
        std::string path = BuildEventsPath();
        FILE* f = std::fopen(path.c_str(), "wb");
        if (!f)
            return false;

        for (uint32_t ev : g_events)
        {
            std::fwrite(&ev, sizeof(ev), 1, f);
        }

        std::fclose(f);
        return true;
    }

    void ReplayAll(GloomMap& map)
    {
        if (g_events.empty())
            return;

        bool     dummyTele = false;
        Teleport dummyTp;

        for (uint32_t ev : g_events)
        {
            map.ExecuteEvent(ev, dummyTele, dummyTp);
        }
    }
}
