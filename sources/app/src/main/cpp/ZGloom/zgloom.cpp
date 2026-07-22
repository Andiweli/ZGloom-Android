// zgloom.cpp : Defines the entry point for the console application.
//

#include <sdl2/SDL.h>
#include <sdl2/SDL_mixer.h>
#ifdef __ANDROID__
#include <sdl2/SDL_system.h>
#include <jni.h>
#include <errno.h>
#endif
#include "xmp/include/xmp.h"


// Global XMP context to avoid scope issues across code paths
static xmp_context g_xmp = nullptr;
#include "config.h"
#include "gloommap.h"
#include "script.h"
#include "crmfile.h"
#include "iffhandler.h"
#include "renderer.h"
#include "effects/RendererHooks.h"
#include "audio/EmbeddedBGM.h"
#include "audio/AtmosphereVolume.h"
#include "objectgraphics.h"
#include <iostream>
#include "SaveSystem.h"
#include "EventReplay.h"

#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdlib>

#ifndef _WIN32
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "assets/launcher_bg_4_3_embed.h"
#include "assets/launcher_bg_16_9_embed.h"
#include "assets/gloom_classic_title_fallback_embed.h"

#include "gamelogic.h"
#include "soundhandler.h"
#include "font.h"
#include "titlescreen.h"
#include "menuscreen.h"
#include "hud.h"
#include "effects/MuzzleFlashFX.h"

static const unsigned char kEmbeddedBigfont2Crm2[] = {
    0x43, 0x72, 0x4D, 0x32, 0x00, 0x00, 0x00, 0x00, 0x0D, 0xD2, 0x00, 0x00,
    0x04, 0x42, 0x01, 0x76, 0x67, 0x9A, 0x65, 0xC0, 0x16, 0x73, 0x4D, 0x7C,
    0xD1, 0xAE, 0x01, 0xFC, 0x4C, 0xD3, 0x2E, 0x68, 0x37, 0x00, 0x6B, 0x1E,
    0x68, 0x56, 0x68, 0xB7, 0x02, 0x0E, 0x4F, 0x34, 0xEB, 0x9A, 0x58, 0x0D,
    0xFA, 0x9E, 0x68, 0xA7, 0x34, 0xFB, 0x80, 0xBF, 0xB3, 0xCD, 0x26, 0xE6,
    0x89, 0x40, 0xEA, 0x17, 0x9A, 0x2D, 0xCD, 0x05, 0xE0, 0x03, 0xC5, 0xE6,
    0x9D, 0x66, 0x92, 0xF9, 0xA2, 0x5C, 0x0F, 0xBD, 0x33, 0x45, 0x7C, 0xD2,
    0xAE, 0x03, 0xDF, 0x9C, 0xD3, 0x8E, 0x69, 0xD7, 0x02, 0x7E, 0x17, 0x34,
    0x93, 0x9A, 0x4D, 0xC0, 0x1F, 0x89, 0xE6, 0x8D, 0x73, 0x4D, 0xB8, 0x00,
    0x71, 0x73, 0x44, 0xF8, 0x17, 0x71, 0x7A, 0xFB, 0xBF, 0x8E, 0x39, 0x71,
    0xB7, 0x35, 0x05, 0x44, 0x55, 0xC6, 0xDC, 0x8A, 0x52, 0xFF, 0xFE, 0x59,
    0x1A, 0x45, 0x95, 0xC6, 0xFC, 0xD6, 0x21, 0xA2, 0x4B, 0xF7, 0xAE, 0x31,
    0x14, 0xAF, 0xC2, 0x33, 0xBD, 0x1B, 0x1B, 0xCE, 0xB8, 0x46, 0x67, 0x12,
    0x5B, 0x37, 0xC2, 0x5E, 0xB5, 0x4C, 0x19, 0xA5, 0x3A, 0xF2, 0x22, 0x8C,
    0x86, 0x91, 0x1D, 0xB4, 0xC1, 0x1A, 0x3B, 0x34, 0x70, 0xBD, 0xE2, 0xF4,
    0x09, 0xB5, 0xE8, 0x7B, 0xE7, 0x09, 0xD8, 0x1B, 0x15, 0x7B, 0x6F, 0x8B,
    0x30, 0x52, 0xBA, 0xB3, 0xA6, 0x98, 0x13, 0xB7, 0x43, 0xB6, 0x3A, 0xB8,
    0x44, 0xDC, 0x38, 0xB3, 0x6E, 0x7A, 0x27, 0x17, 0x08, 0xC9, 0x3B, 0xA2,
    0x3E, 0xD0, 0xA5, 0x02, 0x74, 0x66, 0xD4, 0x7D, 0x6A, 0x1C, 0x73, 0x26,
    0x71, 0x75, 0xE3, 0x49, 0xE2, 0xC2, 0xEC, 0xC2, 0x9D, 0x7C, 0xDE, 0xA0,
    0x71, 0x0F, 0x7F, 0xA8, 0x73, 0x4C, 0xF0, 0x6F, 0xE7, 0x0F, 0x50, 0x06,
    0x1A, 0xEF, 0xE2, 0xCC, 0x14, 0xAD, 0x08, 0xD1, 0x5E, 0xFE, 0x8A, 0x1A,
    0xD1, 0x3E, 0x6F, 0x7A, 0xDF, 0x22, 0x4C, 0x59, 0x24, 0x4A, 0x91, 0x58,
    0xFE, 0x63, 0x98, 0x48, 0x94, 0x38, 0x01, 0xF9, 0x91, 0x24, 0x00, 0x7E,
    0xE1, 0x4A, 0xFC, 0xD0, 0x01, 0xFA, 0x1A, 0x3A, 0x43, 0xD0, 0x88, 0xF7,
    0x64, 0x3D, 0x0E, 0x14, 0xA6, 0xC6, 0xC2, 0x3D, 0x0F, 0xF1, 0xEE, 0xCB,
    0x02, 0x31, 0x1D, 0x30, 0xEB, 0x7A, 0x1E, 0xEC, 0xC2, 0xB4, 0x34, 0xDB,
    0x86, 0x3B, 0x24, 0x74, 0xC8, 0xF8, 0x87, 0x09, 0x1F, 0xB4, 0x4F, 0xDC,
    0xEB, 0x83, 0x0A, 0x31, 0xAE, 0x27, 0x30, 0x24, 0x39, 0x93, 0xBE, 0xEB,
    0x85, 0x61, 0x46, 0xA6, 0x76, 0xB0, 0xA1, 0xD3, 0x3B, 0x42, 0x91, 0xE2,
    0x82, 0xB9, 0x8D, 0x4C, 0xE5, 0x22, 0x74, 0x3D, 0x1A, 0x44, 0x98, 0x00,
    0xFF, 0x11, 0x4A, 0x7F, 0x0C, 0x3D, 0x0B, 0x47, 0x35, 0x66, 0x39, 0xE9,
    0x98, 0x20, 0x58, 0xCC, 0xA2, 0xCC, 0xCC, 0x62, 0x29, 0x40, 0xCD, 0x94,
    0x4E, 0xCD, 0x33, 0x31, 0xFF, 0x0A, 0x37, 0x51, 0xE3, 0xCE, 0xFA, 0xCF,
    0x75, 0xC1, 0x10, 0x46, 0x37, 0xB7, 0x88, 0xF1, 0x88, 0xA7, 0x27, 0xD0,
    0x13, 0xC8, 0xF4, 0x84, 0x60, 0x99, 0x9B, 0x7E, 0x77, 0x66, 0x72, 0x95,
    0xF8, 0x66, 0x67, 0x29, 0x14, 0x89, 0x77, 0x66, 0x99, 0xCB, 0x47, 0x1D,
    0x54, 0x31, 0x87, 0x3F, 0x38, 0x70, 0xA1, 0xC5, 0x98, 0x29, 0x57, 0x30,
    0x6A, 0x18, 0x24, 0x7E, 0xD1, 0x1C, 0xC9, 0xB0, 0x8F, 0x3C, 0xA7, 0x1F,
    0x38, 0x77, 0x20, 0x32, 0xCA, 0x16, 0x60, 0xA5, 0x21, 0xB7, 0xED, 0xD0,
    0x6C, 0x49, 0x54, 0x96, 0x41, 0x54, 0x77, 0x68, 0xE1, 0x0F, 0x43, 0xBC,
    0x76, 0x25, 0x11, 0x47, 0xA3, 0x11, 0x4A, 0x23, 0x7D, 0x34, 0xD2, 0x3D,
    0x1E, 0x47, 0x11, 0x1E, 0xEC, 0xB7, 0x76, 0x51, 0xEE, 0xFC, 0x29, 0x59,
    0x77, 0xC4, 0x48, 0xF7, 0x73, 0x51, 0x3A, 0x98, 0x8C, 0x3D, 0xC2, 0x87,
    0x26, 0x42, 0x59, 0xDA, 0x14, 0x4A, 0x22, 0x8B, 0xF0, 0xC2, 0xEE, 0x4C,
    0xC2, 0xB4, 0xCD, 0x04, 0xDB, 0x75, 0xC2, 0x31, 0x87, 0x8B, 0xBE, 0x62,
    0x11, 0x8A, 0x39, 0x98, 0x72, 0xE1, 0x0B, 0x4E, 0x7B, 0x72, 0x3E, 0x43,
    0xEB, 0xC8, 0xA3, 0xBA, 0xEB, 0xDA, 0x14, 0xA2, 0x35, 0xDD, 0x79, 0x48,
    0xEB, 0x87, 0x33, 0x4F, 0xCE, 0x14, 0x74, 0xB7, 0xC2, 0xCC, 0x14, 0xA2,
    0x34, 0xC2, 0x40, 0x91, 0x95, 0x1D, 0x9A, 0x8F, 0xB3, 0x0C, 0x6E, 0xDB,
    0x8E, 0xB3, 0xC8, 0xEE, 0x1E, 0x71, 0x8C, 0x51, 0x82, 0xCC, 0x14, 0xA1,
    0x98, 0xD0, 0x5E, 0xC4, 0x3B, 0x8C, 0x0F, 0xA3, 0xBB, 0x57, 0x57, 0x29,
    0x86, 0x3C, 0xC6, 0x18, 0x99, 0xC5, 0x27, 0x9D, 0xF4, 0x90, 0x94, 0x31,
    0xCC, 0xEF, 0x0A, 0x55, 0xA7, 0x22, 0x77, 0x82, 0xEC, 0x65, 0x26, 0x72,
    0x23, 0x98, 0xBA, 0x38, 0x97, 0xA3, 0xAF, 0x3A, 0x1D, 0xCC, 0xAB, 0xD3,
    0xAF, 0x68, 0x52, 0xA3, 0xF1, 0x1D, 0xAD, 0x8C, 0x47, 0x66, 0x8E, 0x68,
    0x82, 0x31, 0x4C, 0x7A, 0x03, 0x27, 0x77, 0x25, 0x98, 0x29, 0x52, 0x41,
    0xD1, 0x27, 0x7C, 0x47, 0xA1, 0x14, 0x4B, 0x8F, 0x51, 0xF6, 0xD2, 0xC9,
    0x23, 0xE7, 0x0A, 0x37, 0xE3, 0xAD, 0xDD, 0x22, 0x56, 0x9E, 0xD8, 0xF6,
    0xD4, 0xF1, 0x9F, 0x1D, 0xFE, 0x70, 0x9E, 0x4A, 0xAC, 0xB0, 0x22, 0x95,
    0xF8, 0x5B, 0x98, 0x36, 0xE1, 0x98, 0xFB, 0x47, 0x7C, 0xE9, 0xB0, 0x9B,
    0x22, 0x5D, 0xAB, 0x12, 0xE3, 0x28, 0x89, 0x70, 0xA4, 0x48, 0xDD, 0x00,
    0x41, 0x22, 0x54, 0xBB, 0x63, 0xB8, 0x93, 0x18, 0xF0, 0x17, 0x68, 0xBA,
    0xCF, 0x3E, 0xEF, 0x4F, 0xD0, 0x67, 0xCE, 0x8F, 0x79, 0xA2, 0x24, 0x85,
    0x23, 0x6B, 0x07, 0x25, 0x19, 0xC0, 0xB6, 0x7D, 0x44, 0xB9, 0x1C, 0xC2,
    0x3A, 0xF4, 0x8B, 0x6B, 0x36, 0xD3, 0x6B, 0x7F, 0x38, 0x5F, 0x57, 0xA0,
    0x98, 0x83, 0x5E, 0x85, 0x98, 0x29, 0x4F, 0xA0, 0x2D, 0x82, 0xAF, 0xCC,
    0xC4, 0x04, 0xE7, 0xEA, 0x3E, 0xDB, 0x26, 0xD5, 0xF1, 0x26, 0x01, 0xE2,
    0x8E, 0x3C, 0xE8, 0xF8, 0x20, 0xE0, 0xEC, 0x19, 0x82, 0xA0, 0x9E, 0x8F,
    0xB1, 0x14, 0x88, 0x8E, 0xC3, 0x36, 0xD3, 0x1F, 0xA2, 0xC8, 0x3A, 0x11,
    0xF2, 0x83, 0x32, 0xAA, 0x3B, 0xB5, 0x7E, 0x91, 0xD7, 0x0A, 0x31, 0xAE,
    0xD2, 0xFC, 0x44, 0xAD, 0x34, 0x61, 0xCE, 0xB9, 0xCC, 0x68, 0xC2, 0x94,
    0xD9, 0xAA, 0x51, 0x26, 0x27, 0x35, 0x50, 0xA7, 0xCE, 0x17, 0xCF, 0x19,
    0xA1, 0xB4, 0x6D, 0xFA, 0x27, 0x70, 0x7A, 0x4D, 0x51, 0x2E, 0x6B, 0x10,
    0xF7, 0x31, 0x66, 0x0A, 0x51, 0x91, 0xB9, 0x8E, 0xD8, 0xCA, 0xAC, 0xEB,
    0x03, 0x46, 0xE5, 0x81, 0xA3, 0x99, 0x54, 0x55, 0x11, 0xFE, 0x75, 0x2D,
    0x59, 0xD0, 0xB3, 0x11, 0x5E, 0x5A, 0xEF, 0xE2, 0xB0, 0x54, 0x2D, 0xAE,
    0x11, 0x06, 0x34, 0xBE, 0x06, 0x57, 0xDB, 0xE3, 0x81, 0x69, 0xD9, 0x01,
    0xD0, 0xBB, 0x42, 0x2C, 0xA1, 0x60, 0xA1, 0x40, 0xB1, 0x66, 0x03, 0x04,
    0x82, 0xC2, 0xA1, 0x71, 0x48, 0xCC, 0x76, 0x41, 0x22, 0x93, 0x4A, 0x27,
    0x73, 0xDA, 0x05, 0x06, 0x91, 0x4C, 0xA7, 0x56, 0x2C, 0x56, 0x4B, 0x45,
    0xA6, 0xE1, 0x71, 0xB9, 0x5C, 0xEE, 0x97, 0x6B, 0xBD, 0xEE, 0xFB, 0x82,
    0xC5, 0x63, 0x32, 0x99, 0x6C, 0xF6, 0x83, 0x45, 0xA8, 0xD5, 0x6D, 0x37,
    0xDC, 0x0E, 0x0F, 0x23, 0x95, 0xD0, 0xE9, 0x77, 0x3B, 0xDE, 0x2F, 0x37,
    0xBF, 0xED, 0xFD, 0x09, 0x41, 0xE1, 0x10, 0x98, 0x6C, 0x3A, 0x1F, 0x10,
    0x8D, 0x47, 0xA5, 0xD3, 0x09, 0xB4, 0xF2, 0x85, 0x6B, 0xEA, 0x7A, 0x3E,
    0x5F, 0x3F, 0xD4, 0x72, 0x3F, 0x24, 0x9D, 0x4F, 0xE8, 0x96, 0x6B, 0x3D,
    0xBF, 0xF2, 0x11, 0x81, 0xC3, 0x27, 0xD6, 0x3B, 0xCF, 0xD3, 0xF1, 0xFE,
    0xFF, 0x83, 0x01, 0xA0, 0xE8, 0x14, 0x1A, 0x31, 0x34, 0x9C, 0x58, 0x2D,
    0x97, 0x8F, 0xB8, 0x40, 0x11, 0x7C, 0xBF, 0xFF, 0x01, 0x57, 0xEF, 0x80,
    0x18, 0x16, 0x04, 0x01, 0x01, 0xF0, 0x00, 0x90, 0x28, 0x3C, 0x06, 0x01,
    0x00, 0x40, 0x07, 0xC4, 0xE5, 0x42, 0x39, 0x91, 0x25, 0x42, 0x00, 0x03,
};
static const unsigned int kEmbeddedBigfont2Crm2Size = 1104u;

