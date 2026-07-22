#include "titlescreen.h"
#include "SaveSystem.h"

#include <algorithm>
#include <cctype>
#include <string>

extern bool g_RequestTitleContinue;

namespace
{
    std::string TrimSpaces(const std::string& value)
    {
        std::size_t first = 0;
        while (first < value.size() && value[first] == ' ')
            ++first;

        std::size_t last = value.size();
        while (last > first && value[last - 1] == ' ')
            --last;

        return value.substr(first, last - first);
    }

    void AppendSpace(std::string& output)
    {
        if (!output.empty() && output.back() != ' ')
            output.push_back(' ');
    }

    std::string FormatMapName(const std::string& mapPath)
    {
        std::string output;
        output.reserve(mapPath.size());

        for (unsigned char raw : mapPath)
        {
            if (std::isalnum(raw))
            {
                output.push_back(static_cast<char>(std::toupper(raw)));
            }
            else if (raw == '_')
            {
                // Script/map separators are shown as a dot in the level list:
                // MAP1_1 becomes MAP1.1, while compact names such as MAP1G
                // remain unchanged.
                output.push_back('.');
            }
            else
            {
                AppendSpace(output);
            }
        }

        output = TrimSpaces(output);
        return output.empty() ? std::string("UNKNOWN") : output;
    }

    std::string ShortenDescription(const std::string& description)
    {
        std::string output;
        output.reserve(description.size());

        int wordCount = 0;
        bool inWord = false;

        for (unsigned char raw : description)
        {
            if (std::isalnum(raw))
            {
                if (!inWord)
                {
                    if (wordCount >= 4)
                    {
                        output = TrimSpaces(output);
                        while (!output.empty() && output.back() == '.')
                            output.pop_back();
                        return output + "...";
                    }
                    ++wordCount;
                    inWord = true;
                }

                output.push_back(static_cast<char>(std::toupper(raw)));
                continue;
            }

            // Apostrophes are omitted because the original menu font has no
            // apostrophe glyph: DON'T becomes DONT without creating a gap.
            if (raw == '\'' || raw == '`')
                continue;

            const bool sentencePunctuation =
                raw == '.' || raw == '!' || raw == '?' || raw == ':' || raw == ';' || raw == ',';

            if (sentencePunctuation)
            {
                output = TrimSpaces(output);
                if (raw == '!')
                    output.push_back('!');
                else if (raw == ':')
                    output.push_back(':');
                else
                    output.push_back('.');
                return output;
            }

            if (std::isspace(raw) || raw == '-' || raw == '/' || raw == '\\')
            {
                AppendSpace(output);
                inWord = false;
            }
            else
            {
                // Unsupported punctuation acts as a word separator but is not
                // allowed to create invisible holes in the fixed-width label.
                AppendSpace(output);
                inWord = false;
            }
        }

        return TrimSpaces(output);
    }

    std::string FitLabelToWidth(const std::string& mapName,
                                const std::string& shortDescription,
                                Font& font,
                                int maxWidth)
    {
        const std::string prefix = mapName + " ";
        std::string body = shortDescription;
        std::string label = prefix + body;

        if (font.MeasureText(label, 1) <= maxWidth)
            return label;

        while (!body.empty() && body.back() == '.')
            body.pop_back();
        body = TrimSpaces(body);

        while (!body.empty() && font.MeasureText(prefix + body + "...", 1) > maxWidth)
            body.pop_back();

        body = TrimSpaces(body);
        if (body.empty())
            return mapName;

        return prefix + body + "...";
    }

    std::string BuildLevelLabel(const LevelDescriptor& level, Font& font, int maxWidth)
    {
        const std::string mapName = FormatMapName(level.mapPath);
        std::string description = ShortenDescription(level.description);
        if (description.empty())
            description = "NO DESCRIPTION";
        return FitLabelToWidth(mapName, description, font, maxWidth);
    }
}

void TitleScreen::SetLevels(const std::vector<LevelDescriptor>& newLevels)
{
    levels = newLevels;
    if (selection < 0 || selection >= static_cast<int>(levels.size()))
        selection = 0;
}

