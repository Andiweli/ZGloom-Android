#include "renderer.h"
#include <cstdint>
#include "quick.h"
#include "gloommaths.h"
#include "objectgraphics.h"
#include "config.h"
#include "effects/RendererHooks.h"
#include "ConfigOverlays.h"
#include "effects/BlobShadowToggle.h"
// Forward decl from hud.cpp
void Hud_GetWeaponTint(int wepIndex, float& r, float& g, float& b);

// ---- Fast Smooth-Lighting LUT (8-bit fixed point) ---------------------------
#include <math.h>
static const int Z_MAX = 4096;
static uint8_t gLightLUT[Z_MAX+1];
static uint8_t gLightLUTFade[Z_MAX+1];
static bool gLightLUTInit = false;
static inline float SL_Curve(float z){
    const float k = 380.0f; const float gamma = 1.25f;
    float base = 1.0f / (1.0f + z / k);
    if (base < 0.f) base = 0.f;
    return powf(base, gamma);
}
static void SL_EnsureLUT(){
    if (gLightLUTInit) return;
    for (int z=0; z<=Z_MAX; ++z){
        float f  = SL_Curve((float)z);
        float ff = f * 0.85f;
        int fi  = (int)(f  * 255.0f + 0.5f);
        int ffi = (int)(ff * 255.0f + 0.5f);
        if (fi  < 0) fi  = 0; if (fi  > 255) fi  = 255;
        if (ffi < 0) ffi = 0; if (ffi > 255) ffi = 255;
        gLightLUT[z]     = (uint8_t)fi;
        gLightLUTFade[z] = (uint8_t)ffi;
    }
    gLightLUTInit = true;
}
static inline uint8_t SL_Factor(int z){
    if (!gLightLUTInit) SL_EnsureLUT();
    if (z < 0) z = 0; else if (z > Z_MAX) z = Z_MAX;
    return gLightLUT[z];
}
static inline uint8_t SL_FactorFade(int z){
    if (!gLightLUTInit) SL_EnsureLUT();
    if (z < 0) z = 0; else if (z > Z_MAX) z = Z_MAX;
    return gLightLUTFade[z];
}
static inline void ColourModifySmooth(uint8_t r, uint8_t g, uint8_t b, uint32_t& out, int z){
    uint8_t f = SL_Factor(z);
    uint32_t R = (uint32_t(r) * f) >> 8;
    uint32_t G = (uint32_t(g) * f) >> 8;
    uint32_t B = (uint32_t(b) * f) >> 8;
    out = 0xFF000000u | (R<<16) | (G<<8) | B;
}
static inline void ColourModifySmoothFade(uint8_t r, uint8_t g, uint8_t b, uint32_t& out, int z){
    uint8_t f = SL_FactorFade(z);
    uint32_t R = (uint32_t(r) * f) >> 8;
    uint32_t G = (uint32_t(g) * f) >> 8;
    uint32_t B = (uint32_t(b) * f) >> 8;
    out = 0xFF000000u | (R<<16) | (G<<8) | B;
}



// ---- Simple blob shadow helpers (ZGloom-Vita style, enemies only) ----------
static inline uint32_t ZG_DarkenPixel(uint32_t c, uint8_t alpha/*0..255*/) {
    // Darken towards black: alpha ~ 120 ~ ca. 47% Darkening
    uint32_t a = c & 0xFF000000u;
    uint32_t r = (c >> 16) & 0xFFu;
    uint32_t g = (c >>  8) & 0xFFu;
    uint32_t b =  c        & 0xFFu;

    r = (r * (255 - alpha)) >> 8;
    g = (g * (255 - alpha)) >> 8;
    b = (b * (255 - alpha)) >> 8;
    return a | (r << 16) | (g << 8) | b;
}
// ZGloom-PC: Vita-style projectile ground glow under flying bullets (tied to MUZZLE FLASH).
static inline void ZG_DrawProjectileGlow(
    uint32_t* surface,
    int renderwidth,
    int renderheight,
    const int32_t* zbuff,
    const int32_t* floorstart,
    int cx,
    int spriteWidth,
    int iz,
    float tintR,
    float tintG,
    float tintB,
    int cameraY,
    int focmult,
    int halfrenderheight)
{
    if (Config::GetMuzzleFlash() == 0) return;
    if (!surface || !zbuff || !floorstart) return;
    if (cx < 0 || cx >= renderwidth) return;
    if (iz <= 0) return;

    int fyCenter = halfrenderheight + (cameraY * focmult) / iz;
    if (fyCenter < 0) fyCenter = 0;
    if (fyCenter >= renderheight) fyCenter = renderheight - 1;

    const int zNear = 128;
    const int zFar  = 4096;
    int z = iz; if (z < zNear) z = zNear; if (z > zFar) z = zFar;
    float distF = 1.0f - (float)(z - zNear) / (float)(zFar - zNear);
    if (distF <= 0.01f) return;
    if (distF < 0.0f) distF = 0.0f; if (distF > 1.0f) distF = 1.0f;

    int baseRx = (spriteWidth * 3) / 4; if (baseRx < 1) baseRx = 1;
    int rx = (int)((float)baseRx * (0.5f + 0.5f * distF)); if (rx < 1) rx = 1;

    int baseRy = spriteWidth / 3; if (baseRy < 1) baseRy = 1;
    int ry = (int)((float)baseRy * (0.5f + 0.5f * distF)); if (ry < 1) ry = 1;

    for (int dx = -rx; dx <= rx; ++dx) {
        int sx = cx + dx;
        if (sx < 0 || sx >= renderwidth) continue;
        if (iz > zbuff[sx]) continue;

        int floorClip = floorstart[sx];
        if (floorClip < 0) floorClip = 0;
        if (floorClip >= renderheight) continue;

        for (int dy = -ry; dy <= ry; ++dy) {
            int sy = fyCenter + dy;
            if (sy < floorClip) continue;
            if (sy >= renderheight) break;

            float nx = (float)dx / (float)rx;
            float ny = (float)dy / (float)ry;
            float radial = 1.0f - (nx*nx + ny*ny);
            if (radial <= 0.0f) continue;

            float colStrength = distF * radial;
            if (colStrength <= 0.01f) continue;

            uint32_t* p = &surface[sx + sy * renderwidth];
            uint32_t c  = *p;
            int br = (c >> 16) & 0xFF;
            int bg = (c >>  8) & 0xFF;
            int bb = (c      ) & 0xFF;

            const float kGlowIntensity = 0.166667f; // ~1/6 of full brightness
            int addR = (int)(tintR * 255.0f * colStrength * kGlowIntensity);
            int addG = (int)(tintG * 255.0f * colStrength * kGlowIntensity);
            int addB = (int)(tintB * 255.0f * colStrength * kGlowIntensity);
            br += addR; if (br > 255) br = 255;
            bg += addG; if (bg > 255) bg = 255;
            bb += addB; if (bb > 255) bb = 255;
            *p = 0xFF000000u | (br << 16) | (bg << 8) | bb;
        }
    }
}