static bool GL_LoadEmbeddedCrm2(const unsigned char* src, unsigned int srcSize, CrmFile& out)
{
	if (out.data)
	{
		std::free(out.data);
		out.data = nullptr;
	}
	out.size = 0;

	if (!src || srcSize == 0)
	{
		return false;
	}

	if (srcSize > 14 && GetSize((void*)src) != 0)
	{
		const unsigned int outSize = GetSize((void*)src);
		const unsigned int headroom = GetSecDist((void*)src);
		unsigned char* indata = static_cast<unsigned char*>(std::malloc(srcSize));
		unsigned char* outdata = static_cast<unsigned char*>(std::malloc(outSize + headroom));
		out.data = static_cast<unsigned char*>(std::malloc(outSize));
		if (!indata || !outdata || !out.data)
		{
			if (indata) std::free(indata);
			if (outdata) std::free(outdata);
			if (out.data) std::free(out.data);
			out.data = nullptr;
			return false;
		}

		std::memcpy(indata, src, srcSize);
		Decrunch(indata, outdata);
		std::memcpy(out.data, outdata, outSize);
		out.size = outSize;
		std::free(indata);
		std::free(outdata);
		return true;
	}

	out.data = static_cast<unsigned char*>(std::malloc(srcSize));
	if (!out.data)
	{
		return false;
	}
	std::memcpy(out.data, src, srcSize);
	out.size = srcSize;
	return true;
}

static bool GL_DecodeEmbeddedPic(const std::uint8_t* src,
                                 std::size_t srcSize,
                                 std::vector<std::uint8_t>& pic,
                                 std::uint32_t& width)
{
    CrmFile picfile;
    if (!GL_LoadEmbeddedCrm2(src, static_cast<unsigned int>(srcSize), picfile) ||
        !picfile.data || picfile.size < 12)
    {
        return false;
    }

    IffHandler::DecodeIff(picfile.data, pic, width);
    return width > 0 && !pic.empty();
}

static bool GL_ApplyPaletteData(const std::uint8_t* data,
                                std::uint32_t size,
                                SDL_Surface* render8)
{
    if (!data || size == 0 || !render8 || !render8->format ||
        !render8->format->palette)
    {
        return false;
    }

    const std::uint32_t numColours = std::min<std::uint32_t>(256, size / 4);
    if (numColours == 0)
    {
        return false;
    }

    for (std::uint32_t c = 0; c < numColours; ++c)
    {
        SDL_Color col;
        col.a = 0xFF;
        col.r = data[c * 4 + 0] & 0x0F;
        col.g = data[c * 4 + 1] >> 4;
        col.b = data[c * 4 + 1] & 0x0F;

        col.r <<= 4;
        col.g <<= 4;
        col.b <<= 4;

        col.r |= data[c * 4 + 2] & 0x0F;
        col.g |= data[c * 4 + 3] >> 4;
        col.b |= data[c * 4 + 3] & 0x0F;

        SDL_SetPaletteColors(render8->format->palette, &col, c, 1);
    }

    return true;
}

static bool GL_ApplyEmbeddedPalette(const std::uint8_t* src,
                                    std::size_t srcSize,
                                    SDL_Surface* render8)
{
    CrmFile palfile;
    if (!GL_LoadEmbeddedCrm2(src, static_cast<unsigned int>(srcSize), palfile) ||
        !palfile.data || palfile.size == 0)
    {
        return false;
    }

    return GL_ApplyPaletteData(palfile.data, palfile.size, render8);
}

static bool GL_CopyDecodedPicToSurface(const std::vector<std::uint8_t>& pic,
                                       std::uint32_t width,
                                       SDL_Surface* render8,
                                       int dstY = 0,
                                       bool clearSurface = true)
{
    if (!render8 || width == 0 || pic.empty() || dstY >= render8->h)
    {
        return false;
    }

    if (clearSurface)
    {
        SDL_FillRect(render8, nullptr, 0);
    }

    const std::uint32_t height = static_cast<std::uint32_t>(pic.size() / width);
    const std::uint32_t copyW = std::min<std::uint32_t>(width,
        static_cast<std::uint32_t>(render8->w));
    const std::uint32_t copyH = std::min<std::uint32_t>(height,
        static_cast<std::uint32_t>(std::max(0, render8->h - dstY)));

    for (std::uint32_t y = 0; y < copyH; ++y)
    {
        const std::uint8_t* src = pic.data() + y * width;
        std::uint8_t* dst = static_cast<std::uint8_t*>(render8->pixels) +
            (dstY + static_cast<int>(y)) * render8->pitch;
        std::copy(src, src + copyW, dst);
    }

    return true;
}


#ifdef __ANDROID__
static void ConfigureAndroidDataRoot()
{
    // Preferred: app-specific external files dir used by Java installer:
    //   getExternalFilesDir(null)/ZGloom
    const char* ext = SDL_AndroidGetExternalStoragePath();
    if (ext && *ext) {
        std::string root = std::string(ext);
        if (!root.empty() && root.back() != '/' && root.back() != '\\') {
            root += "/";
        }
        root += "ZGloom/";

        Config::SetDataRoot(root);
        SDL_Log("ZGloom: Android DataRoot set to '%s'", root.c_str());

        if (chdir(root.c_str()) != 0) {
            SDL_Log("ZGloom: chdir('%s') failed (errno=%d)", root.c_str(), errno);
        } else {
            SDL_Log("ZGloom: chdir to DataRoot OK");
        }
    } else {
        SDL_Log("ZGloom: SDL_AndroidGetExternalStoragePath() returned null; keeping default DataRoot");
    }
}

static void GL_AndroidSetBuildInfoOverlayVisible(bool visible)
{
    JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());

    if (!env || !activity)
        return;

    jclass activityClass = env->GetObjectClass(activity);
    if (!activityClass)
    {
        env->DeleteLocalRef(activity);
        return;
    }

    const char* methodName = visible ? "showBuildInfoOverlay" : "hideBuildInfoOverlay";
    jmethodID method = env->GetMethodID(activityClass, methodName, "()V");
    if (method)
    {
        env->CallVoidMethod(activity, method);
    }

    if (env->ExceptionCheck())
    {
        env->ExceptionClear();
    }

    env->DeleteLocalRef(activityClass);
    env->DeleteLocalRef(activity);
}
#endif



// ==================== ZHUD NO-INCLUDE GLUE BLOCK (drop-in) ====================
// Paste this block *after* your existing #includes in zgloom.cpp (no extra headers).
// It provides hudTex/hudLayer32 + helpers, and forward-declares the RendererHooks
// functions so you don't need to include any new headers here.

// (removed) // #include <sdl2/SDL.h>

// RendererHooks forward declarations removed (using included header)
// Global HUD resources (stay internal to this TU)
static SDL_Texture* g_ZHudTex = nullptr;
static SDL_Surface* g_ZHudLayer32 = nullptr;

// Create HUD texture/surface with ARGB8888 if missing
static inline void ZHUD_EnsureCreated(SDL_Renderer* ren, int w, int h) {
    if (!g_ZHudTex) {
        g_ZHudTex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
        SDL_SetTextureBlendMode(g_ZHudTex, SDL_BLENDMODE_BLEND);
    }
    if (!g_ZHudLayer32) {
        g_ZHudLayer32 = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    }
}

// Transparent clear at frame begin
static inline void ZHUD_Clear() {
    if (g_ZHudLayer32) SDL_FillRect(g_ZHudLayer32, nullptr, 0x00000000);
}

// Submit HUD for this frame: upload & register for on-top composition
static inline void ZHUD_Submit(SDL_Renderer* ren) {
    if (!g_ZHudTex || !g_ZHudLayer32) return;
    SDL_UpdateTexture(g_ZHudTex, nullptr, g_ZHudLayer32->pixels, g_ZHudLayer32->pitch);
    RendererHooks::SetHudTexture(g_ZHudTex);
}

// Optional cleanup
static inline void ZHUD_Destroy() {
    if (g_ZHudLayer32) { SDL_FreeSurface(g_ZHudLayer32); g_ZHudLayer32 = nullptr; }
    if (g_ZHudTex) { SDL_DestroyTexture(g_ZHudTex); g_ZHudTex = nullptr; }
}