void TitleScreen::Render(SDL_Surface* src, SDL_Surface* dest, Font& font)
{
    SDL_BlitSurface(src, nullptr, dest, nullptr);
    const bool flash = (timer / 5) & 1;

    if (status == TITLESTATUS_MAIN)
    {
        const bool hasSave = SaveSystem::HasSave();

        if (!hasSave && selection == MAINENTRY_RESUME)
            selection = MAINENTRY_PLAY;

        const int lineStep = 15;

        if (hasSave)
        {
            const int menuY = 91;
            if (flash || (selection != MAINENTRY_RESUME)) font.PrintMessage("RESUME SAVED POSITION", menuY + lineStep * 0, dest, 1);
            if (flash || (selection != MAINENTRY_PLAY))   font.PrintMessage("START NEW GAME",          menuY + lineStep * 1, dest, 1);
            if (flash || (selection != MAINENTRY_SELECT)) font.PrintMessage("SELECT LEVEL",             menuY + lineStep * 2, dest, 1);
            if (flash || (selection != MAINENTRY_ABOUT))  font.PrintMessage("ABOUT GLOOM AND THIS PORT", menuY + lineStep * 3, dest, 1);
            if (flash || (selection != MAINENTRY_QUIT))   font.PrintMessage("EXIT",                     menuY + lineStep * 4, dest, 1);
        }
        else
        {
            const int menuY = 98;
            if (flash || (selection != MAINENTRY_PLAY))   font.PrintMessage("START NEW GAME",          menuY + lineStep * 0, dest, 1);
            if (flash || (selection != MAINENTRY_SELECT)) font.PrintMessage("SELECT LEVEL",             menuY + lineStep * 1, dest, 1);
            if (flash || (selection != MAINENTRY_ABOUT))  font.PrintMessage("ABOUT GLOOM AND THIS PORT", menuY + lineStep * 2, dest, 1);
            if (flash || (selection != MAINENTRY_QUIT))   font.PrintMessage("EXIT",                     menuY + lineStep * 3, dest, 1);
        }

        font.PrintMessage("ZGLOOM ANDROID 08.2026", 243, dest, 1);
    }
    else if (status == TITLESTATUS_SELECT)
    {
        font.PrintMessage("SELECT LEVEL", 4, dest, 1);

        if (levels.empty())
        {
            font.PrintMessage("NO LEVELS FOUND", 116, dest, 1);
            return;
        }

        const int visibleRows = 21;
        const int topY = 22;
        const int lineStep = 10;
        const int leftX = 8;
        const int maxWidth = std::max(0, dest->w - leftX * 2);

        int first = selection - visibleRows / 2;
        const int maxFirst = std::max(0, static_cast<int>(levels.size()) - visibleRows);
        first = std::max(0, std::min(first, maxFirst));
        const int last = std::min(static_cast<int>(levels.size()), first + visibleRows);

        for (int i = first; i < last; ++i)
        {
            if (!flash && i == selection)
                continue;

            const int row = i - first;
            const std::string label = BuildLevelLabel(levels[i], font, maxWidth);
            font.PrintMessageAt(label, leftX, topY + row * lineStep, dest, 1);
        }
    }
    else
    {
        font.PrintMessage("GLOOM BLACK MAGIC ENGINE", 40, dest, 1);
        font.PrintMessage("BY BLACK MAGIC", 50, dest, 1);

        font.PrintMessage("PROGRAMMED BY MARK SIBLY", 65, dest, 1);
        font.PrintMessage("GRAPHICS BY THE BUTLER BROTHERS", 75, dest, 1);
        font.PrintMessage("MUSIC BY KEV STANNARD", 85, dest, 1);
        font.PrintMessage("AUDIO BY BLACK MAGIC", 95, dest, 1);
        font.PrintMessage("DECRUNCHCODE BY THOMAS SCHWARZ", 105, dest, 1);

        font.PrintMessage("GLOOM3 AND ZOMBIE MASSACRE", 120, dest, 1);
        font.PrintMessage("BY ALPHA SOFTWARE", 130, dest, 1);

        font.PrintMessage("ABOUT THIS PORT", 145, dest, 1);
        font.PrintMessage("CODE AND FIXES BY ANDIWELI", 160, dest, 1);
        font.PrintMessage("AMBIENCE BY PROPHET", 170, dest, 1);
        font.PrintMessage("BASED ON PORT BY SWIZPIG", 185, dest, 1);

        font.PrintMessage("IN HONOR AND MEMORY OF MARK SIBLY", 200, dest, 1);
    }
}

TitleScreen::TitleScreen()
{
    status = TITLESTATUS_MAIN;
    selection = 0;
    timer = 0;
    heldDirection = 0;
    repeatCountdown = 0;
}