static inline bool ZG_IsEnemyLogicType(int t)
{
    using OLT = ObjectGraphics::ObjectLogicType;
    switch (t)
    {
        case OLT::OLT_MARINE:
        case OLT::OLT_BALDY:
        case OLT::OLT_TERRA:
        case OLT::OLT_GHOUL:
        case OLT::OLT_PHANTOM:
        case OLT::OLT_DRAGON:
        case OLT::OLT_LIZARD:
        case OLT::OLT_DEATHHEAD:
        case OLT::OLT_TROLL:
        case OLT::OLT_DEMON:
            return true;
        default:
            return false;
    }
}

// Draw a flat ellipse under the feet with Z-occlusion against geometry.
static inline void ZG_DrawBlobShadow(
    uint32_t* surface, int renderwidth, int renderheight, const int32_t* zbuff,
    int cx, int cy, int rx, int ry, int iz)
{
    if (!ZGloomPC::gBlobShadows) return;  // Toggle OFF -> kein Schatten
    if (rx <= 0 || ry <= 0) return;

    int y0 = cy - ry;
    int y1 = cy;
    if (y0 < 0) y0 = 0;
    if (y1 >= renderheight) y1 = renderheight - 1;

    for (int y = y0; y <= y1; ++y)
    {
        float ny  = (float)(y - cy) / (float)ry;            // [-1..0]
        float base = 1.0f - ny*ny; if (base < 0.0f) base = 0.0f;
        float spanf = (float)rx * sqrtf(base);
        int xl = cx - (int)spanf;
        int xr = cx + (int)spanf;

        if (xl < 0) xl = 0;
        if (xr >= renderwidth) xr = renderwidth - 1;
        if (xl > xr) continue;

        for (int x = xl; x <= xr; ++x)
        {
            if (iz > zbuff[x]) continue;  // von Geometrie verdeckt
            uint32_t idx = (uint32_t)(x + y * renderwidth);
            surface[idx] = ZG_DarkenPixel(surface[idx], 120);
        }
    }
}


const uint32_t Renderer::darkpalettes[16][16] =
{ { 0x00000000, 0x00000011, 0x00000022, 0x00000033, 0x00000044, 0x00000055, 0x00000066, 0x00000077, 0x00000088, 0x00000099, 0x000000aa, 0x000000bb, 0x000000cc, 0x000000dd, 0x000000ee, 0x000000ff },
{ 0x00000000, 0x00000000, 0x00000011, 0x00000022, 0x00000033, 0x00000044, 0x00000055, 0x00000066, 0x00000077, 0x00000088, 0x00000099, 0x000000aa, 0x000000bb, 0x000000cc, 0x000000dd, 0x000000ee },
{ 0x00000000, 0x00000000, 0x00000011, 0x00000022, 0x00000033, 0x00000044, 0x00000055, 0x00000066, 0x00000077, 0x00000077, 0x00000088, 0x00000099, 0x000000aa, 0x000000bb, 0x000000cc, 0x000000dd },
{ 0x00000000, 0x00000000, 0x00000011, 0x00000022, 0x00000033, 0x00000044, 0x00000044, 0x00000055, 0x00000066, 0x00000077, 0x00000088, 0x00000088, 0x00000099, 0x000000aa, 0x000000bb, 0x000000cc },
{ 0x00000000, 0x00000000, 0x00000011, 0x00000022, 0x00000033, 0x00000033, 0x00000044, 0x00000055, 0x00000066, 0x00000066, 0x00000077, 0x00000088, 0x00000099, 0x00000099, 0x000000aa, 0x000000bb },
{ 0x00000000, 0x00000000, 0x00000011, 0x00000022, 0x00000022, 0x00000033, 0x00000044, 0x00000044, 0x00000055, 0x00000066, 0x00000066, 0x00000077, 0x00000088, 0x00000088, 0x00000099, 0x000000aa },
{ 0x00000000, 0x00000000, 0x00000011, 0x00000011, 0x00000022, 0x00000033, 0x00000033, 0x00000044, 0x00000055, 0x00000055, 0x00000066, 0x00000066, 0x00000077, 0x00000088, 0x00000088, 0x00000099 },
{ 0x00000000, 0x00000000, 0x00000011, 0x00000011, 0x00000022, 0x00000022, 0x00000033, 0x00000033, 0x00000044, 0x00000055, 0x00000055, 0x00000066, 0x00000066, 0x00000077, 0x00000077, 0x00000088 },
{ 0x00000000, 0x00000000, 0x00000011, 0x00000011, 0x00000022, 0x00000022, 0x00000033, 0x00000033, 0x00000044, 0x00000044, 0x00000055, 0x00000055, 0x00000066, 0x00000066, 0x00000077, 0x00000077 },
{ 0x00000000, 0x00000000, 0x00000000, 0x00000011, 0x00000011, 0x00000022, 0x00000022, 0x00000033, 0x00000033, 0x00000033, 0x00000044, 0x00000044, 0x00000055, 0x00000055, 0x00000066, 0x00000066 },
{ 0x00000000, 0x00000000, 0x00000000, 0x00000011, 0x00000011, 0x00000011, 0x00000022, 0x00000022, 0x00000033, 0x00000033, 0x00000033, 0x00000044, 0x00000044, 0x00000044, 0x00000055, 0x00000055 },
{ 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000011, 0x00000011, 0x00000011, 0x00000022, 0x00000022, 0x00000022, 0x00000033, 0x00000033, 0x00000033, 0x00000044, 0x00000044, 0x00000044 },
{ 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000011, 0x00000011, 0x00000011, 0x00000011, 0x00000022, 0x00000022, 0x00000022, 0x00000022, 0x00000033, 0x00000033, 0x00000033, 0x00000033 },
{ 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000011, 0x00000011, 0x00000011, 0x00000011, 0x00000011, 0x00000022, 0x00000022, 0x00000022, 0x00000022, 0x00000022 },
{ 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000011, 0x00000011, 0x00000011, 0x00000011, 0x00000011, 0x00000011, 0x00000011, 0x00000011 },
{ 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 } };