// Ensure HUD resources are valid for this frame (handle context loss / size change)
static inline void ZHUD_EnsureAlive(SDL_Renderer* ren, int w, int h) {
    // If nothing yet, just create fresh resources
    if (!g_ZHudTex || !g_ZHudLayer32) {
        ZHUD_EnsureCreated(ren, w, h);
        return;
    }

    // Verify texture is still valid and matches expected size
    Uint32 fmt = 0;
    int access = 0, tw = 0, th = 0;
    if (SDL_QueryTexture(g_ZHudTex, &fmt, &access, &tw, &th) != 0 || tw != w || th != h) {
        // Renderer may have been reset or size changed; recreate HUD resources
        ZHUD_Destroy();
        ZHUD_EnsureCreated(ren, w, h);
    }
}

// Convenience accessors if your code used 'hudTex' / 'hudLayer32' names before:
#define hudTex      g_ZHudTex
#define hudLayer32  g_ZHudLayer32

// ================== END ZHUD NO-INCLUDE GLUE BLOCK (drop-in) ===================


Uint32 my_callbackfunc(Uint32 interval, void *param)
{
	SDL_Event event;
	SDL_UserEvent userevent;

	/* In this example, our callback pushes an SDL_USEREVENT event
	into the queue, and causes our callback to be called again at the
	same interval: */

	userevent.type = SDL_USEREVENT;
	userevent.code = 0;
	userevent.data1 = NULL;
	userevent.data2 = NULL;

	event.type = SDL_USEREVENT;
	event.user = userevent;

	SDL_PushEvent(&event);
	return(interval);
}

static void fill_audio(void *udata, Uint8 *stream, int len)
{
	auto res = xmp_play_buffer((xmp_context)udata, stream, len, 0);
}

static bool DecodePicFile(const std::string& name, std::vector<uint8_t>& pic, uint32_t& width)
{
	CrmFile picfile;
	if (!picfile.Load(name.c_str()) || !picfile.data || picfile.size < 12)
	{
		SDL_Log("ZGloom: DecodePicFile('%s') failed", name.c_str());
		return false;
	}

	IffHandler::DecodeIff(picfile.data, pic, width);
	return (width > 0) && !pic.empty();
}

static bool ApplyPicPalette(const std::string& name, SDL_Surface* render8)
{
	CrmFile palfile;
	if (!palfile.Load((name + ".pal").c_str()) || !palfile.data || palfile.size == 0)
	{
		SDL_Log("ZGloom: ApplyPicPalette('%s') failed", name.c_str());
		return false;
	}

	return GL_ApplyPaletteData(palfile.data, palfile.size, render8);
}

bool LoadPic(std::string name, SDL_Surface* render8)
{
	if (!render8)
	{
		return false;
	}

	std::vector<uint8_t> pic;
	uint32_t width = 0;
	if (!DecodePicFile(name, pic, width))
	{
		SDL_FillRect(render8, nullptr, 0);
		return false;
	}

	ApplyPicPalette(name, render8);
	return GL_CopyDecodedPicToSurface(pic, width, render8);
}

static bool GL_LoadPicWithEmbeddedFallback(const std::string& name,
                                           SDL_Surface* render8,
                                           const std::uint8_t* embeddedPic,
                                           std::size_t embeddedPicSize,
                                           const std::uint8_t* embeddedPalette,
                                           std::size_t embeddedPaletteSize)
{
    if (!render8)
    {
        return false;
    }

    std::vector<std::uint8_t> pic;
    std::uint32_t width = 0;
    const bool loadedFromDisk = DecodePicFile(name, pic, width);

    if (!loadedFromDisk &&
        !GL_DecodeEmbeddedPic(embeddedPic, embeddedPicSize, pic, width))
    {
        SDL_FillRect(render8, nullptr, 0);
        SDL_Log("ZGloom: embedded fallback for '%s' also failed", name.c_str());
        return false;
    }

    bool paletteLoaded = false;
    if (loadedFromDisk)
    {
        paletteLoaded = ApplyPicPalette(name, render8);
    }
    if (!paletteLoaded)
    {
        paletteLoaded = GL_ApplyEmbeddedPalette(embeddedPalette,
                                                embeddedPaletteSize,
                                                render8);
        if (paletteLoaded)
        {
            SDL_Log("ZGloom: using embedded palette fallback for '%s'", name.c_str());
        }
    }

    if (!loadedFromDisk)
    {
        SDL_Log("ZGloom: using embedded picture fallback for '%s'", name.c_str());
    }

    return GL_CopyDecodedPicToSurface(pic, width, render8);
}

static bool OverlayPicAt(const std::string& name, SDL_Surface* render8, int dstY)
{
	if (!render8 || dstY >= render8->h)
	{
		return false;
	}

	std::vector<uint8_t> pic;
	uint32_t width = 0;
	if (!DecodePicFile(name, pic, width))
	{
		return false;
	}

	return GL_CopyDecodedPicToSurface(pic, width, render8, dstY, false);
}

static bool GL_OverlayEmbeddedPicAt(const std::uint8_t* embeddedPic,
                                    std::size_t embeddedPicSize,
                                    SDL_Surface* render8,
                                    int dstY)
{
    std::vector<std::uint8_t> pic;
    std::uint32_t width = 0;
    if (!GL_DecodeEmbeddedPic(embeddedPic, embeddedPicSize, pic, width))
    {
        return false;
    }

    return GL_CopyDecodedPicToSurface(pic, width, render8, dstY, false);
}

// Present a 320-pixel static screen in widescreen using the same principle as
// gloom2.s c87w1: keep the original picture untouched in the centre and extend
// only the first/last pixel of each scanline into a four-step dark edge wash.
// This avoids the visibly smeared 16-pixel side strips used by older Android
// builds and never stretches menu/intermission text into the side areas.
static void GL_BlitStaticWideLikeGloom2(SDL_Surface* source32,
                                        SDL_Surface* destination32,
                                        const SDL_Rect& centre)
{
    if (!source32 || !destination32 ||
        source32->format->BytesPerPixel != 4 || destination32->format->BytesPerPixel != 4)
    {
        if (source32 && destination32)
            SDL_BlitScaled(source32, nullptr, destination32, const_cast<SDL_Rect*>(&centre));
        return;
    }

    const int leftWidth = std::max(0, centre.x);
    const int rightStart = centre.x + centre.w;
    const int rightWidth = std::max(0, destination32->w - rightStart);

    if (SDL_MUSTLOCK(source32) && SDL_LockSurface(source32) != 0)
        return;
    if (SDL_MUSTLOCK(destination32) && SDL_LockSurface(destination32) != 0)
    {
        if (SDL_MUSTLOCK(source32)) SDL_UnlockSurface(source32);
        return;
    }

    for (int dy = 0; dy < centre.h; ++dy)
    {
        const int dstY = centre.y + dy;
        if (dstY < 0 || dstY >= destination32->h)
            continue;

        const int srcY = std::min(source32->h - 1,
                                  std::max(0, (dy * source32->h) / std::max(1, centre.h)));
        const uint32_t* srcRow = reinterpret_cast<const uint32_t*>(
            static_cast<const uint8_t*>(source32->pixels) + srcY * source32->pitch);
        uint32_t* dstRow = reinterpret_cast<uint32_t*>(
            static_cast<uint8_t*>(destination32->pixels) + dstY * destination32->pitch);

        Uint8 lr = 0, lg = 0, lb = 0, la = 255;
        Uint8 rr = 0, rg = 0, rb = 0, ra = 255;
        SDL_GetRGBA(srcRow[0], source32->format, &lr, &lg, &lb, &la);
        SDL_GetRGBA(srcRow[source32->w - 1], source32->format, &rr, &rg, &rb, &ra);

        uint32_t leftShades[4];
        uint32_t rightShades[4];
        for (int shade = 0; shade < 4; ++shade)
        {
            const int numerator = shade + 1; // quarter, half, three-quarter, full
            leftShades[shade] = SDL_MapRGBA(destination32->format,
                static_cast<Uint8>((static_cast<int>(lr) * numerator) / 4),
                static_cast<Uint8>((static_cast<int>(lg) * numerator) / 4),
                static_cast<Uint8>((static_cast<int>(lb) * numerator) / 4), 255);
            rightShades[shade] = SDL_MapRGBA(destination32->format,
                static_cast<Uint8>((static_cast<int>(rr) * numerator) / 4),
                static_cast<Uint8>((static_cast<int>(rg) * numerator) / 4),
                static_cast<Uint8>((static_cast<int>(rb) * numerator) / 4), 255);
        }

        for (int x = 0; x < leftWidth; ++x)
        {
            const int shade = std::min(3, (x * 4) / std::max(1, leftWidth));
            dstRow[x] = leftShades[shade];
        }
        for (int x = 0; x < rightWidth; ++x)
        {
            const int shade = std::min(3,
                ((rightWidth - 1 - x) * 4) / std::max(1, rightWidth));
            dstRow[rightStart + x] = rightShades[shade];
        }
    }

    if (SDL_MUSTLOCK(destination32)) SDL_UnlockSurface(destination32);
    if (SDL_MUSTLOCK(source32)) SDL_UnlockSurface(source32);

    SDL_BlitScaled(source32, nullptr, destination32, const_cast<SDL_Rect*>(&centre));
}

// Gloom Classic deliberately uses a coarser software look.  The world is still
// rendered normally for compatibility, then reduced to stable nearest-neighbour
// 2x2 blocks before the separate HUD layer is composed.  Menus never pass here.
static void GL_PixelateWorld2x2(SDL_Surface* surface)
{
    if (!surface || surface->format->BytesPerPixel != 4)
        return;

    if (SDL_MUSTLOCK(surface) && SDL_LockSurface(surface) != 0)
        return;

    for (int y = 0; y < surface->h; y += 2)
    {
        uint32_t* row0 = reinterpret_cast<uint32_t*>(
            static_cast<uint8_t*>(surface->pixels) + y * surface->pitch);
        uint32_t* row1 = (y + 1 < surface->h)
            ? reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(surface->pixels) + (y + 1) * surface->pitch)
            : row0;

        for (int x = 0; x < surface->w; x += 2)
        {
            const uint32_t pixel = row0[x];
            row0[x] = pixel;
            if (x + 1 < surface->w) row0[x + 1] = pixel;
            row1[x] = pixel;
            if (x + 1 < surface->w) row1[x + 1] = pixel;
        }
    }

    if (SDL_MUSTLOCK(surface)) SDL_UnlockSurface(surface);
}


enum GameState
{
	STATE_PLAYING,
	STATE_PARSING,
	STATE_SPOOLING,
	STATE_WAITING,
	STATE_MENU,
	STATE_SPLASH,
	STATE_TITLE
};


bool g_RequestSavePosition = false;
bool g_RequestTitleContinue = false;


// ------------------------- Multi-game launcher support -------------------------

struct GameInstall
{
    std::string baseDir; // "" = current folder, otherwise subdirectory
    std::string label;   // human-readable name (e.g., "Gloom Deluxe")
    bool isZM;           // true if pure Zombie Massacre layout (stuf/stages without misc/script)
    bool hasGloom;       // true for misc/script based Gloom-engine data
    bool androidForceWide; // Android: unknown Gloom-engine data should still use 16:9 fullscreen presentation
};

static inline bool GL_FileExistsIn(const std::string& baseDir, const char* relPath)
{
    std::string full;
    if (!baseDir.empty())
    {
        full = baseDir;
        if (!full.empty() && full.back() != '/' && full.back() != '\\')
            full.push_back('/');
        full += relPath;
    }
    else
    {
        full = relPath;
    }

    FILE* f = fopen(full.c_str(), "rb");
    if (f)
    {
        fclose(f);
        return true;
    }
    return false;
}

static std::string GL_ToLower(const std::string& sIn)
{
    std::string s = sIn;
    for (size_t i = 0; i < s.size(); ++i)
        s[i] = (char)std::tolower((unsigned char)s[i]);
    return s;
}

static std::string GL_CompactGameName(const std::string& dirName)
{
    std::string lower = GL_ToLower(dirName);
    std::string out;
    out.reserve(lower.size());
    for (size_t i = 0; i < lower.size(); ++i)
    {
        unsigned char c = (unsigned char)lower[i];
        if (std::isalnum(c))
            out.push_back((char)c);
    }
    return out;
}

static bool GL_IsKnownOfficialGameName(const std::string& dirName, bool hasGloom, bool hasZM)
{
    if (hasZM && !hasGloom)
        return true;

    std::string compact = GL_CompactGameName(dirName);
    if (compact.empty())
        return true; // current root / original-layout fallback

    return compact == "gloom" ||
           compact == "gloomclassic" ||
           compact == "gloomdeluxe" ||
           compact == "gloom3" ||
           compact == "zombiemassacre";
}

