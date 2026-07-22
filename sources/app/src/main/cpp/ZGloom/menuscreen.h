#pragma once
#include <sdl2/SDL.h>
#include "font.h"

#include <vector>
#include "config.h"

class MenuScreen
{
public:
    enum MenuReturn
    {
        MENURET_PLAY,
        MENURET_QUIT,
        MENURET_NOTHING
    };

    MenuScreen();
    void Render(SDL_Surface* src, SDL_Surface* dest, Font& font);
    void Clock() { timer++; };
    MenuReturn Update(SDL_Event& tevent);

private:

    enum MenuEntryAction
    {
        ACTION_SWITCHMENU,
        ACTION_BOOL,
        ACTION_INT,
        ACTION_RETURN,
        ACTION_LABEL
    };

    struct MenuEntry
    {
        std::string name;
        int(*getval)();
        void(*setval)(int);

        MenuEntryAction action;
        int arg;

        MenuEntry(std::string _name, MenuEntryAction _action, int _arg,
                  int(*_getval)(), void(*_setval)(int))
        {
            arg    = _arg;
            name   = _name;
            action = _action;
            getval = _getval;
            setval = _setval;
        };
    };

    enum MENUSTATUS
    {
        MENUSTATUS_MAIN,
        MENUSTATUS_KEYCONFIG,
        MENUSTATUS_SOUNDOPTIONS,
        MENUSTATUS_CONTROLOPTIONS,
        MENUSTATUS_DISPLAYOPTIONS,
        MENUSTATUS_CHEATOPTIONS,
        MENUSTATUS_EFFECTSOPTIONS,
        MENUSTATUS_COUNT
    };

    MENUSTATUS status;
    int selection;
    int timer;

    // Each submenu remembers exactly where it was entered from. Returning with
    // B/ESC therefore restores the parent entry instead of jumping to CONTINUE.
    MENUSTATUS parentStatus[MENUSTATUS_COUNT];
    int parentSelection[MENUSTATUS_COUNT];
    bool parentValid[MENUSTATUS_COUNT];

    void HandleKeyMenu(SDL_Keycode sym);
    MenuReturn HandleStandardMenu(SDL_Keycode sym, std::vector<MenuEntry>& menu);
    void DisplayStandardMenu(std::vector<MenuEntry>& menu, bool flash, int scale, SDL_Surface* dest, Font& font);

    void EnterSubmenu(MENUSTATUS nextStatus);
    bool ReturnToParentMenu();
    static bool IsBinaryIntEntry(const MenuEntry& entry);
    static void IncrementIntEntry(MenuEntry& entry);
    static void DecrementIntEntry(MenuEntry& entry);

    std::vector<MenuEntry> soundmenu;
    std::vector<MenuEntry> mainmenu;
    std::vector<MenuEntry> controlmenu;
    std::vector<MenuEntry> displaymenu;
    std::vector<MenuEntry> cheatmenu;
};