static int FloorThreadKicker(void* data)
{
	Renderer* r = (Renderer*)data;
	r->DrawFloor(r->camerastash);
	return 0;
}

static int WallThreadKicker(void* data)
{
	Renderer* r = (Renderer*)data;
	r->RenderColumns(1, 2);
	return 0;
}

static void debugVline(int x, int y1, int y2, SDL_Surface* s, uint32_t c)
{
	if ((x < 0) || (x >= s->w)) return;
	if (y2 < y1) std::swap(y1, y2);

	for (int y = y1; y <= y2; y++)
	{
		if ((y >= 0) && (y < s->h))
		{
			((uint32_t*)(s->pixels))[x + s->pitch/4 * y] = c;
		}
	}	
}

static void debugline(int x1, int y1, int x2, int y2, SDL_Surface* s, uint32_t c)
{
	if ((x1 < 0) && (x2 < 0)) return;
	if ((y1 < 0) && (y2 < 0)) return;
	if ((x1 >= s->w) && (x2 >= s->w)) return;
	if ((y1 >= s->h) && (y2 >= s->h)) return;

	if (x1 == x2)
	{
		debugVline(x1, y1, y2, s, c);
		return;
	}

	if (x1 > x2)
	{
		std::swap(x1, x2);
		std::swap(y1, y2);
	}

	// can't be arsed implementing full brenhhnhnnnsnnann for just debug
	// Also : TURNS OUT BLOODY SDL2 HAS A LINE FUNCTION NOW. Actually, no: only onto renderers, not surfaces?

	float m = (float)(y2 - y1) / (float)(x2 - x1);
	float fy = (float)y1;

	int ly = y1;

	for (auto x = x1; x <= x2; x++)
	{
		int y = (int)fy;

		debugVline(x, y, ly, s, c);

		ly = y;
		fy += m;
	}
}

bool Renderer::PointInFront(int16_t fx, int16_t fz, Wall& z)
{

	if (fz < z.wl_nz) return true;
	if (fz > z.wl_fz) return false;
	// cross product to check if angle pivots towards or away from the origin compared to the wall

	// wall deltas
	int32_t wdx = z.wl_rx;
	int32_t wdz = z.wl_rz;
	wdx -= z.wl_lx;
	wdz -= z.wl_lz;

	// left wall to test point
	int32_t tx = fx;
	int32_t tz = fz;

	tx -= z.wl_lx;
	tz -= z.wl_lz;

	return (wdx*tz - wdz*tx) <= 0;
}

void Renderer::DrawMap()
{
	int16_t scale = 8;
	int16_t sx1, sx2, sz1, sz2;

	int z = 0;
	for (auto w : walls)
	{
		if ((gloommap->GetZones()[z].ztype == 1) && (gloommap->GetZones()[z].a || gloommap->GetZones()[z].b))
		{
			sx1 = rendersurface->w/2 + w.wl_lx / scale;
			sz1 = rendersurface->h/2 - w.wl_lz / scale;
			sx2 = rendersurface->w/2 + w.wl_rx / scale;
			sz2 = rendersurface->h/2 - w.wl_rz / scale;

			uint32_t col;

			if (w.valid)
			{
				col = 0xffffffff;
			}
			else
			{
				col = 0xff0000ff;
			}

			debugline(sx1, sz1, sx2, sz2, rendersurface, col);
		}

		++z;
	}
}

bool Renderer::OriginSide(int16_t fx, int16_t fz, int16_t bx, int16_t bz)
{
	// quick cross product to check what side of the origin a line that goes behing the camera falls

	// front to origin
	int32_t ftoox = -fx;
	int32_t ftooz = -fz;

	//front to back
	int32_t ftobx = bx;
	int32_t ftobz = bz;

	ftobx -= fx;
	ftobz -= fz;

	return (ftobx*ftooz - ftobz*ftoox) > 0;
}

void Renderer::Init(SDL_Surface* nrendersurface, GloomMap* ngloommap, ObjectGraphics* nobjectgraphics)
{
	rendersurface = nrendersurface;
	renderwidth = rendersurface->w;
	renderheight = rendersurface->h;
	halfrenderwidth = renderwidth / 2;
	halfrenderheight = renderheight / 2;
	gloommap = ngloommap;
	objectgraphics = nobjectgraphics;

	castgrads.resize(renderwidth);
	zbuff.resize(renderwidth);
	ceilend.resize(renderwidth);
	floorstart.resize(renderwidth);

	walls.resize(gloommap->GetZones().size());

	focmult = Config::GetFocalLength();

	for (auto x = 0; x < renderwidth; x++)
	{
		Quick f;
		Quick g;

		f.SetVal(focmult);
		g.SetVal(x - halfrenderwidth);

		castgrads[x] = g / f;
	}

	for (auto x = 0; x < renderwidth; x++)
	{
		Quick f;
		Quick g;

		f.SetVal(focmult);
		g.SetVal(x - halfrenderwidth);

		castgrads[x] = g / f;
	}

	// darkness tables
	//or (auto d = 0; d < 16; d++)
	//
	//	for (auto c = 0; c < 16; c++)
	//	{
	//		darkpalettes[d][c] = c * (16 - d) / 16;
	//		darkpalettes[d][c] |= darkpalettes[d][c] << 4;
	//
	//		printf("0x%08x,", darkpalettes[d][c]);
	//	}
	//	printf("\n");
	//
}