static std::string GL_TitleCaseFromDir(const std::string& dirName)
{
    std::string s = dirName;
    bool newWord = true;
    for (size_t i = 0; i < s.size(); ++i)
    {
        unsigned char c = (unsigned char)s[i];
        if (std::isspace(c) || c == '_' || c == '-' || c == '.')
        {
            if (c == '_' || c == '-' || c == '.')
                s[i] = ' ';
            newWord = true;
        }
        else
        {
            if (newWord)
                s[i] = (char)std::toupper(c);
            else
                s[i] = (char)std::tolower(c);
            newWord = false;
        }
    }
    return s;
}

static bool GL_IsGloom3Name(const std::string& name)
{
    std::string lower = GL_ToLower(name);
    return (lower.find("gloom3") != std::string::npos ||
            lower.find("gloom 3") != std::string::npos ||
            lower.find("gloom_3") != std::string::npos);
}

static std::string GL_MakeInstallLabel(const std::string& dirName, bool hasGloom, bool hasZM)
{
    // Current folder: prefer semantic names
    if (dirName.empty())
    {
        if (hasZM && !hasGloom)  return "Zombie Massacre";
        if (hasGloom && !hasZM)  return "Gloom";
        if (hasGloom && hasZM)   return "Gloom / Zombie Massacre";
        return "Current folder";
    }

    std::string lower = GL_ToLower(dirName);

    // Explicit special cases
    if (lower.find("8bitkiller") != std::string::npos || lower.find("8bit_killer") != std::string::npos)
        return "8Bit Killer";

    if (lower.find("deathmask") != std::string::npos || lower.find("death_mask") != std::string::npos)
        return "Death Mask";

    if (hasZM && !hasGloom)
        return "Zombie Massacre";

    if (GL_IsGloom3Name(dirName))
        return "Gloom 3";

    if (lower.find("deluxe") != std::string::npos)
        return "Gloom Deluxe";

    if (lower.find("classic") != std::string::npos)
        return "Gloom Classic";

    if (lower.find("gloom") != std::string::npos)
        return "Gloom";

    if (lower.find("zombie") != std::string::npos || lower.find("massacre") != std::string::npos)
        return "Zombie Massacre";

    if (hasGloom)
        return GL_TitleCaseFromDir(dirName);

    if (hasZM)
        return "Zombie Massacre";

    return GL_TitleCaseFromDir(dirName);
}

static void GL_TryAddInstall(const std::string& baseDir, std::vector<GameInstall>& out)
{
    bool hasGloom = GL_FileExistsIn(baseDir, "misc/script");
    bool hasZM    = GL_FileExistsIn(baseDir, "stuf/stages");

    if (!hasGloom && !hasZM)
        return;

    GameInstall gi;
    gi.baseDir = baseDir;
    gi.isZM    = hasZM && !hasGloom;
    gi.hasGloom = hasGloom;
    gi.androidForceWide = hasGloom && !hasZM && !GL_IsKnownOfficialGameName(baseDir, hasGloom, hasZM);
    gi.label   = GL_MakeInstallLabel(baseDir, hasGloom, hasZM);
    out.push_back(gi);
}

static void GL_DiscoverGameInstalls(std::vector<GameInstall>& out)
{
    out.clear();

    // Current folder first
    GL_TryAddInstall(std::string(), out);

    // One level of subdirectories
#ifdef _WIN32
    struct _finddata_t info;
    intptr_t handle = _findfirst("*", &info);
    if (handle != -1)
    {
        do
        {
            if (info.attrib & _A_SUBDIR)
            {
                if (std::strcmp(info.name, ".") == 0 || std::strcmp(info.name, "..") == 0)
                    continue;
                GL_TryAddInstall(info.name, out);
            }
        } while (_findnext(handle, &info) == 0);
        _findclose(handle);
    }
#else
    DIR* dir = opendir(".");
    if (dir)
    {
        struct dirent* ent;
        while ((ent = readdir(dir)) != nullptr)
        {
            const char* name = ent->d_name;
            if (!name || name[0] == '.')
                continue;

            std::string dname(name);

            struct stat st;
            if (stat(dname.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
            {
                GL_TryAddInstall(dname, out);
            }
        }
        closedir(dir);
    }
#endif
}

// ------------------------- Minimal 8x8 bitmap font for launcher ----------------

struct LauncherGlyph8 { char c; unsigned char r[8]; };

static const LauncherGlyph8 kLaunchFont8[] =
{
    {' ', {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
    {'A',{0x18,0x24,0x42,0x7E,0x42,0x42,0x42,0x00}},
    {'B',{0x7C,0x42,0x42,0x7C,0x42,0x42,0x7C,0x00}},
    {'C',{0x3C,0x42,0x40,0x40,0x40,0x42,0x3C,0x00}},
    {'D',{0x78,0x44,0x42,0x42,0x42,0x44,0x78,0x00}},
    {'E',{0x7E,0x40,0x40,0x7C,0x40,0x40,0x7E,0x00}},
    {'F',{0x7E,0x40,0x40,0x7C,0x40,0x40,0x40,0x00}},
    {'G',{0x3C,0x42,0x40,0x4E,0x42,0x42,0x3C,0x00}},
    {'H',{0x42,0x42,0x42,0x7E,0x42,0x42,0x42,0x00}},
    {'I',{0x38,0x10,0x10,0x10,0x10,0x10,0x38,0x00}},
    {'J',{0x02,0x02,0x02,0x02,0x42,0x42,0x3C,0x00}},
    {'K',{0x42,0x44,0x48,0x70,0x48,0x44,0x42,0x00}},
    {'L',{0x40,0x40,0x40,0x40,0x40,0x40,0x7E,0x00}},
    {'M',{0x42,0x66,0x5A,0x42,0x42,0x42,0x42,0x00}},
    {'N',{0x42,0x62,0x52,0x4A,0x46,0x42,0x42,0x00}},
    {'O',{0x3C,0x42,0x42,0x42,0x42,0x42,0x3C,0x00}},
    {'P',{0x7C,0x42,0x42,0x7C,0x40,0x40,0x40,0x00}},
    {'Q',{0x3C,0x42,0x42,0x42,0x4A,0x44,0x3A,0x00}},
    {'R',{0x7C,0x42,0x42,0x7C,0x48,0x44,0x42,0x00}},
    {'S',{0x3C,0x40,0x40,0x3C,0x02,0x02,0x3C,0x00}},
    {'T',{0x7C,0x10,0x10,0x10,0x10,0x10,0x10,0x00}},
    {'U',{0x42,0x42,0x42,0x42,0x42,0x42,0x3C,0x00}},
    {'V',{0x42,0x42,0x42,0x42,0x42,0x24,0x18,0x00}},
    {'W',{0x42,0x42,0x42,0x42,0x5A,0x66,0x42,0x00}},
    {'X',{0x42,0x42,0x24,0x18,0x24,0x42,0x42,0x00}},
    {'Y',{0x44,0x44,0x28,0x10,0x10,0x10,0x10,0x00}},
    {'Z',{0x7E,0x02,0x04,0x18,0x20,0x40,0x7E,0x00}},
    {'0',{0x3C,0x46,0x4A,0x52,0x62,0x46,0x3C,0x00}},
    {'1',{0x10,0x30,0x10,0x10,0x10,0x10,0x38,0x00}},
    {'2',{0x3C,0x42,0x02,0x04,0x18,0x20,0x7E,0x00}},
    {'3',{0x3C,0x42,0x02,0x1C,0x02,0x42,0x3C,0x00}},
    {'4',{0x04,0x0C,0x14,0x24,0x44,0x7E,0x04,0x00}},
    {'5',{0x7E,0x40,0x7C,0x02,0x02,0x42,0x3C,0x00}},
    {'6',{0x1C,0x20,0x40,0x7C,0x42,0x42,0x3C,0x00}},
    {'7',{0x7E,0x02,0x04,0x08,0x10,0x10,0x10,0x00}},
    {'8',{0x3C,0x42,0x42,0x3C,0x42,0x42,0x3C,0x00}},
    {'9',{0x3C,0x42,0x42,0x3E,0x02,0x04,0x38,0x00}},
    {':',{0x00,0x18,0x18,0x00,0x18,0x18,0x00,0x00}},
    {'-',{0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00}},
    {'.',{0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}},
    {',',{0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x10}},
    {'(',{0x0C,0x10,0x20,0x20,0x20,0x10,0x0C,0x00}},
    {')',{0x30,0x08,0x04,0x04,0x04,0x08,0x30,0x00}},
};

static const unsigned char* GL_FontRows(char c)
{
    if (c >= 'a' && c <= 'z')
        c = (char)(c - 32);
    for (size_t i = 0; i < sizeof(kLaunchFont8) / sizeof(kLaunchFont8[0]); ++i)
    {
        if (kLaunchFont8[i].c == c)
            return kLaunchFont8[i].r;
    }
    return kLaunchFont8[0].r; // space fallback
}

static void GL_DrawGlyph8(SDL_Renderer* ren, int x, int y, char c, int scale, const SDL_Color& col)
{
    const unsigned char* rows = GL_FontRows(c);
    SDL_SetRenderDrawColor(ren, col.r, col.g, col.b, col.a);
    for (int row = 0; row < 8; ++row)
    {
        unsigned char bits = rows[row];
        for (int colx = 0; colx < 8; ++colx)
        {
            if (bits & (0x80 >> colx))
            {
                SDL_Rect r;
                r.x = x + colx * scale;
                r.y = y + row * scale;
                r.w = scale;
                r.h = scale;
                SDL_RenderFillRect(ren, &r);
            }
        }
    }
}

static int GL_TextWidth(const std::string& text, int scale)
{
    int n = 0;
    for (size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] != '\n')
            ++n;
    }
    return n * 8 * scale;
}

static void GL_DrawText(SDL_Renderer* ren, int x, int y, const std::string& text, int scale, const SDL_Color& col)
{
    int cx = x;
    int cy = y;
    for (size_t i = 0; i < text.size(); ++i)
    {
        char ch = text[i];
        if (ch == '\n')
        {
            cy += 8 * scale + 2;
            cx = x;
            continue;
        }
        GL_DrawGlyph8(ren, cx, cy, ch, scale, col);
        cx += 8 * scale;
    }
}


// Launcher window: background + big centered list with fade-out
static bool GL_RunGameLauncher(const std::vector<GameInstall>& installs, GameInstall& outSelection)
{
    if (installs.empty())
        return false;

    // Determine window size & which background to use (match actual display size)
    int winW = 960;
    int winH = 720;
    bool useWideBG = false;

    SDL_DisplayMode dm;
    if (SDL_GetCurrentDisplayMode(0, &dm) == 0 && dm.w > 0 && dm.h > 0)
    {
        float aspect = dm.w / (float)dm.h;
        useWideBG = (aspect > 1.5f); // 16:9 vs 4:3/5:4 etc.

        // Use the real display size to avoid affecting game scaling
        winW = dm.w;
        winH = dm.h;
    }

    SDL_Window* win = SDL_CreateWindow(
        "ZGloom Launcher",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        winW, winH,
        SDL_WINDOW_SHOWN);

    if (!win)
        return false;

    Uint32 renderFlags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, renderFlags);
    if (!ren)
    {
        SDL_DestroyWindow(win);
        return false;
    }

    SDL_Texture* bgTex = nullptr;
    {
        const unsigned char* bgData = useWideBG ? kLauncherBG_16_9_BMP : kLauncherBG_4_3_BMP;
        int bgSize = useWideBG ? (int)sizeof(kLauncherBG_16_9_BMP) : (int)sizeof(kLauncherBG_4_3_BMP);
        SDL_RWops* rw = SDL_RWFromConstMem(bgData, bgSize);
        SDL_Surface* bg = rw ? SDL_LoadBMP_RW(rw, 1) : nullptr;
        if (bg)
        {
            bgTex = SDL_CreateTextureFromSurface(ren, bg);
            SDL_FreeSurface(bg);
        }
    }

    const SDL_Color colTitle     = { 255, 255, 255, 255 };
    const SDL_Color colNormal    = { 255, 255, 255, 255 };
    const SDL_Color colSelected  = { 255, 230, 100, 255 };

    // Font scales
    const int scaleTitle     = 7; // SELECT GAME
    const int scaleList      = 7; // Spieleliste
    const int scaleHint      = 4; // etwas kleiner fuer Hint-Zeile

    // Uppercase labels for drawing
    std::vector<std::string> labelsUpper;
    labelsUpper.reserve(installs.size());
    for (size_t i = 0; i < installs.size(); ++i)
    {
        std::string s = installs[i].label;
        for (size_t j = 0; j < s.size(); ++j)
            s[j] = (char)std::toupper((unsigned char)s[j]);
        labelsUpper.push_back(s);
    }

    const std::string title     = "SELECT GAME";
    const std::string hint      = "DPAD TO MOVE     A TO START     B TO EXIT";

    const int fontHTitle     = 8 * scaleTitle;
    const int fontHHint      = 8 * scaleHint;
    const int fontHList      = 8 * scaleList;
    const int listGap        = fontHList / 2;

    // Titel oben, Hint direkt darunter (beide zentriert)
    const int titleY = winH / 12;
    const int hintY  = titleY + fontHTitle + fontHHint / 2;

    // Spieleliste vertikal zentriert
    const int numEntries       = (int)installs.size();
    const int listBlockHeight  = numEntries * fontHList + (numEntries - 1) * listGap;
    const int listY0           = (winH - listBlockHeight) / 2;

    bool running  = true;
    int  selected = 0;

    // Optional: erstes Gamepad oeffnen
    SDL_GameController* pad = nullptr;
    for (int i = 0; i < SDL_NumJoysticks(); ++i)
    {
        if (SDL_IsGameController(i))
        {
            pad = SDL_GameControllerOpen(i);
            if (pad)
                break;
        }
    }

    while (running)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
            {
                selected = -1;
                running = false;
                break;
            }
            else if (e.type == SDL_KEYDOWN)
            {
                switch (e.key.keysym.sym)
                {
                case SDLK_UP:
                    if (selected > 0) --selected;
                    else selected = numEntries - 1;
                    break;
                case SDLK_DOWN:
                    if (selected < numEntries - 1) ++selected;
                    else selected = 0;
                    break;
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                    running = false;
                    break;
                case SDLK_ESCAPE:
                    selected = -1;
                    running = false;
                    break;
                default:
                    break;
                }
            }
            else if (e.type == SDL_CONTROLLERBUTTONDOWN)
            {
                switch (e.cbutton.button)
                {
                case SDL_CONTROLLER_BUTTON_DPAD_UP:
                    if (selected > 0) --selected;
                    else selected = numEntries - 1;
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                    if (selected < numEntries - 1) ++selected;
                    else selected = 0;
                    break;
                case SDL_CONTROLLER_BUTTON_A:
                    running = false;
                    break;
                case SDL_CONTROLLER_BUTTON_B:
                    selected = -1;
                    running = false;
                    break;
                default:
                    break;
                }
            }
        }

        if (!running)
            break;

        if (bgTex)
        {
            SDL_RenderClear(ren);
            SDL_RenderCopy(ren, bgTex, nullptr, nullptr);
        }
        else
        {
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
            SDL_RenderClear(ren);
        }

        // Titel oben mittig
        int titleW = GL_TextWidth(title, scaleTitle);
        int titleX = (winW - titleW) / 2;
        GL_DrawText(ren, titleX, titleY, title, scaleTitle, colTitle);

        // Hint-Zeile darunter, mittig
        int hintW = GL_TextWidth(hint, scaleHint);
        int hintX = (winW - hintW) / 2;
        GL_DrawText(ren, hintX, hintY, hint, scaleHint, colNormal);

        // Spieleliste mittig
        for (int i = 0; i < numEntries; ++i)
        {
            const SDL_Color& col = (i == selected) ? colSelected : colNormal;
            int w  = GL_TextWidth(labelsUpper[i], scaleList);
            int x  = (winW - w) / 2;
            int y  = listY0 + i * (fontHList + listGap);
            GL_DrawText(ren, x, y, labelsUpper[i], scaleList, col);
        }


        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }

    int resultIndex = selected;

    // Fade-out (0.6 s) if a game was selected
    if (resultIndex >= 0)
    {
        const int fadeFrames  = 24;
        const int fadeDelayMs = 25; // 24 * 25 ms = 600 ms

        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

        for (int i = 0; i < fadeFrames; ++i)
        {
            float t = (float)(i + 1) / (float)fadeFrames;
            Uint8 alpha = (Uint8)(t * 255.0f + 0.5f);

            SDL_SetRenderDrawColor(ren, 0, 0, 0, alpha);
            SDL_RenderFillRect(ren, nullptr);
            SDL_RenderPresent(ren);
            SDL_Delay(fadeDelayMs);
        }

        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    }

    if (pad)
        SDL_GameControllerClose(pad);
    if (bgTex)
        SDL_DestroyTexture(bgTex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);

    if (resultIndex < 0)
        return false;

    outSelection = installs[resultIndex];
    return true;
}



