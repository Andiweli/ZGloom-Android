#include "font.h"
#include <algorithm>
#include <cctype>
#include <vector>

namespace
{
    bool IsTypewriterCharacter(char c)
    {
        const unsigned char value = static_cast<unsigned char>(c);
        return (c >= '0' && c <= '9') ||
               (c >= 'a' && c <= 'z') ||
               (c >= 'A' && c <= 'Z') ||
               c == '!' || c == '.' || c == ':' || c == '_' || value == 127;
    }

    std::string TrimLine(const std::string& value)
    {
        std::size_t first = 0;
        while (first < value.size() && value[first] == ' ')
            ++first;

        std::size_t last = value.size();
        while (last > first && value[last - 1] == ' ')
            --last;

        return value.substr(first, last - first);
    }

    void WrapSegment(const std::string& source, std::size_t maxCharacters, std::vector<std::string>& lines)
    {
        std::string remaining = TrimLine(source);
        if (remaining.empty())
        {
            lines.push_back(std::string());
            return;
        }

        while (remaining.size() > maxCharacters)
        {
            std::size_t split = remaining.rfind(' ', maxCharacters);
            if (split == std::string::npos || split == 0)
                split = maxCharacters;

            lines.push_back(TrimLine(remaining.substr(0, split)));
            remaining = TrimLine(remaining.substr(split));
        }

        lines.push_back(remaining);
    }

    std::vector<std::string> BuildIntermissionLines(const std::string& message, std::size_t maxCharacters)
    {
        std::vector<std::string> lines;
        std::string segment;

        for (char c : message)
        {
            if (c == '\\')
            {
                WrapSegment(segment, maxCharacters, lines);
                segment.clear();
            }
            else if (c != '\r' && c != '\n')
            {
                segment.push_back(c);
            }
        }

        WrapSegment(segment, maxCharacters, lines);
        return lines;
    }
}

static uint16_t Get16(const uint8_t* p)
{
	return (static_cast<uint16_t>(p[0])) << 8 | static_cast<uint16_t>(p[1]);
}

static uint32_t Get32(const uint8_t* p)
{
	return (static_cast<uint16_t>(p[0])) << 24 | (static_cast<uint16_t>(p[1]) << 16) | (static_cast<uint16_t>(p[2])) << 8 | (static_cast<uint16_t>(p[3]) << 0);
}

void Font::SetPal(SDL_Surface* palsurface)
{
	for (int i = 0; i < glyphs; i++)
	{
		SDL_SetPaletteColors(surfaces[i]->format->palette, palsurface->format->palette->colors, 0, palsurface->format->palette->ncolors);
	}
}