void TitleScreen::MoveLevelSelection(int direction)
{
    if (status != TITLESTATUS_SELECT || levels.empty() || direction == 0)
        return;

    selection += direction;
    const int levelCount = static_cast<int>(levels.size());
    if (selection >= levelCount)
        selection = 0;
    else if (selection < 0)
        selection = levelCount - 1;
}

void TitleScreen::UpdateControllerHold(bool upHeld, bool downHeld)
{
    if (status != TITLESTATUS_SELECT || levels.empty())
    {
        heldDirection = 0;
        repeatCountdown = 0;
        return;
    }

    int direction = 0;
    if (downHeld && !upHeld)
        direction = 1;
    else if (upHeld && !downHeld)
        direction = -1;

    if (direction == 0)
    {
        heldDirection = 0;
        repeatCountdown = 0;
        return;
    }

    // The SDL_CONTROLLERBUTTONDOWN event already performs the first step.
    // Wait roughly 320 ms before auto-repeat, then scroll every 80 ms.
    // Clocked at the engine timer rate of 25 Hz this feels responsive without
    // racing through the list on a short tap.
    if (direction != heldDirection)
    {
        heldDirection = direction;
        repeatCountdown = 8;
        return;
    }

    if (repeatCountdown > 0)
    {
        --repeatCountdown;
        return;
    }

    MoveLevelSelection(direction);
    repeatCountdown = 1;
}

TitleScreen::TitleReturn TitleScreen::Update(SDL_Event& tevent, int& levelout)
{
    if (tevent.type != SDL_KEYDOWN)
        return TITLERET_NOTHING;

    if (status == TITLESTATUS_MAIN)
    {
        const bool hasSave = SaveSystem::HasSave();

        if (!hasSave && selection == MAINENTRY_RESUME)
            selection = MAINENTRY_PLAY;

        switch (tevent.key.keysym.sym)
        {
            case SDLK_DOWN:
                if (hasSave)
                {
                    ++selection;
                    if (selection >= MAINENTRY_END)
                        selection = 0;
                }
                else
                {
                    ++selection;
                    if (selection > MAINENTRY_QUIT)
                        selection = MAINENTRY_PLAY;
                }
                break;

            case SDLK_UP:
                if (hasSave)
                {
                    --selection;
                    if (selection < 0)
                        selection = MAINENTRY_END - 1;
                }
                else
                {
                    --selection;
                    if (selection < MAINENTRY_PLAY)
                        selection = MAINENTRY_QUIT;
                }
                break;

            case SDLK_SPACE:
            case SDLK_RETURN:
            case SDLK_LCTRL:
                if (hasSave && selection == MAINENTRY_RESUME)
                {
                    g_RequestTitleContinue = true;
                    return TITLERET_PLAY;
                }
                if (selection == MAINENTRY_PLAY)
                    return TITLERET_PLAY;
                if (selection == MAINENTRY_QUIT)
                    return TITLERET_QUIT;
                if (selection == MAINENTRY_ABOUT)
                    status = TITLESTATUS_ABOUT;
                if (selection == MAINENTRY_SELECT)
                {
                    selection = 0;
                    heldDirection = 0;
                    repeatCountdown = 0;
                    status = TITLESTATUS_SELECT;
                }
                break;

            default:
                break;
        }
    }
    else if (status == TITLESTATUS_SELECT)
    {
        switch (tevent.key.keysym.sym)
        {
            case SDLK_DOWN:
                MoveLevelSelection(1);
                break;

            case SDLK_UP:
                MoveLevelSelection(-1);
                break;

            case SDLK_ESCAPE:
                status = TITLESTATUS_MAIN;
                selection = MAINENTRY_SELECT;
                heldDirection = 0;
                repeatCountdown = 0;
                break;

            case SDLK_SPACE:
            case SDLK_RETURN:
            case SDLK_LCTRL:
                if (!levels.empty() && selection >= 0 && selection < static_cast<int>(levels.size()))
                {
                    levelout = selection;
                    status = TITLESTATUS_MAIN;
                    selection = MAINENTRY_SELECT;
                    heldDirection = 0;
                    repeatCountdown = 0;
                    return TITLERET_SELECT;
                }
                break;

            default:
                break;
        }
    }
    else
    {
        status = TITLESTATUS_MAIN;
        heldDirection = 0;
        repeatCountdown = 0;
    }

    return TITLERET_NOTHING;
}