void Renderer::DrawCeil(Camera* camera)
{
	//TODO
	// skip over invalid runs for performance
	// work out why tz needs a weird +32 to align properly
	if (!gloommap->HasFlat())
	{
		return;
	}

	Flat& ceil = gloommap->GetCeil();

	Quick camrots[4];

	int32_t maxend = *std::max_element(ceilend.begin(), ceilend.end());

	if (maxend >= halfrenderheight) maxend = halfrenderheight - 1;

	GloomMaths::GetCamRot(-camera->rotquick.GetInt(), camrots);

	for (int32_t y = 0; y < maxend; y++)
	{
		int32_t z = ((int32_t)(256 - camera->y) * focmult) / (halfrenderheight - y);

		uint32_t pal = GetDimPalette(z);

		Quick qx, dx, qz, temp, dz;
		Quick f;

		f.SetInt(focmult);

		qx.SetInt(z*-halfrenderwidth / (focmult));
		qz.SetInt(z);

		temp = qx;

		qx = qx*camrots[0] + qz*camrots[1];
		qz = temp*camrots[2] + qz*camrots[3];

		dx.SetInt(z);
		dx = dx / f;
		temp = dx;

		//dz is initially zero!

		dx = dx*camrots[0];
		dz = temp*camrots[2];

		qz = qz + camera->z;
		qx = qx + camera->x;

		for (int32_t x = 0; x < renderwidth; x++)
		{
			if (y < ceilend[x])
			{
				auto ix = qx.GetInt() & 0x7F;
				auto iz = (qz.GetInt()+32) & 0x7F;

				uint8_t r = ceil.palette[ceil.data[ix][iz]][0];
				uint8_t g = ceil.palette[ceil.data[ix][iz]][1];
				uint8_t b = ceil.palette[ceil.data[ix][iz]][2];

				// dim it
				uint32_t dimcol;

				if (fadetimer)
				{
					ColourModifySmoothFade(r,g,b,dimcol,z);
				}
				else
				{
					ColourModifySmooth(r,g,b,dimcol,z);
				}

				((uint32_t*)(rendersurface->pixels))[x + y*renderwidth] = dimcol;
			}

			qx = qx + dx;
			qz = qz + dz;
		}
	}
}

void Renderer::DrawFloor(Camera* camera)
{
	//TODO
	// skip over invalid runs for performance
	// work out why tz needs a weird +32 to align properly
	if (!gloommap->HasFlat())
	{
		return;
	}

	Flat& floor = gloommap->GetFloor();

	Quick camrots[4];

	int32_t minstart = *std::min_element(floorstart.begin(), floorstart.end());

	if (minstart <= halfrenderheight) minstart = halfrenderheight + 1;

	GloomMaths::GetCamRot(-camera->rotquick.GetInt(), camrots);

	for (int32_t y = minstart; y < renderheight; y++)
	{
		int32_t z = (int32_t)(camera->y * focmult) / (y - halfrenderheight);

		uint32_t pal = GetDimPalette(z);

		Quick qx, dx, qz, temp, dz;
		Quick f;

		f.SetInt(focmult);

		qx.SetInt(z*-halfrenderwidth / (focmult));
		qz.SetInt(z);

		temp = qx;

		qx = qx*camrots[0] + qz*camrots[1];
		qz = temp*camrots[2] + qz*camrots[3];

		dx.SetInt(z);
		dx = dx / f;
		temp = dx;

		//dz is initially zero!

		dx = dx*camrots[0];
		dz = temp*camrots[2];

		qz = qz + camera->z;
		qx = qx + camera->x;

		for (int32_t x = 0; x < renderwidth; x++)
		{
			if (y >= floorstart[x])
			{
				auto ix = qx.GetInt() & 0x7F;
				auto iz = (qz.GetInt() + 32) & 0x7F;

				uint8_t r = floor.palette[floor.data[ix][iz]][0];
				uint8_t g = floor.palette[floor.data[ix][iz]][1];
				uint8_t b = floor.palette[floor.data[ix][iz]][2];

				// dim it
				uint32_t dimcol;
				if (fadetimer)
				{
					ColourModifySmoothFade(r,g,b,dimcol,z);
				}
				else
				{
					ColourModifySmooth(r,g,b,dimcol,z);
				}

				((uint32_t*)(rendersurface->pixels))[x + y*renderwidth] = dimcol;
			}

			qx = qx + dx;
			qz = qz + dz;
		}
	}
}

void Renderer::DrawColumn(int32_t x, int32_t ystart, int32_t h, Column* texturedata, int32_t z, int32_t palused)
{
	Quick temp;
	Quick tscale;
	Quick tstart;
	int32_t yend = ystart + h;
	uint32_t pal = GetDimPalette(z);

	if (h == 0) return;
	if (h > 65535) return; // this overflows a quick! Can happen in high res

	uint32_t* surface = (uint32_t*)(rendersurface->pixels);

	if (yend > renderheight) yend = renderheight;

	tscale.SetInt(64);
	temp.SetInt(h);
	tstart.SetInt(0);

	tscale = tscale / temp;

	if (ystart < 0)
	{
		temp.SetInt(-ystart);
		ystart = 0;
		tstart = tscale * temp;
	}

	for (auto y = ystart; y < yend; y++)
	{
		uint16_t row = tstart.GetInt();

		if (row>63) row = 63;

		//if (gloommap->GetTextures()[t / 1280].columns.size())
		{
			uint8_t colour = texturedata->data[row];

			uint8_t r = gloommap->GetTextures()[palused].palette[colour][0];
			uint8_t g = gloommap->GetTextures()[palused].palette[colour][1];
			uint8_t b = gloommap->GetTextures()[palused].palette[colour][2];

			if (texturedata->flag && (colour == 0))
			{
				//translucent
				const uint32_t stripands[] = { 0xff00ffff, 0xffff00ff, 0xffffff00, 0xff0000ff, 0xff00ff00, 0xffff0000, 0xffffffff };

				uint32_t andval = stripands[7 + texturedata->flag];

				surface[x + y*renderwidth] = surface[x + y*renderwidth] & andval;
			}
			else
			{
				// dim it
				uint32_t dimcol;
				if (fadetimer)
				{
					ColourModifySmoothFade(r,g,b,dimcol,z);
				}
				else
				{
					ColourModifySmooth(r,g,b,dimcol,z);
				}
				surface[x + y*renderwidth] = dimcol;
			}
		}

		tstart = tstart + tscale;
	}
}