static inline bool GL_IsMenuToggleKey(SDL_Keycode sym)
{
	return sym == SDLK_MENU || sym == SDLK_APPLICATION;
}

int main(int argc, char* argv[])
{
	SDL_Log("ZGloom: main() start (argc=%d)", argc);
#ifdef __ANDROID__
	SDL_Log("ZGloom: running on Android");
#endif

	// Initialize SDL first (for launcher + gamepad)
	if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0)
	{
		std::cout << "SDL_Init Error: " << SDL_GetError() << std::endl;
		return 1;
	}

#ifdef __ANDROID__
	// SDL needs to be inited before this to pick up gamepad and Android paths
	ConfigureAndroidDataRoot();
#endif

	bool selectedGloom3 = false;
	bool selectedZombieMassacre = false;
	bool selectedGloomClassic = false;
	GameInstall chosenInstall;
	bool haveChosenInstall = false;

	// Discover compatible game installs (current DataRoot + subdirectories)
	std::vector<GameInstall> installs;
	GL_DiscoverGameInstalls(installs);

	if (installs.empty())
	{
		// Fallback: original Zombie Massacre auto-detect in current DataRoot
		if (FILE* file = fopen("stuf/stages", "rb"))
		{
			fclose(file);
			Config::SetZM(true);
			selectedZombieMassacre = true;
		}
	}
	else
	{
		GameInstall chosen;

		if (installs.size() == 1)
		{
			chosen = installs[0];
#ifdef __ANDROID__
			GL_AndroidSetBuildInfoOverlayVisible(false);
#endif
		}
		else
		{
#ifdef __ANDROID__
			GL_AndroidSetBuildInfoOverlayVisible(true);
#endif
			if (!GL_RunGameLauncher(installs, chosen))
			{
#ifdef __ANDROID__
				GL_AndroidSetBuildInfoOverlayVisible(false);
#endif
				SDL_Quit();
				return 0;
			}
#ifdef __ANDROID__
			GL_AndroidSetBuildInfoOverlayVisible(false);
#endif
		}

		selectedGloom3 = (chosen.label == "Gloom 3") || GL_IsGloom3Name(chosen.baseDir);
		selectedZombieMassacre = chosen.isZM || (chosen.label == "Zombie Massacre");
		const std::string compactChosenName = GL_CompactGameName(chosen.baseDir);
		const bool explicitClassicName = (chosen.label == "Gloom Classic" ||
			compactChosenName == "gloom" || compactChosenName == "gloomclassic");
		const bool rootClassicFallback = chosen.baseDir.empty() && chosen.label == "Gloom" &&
			!GL_FileExistsIn(chosen.baseDir, "pics/title");
		selectedGloomClassic = !selectedZombieMassacre && !selectedGloom3 &&
			(explicitClassicName || rootClassicFallback);

#ifdef __ANDROID__
		// Refine DataRoot to chosen subfolder and chdir there
		std::string root = Config::GetDataRoot();
		if (!root.empty())
		{
			char last = root.back();
			if (last != '/' && last != '\\')
				root.push_back('/');
		}

		if (!chosen.baseDir.empty())
		{
			root += chosen.baseDir;
			if (!root.empty())
			{
				char last2 = root.back();
				if (last2 != '/' && last2 != '\\')
					root.push_back('/');
			}

			Config::SetDataRoot(root);
			if (chdir(root.c_str()) != 0)
			{
				SDL_Log("ZGloom: chdir('%s') to chosen DataRoot failed (errno=%d)", root.c_str(), errno);
			}
			else
			{
				SDL_Log("ZGloom: chdir to chosen DataRoot OK");
			}
		}
#else
		if (!chosen.baseDir.empty())
		{
#ifdef _WIN32
			_chdir(chosen.baseDir.c_str());
#else
			chdir(chosen.baseDir.c_str());
#endif
		}
#endif // __ANDROID__

		Config::SetZM(chosen.isZM);
		chosenInstall = chosen;
		haveChosenInstall = true;
	}

	SDL_Log("ZGloom: SDL_Init succeeded, calling Config::Init()");
	Config::Init();
#ifdef __ANDROID__
	if (haveChosenInstall && chosenInstall.androidForceWide && Config::GetDisplayAspect() != 1)
	{
		SDL_Log("ZGloom: Android unknown Gloom-engine install '%s' uses 4:3 config; forcing 16:9 fullscreen presentation", chosenInstall.label.c_str());
		Config::SetDisplayAspect(1);
	}
#endif
	AtmosphereVolume::LoadFromConfig();
	BGM::Init();
	BGM::SetVolume9(AtmosphereVolume::Get());

	SDL_Log("ZGloom: Config and BGM initialized");

	GloomMap gmap;
	Script script;
	TitleScreen titlescreen;
	MenuScreen menuscreen;
	GameState state = STATE_TITLE;
	SDL_Log("ZGloom: core objects constructed (GloomMap, Script, TitleScreen, MenuScreen)");
/* xmp_context g_xmp;  // replaced by global g_xmp */
	g_xmp = xmp_create_context();
	Config::RegisterMusContext(g_xmp);

	int renderwidth, renderheight, windowwidth, windowheight;

	Config::GetRenderSizes(renderwidth, renderheight, windowwidth, windowheight);

	// Apply aspect preset: 0 = 4:3, 1 = 16:9
	int aspect = Config::GetDisplayAspect();
	if (aspect == 0)
	{
		// Original 4:3
		renderwidth  = 320;
		renderheight = 256;
		windowwidth  = 960;
		windowheight = 768;
	}
	else if (aspect == 1)
	{
		// 16:9 widescreen: keep vertical res, widen horizontally (455x256 -> 1365x768)
		renderwidth  = 455;
		renderheight = 256;
		windowwidth  = 1365;
		windowheight = 768;
	}

	CrmFile titlemusic;
	CrmFile intermissionmusic;
	CrmFile ingamemusic;
	CrmFile titlepic;

	titlemusic.Load(Config::GetMusicFilename(0).c_str());
	intermissionmusic.Load(Config::GetMusicFilename(1).c_str());

	SoundHandler::Init();

	Uint32 windowFlags = SDL_WINDOW_SHOWN | (Config::GetFullscreen()?SDL_WINDOW_FULLSCREEN:0);
#ifdef __ANDROID__
	if (haveChosenInstall && chosenInstall.androidForceWide)
	{
		windowFlags |= SDL_WINDOW_FULLSCREEN;
	}
