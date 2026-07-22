#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct LevelDescriptor
{
    std::string mapPath;
    std::string description;
    std::string pictureName;
    int flatIndex = -1;
    uint32_t entryScriptLine = 0;
    uint32_t textScriptLine = 0;
    uint32_t playScriptLine = 0;
};

class Script
{
    public:
        Script();
        uint32_t numlines;
        std::vector<std::string> lines;
        void Reset()
        {
            line = 0;
        };

        enum ScriptOp
        {
            SOP_LOADFLAT,
            SOP_LOADMAP,
            SOP_SETPICT,
            SOP_DRAW,
            SOP_TEXT,
            SOP_WAIT,
            SOP_PLAY,
            SOP_SONG,
            SOP_END,
            SOP_NOP
        };

        ScriptOp NextLine(std::string& name);

        // Compatibility helper retained for older call sites.  New code should
        // use GetLevels(), which binds each description to its actual play_,
        // tile_ and pict_ script context.
        void GetLevelNames(std::vector<std::string>& names) const;

        const std::vector<LevelDescriptor>& GetLevels() const
        {
            return levels;
        }

        bool SeekToLevel(std::size_t levelIndex);
        void SeekAfterPlayFor(const std::string& levelName);
        bool GetFlatForLevel(const std::string& levelName, int& flatIndex) const;

    private:
        void BuildLevelCatalog();

        uint32_t line = 0;
        std::vector<LevelDescriptor> levels;
};