bool zcompare(const MapObject& first, const MapObject& second)
{
	return first.rotz > second.rotz;
}

void Renderer::DrawBlood(Camera* camera)
{
	Quick x, z, temp;
	Quick cammatrix[4];
	int32_t rotx, rotz;

	GloomMaths::GetCamRot(camera->rotquick.GetInt()&0xFF, cammatrix);

	uint32_t* surface = (uint32_t*)(rendersurface->pixels);

	for (auto &b : gloommap->GetBlood())
	{
		x = b.x;
		z = b.z;

		x = x - camera->x;
		z = z - camera->z;

		temp = x;
		x = (x * cammatrix[0]) + (z * cammatrix[1]);
		z = (temp * cammatrix[2]) + (z * cammatrix[3]);

		rotx = x.GetInt();
		rotz = z.GetInt();

		int32_t ix = rotx;
		int32_t iz = rotz;
		int32_t iy = -b.y.GetInt();

		if (iy<0) iy = -iy;// this encodes deathheads soul logic
		uint32_t pal = GetDimPalette(iz);

		iy -= camera->y;

		if (iz > 0)
		{
			ix *= focmult;
			ix /= iz;

			iy *= focmult;
			iy /= iz;

			ix += halfrenderwidth;
			iy = halfrenderheight - iy;

			uint32_t mask;
			if (fadetimer)
			{
				ColourModifyFade((b.color & 0xf00) >> 4, b.color & 0x0f0, (b.color & 0xf) << 4, mask, pal);
			}
			else
			{
				ColourModify((b.color & 0xf00) >> 4, b.color & 0x0f0, (b.color & 0xf) << 4, mask, pal);
			}

			for (int32_t dy = iy; dy < (iy + Config::GetBlood()); dy++)
			{
				for (int32_t dx = ix; dx < (ix + Config::GetBlood()); dx++)
				{
					if ((dx >= 0) && (dx < renderwidth))
					{
						if ((dy>0) && (dy < renderheight))
						{
							if (iz <= zbuff[dx]) surface[dx + dy*renderwidth] = mask;
						}
					}
				}
			}
		}
	}
}