#endif
	SDL_Window* win = SDL_CreateWindow("ZGloom", 100, 100, windowwidth, windowheight, windowFlags);
	if (win == nullptr)
	{
		std::cout << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
		return 1;
	}

	SDL_Log("ZGloom: SDL_CreateWindow OK (%dx%d)", windowwidth, windowheight);

	Config::RegisterWin(win);

	SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | (Config::GetVSync()?SDL_RENDERER_PRESENTVSYNC:0));
	if (ren == nullptr)
	{
		std::cout << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
		return 1;
	}
	SDL_Log("ZGloom: SDL_CreateRenderer OK");
	RendererHooks::init(ren, windowwidth, windowheight);

	SDL_Texture* rendertex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, renderwidth, renderheight);
	if (rendertex == nullptr)
	{
		std::cout << "SDL_CreateTexture Error: " << SDL_GetError() << std::endl;
		return 1;
	}

	SDL_Log("ZGloom: SDL_CreateTexture OK (%dx%d)", renderwidth, renderheight);

	SDL_ShowCursor(SDL_DISABLE);

	SDL_Surface* render8 = SDL_CreateRGBSurface(0, 320, 256, 8, 0, 0, 0, 0);
	SDL_Surface* intermissionscreen = SDL_CreateRGBSurface(0, 320, 256, 8, 0, 0, 0, 0);
	SDL_Surface* titlebitmap = SDL_CreateRGBSurface(0, 320, 256, 8, 0, 0, 0, 0);
	SDL_Surface* titlemenubitmap = SDL_CreateRGBSurface(0, 320, 256, 8, 0, 0, 0, 0);
	SDL_Surface* splashbitmap = SDL_CreateRGBSurface(0, 320, 256, 8, 0, 0, 0, 0);
	SDL_Surface* render32 = SDL_CreateRGBSurface(0, renderwidth, renderheight, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
	SDL_Surface* screen32 = SDL_CreateRGBSurface(0, 320, 256, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
	SDL_Surface* zmTitleOverlay8 = nullptr;
	SDL_Surface* zmTitleOverlay32 = nullptr;


	SDL_Log("ZGloom: Surfaces created (render8/intermission/title/render32/screen32)");

	ZHUD_EnsureCreated(ren, renderwidth, renderheight);
    // HUD resources are created via ZHUD_EnsureCreated below; removed local redeclarations here.
	ObjectGraphics objgraphics;
	Renderer renderer;
	GameLogic logic;
	Camera cam;
	Hud hud;

	SDL_Log("ZGloom: before logic.Init");
	logic.Init(&objgraphics);
	SDL_Log("ZGloom: after logic.Init");
	SDL_AddTimer(1000 / 25, my_callbackfunc, NULL);
	SDL_Log("ZGloom: SDL_AddTimer installed");

	SDL_Event sEvent;

	bool notdone = true;

#if 1
	Font smallfont, bigfont;
	CrmFile fontfile;
	SDL_Log("ZGloom: loading embedded bigfont2/smallfont fallback fonts");
	if (GL_LoadEmbeddedCrm2(kEmbeddedBigfont2Crm2, kEmbeddedBigfont2Crm2Size, fontfile))
	{
		bigfont.Load2(fontfile);
		smallfont.Load2(fontfile);
	}
	else
	{
		fontfile.Load((Config::GetMiscDir() + "bigfont2.bin").c_str());
		if (fontfile.data)
		{
			bigfont.Load2(fontfile);
			smallfont.Load2(fontfile);
		}
		else
		{
			fontfile.Load((Config::GetMiscDir() + "smallfont.bin").c_str());
			if (fontfile.data)smallfont.Load(fontfile);
			fontfile.Load((Config::GetMiscDir() + "bigfont.bin").c_str());
			if (fontfile.data)bigfont.Load(fontfile);
		}
	}
	SDL_Log("ZGloom: fonts loaded");
#endif

	SDL_Log("ZGloom: loading original-style title flow pictures");
	const bool haveSplash = LoadPic(Config::GetPicsDir() + "blackmagic", splashbitmap);
	const bool haveTitle = selectedGloomClassic
		? GL_LoadPicWithEmbeddedFallback(
			Config::GetPicsDir() + "title",
			titlebitmap,
			kEmbeddedGloomClassicTitleCrm2,
			kEmbeddedGloomClassicTitleCrm2Size,
			kEmbeddedGloomClassicTitlePal,
			kEmbeddedGloomClassicTitlePalSize)
		: LoadPic(Config::GetPicsDir() + "title", titlebitmap);

	if (haveTitle)
	{
		SDL_SetPaletteColors(titlemenubitmap->format->palette, titlebitmap->format->palette->colors, 0, 256);
		SDL_BlitSurface(titlebitmap, nullptr, titlemenubitmap, nullptr);

		// Gloom 3 already has the complete title artwork in pics/title.
		// Do not draw pics/gloom or pics/gloombrush over it.
		const bool suppressTitleLogoOverlay = selectedGloom3 || selectedZombieMassacre ||
			GL_IsGloom3Name(Config::GetDataRoot());
		if (!suppressTitleLogoOverlay)
		{
			if (selectedGloomClassic)
			{
				// Gloom Classic uses the brush artwork from the original start flow.
				// Keep the plain titlebitmap untouched so ABOUT can hide the brush,
				// while titlemenubitmap restores it automatically on return to MAIN.
				if (OverlayPicAt(Config::GetPicsDir() + "gloombrush", titlemenubitmap, 168))
				{
					SDL_Log("ZGloom: Gloom Classic gloombrush title overlay loaded at y=168");
				}
				else if (OverlayPicAt(Config::GetPicsDir() + "gloom", titlemenubitmap, 168))
				{
					SDL_Log("ZGloom: Gloom Classic gloom title overlay loaded at y=168");
				}
				else if (GL_OverlayEmbeddedPicAt(
					kEmbeddedGloomClassicBrushCrm2,
					kEmbeddedGloomClassicBrushCrm2Size,
					titlemenubitmap,
					168))
				{
					SDL_Log("ZGloom: using embedded Gloom Classic brush fallback at y=168");
				}
				else
				{
					SDL_Log("ZGloom: Gloom Classic title brush missing on disk and in embedded fallback");
				}
			}
			else
			{
				// Gloom Deluxe and compatible data sets normally use pics/gloom.
				// Keep gloombrush as a fallback for older/private data sets.
				if (!OverlayPicAt(Config::GetPicsDir() + "gloom", titlemenubitmap, 168))
				{
					OverlayPicAt(Config::GetPicsDir() + "gloombrush", titlemenubitmap, 168);
				}
			}
		}
	}
	else if (haveSplash)
	{
		SDL_Log("ZGloom: title picture missing, using blackmagic as title fallback");
		SDL_SetPaletteColors(titlebitmap->format->palette, splashbitmap->format->palette->colors, 0, 256);
		SDL_BlitSurface(splashbitmap, nullptr, titlebitmap, nullptr);
		SDL_SetPaletteColors(titlemenubitmap->format->palette, splashbitmap->format->palette->colors, 0, 256);
		SDL_BlitSurface(splashbitmap, nullptr, titlemenubitmap, nullptr);
	}
	else
	{
		SDL_Log("ZGloom: neither title nor blackmagic found, using black title screen");
		SDL_FillRect(titlebitmap, nullptr, 0);
		SDL_FillRect(titlemenubitmap, nullptr, 0);
	}

	// Zombie Massacre's g3-dc uses its own palette.  Keep it as a separate
	// 32-bit overlay instead of copying its indices into the title palette.
	// It is composed only on the main title menu, never on ABOUT.
	if (selectedZombieMassacre)
	{
		zmTitleOverlay8 = SDL_CreateRGBSurface(0, 320, 70, 8, 0, 0, 0, 0);
		if (zmTitleOverlay8 && LoadPic(Config::GetPicsDir() + "g3-dc", zmTitleOverlay8))
		{
			zmTitleOverlay32 = SDL_ConvertSurfaceFormat(zmTitleOverlay8, SDL_PIXELFORMAT_ARGB8888, 0);
			if (zmTitleOverlay32)
			{
				SDL_SetSurfaceBlendMode(zmTitleOverlay32, SDL_BLENDMODE_NONE);
				SDL_Log("ZGloom: Zombie Massacre g3-dc title overlay loaded at y=167");
			}
		}
		else
		{
			SDL_Log("ZGloom: Zombie Massacre g3-dc title overlay missing");
		}
	}

	if (selectedGloomClassic)
		SDL_Log("ZGloom: Gloom Classic 2x2 world rendering enabled");

	if (haveSplash && haveTitle)
	{
		state = STATE_SPLASH;
	}
	uint32_t splashStartTicks = SDL_GetTicks();
	const uint32_t splashDurationMs = 1200;

	if (titlemusic.data)
	{
		if (xmp_load_module_from_memory(g_xmp, titlemusic.data, titlemusic.size))
		{
			std::cout << "music error";
		}

		if (xmp_start_player(g_xmp, 22050, 0))
		{
			std::cout << "music error";
		}
		Mix_HookMusic(fill_audio, g_xmp);
		Config::SetMusicVol(Config::GetMusicVol());
	}

	std::string intermissiontext;
	std::size_t intermissionTypewriterTotal = 0;
	uint32_t intermissionTypewriterStartTicks = 0;
	bool intermissionTypewriterComplete = true;
	const uint32_t intermissionTypewriterCharacterMs = 40;

	// RESUME SAVED POSITION must not show the first script intermission.  We
	// still parse the opening picture/song/tile commands so the normal level
	// context is available, but suppress visible drawing, music and WAIT.
	bool skipIntermissionForResume = false;
	bool intermissionPictureValid = false;

	bool intermissionmusplaying = false;
	bool haveingamemusic = false;
	bool printscreen = false;
	int screennum = 0;
	uint32_t fps = 0;
	uint32_t fpscounter = 0;

	Mix_Volume(-1, Config::GetSFXVol()*12);
	Mix_VolumeMusic(Config::GetMusicVol() * 12);

	//try and blit title etc into the middle of the screen
	SDL_Rect blitrect;

	int screenscale = renderheight / 256;
	blitrect.w = 320 * screenscale;
	blitrect.h = 256 * screenscale;
	blitrect.x = (renderwidth - 320 * screenscale) / 2;
	blitrect.y = (renderheight - 256 * screenscale) / 2;

	SDL_SetRelativeMouseMode(SDL_TRUE);
	
	//set up the level select

	const std::vector<LevelDescriptor>& levelcatalog = script.GetLevels();
	titlescreen.SetLevels(levelcatalog);
	int levelselect = 0;

	SDL_Log("ZGloom: entering main loop");

	while (notdone)
	{
		ZHUD_EnsureAlive(ren, renderwidth, renderheight);
		RendererHooks::beginFrame();
		ZHUD_Clear();
		if ((state == STATE_PARSING) || (state == STATE_SPOOLING))
		{
			std::string scriptstring;
			Script::ScriptOp sop;

			sop = script.NextLine(scriptstring);

			switch (sop)
			{
				case Script::SOP_SETPICT:
				{
					// RESUME suppresses the visible intermission, not its resources.
					// Original scripts often set pict_ only once for an entire episode.
					scriptstring.insert(0, Config::GetPicsDir());
					intermissionPictureValid = LoadPic(scriptstring, intermissionscreen);
					if (intermissionPictureValid)
					{
						SDL_SetPaletteColors(render8->format->palette,
							intermissionscreen->format->palette->colors, 0, 256);
					}
					else
					{
						SDL_Log("ZGloom: failed to prepare intermission picture '%s'",
							scriptstring.c_str());
					}
					break;
				}
				case Script::SOP_SONG:
				{
					scriptstring.insert(0, Config::GetMusicDir());
					ingamemusic.Load(scriptstring.c_str());
					haveingamemusic = (ingamemusic.data != nullptr);
					break;
				}
				case Script::SOP_LOADFLAT:
				{
					char* end = nullptr;
					const long parsedFlat = std::strtol(scriptstring.c_str(), &end, 10);
					while (end && *end && std::isspace(static_cast<unsigned char>(*end)))
						++end;

					if (end != scriptstring.c_str() && end && *end == '\0' && parsedFlat >= 0 && parsedFlat <= 999)
					{
						if (!gmap.SetFlat(static_cast<int>(parsedFlat)))
							SDL_Log("ZGloom SaveSystem: failed to load floor/roof set %ld", parsedFlat);
					}
					else
					{
						SDL_Log("ZGloom SaveSystem: invalid flat token '%s'", scriptstring.c_str());
					}
					break;
				}
				case Script::SOP_LOADMAP:
				case Script::SOP_NOP:
					// No action needed here in intermission script
					break;
				case Script::SOP_TEXT:
				{
					intermissiontext = scriptstring;
					break;
				}
				case Script::SOP_DRAW:
				{
					if (state == STATE_PARSING && !skipIntermissionForResume)
					{
						if (intermissionmusic.data)
						{
							if (xmp_load_module_from_memory(g_xmp, intermissionmusic.data, intermissionmusic.size))
							{
								std::cout << "music error";
							}

							if (xmp_start_player(g_xmp, 22050, 0))
							{
								std::cout << "music error";
							}
							Mix_HookMusic(fill_audio, g_xmp);
							Config::SetMusicVol(Config::GetMusicVol());
							intermissionmusplaying = true;
						}
					}
					break;
				}
				case Script::SOP_WAIT:
				{
					if (state == STATE_PARSING)
					{
						if (skipIntermissionForResume && g_RequestTitleContinue)
						{
							// Continue parsing immediately.  The first PLAY command will
							// replace its map with the saved one and seek the script to
							// the command following that saved level's PLAY entry.
							intermissiontext.clear();
							intermissionTypewriterTotal = 0;
							intermissionTypewriterComplete = true;
							break;
						}

						state = STATE_WAITING;
						intermissionTypewriterTotal = smallfont.CountTypewriterCharacters(intermissiontext);
						intermissionTypewriterStartTicks = SDL_GetTicks();
						intermissionTypewriterComplete = (intermissionTypewriterTotal == 0);

						SDL_SetPaletteColors(render8->format->palette, smallfont.GetPalette()->colors, 0, 16);
						SDL_BlitSurface(intermissionscreen, NULL, render8, NULL);
					}
					break;
				}
				case Script::SOP_PLAY:
				{
					if (state == STATE_PARSING)
					{

					// Determine which level to load: script default or saved position
					std::string levelRel = scriptstring;
					SaveSystem::SaveData s;
					bool haveSavePos = false;
					bool haveReplay  = false;

					// For each new PLAY operation we start from a clean event history
					EventReplay::Clear();

					if (g_RequestTitleContinue)
					{
						g_RequestTitleContinue = false;
						skipIntermissionForResume = false;
						if (SaveSystem::LoadFromDisk(s))
						{
							levelRel     = s.levelPath;
							haveSavePos = true;

							// V1 stored only the first digit of tile_10, tile_11, etc.
							// Recover the canonical flat from the script when migrating an
							// older save, or when a save has no valid flat index.
							int scriptedFlat = -1;
							if (script.GetFlatForLevel(levelRel, scriptedFlat))
							{
								if (s.formatVersion == 1 || s.flatIndex < 0)
								{
									s.flatIndex = scriptedFlat;
								}
								else if (s.flatIndex != scriptedFlat)
								{
									SDL_Log("ZGloom SaveSystem: saved flat %d differs from script flat %d for %s",
										s.flatIndex, scriptedFlat, levelRel.c_str());
								}
							}

							script.SeekAfterPlayFor(levelRel);

							// V2 keeps the event history in savepos.txt.  V1 falls back
							// to the legacy last.events sidecar for compatibility.
							if (s.formatVersion >= 2)
							{
								EventReplay::SetEvents(s.eventHistory);
								haveReplay = !EventReplay::GetEvents().empty();
							}
							else if (EventReplay::LoadFromDisk())
							{
								haveReplay = true;
							}
						}
					}

					// Remember level path (relative) for save/resume
					SaveSystem::SetCurrentLevelPath(levelRel);

					// Default camera for new game; may be overridden by saved data
					cam.x.SetInt(0);
					cam.y = 120;
					cam.z.SetInt(0);
					cam.rotquick.SetInt(0);

					// Build full path and load requested level
					std::string levelFull = levelRel;
					levelFull.insert(0, Config::GetLevelDir());
					gmap.Load(levelFull.c_str(), &objgraphics);

					// The script has normally loaded episode 1's flat before reaching
					// the first PLAY command.  A continued game must explicitly reload
					// the floor/roof pair stored with its own episode before renderer init.
					if (haveSavePos && s.flatIndex >= 0)
					{
						if (!gmap.SetFlat(s.flatIndex))
						{
							SDL_Log("ZGloom SaveSystem: failed to restore floor/roof set %d for %s",
								s.flatIndex,
								levelRel.c_str());
						}
					}

					// Restore the shared life reserve before level initialisation.  Old V1/V2
					// saves have no LIVES line and therefore retain SaveData's default of 3.
					if (haveSavePos)
						logic.SetLives(s.lives);

					renderer.Init(render32, &gmap, &objgraphics);
					logic.InitLevel(&gmap, &cam, &objgraphics);

					// Restore only durable world state.  The replay path deliberately
					// suppresses teleports, sounds and a second monster spawn.
					if (haveReplay)
					{
						EventReplay::ReplayAll(gmap);
						logic.RestoreTriggeredEvents(EventReplay::GetEvents());
					}

					// If we continue from a save, restore camera and player state
					if (haveSavePos)
					{
						cam.x.SetInt(s.camX);
						cam.y = s.camY;
						cam.z.SetInt(s.camZ);
						cam.rotquick.SetInt(s.camRot);

						// Restore player stats (HP, weapon, reload state)
						for (auto& o : gmap.GetMapObjects())
						{
							if (o.t == ObjectGraphics::OLT_PLAYER1)
							{
								o.data.ms.hitpoints = s.hp;
								o.data.ms.weapon    = s.weapon;
								o.data.ms.reload    = s.reload;
								o.data.ms.reloadcnt = s.reloadcnt;
								break;
							}
						}
					}


					state = STATE_PLAYING;


						// Start embedded atmosphere BGM for each level (Vita-style)
						BGM::PlayLooping();
						BGM::SetVolume9(AtmosphereVolume::Get());

						if (haveingamemusic)
						{
							if (xmp_load_module_from_memory(g_xmp, ingamemusic.data, ingamemusic.size))
							{
								std::cout << "music error";
							}

							if (xmp_start_player(g_xmp, 22050, 0))
							{
								std::cout << "music error";
							}
							Mix_HookMusic(fill_audio, g_xmp);
							Config::SetMusicVol(Config::GetMusicVol());
						}
					}
					break;
				}
				case Script::SOP_END:
				{
					BGM::Stop(); // stop Atmosphere BGM on SOP_END
					state = STATE_TITLE;
					if (intermissionmusic.data && intermissionmusplaying)
					{
						Mix_HookMusic(nullptr, nullptr);
						xmp_end_player(g_xmp);
						xmp_release_module(g_xmp);
						intermissionmusplaying = false;
					}
					if (titlemusic.data)
					{
						if (xmp_load_module_from_memory(g_xmp, titlemusic.data, titlemusic.size))
						{
							std::cout << "music error";
						}

						if (xmp_start_player(g_xmp, 22050, 0))
						{
							std::cout << "music error";
						}
						Mix_HookMusic(fill_audio, g_xmp);
						Config::SetMusicVol(Config::GetMusicVol());
					}
					break;
				}
			}
		}

		if (state == STATE_SPLASH)
		{
			SDL_SetPaletteColors(render8->format->palette, splashbitmap->format->palette->colors, 0, 256);
			SDL_BlitSurface(splashbitmap, nullptr, render8, nullptr);
			if (SDL_GetTicks() - splashStartTicks >= splashDurationMs)
			{
				state = STATE_TITLE;
			}
		}
		else if (state == STATE_TITLE)
		{
			SDL_Surface* titleSource = titlescreen.WantsPlainTitleBackground() ? titlebitmap : titlemenubitmap;
			SDL_SetPaletteColors(render8->format->palette, titleSource->format->palette->colors, 0, 256);
			titlescreen.Render(titleSource, render8, smallfont);
		}
		else if (state == STATE_WAITING)
		{
			// Always restore the full intermission palette after gameplay/title
			// rendering, then reserve entries 0..15 for the small font.
			if (intermissionPictureValid)
			{
				SDL_SetPaletteColors(render8->format->palette,
					intermissionscreen->format->palette->colors, 0, 256);
			}
			SDL_SetPaletteColors(render8->format->palette, smallfont.GetPalette()->colors, 0, 16);
			SDL_BlitSurface(intermissionscreen, NULL, render8, NULL);

			std::size_t visibleCharacters = intermissionTypewriterTotal;
			if (!intermissionTypewriterComplete)
			{
				const uint32_t elapsed = SDL_GetTicks() - intermissionTypewriterStartTicks;
				visibleCharacters = std::min<std::size_t>(
					intermissionTypewriterTotal,
					static_cast<std::size_t>(elapsed / intermissionTypewriterCharacterMs) + 1);
				if (visibleCharacters >= intermissionTypewriterTotal)
					intermissionTypewriterComplete = true;
			}

			smallfont.PrintMultiLineMessageProgressive(
				intermissiontext, 220, render8, visibleCharacters);
		}

		while ((state!= STATE_SPOOLING) && SDL_PollEvent(&sEvent))
		{
			if (sEvent.type == SDL_WINDOWEVENT)
			{
				if (sEvent.window.event == SDL_WINDOWEVENT_CLOSE)
				{
					notdone = false;
				}
			}

			if (Config::HaveController() && (sEvent.type == SDL_CONTROLLERBUTTONDOWN))
			{
				// START/GUIDE/Menu is a true pause-menu toggle: open in-game, close in-game menu again.
				if (state == STATE_MENU && Config::GetControllerStart())
				{
					state = STATE_PLAYING;
					continue;
				}

				//fake up a key event
				if ((state == STATE_SPLASH) || (state == STATE_TITLE) || (state == STATE_MENU) || (state == STATE_WAITING))
				{
					// Confirm / activate (menus only)
					if (Config::GetControllerConfirm())
					{
						sEvent.type = SDL_KEYDOWN;
						sEvent.key.keysym.sym = SDLK_SPACE;
					}
					// Menu navigation (DPAD)
					if (Config::GetControllerUp())
					{
						sEvent.type = SDL_KEYDOWN;
						sEvent.key.keysym.sym = SDLK_UP;
					}
					if (Config::GetControllerDown())
					{
						sEvent.type = SDL_KEYDOWN;
						sEvent.key.keysym.sym = SDLK_DOWN;
					}
					// Numeric and multi-choice values are adjusted exclusively with DPAD.
					if (state == STATE_MENU && Config::GetControllerLeft())
					{
						sEvent.type = SDL_KEYDOWN;
						sEvent.key.keysym.sym = SDLK_LEFT;
					}
					if (state == STATE_MENU && Config::GetControllerRight())
					{
						sEvent.type = SDL_KEYDOWN;
						sEvent.key.keysym.sym = SDLK_RIGHT;
					}
					// B / BACK returns one submenu level; from MAIN it closes the menu.
					if (Config::GetControllerBack())
					{
						sEvent.type = SDL_KEYDOWN;
						sEvent.key.keysym.sym = SDLK_ESCAPE;
					}
				}

				if (state == STATE_PLAYING)
				{
					// call up menu (START or GUIDE, plus Y on OUYA handled in GetControllerStart)
					if (Config::GetControllerStart())
					{
						sEvent.type = SDL_KEYDOWN;
						sEvent.key.keysym.sym = SDLK_ESCAPE;
					}
					// Map / debug overlay -> TAB (U/X/Square)
					if (Config::GetControllerMap())
					{
						sEvent.type = SDL_KEYDOWN;
						sEvent.key.keysym.sym = SDLK_TAB;
					}
				}

			}

			if (state == STATE_SPLASH)
			{
				if ((sEvent.type == SDL_KEYDOWN) || (sEvent.type == SDL_MOUSEBUTTONDOWN) || (sEvent.type == SDL_CONTROLLERBUTTONDOWN))
				{
					state = STATE_TITLE;
				}
				continue;
			}

			if ((sEvent.type == SDL_KEYDOWN) && (sEvent.key.keysym.sym == SDLK_SPACE ||
				sEvent.key.keysym.sym == SDLK_RETURN ||
			   sEvent.key.keysym.sym == SDLK_LCTRL))
			{
				if (state == STATE_WAITING)
				{
					if (!intermissionTypewriterComplete)
					{
						// First press while text is still typing reveals the complete
						// message.  A second press continues to the level.
						intermissionTypewriterComplete = true;
					}
					else
					{
						state = STATE_PARSING;
						if (intermissionmusic.data)
						{
							Mix_HookMusic(nullptr, nullptr);
							xmp_end_player(g_xmp);
							xmp_release_module(g_xmp);
							intermissionmusplaying = false;
						}
					}
				}
			}

			if (sEvent.type == SDL_KEYDOWN)
			{
				if (state == STATE_MENU && GL_IsMenuToggleKey(sEvent.key.keysym.sym))
				{
					state = STATE_PLAYING;
					continue;
				}

				if (state == STATE_TITLE)
				{
					switch (titlescreen.Update(sEvent, levelselect))
					{
						case TitleScreen::TITLERET_PLAY:
							// A resume starts at the normal beginning of the script so
							// global song/tile setup is parsed, but its first visual
							// intermission block is suppressed.  SOP_PLAY then restores
							// the saved map and advances to its correct script position.
							skipIntermissionForResume = g_RequestTitleContinue;
							if (skipIntermissionForResume)
							{
								script.Reset();
								intermissiontext.clear();
								SDL_Log("ZGloom SaveSystem: resume requested; skipping intermission");
							}

							state = STATE_PARSING;
							BGM::Stop(); // ensure Atmosphere BGM is stopped on TITLERET_PLAY
							logic.Init(&objgraphics);
							if (titlemusic.data)
							{
								Mix_HookMusic(nullptr, nullptr);
								xmp_end_player(g_xmp);
								xmp_release_module(g_xmp);
							}
							break;
						case TitleScreen::TITLERET_SELECT:
						{
							if (levelselect < 0 || levelselect >= static_cast<int>(levelcatalog.size()) ||
								!script.SeekToLevel(static_cast<std::size_t>(levelselect)))
							{
								SDL_Log("ZGloom LevelSelect: invalid selection %d", levelselect);
								break;
							}

							const LevelDescriptor& selectedLevel = levelcatalog[static_cast<std::size_t>(levelselect)];
							g_RequestTitleContinue = false;
							EventReplay::Clear();
							intermissiontext.clear();

							// Directly restore the selected level's script context.  The script
							// resumes at that level's draw_ line, so its own intermission text
							// is still shown without spooling through earlier descriptions.
							if (!selectedLevel.pictureName.empty())
							{
								std::string picturePath = Config::GetPicsDir() + selectedLevel.pictureName;
								intermissionPictureValid = LoadPic(picturePath, intermissionscreen);
								if (intermissionPictureValid)
								{
									SDL_SetPaletteColors(render8->format->palette,
										intermissionscreen->format->palette->colors, 0, 256);
								}
							}

							if (selectedLevel.flatIndex >= 0 && !gmap.SetFlat(selectedLevel.flatIndex))
							{
								SDL_Log("ZGloom LevelSelect: failed to load flat %d for %s",
									selectedLevel.flatIndex, selectedLevel.mapPath.c_str());
							}

							state = STATE_PARSING;
							logic.Init(&objgraphics);
							if (titlemusic.data)
							{
								Mix_HookMusic(nullptr, nullptr);
								xmp_end_player(g_xmp);
								xmp_release_module(g_xmp);
							}

							SDL_Log("ZGloom LevelSelect: selected %s (flat=%d, script=%u)",
								selectedLevel.mapPath.c_str(), selectedLevel.flatIndex,
								selectedLevel.entryScriptLine);
							break;
						}
						case TitleScreen::TITLERET_QUIT:
							notdone = false;
							break;
						default:
							break;
					}
				}
				if (state == STATE_MENU)
				{
					MenuScreen::MenuReturn mr = menuscreen.Update(sEvent);

					if (g_RequestSavePosition)
					{
						g_RequestSavePosition = false;
						if (!logic.CanSavePosition())
						{
							SDL_Log("ZGloom SaveSystem: save ignored while Defender is active");
							continue;
						}
						SaveSystem::SaveData s;
						s.levelPath = SaveSystem::GetCurrentLevelPath();
						s.flatIndex = SaveSystem::GetCurrentFlat();
						s.camX = cam.x.GetInt();
						s.camY = cam.y;
						s.camZ = cam.z.GetInt();
						s.camRot = cam.rotquick.GetInt();

						// Capture current player state for save/resume
						MapObject player = logic.GetPlayerObj();

						s.hp        = player.data.ms.hitpoints;
						s.lives     = logic.GetLives();
						s.weapon    = player.data.ms.weapon;
						s.reload    = player.data.ms.reload;
						s.reloadcnt = player.data.ms.reloadcnt;

						s.eventHistory = EventReplay::GetEvents();

						const bool saveOk = SaveSystem::SaveToDisk(s);
						if (saveOk)
						{
							// Keep the legacy sidecar updated so the save can still be
							// imported into older ZGLOOM builds during the transition.
							EventReplay::SaveToDisk();
							SDL_Log("ZGloom SaveSystem: V2 save written (level=%s, flat=%d, lives=%d, events=%lu)",
								s.levelPath.c_str(),
								s.flatIndex,
								s.lives,
								static_cast<unsigned long>(s.eventHistory.size()));
						}
						else
						{
							SDL_Log("ZGloom SaveSystem: SAVE failed for level=%s", s.levelPath.c_str());
						}
					}

					switch (mr)
					{
						case MenuScreen::MENURET_PLAY:
							state = STATE_PLAYING;
							// The same ESC event used by B/BACK to close the main menu
							// reaches the generic PLAYING-state menu opener below.  Consume
							// it here, otherwise the menu is closed and immediately reopened
							// within the same SDL event cycle.
							continue;
						case MenuScreen::MENURET_QUIT:
							script.Reset();
							state = STATE_TITLE;
							BGM::Stop(); // ensure Atmosphere BGM is stopped on MENU->TITLE
							if (titlemusic.data)
							{
								if (xmp_load_module_from_memory(g_xmp, titlemusic.data, titlemusic.size))
								{
									std::cout << "music error";
								}

								if (xmp_start_player(g_xmp, 22050, 0))
								{
									std::cout << "music error";
								}
								Mix_HookMusic(fill_audio, g_xmp);
								Config::SetMusicVol(Config::GetMusicVol());
							}
							break;
						default:
							break;
					}
				}
				if ((state == STATE_PLAYING) && (sEvent.key.keysym.sym == SDLK_ESCAPE || GL_IsMenuToggleKey(sEvent.key.keysym.sym)))
				{
					state = STATE_MENU;
				}
			}

			if ((sEvent.type == SDL_KEYDOWN) && sEvent.key.keysym.sym == SDLK_F12)
			{
				Config::SetFullscreen(!Config::GetFullscreen());
			}

			if ((sEvent.type == SDL_KEYDOWN) && sEvent.key.keysym.sym == SDLK_TAB)
			{
				Config::SetDebug(!Config::GetDebug());
			}

			if ((sEvent.type == SDL_KEYDOWN) && sEvent.key.keysym.sym == SDLK_PRINTSCREEN)
			{
				printscreen = true;
			}

			if (sEvent.type == SDL_USEREVENT)
			{
				if (state == STATE_PLAYING)
				{
					const bool levelFinished = logic.Update(&cam);

					if (logic.ConsumeGameOverRequest())
					{
						// All lives are gone.  Leave the last Save Position untouched, reset
						// script progress for START NEW GAME, and return to the title menu.
						SDL_Log("ZGloom Lives: GAME OVER; returning to title menu");
						script.Reset();
						EventReplay::Clear();
						g_RequestTitleContinue = false;
						titlescreen.ResetToMain();
						BGM::Stop();

						if (haveingamemusic)
						{
							Mix_HookMusic(nullptr, nullptr);
							xmp_end_player(g_xmp);
							xmp_release_module(g_xmp);
							intermissionmusplaying = false;
						}

						state = STATE_TITLE;
						if (titlemusic.data)
						{
							if (xmp_load_module_from_memory(g_xmp, titlemusic.data, titlemusic.size))
								std::cout << "music error";
							if (xmp_start_player(g_xmp, 22050, 0))
								std::cout << "music error";
							Mix_HookMusic(fill_audio, g_xmp);
							Config::SetMusicVol(Config::GetMusicVol());
						}
					}
					else if (levelFinished)
					{
						BGM::Stop(); // stop Atmosphere BGM on level end
						if (haveingamemusic)
						{
							Mix_HookMusic(nullptr, nullptr);
							xmp_end_player(g_xmp);
							xmp_release_module(g_xmp);
							intermissionmusplaying = false;
						}
						state = STATE_PARSING;
					}
				}
				if (state == STATE_TITLE)
				{
					const bool controllerUp = Config::HaveController() && Config::GetControllerUp();
					const bool controllerDown = Config::HaveController() && Config::GetControllerDown();
					titlescreen.UpdateControllerHold(controllerUp, controllerDown);
					titlescreen.Clock();
				}
				if (state == STATE_MENU)
				{
					menuscreen.Clock();
				}

				fpscounter++;

				if (fpscounter >= 25)
				{
					Config::SetFPS(fps);
					fpscounter = 0;
					fps = 0;
				}
			}
		}

        // Atmosphere BGM is controlled explicitly on Vita-style transitions

		SDL_FillRect(render32, NULL, 0);

		if (state == STATE_PLAYING)
		{
			renderer.SetTeleEffect(logic.GetTeleEffect());
			renderer.SetPlayerHit(logic.GetPlayerHit());
			renderer.SetThermo(logic.GetThermo());

			//cam.x.SetInt(3969);
			//cam.z.SetInt(5359);
			//cam.rotquick.SetInt(254);
			renderer.Render(&cam);
			if (selectedGloomClassic)
				GL_PixelateWorld2x2(render32);
			MapObject pobj = logic.GetPlayerObj();
			hud.Render(hudLayer32, pobj, smallfont, logic.GetLives());
			fps++;
		}
		if (state == STATE_MENU)
		{
			renderer.Render(&cam);
			// Keep the paused Gloom Classic world at the same coarse 2x2
			// resolution as active gameplay.  Apply this before drawing the
			// menu so menu text and panels remain full-resolution and sharp.
			if (selectedGloomClassic)
				GL_PixelateWorld2x2(render32);
			menuscreen.Render(render32, render32, smallfont);
		}
		
		if ((state == STATE_WAITING) || (state == STATE_SPLASH) || (state == STATE_TITLE))
		{
			// SDL does not seem to like scaled 8->32 copy?
			SDL_BlitSurface(render8, NULL, screen32, NULL);

			// g3-dc belongs only to Zombie Massacre's main title menu.  ABOUT
			// remains the clean title; returning to MAIN composes it again here.
			if (state == STATE_TITLE && selectedZombieMassacre &&
				!titlescreen.WantsPlainTitleBackground() && zmTitleOverlay32)
			{
				SDL_Rect overlayDst = { 0, 167, zmTitleOverlay32->w, zmTitleOverlay32->h };
				SDL_BlitSurface(zmTitleOverlay32, nullptr, screen32, &overlayDst);
			}

			int aspect = Config::GetDisplayAspect();

			// 4:3 (oder sehr schmale Renderbreite): altes Verhalten
			if (aspect == 0 || renderwidth <= 320)
			{
				SDL_BlitScaled(screen32, NULL, render32, &blitrect);
			}
			else
			{
				// 16:9 static presentation matching gloom2.s WIDE: keep the
				// complete 320x256 title/intermission untouched in the centre and
				// extend only each scanline's edge colour as a four-step dark wash.
				// No image strip or text is horizontally stretched.
				SDL_Rect center = blitrect;
				GL_BlitStaticWideLikeGloom2(screen32, render32, center);
			}
		}



		if (printscreen)
		{
			std::string filename("img");

			filename += std::to_string(screennum);
			filename += ".bmp";
			screennum++;

			SDL_SaveBMP(render32, filename.c_str());
			printscreen = false;
		}

		if (state != STATE_SPOOLING)
		{
			
			// --- Muzzle Flash (PC port from Vita 7.19): apply per frame ---
			{
				static uint32_t s_last = SDL_GetTicks();
				uint32_t now = SDL_GetTicks();
				uint32_t dt = now - s_last; s_last = now;
				MuzzleFlashFX::Get().ApplyToSurface(render32);
				MuzzleFlashFX::Get().Update((float)dt);
			}
			// --- end muzzle flash port ---
			
			SDL_UpdateTexture(rendertex, NULL, render32->pixels, render32->pitch);
			SDL_RenderClear(ren);

			// Compute letterboxed destination rect for world texture
			int outW = 0, outH = 0;
			if (SDL_GetRendererOutputSize(ren, &outW, &outH) != 0 || outW <= 0 || outH <= 0)
			{
				outW = windowwidth;
				outH = windowheight;
			}

			float scaleX = outW / (float)renderwidth;
			float scaleY = outH / (float)renderheight;
			float scale = (scaleX < scaleY) ? scaleX : scaleY;

			int dstW = (int)(renderwidth * scale + 0.5f);
			int dstH = (int)(renderheight * scale + 0.5f);

			SDL_Rect dst;
			dst.w = dstW;
			dst.h = dstH;
			dst.x = (outW - dstW) / 2;
			dst.y = (outH - dstH) / 2;

			SDL_RenderCopy(ren, rendertex, NULL, &dst);

			SDL_UpdateTexture(hudTex, NULL, hudLayer32->pixels, hudLayer32->pitch);
			RendererHooks::SetHudTexture(hudTex);
			RendererHooks::endFramePresent();
		}
	}

	BGM::Shutdown();
	xmp_free_context(g_xmp);

	Config::Save();

	SoundHandler::Quit();

	SDL_FreeSurface(render8);
	SDL_FreeSurface(render32);
	SDL_FreeSurface(screen32);
	SDL_FreeSurface(intermissionscreen);
	SDL_FreeSurface(titlebitmap);
	SDL_FreeSurface(titlemenubitmap);
	if (zmTitleOverlay32) SDL_FreeSurface(zmTitleOverlay32);
	if (zmTitleOverlay8) SDL_FreeSurface(zmTitleOverlay8);
	SDL_FreeSurface(splashbitmap);
	SDL_DestroyRenderer(ren);
	SDL_DestroyWindow(win);
	SDL_Quit();

	return 0;
}