bool Font::Load2(CrmFile& file)
{
	w = 8;
	h = 10;

	for (int i = 0; i < glyphs; i++)
	{
		uint32_t pos = Get32(file.data + 4 + 4 * i);

		uint32_t maskpos = pos + 10;

		pos += 4;
		uint32_t modulo = Get16(file.data + pos);

		pos += 2;
		uint32_t widthheight = Get16(file.data + pos);

		uint32_t width = (widthheight & 0x3F) * 16;
		uint32_t height = (widthheight >> 6);

		width = modulo * 8;

		surfaces[i]   = SDL_CreateRGBSurface(0, width, height, 8, 0, 0, 0, 0);
		surfaces32[i] = SDL_CreateRGBSurface(0, width, height, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
		SDL_SetColorKey(surfaces[i], 1, 0);

		//printf("%i %i %i %i\n", shapepos, maskpos, width, height);

		// assume always 2 bitplanes?

		for (uint32_t plane = 0; plane < 2; plane++)
		{
			for (uint32_t y = 0; y < height; y++)
			{
				for (uint32_t x = 0; x < width; x++)
				{
					uint8_t thebyte = file.data[maskpos + x / 8 + y *((width + 7) / 8) + plane*height*((width + 7) / 8)];

					if (plane == 0)
					{
						((char*)surfaces[i]->pixels)[x + y*surfaces[i]->pitch] = 0;
					}

					if (thebyte & (1 << (7 - (x % 8))))
					{
						((char*)surfaces[i]->pixels)[x + y*surfaces[i]->pitch] |= (1 << plane);
					}
				}
			}

			// now the palette

			uint32_t pos = Get32(file.data);
			int colnum = 0;
			while (pos < file.size)
			{
				SDL_Color col;
				col.a = 0xFF;
				col.r = file.data[pos + 0] & 0xf;
				col.g = file.data[pos + 1] >> 4;
				col.b = file.data[pos + 1] & 0xF;

				col.r <<= 4;
				col.g <<= 4;
				col.b <<= 4;

				SDL_SetPaletteColors(surfaces[i]->format->palette, &col, colnum, 1);

				colnum++;
				pos += 2;
			}
		}

		// SDL does not like scaled blits from 8->32 surfaces
		SDL_BlitSurface(surfaces[i], NULL, surfaces32[i], NULL);
	}
	return true;
}

bool Font::Load(CrmFile& file)
{
	w = 6;
	h = 8;

	for (int i = 0; i < glyphs; i++)
	{
		uint32_t pos = Get32(file.data + 4 + 4*i);

		uint32_t maskpos = pos + 8;

		pos += 4;
		uint32_t modulo = Get16(file.data + pos);

		pos += 2;
		uint32_t widthheight = Get16(file.data + pos);

		uint32_t width = (widthheight & 0x3F) * 16;
		uint32_t height = (widthheight >> 6);

		width = modulo * 8;

		surfaces[i] = SDL_CreateRGBSurface(0, width, height / 7, 8, 0, 0, 0, 0);
		SDL_SetColorKey(surfaces[i], 1, 0);

		//printf("%i %i %i %i\n", shapepos, maskpos, width, height);

		// assume always 7 bitplanes?

		for (uint32_t y = 0; y < height / 7; y++)
		{
			for (uint32_t x = 0; x < width; x++)
			{
				for (uint32_t plane = 0; plane < 7; plane++)
				{
					uint8_t thebyte = file.data[maskpos + (x + (y * 7 + plane)*width) / 8];

					if (plane == 0)
					{
						((char*)surfaces[i]->pixels)[x+y*surfaces[i]->pitch] = 0;
					}

					if (thebyte & (1 << (7 - (x % 8))))
					{
						((char*)surfaces[i]->pixels)[x+y*surfaces[i]->pitch] |= (1<<plane);
					}
				}
			}

			// now the palette

			uint32_t pos = Get32(file.data);
			int colnum = 0;
			while (pos < file.size)
			{
				SDL_Color col;
				col.a = 0xFF;
				col.r = file.data[pos + 0] & 0xf;
				col.g = file.data[pos + 1] >> 4;
				col.b = file.data[pos + 1] & 0xF;

				col.r <<= 4;
				col.g <<= 4;
				col.b <<= 4;

				SDL_SetPaletteColors(surfaces[i]->format->palette, &col, colnum, 1);

				colnum++;
				pos += 2;
			}
		}
	}
	return true;
}

void Font::Blit(int x, int y, int character, SDL_Surface* dest, int scale)
{
	SDL_Rect srcrect, dstrect;

	srcrect.w = w+1;
	srcrect.h = h;
	srcrect.x = 0;
	srcrect.y = 0;

	dstrect.w = (w+1)*scale;
	dstrect.h = (h*scale);
	dstrect.x = x;
	dstrect.y = y;

	if (scale == 1)
	{
		SDL_BlitSurface(surfaces[character], &srcrect, dest, &dstrect);
	}
	else
	{
		// these are ingame blits, and SDL does not like scaled 8->32 blits
		SDL_BlitScaled(surfaces32[character], &srcrect, dest, &dstrect);
	}
}

void Font::BlitUnderscore(int x, int y, SDL_Surface* dest, int scale)
{
    if (!dest || scale <= 0 || !surfaces[39])
        return;

    // Glyph 39 is the original solid cursor block.  Taking only its bottom
    // scanline gives us a proper underscore without adding external font data.
    SDL_Rect srcrect;
    srcrect.x = surfaces[39]->w > 2 ? 1 : 0;
    srcrect.y = surfaces[39]->h > 0 ? surfaces[39]->h - 1 : 0;
    srcrect.w = std::min(w > 2 ? w - 2 : w, surfaces[39]->w - srcrect.x);
    srcrect.h = 1;

    SDL_Rect dstrect;
    dstrect.x = x + scale;
    dstrect.y = y + (h > 0 ? (h - 1) * scale : 0);
    dstrect.w = srcrect.w * scale;
    dstrect.h = scale;

    if (scale == 1)
    {
        SDL_BlitSurface(surfaces[39], &srcrect, dest, &dstrect);
    }
    else if (surfaces32[39])
    {
        SDL_BlitScaled(surfaces32[39], &srcrect, dest, &dstrect);
    }
}

void Font::PrintCharacter(char c, int x, int y, SDL_Surface* dest, int scale)
{
    if (c == ' ')
        return;

    if (c >= '0' && c <= '9')
    {
        Blit(x, y, c - '0', dest, scale);
    }
    else if (c >= 'a' && c <= 'z')
    {
        Blit(x, y, c - 'a' + 10, dest, scale);
    }
    else if (c >= 'A' && c <= 'Z')
    {
        Blit(x, y, c - 'A' + 10, dest, scale);
    }
    else if (c == '!')
    {
        Blit(x, y, 36, dest, scale);
    }
    else if (c == '.')
    {
        Blit(x, y, 37, dest, scale);
    }
    else if (c == ':')
    {
        Blit(x, y, 38, dest, scale);
    }
    else if (c == '_')
    {
        BlitUnderscore(x, y, dest, scale);
    }
    else if (static_cast<unsigned char>(c) == 127)
    {
        Blit(x, y, 39, dest, scale);
    }
}

void Font::PrintMessageAt(const std::string& message, int x, int y, SDL_Surface* dest, int scale)
{
    if (!dest || scale <= 0)
        return;

    int xpos = x;
    for (char c : message)
    {
        PrintCharacter(c, xpos, y, dest, scale);
        xpos += w * scale;
    }
}

int Font::MeasureText(const std::string& message, int scale) const
{
    if (scale <= 0)
        return 0;
    return static_cast<int>(message.size()) * w * scale;
}

void Font::PrintMessage(std::string message, int y, SDL_Surface* dest, int scale)
{
    if (!dest)
        return;

    const int xstart = dest->w / 2 - MeasureText(message, scale) / 2;
    PrintMessageAt(message, xstart, y, dest, scale);
}


std::size_t Font::CountTypewriterCharacters(const std::string& message) const
{
    std::size_t count = 0;
    for (char c : message)
    {
        if (IsTypewriterCharacter(c))
            ++count;
    }
    return count;
}

void Font::PrintMultiLineMessageProgressive(const std::string& message, int y, SDL_Surface* dest, std::size_t visibleCharacters)
{
    if (!dest || w <= 0 || h <= 0)
        return;

    // The original renderer limits intermission lines to roughly 38 glyphs.
    // Build all lines from the complete message before revealing characters,
    // so the centred text never shifts sideways while it is being typed.
    const std::size_t surfaceLimit = static_cast<std::size_t>(std::max(1, dest->w / w));
    const std::size_t maxCharacters = std::min<std::size_t>(38, surfaceLimit);
    const std::vector<std::string> lines = BuildIntermissionLines(message, maxCharacters);

    std::size_t characterIndex = 0;
    for (const std::string& line : lines)
    {
        const int xstart = dest->w / 2 - MeasureText(line, 1) / 2;
        int xpos = xstart;

        for (char c : line)
        {
            if (IsTypewriterCharacter(c))
            {
                if (characterIndex < visibleCharacters)
                    PrintCharacter(c, xpos, y, dest, 1);
                ++characterIndex;
            }
            xpos += w;
        }

        y += h;
    }
}

void Font::PrintMultiLineMessage(std::string message, int y, SDL_Surface* dest)
{
	size_t charsinline = dest->w / w;

	while (message.size() > charsinline)
	{
		for (int i = message.size() - 1; i >= 0; i--)
		{
			if ((message[i] == ' ') && (i <= (int)charsinline))
			{
				PrintMessage(message.substr(0, i), y, dest, 1);
				message = message.substr(i + 1, std::string::npos);
				y += h;
				break;
			}
		}
	}
	PrintMessage(message, y, dest, 1);
}

Font::~Font()
{
	for (int i = 0; i < glyphs; i++)
	{
		if (surfaces[i])
		{
			SDL_FreeSurface(surfaces[i]);
		}
		if (surfaces32[i])
		{
			SDL_FreeSurface(surfaces32[i]);
		}
	}
}
