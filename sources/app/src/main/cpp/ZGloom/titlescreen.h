#pragma once
#include <sdl2/SDL.h>
#include "font.h"
#include "script.h"

#include <vector>

class TitleScreen
{
public:
    enum TitleReturn
    {
        TITLERET_PLAY,
        TITLERET_SELECT,
        TITLERET_QUIT,
        TITLERET_NOTHING
    };

    TitleScreen();
    void Render(SDL_Surface* src, SDL_Surface* dest, Font& font);
    void Clock() { timer++; }
    void UpdateControllerHold(bool upHeld, bool downHeld);
    void ResetToMain() { status = TITLESTATUS_MAIN; selection = MAINENTRY_RESUME; timer = 0; heldDirection = 0; repeatCountdown = 0; }
    bool WantsPlainTitleBackground() const { return status != TITLESTATUS_MAIN; }
    TitleReturn Update(SDL_Event& tevent, int& levelout);
    void SetLevels(const std::vector<LevelDescriptor>& newLevels);

private:
    enum TITLESTATUS
    {
        TITLESTATUS_MAIN,
        TITLESTATUS_ABOUT,
        TITLESTATUS_SELECT
    };

    enum MAINENTRIES
    {
        MAINENTRY_RESUME,
        MAINENTRY_PLAY,
        MAINENTRY_SELECT,
        MAINENTRY_ABOUT,
        MAINENTRY_QUIT,
        MAINENTRY_END
    };

    std::vector<LevelDescriptor> levels;
    TITLESTATUS status;
    int selection;
    int timer;
    int heldDirection;
    int repeatCountdown;

    void MoveLevelSelection(int direction);
};