void Renderer::DrawObjects(Camera* camera)
{
    // --- Dust parallax: feed camera motion (screen-space deltas) ---
    static int prev_cam_x = 0, prev_cam_z = 0, prev_cam_rot = 0;
    int cam_x = camera->x.GetInt();
    int cam_z = camera->z.GetInt();
    int cam_rot = camera->rotquick.GetInt() & 0xFF;

    int dx = cam_x - prev_cam_x;
    int dz = cam_z - prev_cam_z;
    int drot = cam_rot - prev_cam_rot;

    prev_cam_x = cam_x;
    prev_cam_z = cam_z;
    prev_cam_rot = cam_rot;

    // Approximate mapping to screen-space drift (tweakable)
    float screen_dx = dx * 0.08f;
    float screen_dy = dz * 0.08f;
    float yaw_rate = (float)((drot & 0xFF)) * (6.28318530718f / 256.0f);

    RendererHooks::setCameraMotion(screen_dx, screen_dy, yaw_rate);

	RendererHooks::markWorldFrame();

	Quick x, z, temp;
	Quick cammatrix[4];
	int32_t ix, iz, iy;

	GloomMaths::GetCamRot(camera->rotquick.GetInt()&0xFF, cammatrix);

	uint32_t* surface = (uint32_t*)(rendersurface->pixels);

	strips.insert(strips.end(), gloommap->GetMapObjects().begin(), gloommap->GetMapObjects().end());

	for (auto &o : strips)
	{
		// don't draw the player!

		if (!o.isstrip)
		{
			if ((o.t > 1) && (o.t != 3))
			{
				x = o.x;
				z = o.z;

				x = x - camera->x;
				z = z - camera->z;

				temp = x;
				x = (x * cammatrix[0]) + (z * cammatrix[1]);
				z = (temp * cammatrix[2]) + (z * cammatrix[3]);

				o.rotx = x.GetInt();
				o.rotz = z.GetInt();
			}
		}
	}

	// z sort

	strips.sort(zcompare);

	for (auto o:strips)
	{
		if (o.isstrip)
		{
			int32_t h = (256 * focmult) / (int32_t)o.rotz;
			int32_t ystart = halfrenderheight - ((int32_t)(256 - camera->y) * focmult) / o.rotz;

			if (o.rotz < zbuff[o.rotx]) DrawColumn(o.rotx, ystart, h, o.data.ts.column, o.rotz, o.data.ts.palette);
		}
		else
		{
			// don't draw the player!
			if ((o.t > 1) && (o.t != 3) && o.data.ms.render)
			{
				ix = o.rotx;
				iz = o.rotz;
				iy = -o.y.GetInt();
				iy -= camera->y;

				if (iz > 5) // add a bit of nearclip to prevent slowdown
				{
					std::vector<Shape>* s = o.data.ms.shape;

					uint16_t column = 0;

					int frametouse = 0;

					if (o.data.ms.render == 8)
					{
						// rotatable!
						/*
						bsr	calcangle2
						add	#16, d0
						sub	ob_rot(a5), d0
						lsr	#5, d0
						and	#7, d0
						*/
						//uint16_t ang = GloomMaths::CalcAngle(o.x.GetInt(), o.z.GetInt(), camera->x.GetInt(), camera->z.GetInt());

						uint16_t ang = GloomMaths::CalcAngle(camera->x.GetInt(), camera->z.GetInt(), o.x.GetInt(), o.z.GetInt());

						ang += 16;
						ang -= o.data.ms.rotquick.GetInt();
						ang >>= 5;
						ang &= 7;
						frametouse = ang | (((o.data.ms.frame >> 16) & 7) << 3);
					}
					else
					{
						frametouse = o.data.ms.frame >> 16;
					}

					//TODO: Gloom 3 seems to be missing baldy punch frames
					//update - it is, they vanish in the original when they punch you

					if ((size_t)frametouse >= s->size())
					{
						frametouse = s->size() - 1;
					}

					auto scale = o.data.ms.scale;
					auto shapewidth = (*s)[frametouse].w;
					auto shapeheight = (*s)[frametouse].h;

					ix *= focmult;
					ix /= iz;

					// Add handle! otherwise bullets fill screen
					iy -= (*s)[frametouse].h - (*s)[frametouse].yh - 1;
					iy *= focmult;
					iy /= iz;

					
int h = ((shapeheight * scale / 0x100) * focmult) / iz;
int w = ((shapewidth * scale / 0x100) * focmult) / iz;

if ((w > 0) && (h > 0))
{
    // ZGloom-PC: Vita-Style blob shadow NUR für Gegner
    if (ZG_IsEnemyLogicType(o.t) && ZGloomPC::gBlobShadows)
    {
        const int kShadowFarZ    = 4096;
        const int kShadowMinSize = 12;

        bool skipShadow = (iz > kShadowFarZ) && (w < kShadowMinSize && h < kShadowMinSize);
        if (!skipShadow)
        {
            // Sprite center X in Screen, Fußpunkt unten am Sprite
            int cx = ix + halfrenderwidth;
            int ystart_shadow = halfrenderheight - iy - h;
            int cy = ystart_shadow + h;

            // flache Ellipse unter den Füßen
            int rx = w / 3; if (rx < 1) rx = 1;
            int ry = w / 8; if (ry < 1) ry = 1;

            uint32_t* surface = (uint32_t*)(rendersurface->pixels);
            ZG_DrawBlobShadow(surface, renderwidth, renderheight, zbuff.data(), cx, cy, rx, ry, iz);
        }
    }
        // --- ZGloom-PC: projectile ground glow tied to MUZZLE FLASH (runs for any sprite object) ---
        {
            int wepIndex = -1;
            if (objectgraphics && o.data.ms.shape) {
                // Detect bullet/spark by shape pointer
                for (int wi = 0; wi < 5; ++wi) {
                    if (o.data.ms.shape == &objectgraphics->BulletShapes[wi] ||
                        o.data.ms.shape == &objectgraphics->SparkShapes[wi]) {
                        wepIndex = wi; break;
                    }
                }
                // Fallback: detect by logic type range for bullets
                if (wepIndex < 0 && o.t >= ObjectGraphics::OLT_WEAPON1 && o.t <= ObjectGraphics::OLT_WEAPON5) {
                    wepIndex = (int)o.t - (int)ObjectGraphics::OLT_WEAPON1;
                }
            }
            if (wepIndex >= 0) {
                int cx_proj = ix + halfrenderwidth;
                if (cx_proj >= 0 && cx_proj < renderwidth) {
                    float tr=1.0f, tg=1.0f, tb=1.0f;
                    Hud_GetWeaponTint(wepIndex, tr, tg, tb);
                    int glowW = w;

                    // Enlarge pickup glow at bottom of bobbing by +50%
                    if (o.t >= ObjectGraphics::OLT_WEAPON1 && o.t <= ObjectGraphics::OLT_WEAPON5) {
                        int ystart_sprite = halfrenderheight - iy - h;
                        int spriteBottom = ystart_sprite + h;
                        int floorClip = floorstart[cx_proj];
                        if (floorClip < 0) floorClip = 0;
                        if (floorClip >= renderheight) floorClip = renderheight - 1;
                        int gap = floorClip - spriteBottom; if (gap < 0) gap = 0;
                        int maxGap = h / 2; if (maxGap < 8) maxGap = 8; if (maxGap > 32) maxGap = 32;
                        float t = 1.0f - (float)gap / (float)maxGap; if (t < 0.0f) t = 0.0f; if (t > 1.0f) t = 1.0f;
                        const float kPickupBottomScale = 1.5f; // 50% larger at bottom
                        float sizeScale = 1.0f + (kPickupBottomScale - 1.0f) * t;
                        glowW = (int)((float)w * sizeScale); if (glowW < 1) glowW = 1;
                    }

                    ZG_DrawProjectileGlow(
                        (uint32_t*)rendersurface->pixels,
                        renderwidth, renderheight,
                        zbuff.data(),
                        floorstart.data(),
                        cx_proj,
                        glowW,
                        iz,
                        tr, tg, tb,
                        camera->y,
                        focmult,
                        halfrenderheight);
                }
            }
        }


    Quick temp;


						Quick dx;
						Quick dy;
						Quick tx, ty;

						tx.SetInt(0);
						ty.SetInt(0);

						dx.SetInt(shapewidth);
						dy.SetInt(shapeheight);

						temp.SetInt(w);
						dx = dx / temp;

						temp.SetInt(h);
						dy = dy / temp;

						int32_t ystart = halfrenderheight - iy - h;

						uint32_t pal = GetDimPalette(o.rotz);

						if ((ix + halfrenderwidth + w / 2) > 0)
						{
							for (int32_t sx = ix + halfrenderwidth - w / 2; sx < (ix + halfrenderwidth + w / 2); sx++)
							{
								if (sx >= renderwidth) break;
								ty.SetInt(0);

								for (int32_t sy = ystart; sy < (ystart + h); sy++)
								{
									bool zfail = false;

									if (thermo)
									{
										if ((sx >= 0) && (iz > zbuff[sx])) zfail = true;
									}
									else
									{
										if ((sx >= 0) && (iz > zbuff[sx])) break;
									}
									if (sy >= renderheight) break;

									if ((sx >= 0) && (sy >= 0))
									{
										auto col = (*s)[frametouse].data[ty.GetInt() + tx.GetInt()*shapeheight];

										if (col != 1)
										{
											uint32_t dimcol;

											// thermoglasses effect. Need to look at this more carefully
											if (zfail) col |= 0xFF;

											if (fadetimer)
											{
												ColourModifyFade(0xFF & (col >> 16), 0xFF & (col >> 8), 0xFF & col, dimcol, pal);
											}
											else
											{
												ColourModify(0xFF & (col >> 16), 0xFF & (col >> 8), 0xFF & col, dimcol, pal);
											}

											//transparency flag!
											if (!o.isstrip && (o.data.ms.blood & 0x8000))
											{
												uint32_t surcol = surface[sx + sy*renderwidth];
												uint32_t b = (((dimcol >> 0) & 0xFF) + ((surcol >> 0) & 0xFF)) / 2;
												uint32_t g = (((dimcol >> 8) & 0xFF) + ((surcol >> 8) & 0xFF)) / 2;
												uint32_t r = (((dimcol >>16) & 0xFF) + ((surcol >>16) & 0xFF)) / 2;

												surface[sx + sy*renderwidth] = (r<<16) | (g<<8) | b;
											}
											else
											{
												surface[sx + sy*renderwidth] = dimcol;
											}
										}
									}

									ty = ty + dy;
								}
								tx = tx + dx;
							}
						}
					}
				}
			}
		}
	}
}

