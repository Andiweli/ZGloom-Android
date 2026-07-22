#include "script.h"
#include "crmfile.h"
#include "config.h"

#include <cerrno>
#include <climits>
#include <cctype>
#include <cstdlib>
#include <iostream>

namespace
{
    std::string Trim(const std::string& value)
    {
        std::size_t first = 0;
        while (first < value.size() && std::isspace(static_cast<unsigned char>(value[first])))
            ++first;

        std::size_t last = value.size();
        while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1])))
            --last;

        return value.substr(first, last - first);
    }

    bool StartsWith(const std::string& value, const char* prefix)
    {
        return value.size() >= 5 && value.compare(0, 5, prefix) == 0;
    }

    bool ParseFlatIndex(const std::string& token, int& flatIndex)
    {
        const std::string trimmed = Trim(token);
        const char* text = trimmed.c_str();
        errno = 0;
        char* end = nullptr;
        const long parsed = std::strtol(text, &end, 10);

        if (errno != 0 || end == text || !end || *end != '\0' || parsed < 0 || parsed > INT_MAX)
            return false;

        flatIndex = static_cast<int>(parsed);
        return true;
    }
}

Script::Script()
{
    CrmFile file;

    if (!file.Load(Config::GetScriptName().c_str()))
    {
        std::cout << "Could not load script!";
        numlines = 0;
        return;
    }

    lines.clear();
    numlines = 0;
    std::string tempstring;

    for (uint32_t i = 0; i < file.size; i++)
    {
        const char c = static_cast<char>(file.data[i]);

        if (c != 0x0a)
        {
            tempstring += c;
        }
        else
        {
            // Trim a possible CR from CR/LF scripts while preserving all other
            // script text exactly as authored.
            if (!tempstring.empty() && tempstring.back() == '\r')
                tempstring.pop_back();

            lines.push_back(tempstring);
            tempstring.clear();
        }
    }

    if (!tempstring.empty())
    {
        if (tempstring.back() == '\r')
            tempstring.pop_back();
        lines.push_back(tempstring);
    }

    numlines = static_cast<uint32_t>(lines.size());
    BuildLevelCatalog();
}

Script::ScriptOp Script::NextLine(std::string& name)
{
    if (line >= lines.size())
    {
        line = 0;
        return SOP_END;
    }

    while (line < lines.size() && (lines[line].empty() || lines[line][0] == ';'))
    {
        ++line;
        if (line >= lines.size())
        {
            line = 0;
            return SOP_END;
        }
    }

    const std::string& current = lines[line];

    if (StartsWith(current, "pict_"))
    {
        name = Trim(current.substr(5));
        ++line;
        return SOP_SETPICT;
    }
    if (StartsWith(current, "tile_"))
    {
        name = Trim(current.substr(5));
        ++line;
        return SOP_LOADFLAT;
    }
    if (StartsWith(current, "play_"))
    {
        name = Trim(current.substr(5));
        ++line;
        return SOP_PLAY;
    }
    if (StartsWith(current, "draw_"))
    {
        ++line;
        return SOP_DRAW;
    }
    if (StartsWith(current, "wait_"))
    {
        ++line;
        return SOP_WAIT;
    }
    if (StartsWith(current, "text_"))
    {
        name = current.substr(5);
        if (!name.empty() && name.back() == '\r')
            name.pop_back();
        ++line;
        return SOP_TEXT;
    }
    if (StartsWith(current, "song_"))
    {
        name = Trim(current.substr(5));
        ++line;
        return SOP_SONG;
    }

    ++line;
    if (line >= lines.size())
    {
        line = 0;
        return SOP_END;
    }
    return SOP_NOP;
}

void Script::BuildLevelCatalog()
{
    levels.clear();

    int currentFlat = -1;
    std::string currentPicture;
    std::string pendingDescription;
    uint32_t pendingTextLine = 0;
    uint32_t pendingEntryLine = 0;
    bool havePendingText = false;
    bool havePendingEntry = false;

    for (uint32_t i = 0; i < lines.size(); ++i)
    {
        const std::string entry = Trim(lines[i]);

        if (entry.empty() || entry[0] == ';')
            continue;

        if (StartsWith(entry, "pict_"))
        {
            currentPicture = Trim(entry.substr(5));
        }
        else if (StartsWith(entry, "tile_"))
        {
            int parsedFlat = -1;
            if (ParseFlatIndex(entry.substr(5), parsedFlat))
                currentFlat = parsedFlat;
        }
        else if (StartsWith(entry, "draw_"))
        {
            // Each playable block starts with draw_.  Remember the nearest one
            // so direct level selection still shows the selected intermission.
            pendingEntryLine = i;
            havePendingEntry = true;
        }
        else if (StartsWith(entry, "text_"))
        {
            pendingDescription = lines[i].substr(5);
            if (!pendingDescription.empty() && pendingDescription.back() == '\r')
                pendingDescription.pop_back();
            pendingTextLine = i;
            havePendingText = true;

            if (!havePendingEntry)
            {
                pendingEntryLine = i;
                havePendingEntry = true;
            }
        }
        else if (StartsWith(entry, "play_"))
        {
            const std::string mapPath = Trim(entry.substr(5));
            if (!mapPath.empty())
            {
                LevelDescriptor level;
                level.mapPath = mapPath;
                level.description = havePendingText ? Trim(pendingDescription) : std::string();
                level.pictureName = currentPicture;
                level.flatIndex = currentFlat;
                level.entryScriptLine = havePendingEntry ? pendingEntryLine : i;
                level.textScriptLine = havePendingText ? pendingTextLine : i;
                level.playScriptLine = i;
                levels.push_back(level);
            }

            pendingDescription.clear();
            havePendingText = false;
            havePendingEntry = false;
        }
    }
}

void Script::GetLevelNames(std::vector<std::string>& names) const
{
    names.clear();
    names.reserve(levels.size());
    for (const LevelDescriptor& level : levels)
        names.push_back(level.description);
}

bool Script::SeekToLevel(std::size_t levelIndex)
{
    if (levelIndex >= levels.size())
        return false;

    const LevelDescriptor& level = levels[levelIndex];
    if (level.entryScriptLine >= lines.size() || level.playScriptLine >= lines.size())
        return false;

    line = level.entryScriptLine;
    return true;
}

void Script::SeekAfterPlayFor(const std::string& levelName)
{
    const std::string wanted = Trim(levelName);
    for (const LevelDescriptor& level : levels)
    {
        if (level.mapPath == wanted)
        {
            line = level.playScriptLine + 1;
            if (line > lines.size())
                line = static_cast<uint32_t>(lines.size());
            return;
        }
    }
}

bool Script::GetFlatForLevel(const std::string& levelName, int& flatIndex) const
{
    const std::string wanted = Trim(levelName);
    for (const LevelDescriptor& level : levels)
    {
        if (level.mapPath == wanted && level.flatIndex >= 0)
        {
            flatIndex = level.flatIndex;
            return true;
        }
    }

    return false;
}
