#pragma once
#include "crmfile.h"
#include <sdl2/SDL.h>
#include <string>
#include <cstddef>

class Font
{
    public:
        Font() = default;
        ~Font();
        bool Load(CrmFile& file);
        bool Load2(CrmFile& file);
        void SetPal(SDL_Surface* palsurface);
        void Blit(int x, int y, int character, SDL_Surface* dest, int scale);
        void PrintMessage(std::string message, int y, SDL_Surface* dest, int scale);
        void PrintMessageAt(const std::string& message, int x, int y, SDL_Surface* dest, int scale);
        void PrintMultiLineMessage(std::string message, int y, SDL_Surface* dest);
        void PrintMultiLineMessageProgressive(const std::string& message, int y, SDL_Surface* dest, std::size_t visibleCharacters);
        std::size_t CountTypewriterCharacters(const std::string& message) const;
        int MeasureText(const std::string& message, int scale = 1) const;

    private:
        static const int glyphs = 40;
        SDL_Surface* surfaces[glyphs] = {};
        SDL_Surface* surfaces32[glyphs] = {};
        int w = 0;
        int h = 0;

        void BlitUnderscore(int x, int y, SDL_Surface* dest, int scale);
        void PrintCharacter(char c, int x, int y, SDL_Surface* dest, int scale);

    public:
        SDL_Palette* GetPalette()
        {
            return surfaces[0] ? surfaces[0]->format->palette : nullptr;
        }
};