int16_t Renderer::CastColumn(int32_t x, int16_t& zone, Quick& t)
{
	// I'm not sure what Gloom is doing with its wall casting. Something to do with rotating them into the line of the cast?
	// I've rolled my own
	Quick z;

	z.SetInt(30000);
	int16_t hitwall = 0;

	for (auto w: walls)
	{
		if (w.valid)
		{
			if ((x >= w.wl_lsx) && (x < w.wl_rsx))
			{
				Quick lx, lz, rx, rz, dx, dz, m, thisz;

				lx.SetInt(w.wl_lx);
				lz.SetInt(w.wl_lz);
				rx.SetInt(w.wl_rx);
				rz.SetInt(w.wl_rz);

				dx = rx - lx;
				dz = rz - lz;

				Quick divisor = (dx - castgrads[x] * dz);

				if (divisor.GetVal() != 0)
				{
					m = (castgrads[x] * lz - lx) / (dx - castgrads[x] * dz);
					thisz = lz + m*dz;

					// quick overflow check
					if (thisz.GetInt() < std::min(w.wl_lz, w.wl_rz))
					{
						thisz.SetInt(std::min(w.wl_lz, w.wl_rz));
					}

					if ((thisz < z) && (thisz.GetVal()>0))
					{
						Quick len;

						len.SetInt(w.len);

						if (m.GetVal() < 0) m.SetVal(0);

						// check for transparent column
						int basetexture;
						Column* texcol = GetTexColumn(hitwall, m, basetexture);

						if (texcol && texcol->flag)
						{
							// transparent!
							MapObject o;

							o.isstrip = true;
							o.data.ts.column = texcol;
							o.data.ts.palette = basetexture / 20;
							o.rotx = x;
							o.rotz = thisz.GetInt();
							if (Config::GetMT()) SDL_LockMutex(wallmutex);
							strips.push_back(o);
							if (Config::GetMT()) SDL_UnlockMutex(wallmutex);
						}
						else
						{
							t = m;
							zone = hitwall;
							z = thisz;
						}
					}
				}
			}
		}

		hitwall++;
	}

	return z.GetInt();
}

void Renderer::ProcessColumn(const uint32_t& x, const int16_t& y, std::vector<int32_t>& ceilend, std::vector<int32_t>& floorstart)
{
	int16_t hitzone;
	Quick texpos;
	int16_t z = CastColumn(x, hitzone, texpos);

	if ((z>0) && (z<30000))
	{
		int32_t h = (256 * focmult) / z;
		int32_t ystart = halfrenderheight - ((256 - y) * focmult) / z;

		ceilend[x] = ystart;
		floorstart[x] = ystart + h;

		int basetexture;
		Column* texcol = GetTexColumn(hitzone, texpos, basetexture);

		if (texcol)
		{
			DrawColumn(x, ystart, h, texcol, z, basetexture / 20);
		}
		zbuff[x] = z;
		//debugVline(x, ystart, ystart+h, rendersurface, 0xFFFF0000 + 255 - z / 16);
	}
	else
	{
		ceilend[x] = halfrenderheight;
		floorstart[x] = halfrenderheight;
	}
}

void Renderer::Render(Camera* camera)
{
	SDL_LockSurface(rendersurface);

	std::fill(zbuff.begin(), zbuff.end(), 30000);
	strips.clear();

	focmult = Config::GetFocalLength();

	for (auto x = 0; x < renderwidth; x++)
	{
		Quick f;
		Quick g;

		f.SetVal(focmult);
		g.SetVal(x - halfrenderwidth);

		castgrads[x] = g / f;
	}

	for (size_t z = 0; z < walls.size(); z++)
	{
		Zone zone = gloommap->GetZones()[z];

		if (zone.ztype == Zone::ZT_WALL  && (zone.a | zone.b))
		{
			walls[z].valid = true;

			Quick x1, z1, x2, z2;
			Quick cammatrix[4];

			x1.SetInt(zone.x1);
			z1.SetInt(zone.z1);
			x2.SetInt(zone.x2);
			z2.SetInt(zone.z2);

			x1 = x1 - camera->x;
			z1 = z1 - camera->z;
			x2 = x2 - camera->x;
			z2 = z2 - camera->z;

			GloomMaths::GetCamRot(camera->rotquick.GetInt()&0xFF, cammatrix);

			walls[z].wl_lx = ((x1 * cammatrix[0]) + (z1 * cammatrix[1])).GetInt();
			walls[z].wl_lz = ((x1 * cammatrix[2]) + (z1 * cammatrix[3])).GetInt();
			walls[z].wl_rx = ((x2 * cammatrix[0]) + (z2 * cammatrix[1])).GetInt();
			walls[z].wl_rz = ((x2 * cammatrix[2]) + (z2 * cammatrix[3])).GetInt();
			walls[z].wl_nz = std::min(walls[z].wl_lz, walls[z].wl_rz);
			walls[z].wl_fz = std::max(walls[z].wl_lz, walls[z].wl_rz);

			// a vain attempt to stop z fighting on the doors
			if (zone.open)
			{
				walls[z].wl_rz += 12;
				walls[z].wl_lz += 12;
			}

			walls[z].len = zone.ln;

			// start culling. obvious Z check
			if (walls[z].wl_fz <= 0)
			{
				walls[z].valid = false;
			}
		}
		else
		{
			walls[z].valid = false;
		}
	}
		
	// back face cull

	for (size_t z = 0; z < walls.size(); z++)
	{
		if (walls[z].valid)
		{
			if ((((int32_t)walls[z].wl_lx*(int32_t)walls[z].wl_rz) - ((int32_t)walls[z].wl_rx*(int32_t)walls[z].wl_lz)) >= 0)
			{
				walls[z].valid = false;
			}
		}
	}

	// Z divide
	for (size_t z = 0; z < walls.size(); z++)
	{
		if (walls[z].valid)
		{
			if (walls[z].wl_lz > 0)
			{
				int32_t t = ((int32_t)walls[z].wl_lx * focmult) / (int32_t)walls[z].wl_lz;

				walls[z].wl_lsx = t;

				// I don't know how gloom handles overflows here. There may be something to do with "exshift", I can't figure out what that's for
				
				if ((t > 0) & (walls[z].wl_lsx < 0))
				{
					t = 0x4000;
				}
				if ((t < 0) & (walls[z].wl_lsx > 0))
				{
					t = -0x4000;
				}

				// some more overflow checking
				if (t < 0x4000)
				{
					walls[z].wl_lsx = t + halfrenderwidth;
				}
				else
				{
					walls[z].wl_lsx = 0x4000;
				}
			}
			else
			{
				// uh oh
				walls[z].wl_lsx = OriginSide(walls[z].wl_rx, walls[z].wl_rz, walls[z].wl_lx, walls[z].wl_lz) ? -1 : renderwidth;
			}

			if (walls[z].wl_rz > 0)
			{
				int32_t t = ((int32_t)walls[z].wl_rx * focmult)  / (int32_t)walls[z].wl_rz;

				walls[z].wl_rsx = t;

				// I don't know how gloom handles overflows here. There may be something to do with "exshift", I can't figure out what that's for

				if ((t > 0) & (walls[z].wl_rsx < 0))
				{
					t = 0x4000;
				}
				if ((t < 0) & (walls[z].wl_rsx > 0))
				{
					t = -0x4000;
				}
				// some more overflow checking
				if (t < 0x4000)
				{
					walls[z].wl_rsx = t + halfrenderwidth;
				}
				else
				{
					walls[z].wl_rsx = 0x4000;
				}
			}
			else
			{
				// uh oh
				walls[z].wl_rsx = OriginSide(walls[z].wl_lx, walls[z].wl_lz, walls[z].wl_rx, walls[z].wl_rz) ? -1 : renderwidth;
			}

			if (walls[z].wl_lsx == walls[z].wl_rsx)
			{
				walls[z].valid = false;
			}

			if (walls[z].wl_lsx > walls[z].wl_rsx)
			{
				std::swap(walls[z].wl_lsx, walls[z].wl_rsx);
				std::swap(walls[z].wl_lx, walls[z].wl_rx);
				std::swap(walls[z].wl_lz, walls[z].wl_rz);
			}
		}

		if (walls[z].wl_rsx < 0)
		{
			walls[z].valid = false;
		}
		if (walls[z].wl_lsx >= renderwidth)
		{
			walls[z].valid = false;
		}

		//tidy up
		if (walls[z].wl_lsx < 0) walls[z].wl_lsx = 0;
		if (walls[z].wl_rsx < 0) walls[z].wl_rsx = 0;
		if (walls[z].wl_lsx > renderwidth) walls[z].wl_lsx = renderwidth;
		if (walls[z].wl_rsx > renderwidth) walls[z].wl_rsx = renderwidth;
	}

	if (Config::GetMT())
	{
		camerastash = camera;
		wallthread = SDL_CreateThread(WallThreadKicker, "wallthread", this);
		for (int32_t x = 0; x < renderwidth; x+=2)
		{
			ProcessColumn(x, camera->y, ceilend, floorstart);
		}
		SDL_WaitThread(wallthread, nullptr);
	}
	else
	{
		for (int32_t x = 0; x < renderwidth; x++)
		{
			ProcessColumn(x, camera->y, ceilend, floorstart);
		}
	}

	if (Config::GetMT())
	{
		floorthread = SDL_CreateThread(FloorThreadKicker, "floorthread", this);
		DrawCeil(camera);
		SDL_WaitThread(floorthread, nullptr);
	}
	else
	{
		DrawCeil(camera);
		DrawFloor(camera);
	}
	DrawObjects(camera);
	DrawBlood(camera);

	if (Config::GetDebug())
	{
		DrawMap();
	}

#if 0
	for (size_t z = 0; z < walls.size(); z++)
	{
		if (walls[z].valid)
		{
			//printf("%i: %i %i\n", z, walls[z].wl_lsx, walls[z].wl_rsx);

			//for (int x = walls[z].wl_lsx; x < walls[z].wl_rsx; x++)
			//{
			//	debugVline(x, 0, 25, rendersurface, 0xFFFF0000 + z * 342);
			//}
			//printf("%i: %i %i\n", z, walls[z].wl_lsx, walls[z].wl_rsx);
		}
	}
#endif

	SDL_UnlockSurface(rendersurface);
}

Column* Renderer::GetTexColumn(int hitzone, Quick texpos, int& basetexture)
{
	Quick scale;
	Column* result = nullptr;

	scale.SetInt(gloommap->GetZones()[hitzone].sc / 2);

	// scale is sometimes -ve? What? Possibly reflected texture? I've cobbled this together in a nasty way, don't understand the underlying logic
	if (gloommap->GetZones()[hitzone].sc < 0)
	{
		scale.SetInt(1);
	}

	// not sure how this, well, scales
	if (scale.GetInt() == 0) scale.SetInt(1);

	texpos = texpos*scale;

	auto textouse = texpos.GetInt();

	if (textouse < 0) textouse = 0;
	if (textouse > 7) textouse = 7;

	basetexture = gloommap->GetZones()[hitzone].t[textouse];
	int column = texpos.GetFrac() / (0x10000 / 64);

	// EMPIRICAL F-F-F-F-FUDGE

	if (gloommap->GetZones()[hitzone].sc < 0)
	{
		column /= -gloommap->GetZones()[hitzone].sc * 2;
	}

	Column** tc = gloommap->GetTexPointers();

	if (tc[basetexture])
	{
		result = tc[basetexture] + column;
	}

	return result;
}

Renderer::Renderer()
{
	// create this always, in case of MT switch on the fly
	wallmutex = SDL_CreateMutex();
}

Renderer::~Renderer()
{
	if (wallmutex) SDL_DestroyMutex(wallmutex);
}

