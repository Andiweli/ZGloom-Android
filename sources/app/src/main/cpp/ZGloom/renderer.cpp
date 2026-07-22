#include "renderer.h"
#include <cstdint>
#include <algorithm>
#include <cmath>
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
static inline int ZG_ClampTeleFade(int fadeTimer){
    if (fadeTimer < 0) return 0;
    if (fadeTimer > 24) return 24;
    return fadeTimer;
}

static inline void ColourModifySmoothFade(uint8_t r, uint8_t g, uint8_t b, uint32_t& out, int z, int fadeTimer){
    uint8_t f = SL_FactorFade(z);
    uint32_t R = (uint32_t(r) * f) >> 8;
    uint32_t G = (uint32_t(g) * f) >> 8;
    uint32_t B = (uint32_t(b) * f) >> 8;

    // Gloom Deluxe teleporter/level-exit beam: fade the already-distance-lit
    // colour towards the original blue-white teleport palette.  The previous
    // smooth-lighting path ignored fadetimer completely, so floors/ceilings/
    // walls only became slightly darker instead of turning blue.
    const int ft = ZG_ClampTeleFade(fadeTimer);
    if (ft > 0)
    {
        R = (R * (25 - ft) + 128u * ft) / 25u;
        G = (G * (25 - ft) + 128u * ft) / 25u;
        B = (B * (25 - ft) + 255u * ft) / 25u;
    }

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
    if (Config::GetReflections() < 1) return;
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

static inline bool ZG_IsPickupLogicType(int t)
{
    using OLT = ObjectGraphics::ObjectLogicType;
    switch (t)
    {
        case OLT::OLT_HEALTH:
        case OLT::OLT_WEAPON:
        case OLT::OLT_THERMO:
        case OLT::OLT_INFRA:
        case OLT::OLT_INVISI:
        case OLT::OLT_INVINC:
        case OLT::OLT_BOUNCY:
            return true;
        default:
            return false;
    }
}

static inline uint32_t ZG_BlendReflectionPixel(
    uint32_t dst, uint32_t src, uint8_t alpha, uint8_t brightness)
{
    if (alpha == 0) return dst;

    uint32_t sr = ((src >> 16) & 0xFFu) * brightness / 255u;
    uint32_t sg = ((src >>  8) & 0xFFu) * brightness / 255u;
    uint32_t sb = ( src        & 0xFFu) * brightness / 255u;
    uint32_t dr = (dst >> 16) & 0xFFu;
    uint32_t dg = (dst >>  8) & 0xFFu;
    uint32_t db =  dst        & 0xFFu;
    uint32_t inv = 255u - alpha;

    uint32_t r = (sr * alpha + dr * inv) / 255u;
    uint32_t g = (sg * alpha + dg * inv) / 255u;
    uint32_t b = (sb * alpha + db * inv) / 255u;
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

static inline uint32_t ZG_AlphaBlendPixel(uint32_t dst, uint32_t src, uint8_t alpha)
{
    const uint32_t inv = 255u - alpha;
    const uint32_t dr = (dst >> 16) & 0xFFu;
    const uint32_t dg = (dst >> 8) & 0xFFu;
    const uint32_t db = dst & 0xFFu;
    const uint32_t sr = (src >> 16) & 0xFFu;
    const uint32_t sg = (src >> 8) & 0xFFu;
    const uint32_t sb = src & 0xFFu;
    const uint32_t r = (dr * inv + sr * alpha + 127u) / 255u;
    const uint32_t g = (dg * inv + sg * alpha + 127u) / 255u;
    const uint32_t b = (db * inv + sb * alpha + 127u) / 255u;
    return 0xFF000000u | (r << 16) | (g << 8) | b;
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



static inline float ZG_DustClamp(float v, float mn, float mx)
{
    if (v < mn) return mn;
    if (v > mx) return mx;
    return v;
}

static inline float ZG_DustSaturate(float v)
{
    return ZG_DustClamp(v, 0.0f, 1.0f);
}

static inline float ZG_DustSmoothStep(float a, float b, float x)
{
    const float span = (b - a);
    if (span <= 0.0001f)
    {
        return (x >= b) ? 1.0f : 0.0f;
    }
    float t = ZG_DustSaturate((x - a) / span);
    return t * t * (3.0f - 2.0f * t);
}

static inline void ZG_BlendDustPixel(uint32_t& dst, uint32_t src, uint8_t alpha)
{
    if (alpha == 0) return;
    if (alpha >= 255)
    {
        dst = 0xFF000000 | (src & 0x00FFFFFF);
        return;
    }

    uint32_t dr = (dst >> 16) & 0xFF;
    uint32_t dg = (dst >> 8) & 0xFF;
    uint32_t db = dst & 0xFF;

    uint32_t sr = (src >> 16) & 0xFF;
    uint32_t sg = (src >> 8) & 0xFF;
    uint32_t sb = src & 0xFF;

    uint32_t inv = 255 - alpha;
    uint32_t r = (sr * alpha + dr * inv) / 255;
    uint32_t g = (sg * alpha + dg * inv) / 255;
    uint32_t b = (sb * alpha + db * inv) / 255;

    dst = 0xFF000000 | (r << 16) | (g << 8) | b;
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
	reflectioncover.resize(renderwidth);

	walls.resize(gloommap->GetZones().size());

	DustTuning dusttuning;
	dusttuning.enabled = (Config::GetDustEnabled() != 0);
	dusttuning.densityScale = Config::GetDustDensity();
	dusttuning.visibilityScale = Config::GetDustVisibility();
	dusttuning.speedScale = Config::GetDustSpeedScale();
	dusttuning.enablePlayerInfluence = (Config::GetDustPlayerInfluence() != 0);
	dustsystem.SetTuning(dusttuning);
	dustsystem.BuildFromMap(gloommap);
	SDL_Log("[dust] init zones=%d particles=%d", dustsystem.GetZoneCount(), dustsystem.GetParticleCount());
	dustrendercache.clear();
	dustcamvalid = false;
	dustlastticks = 0;

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
					ColourModifySmoothFade(r,g,b,dimcol,z,fadetimer);
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
					ColourModifySmoothFade(r,g,b,dimcol,z,fadetimer);
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
					ColourModifySmoothFade(r,g,b,dimcol,z,fadetimer);
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

void Renderer::ApplyTeleportPixelate()
{
	const int ft = ZG_ClampTeleFade(fadetimer);
	if (ft <= 1 || !rendersurface || !rendersurface->pixels)
	{
		return;
	}

	// Original Amiga Gloom calls pixelate when ob_pixsize is non-zero.
	// It copies the first pixel of a block over that block, with the first
	// block starting at half size.  This gives the level-exit/teleporter its
	// chunky "beam out" look instead of an instant map change.
	int block = ft;
	if (block < 2) block = 2;
	if (block > 64) block = 64;

	uint32_t* surface = (uint32_t*)rendersurface->pixels;

	for (int x = 0; x < renderwidth; )
	{
		int bw = (x == 0) ? (block >> 1) : block;
		if (bw < 1) bw = 1;
		if (x + bw > renderwidth) bw = renderwidth - x;

		for (int y = 0; y < renderheight; )
		{
			int bh = (y == 0) ? (block >> 1) : block;
			if (bh < 1) bh = 1;
			if (y + bh > renderheight) bh = renderheight - y;

			const uint32_t sample = surface[x + y * renderwidth];
			for (int yy = y; yy < y + bh; ++yy)
			{
				uint32_t* row = surface + yy * renderwidth + x;
				for (int xx = 0; xx < bw; ++xx)
				{
					row[xx] = sample;
				}
			}

			y += bh;
		}

		x += bw;
	}
}

void Renderer::DrawBloodPools(Camera* camera)
{
	if (!Config::BloodPoolsEnabled() || gloommap->GetBloodPools().empty())
		return;

	// A pool is assembled from several deterministic world-space lobes instead
	// of one fixed screen-space ellipse.  The seed is created once when the pool
	// is spawned, so the outline stays perfectly stable while the camera moves.
	// No frame-to-frame RNG is used and no shape data needs to be allocated.
	static const int kDirX[16] = {
		256, 237, 181,  98,   0, -98, -181, -237,
		-256, -237, -181, -98,   0,  98,  181,  237
	};
	static const int kDirZ[16] = {
		0,  98, 181, 237, 256, 237,  181,   98,
		0, -98,-181,-237,-256,-237, -181,  -98
	};

	auto Hash32 = [](uint32_t value) -> uint32_t
	{
		value ^= value >> 16;
		value *= 0x7FEB352Du;
		value ^= value >> 15;
		value *= 0x846CA68Bu;
		value ^= value >> 16;
		return value;
	};

	Quick cammatrix[4];
	GloomMaths::GetCamRot(camera->rotquick.GetInt() & 0xFF, cammatrix);
	uint32_t* surface = static_cast<uint32_t*>(rendersurface->pixels);

	for (auto& pool : gloommap->GetBloodPools())
	{
		Quick centreX = pool.x - camera->x;
		Quick centreZ = pool.z - camera->z;
		Quick temp = centreX;
		centreX = (centreX * cammatrix[0]) + (centreZ * cammatrix[1]);
		centreZ = (temp * cammatrix[2]) + (centreZ * cammatrix[3]);

		const int centreDepth = centreZ.GetInt();
		if (centreDepth <= 24 || centreDepth >= 8192)
			continue;

		const int centreScreenX = halfrenderwidth +
			(centreX.GetInt() * focmult) / centreDepth;
		if (centreScreenX < -192 || centreScreenX >= renderwidth + 192)
			continue;

		const int pal = GetDimPalette(centreDepth);
		uint32_t bloodColour = 0;
		const uint8_t r = static_cast<uint8_t>(((pool.color & 0xF00u) >> 8) * 17u);
		const uint8_t g = static_cast<uint8_t>(((pool.color & 0x0F0u) >> 4) * 17u);
		const uint8_t b = static_cast<uint8_t>((pool.color & 0x00Fu) * 17u);
		if (fadetimer)
			ColourModifyFade(r, g, b, bloodColour, pal);
		else
			ColourModify(r, g, b, bloodColour, pal);

		float distance = 1.0f - static_cast<float>(centreDepth - 128) /
			static_cast<float>(6144 - 128);
		if (distance < 0.0f) distance = 0.0f;
		if (distance > 1.0f) distance = 1.0f;
		const float baseAlpha = 70.0f + 88.0f * distance;

		uint32_t seed = 0x9E3779B9u ^ static_cast<uint32_t>(pool.seed);
		seed ^= static_cast<uint32_t>(pool.source);
		seed ^= static_cast<uint32_t>(pool.source >> 32);
		seed ^= static_cast<uint32_t>(pool.owner * 33u);
		seed = Hash32(seed);
		const int style = static_cast<int>(seed & 3u);
		const int baseDirection = static_cast<int>((seed >> 4) & 15u);

		auto DrawLobe = [&](int offsetX, int offsetZ, int radiusPercent,
			int verticalPercent, int startDelay, int duration,
			int alphaPercent)
		{
			int localAge = static_cast<int>(pool.age) - startDelay;
			if (localAge < 0)
				return;
			if (duration < 1)
				duration = 1;

			float grow = static_cast<float>(localAge) /
				static_cast<float>(duration);
			if (grow < 0.0f) grow = 0.0f;
			if (grow > 1.0f) grow = 1.0f;
			grow = grow * grow * (3.0f - 2.0f * grow);
			const float initial = startDelay == 0 ? 0.22f : 0.08f;
			grow = initial + (1.0f - initial) * grow;

			int worldRadius = (static_cast<int>(pool.targetRadius) *
				radiusPercent + 50) / 100;
			if (worldRadius < 3) worldRadius = 3;

			Quick worldX = pool.x;
			Quick worldZ = pool.z;
			Quick offset;
			offset.SetInt(offsetX);
			worldX = worldX + offset;
			offset.SetInt(offsetZ);
			worldZ = worldZ + offset;

			Quick viewX = worldX - camera->x;
			Quick viewZ = worldZ - camera->z;
			Quick rotateTemp = viewX;
			viewX = (viewX * cammatrix[0]) + (viewZ * cammatrix[1]);
			viewZ = (rotateTemp * cammatrix[2]) + (viewZ * cammatrix[3]);

			const int iz = viewZ.GetInt();
			if (iz <= 24 || iz >= 8192)
				return;

			const int cx = halfrenderwidth + (viewX.GetInt() * focmult) / iz;
			const int groundY = halfrenderheight + (camera->y * focmult) / iz;
			if (cx < -96 || cx >= renderwidth + 96 ||
				groundY < -32 || groundY >= renderheight + 32)
				return;

			int rx = static_cast<int>((worldRadius * grow * focmult) / iz);
			if (rx < 1) rx = 1;
			if (rx > 82) rx = 82;
			int ry = (rx * verticalPercent + 50) / 100;
			if (ry < 1) ry = 1;
			if (ry > 25) ry = 25;

			const int rx2 = rx * rx;
			const int ry2 = ry * ry;
			const int64_t limit = static_cast<int64_t>(rx2) * ry2;

			for (int sy = groundY - ry; sy <= groundY + ry; ++sy)
			{
				if (sy < 0 || sy >= renderheight)
					continue;
				const int dy = sy - groundY;

				for (int sx = cx - rx; sx <= cx + rx; ++sx)
				{
					if (sx < 0 || sx >= renderwidth)
						continue;
					if (iz > zbuff[sx] || iz > reflectioncover[sx])
						continue;
					if (sy < floorstart[sx])
						continue;

					const int dx = sx - cx;
					const int64_t metric = static_cast<int64_t>(dx * dx) * ry2 +
						static_cast<int64_t>(dy * dy) * rx2;
					if (metric > limit)
						continue;

					float radial = 1.0f - static_cast<float>(metric) /
						static_cast<float>(limit);
					if (radial < 0.0f) radial = 0.0f;
					// Dense centre with a softer edge.  Overlapping lobes naturally
					// create darker pooled areas without a separate texture buffer.
					radial = 0.30f + 0.70f * radial;
					int alpha = static_cast<int>(baseAlpha * radial *
						static_cast<float>(alphaPercent) / 100.0f + 0.5f);
					if (alpha < 1)
						continue;
					if (alpha > 190) alpha = 190;

					uint32_t& dst = surface[sx + sy * renderwidth];
					dst = ZG_AlphaBlendPixel(dst, bloodColour,
						static_cast<uint8_t>(alpha));
				}
			}
		};

		// The core varies in size and flatness.  Some styles shift it slightly
		// off-centre so the death position is not always the geometric centre.
		const uint32_t coreHash = Hash32(seed ^ 0xA511E9B3u);
		int coreOffsetX = 0;
		int coreOffsetZ = 0;
		if (style == 2)
		{
			const int shift = (static_cast<int>(pool.targetRadius) *
				(8 + static_cast<int>((coreHash >> 8) % 9u))) / 100;
			coreOffsetX = (kDirX[baseDirection] * shift) / 256;
			coreOffsetZ = (kDirZ[baseDirection] * shift) / 256;
		}
		const int coreRadius = 60 + static_cast<int>((coreHash >> 12) % 20u);
		const int coreVertical = 27 + static_cast<int>((coreHash >> 20) % 13u);
		DrawLobe(coreOffsetX, coreOffsetZ, coreRadius, coreVertical,
			0, 14 + static_cast<int>((coreHash >> 24) % 7u), 100);

		int lobeCount;
		if (pool.targetRadius <= 18)
			lobeCount = 3 + static_cast<int>((seed >> 9) & 1u);
		else
			lobeCount = 5 + static_cast<int>((seed >> 9) % 4u);

		for (int i = 1; i < lobeCount; ++i)
		{
			const uint32_t h = Hash32(seed + 0x6D2B79F5u *
				static_cast<uint32_t>(i));
			int direction = static_cast<int>(h & 15u);

			// Four broad procedural families: rounded, stretched, one-sided
			// spill and explosive starburst.  Jitter keeps members of the same
			// family from looking like preset templates.
			if (style == 1)
			{
				const int jitter = static_cast<int>((h >> 4) % 3u) - 1;
				direction = (baseDirection + ((i & 1) ? 0 : 8) +
					jitter + 16) & 15;
			}
			else if (style == 2)
			{
				const int fan = static_cast<int>((h >> 4) % 7u) - 3;
				direction = (baseDirection + fan + 16) & 15;
			}
			else if (style == 3)
			{
				direction = (direction + i * 3) & 15;
			}

			int radialPercent = 28 + static_cast<int>((h >> 8) % 46u);
			if (style == 1)
				radialPercent += 8;
			else if (style == 2)
				radialPercent += 4;
			if (radialPercent > 86) radialPercent = 86;

			const int offsetDistance = (static_cast<int>(pool.targetRadius) *
				radialPercent) / 100;
			const int offsetX = (kDirX[direction] * offsetDistance) / 256;
			const int offsetZ = (kDirZ[direction] * offsetDistance) / 256;
			int radiusPercent = 24 + static_cast<int>((h >> 15) % 32u);
			if (style == 0 && i == 1)
				radiusPercent += 8;
			const int verticalPercent = 25 +
				static_cast<int>((h >> 21) % 18u);
			const int delay = 2 + static_cast<int>((h >> 26) % 7u);
			const int duration = 10 + static_cast<int>((h >> 29) % 6u);
			const int alphaPercent = 68 + static_cast<int>((h >> 12) % 28u);

			DrawLobe(offsetX, offsetZ, radiusPercent, verticalPercent,
				delay, duration, alphaPercent);
		}

		// A few small detached droplets give larger pools a less uniform outer
		// contour.  They are delayed, so the pool appears to creep outward rather
		// than simply scaling one pre-made silhouette.
		int dropletCount = 0;
		if (pool.targetRadius > 18)
			dropletCount = static_cast<int>((seed >> 18) %
				(style == 3 ? 5u : 4u));
		else
			dropletCount = static_cast<int>((seed >> 18) & 1u);

		for (int i = 0; i < dropletCount; ++i)
		{
			const uint32_t h = Hash32(seed ^ (0xC2B2AE35u +
				0x27D4EB2Fu * static_cast<uint32_t>(i)));
			const int direction = static_cast<int>(h & 15u);
			const int radialPercent = 82 + static_cast<int>((h >> 5) % 42u);
			const int offsetDistance = (static_cast<int>(pool.targetRadius) *
				radialPercent) / 100;
			const int offsetX = (kDirX[direction] * offsetDistance) / 256;
			const int offsetZ = (kDirZ[direction] * offsetDistance) / 256;
			const int radiusPercent = 9 + static_cast<int>((h >> 12) % 11u);
			const int verticalPercent = 28 +
				static_cast<int>((h >> 20) % 15u);
			const int delay = 7 + static_cast<int>((h >> 25) % 6u);
			const int duration = 8 + static_cast<int>((h >> 29) % 5u);
			const int alphaPercent = 52 + static_cast<int>((h >> 15) % 24u);

			DrawLobe(offsetX, offsetZ, radiusPercent, verticalPercent,
				delay, duration, alphaPercent);
		}
	}
}

void Renderer::DrawBlood(Camera* camera)
{
	const int particleSize = Config::GetBloodParticleSize();
	if (particleSize <= 0)
		return;

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

			for (int32_t dy = iy; dy < (iy + particleSize); dy++)
			{
				for (int32_t dx = ix; dx < (ix + particleSize); dx++)
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

void Renderer::DrawDust(Camera* camera, float dt)
{
    DustTuning dusttuning = dustsystem.GetTuning();
    dusttuning.enabled = (Config::GetDustEnabled() != 0);
    dusttuning.densityScale = Config::GetDustDensity();
    dusttuning.visibilityScale = Config::GetDustVisibility();
    dusttuning.speedScale = Config::GetDustSpeedScale();
    dusttuning.enablePlayerInfluence = (Config::GetDustPlayerInfluence() != 0);
    dustsystem.SetTuning(dusttuning);

    if (!dusttuning.enabled)
    {
        return;
    }

    // In case the renderer got initialized before the map was fully ready, or the
    // first zone pass found nothing on a tight map, rebuild once here lazily.
    if (dustsystem.GetZoneCount() == 0 || dustsystem.GetParticleCount() == 0)
    {
        dustsystem.BuildFromMap(gloommap);
        SDL_Log("[dust] lazy rebuild zones=%d particles=%d", dustsystem.GetZoneCount(), dustsystem.GetParticleCount());
    }

    int32_t camx = camera->x.GetInt();
    int32_t camz = camera->z.GetInt();
    int32_t dx = 0;
    int32_t dz = 0;
    if (dustcamvalid)
    {
        dx = camx - lastdustcamx;
        dz = camz - lastdustcamz;
    }
    lastdustcamx = camx;
    lastdustcamz = camz;
    dustcamvalid = true;

    DustCameraState dustcam;
    dustcam.x = camx;
    dustcam.y = camera->y;
    dustcam.z = camz;
    dustcam.rot = (uint8_t)(camera->rotquick.GetInt() & 0xFF);
    dustcam.dx = dx;
    dustcam.dz = dz;

    dustsystem.Update(dustcam, dt);
    dustsystem.GatherRenderParticles(dustrendercache);
    if (dustrendercache.empty())
    {
        return;
    }

    int16_t cammatrix[4];
    GloomMaths::GetCamRotRaw(dustcam.rot, cammatrix);

    uint32_t* surface = (uint32_t*)(rendersurface->pixels);
    const std::vector<DustZone>& dustzones = dustsystem.GetZones();

    for (const DustRenderParticle& p : dustrendercache)
    {
        float dxw = p.x - (float)dustcam.x;
        float dzw = p.z - (float)dustcam.z;
        float rx = (dxw * (float)cammatrix[0] + dzw * (float)cammatrix[1]) / 32768.0f;
        float rz = (dxw * (float)cammatrix[2] + dzw * (float)cammatrix[3]) / 32768.0f;

        if (rz <= 1.0f)
        {
            continue;
        }

        float nearFade = ZG_DustSmoothStep(dusttuning.nearFadeStart, dusttuning.nearFadeEnd, rz);
        float farFade = 1.0f - ZG_DustSmoothStep(dusttuning.farFadeStart, dusttuning.farFadeEnd, rz);

        // Extra distance shaping so dust gets darker and less present the farther away it is.
        // The far end should feel empty rather than filled with bright floating pixels.
        float distanceDarken = 1.0f - ZG_DustSmoothStep(380.0f, 1180.0f, rz) * 0.78f;
        float distancePresence = 1.0f - ZG_DustSmoothStep(760.0f, 1425.0f, rz) * 0.60f;

        float depthFade = nearFade * farFade * distancePresence;
        if (depthFade <= 0.001f)
        {
            continue;
        }

        float sxf = (float)halfrenderwidth + ((rx * (float)focmult) / rz);
        int sx = (int)sxf;
        if (sx < 0 || sx >= renderwidth)
        {
            continue;
        }

        // Match ZGloom sprite/object Y projection exactly.
        // World Y for objects lives roughly in [-256..0], with more negative values being higher.
        float iyf = (-p.y) - (float)camera->y;
        float syf = (float)halfrenderheight - (iyf * (float)focmult) / rz;
        if (syf < -24.0f || syf > (float)renderheight + 24.0f)
        {
            continue;
        }

        if ((int)rz > zbuff[sx])
        {
            continue;
        }

        int sy = (int)syf;
        float ceilFade = ZG_DustSmoothStep((float)ceilend[sx] - 8.0f, (float)ceilend[sx] + 14.0f, syf);
        float floorFade = 1.0f - ZG_DustSmoothStep((float)floorstart[sx] - 14.0f, (float)floorstart[sx] + 8.0f, syf);
        float verticalFade = ceilFade * floorFade;
        if (verticalFade <= 0.001f)
        {
            continue;
        }

        float edgeFade = 1.0f;
        if (p.zoneIndex < dustzones.size())
        {
            const DustZone& zone = dustzones[p.zoneIndex];
            float edgeDist = std::min(std::min(p.x - (float)zone.minX, (float)zone.maxX - p.x),
                                      std::min(p.z - (float)zone.minZ, (float)zone.maxZ - p.z));
            edgeFade = ZG_DustSmoothStep(0.0f, (float)dusttuning.zoneFadeDistance, edgeDist);
        }
        if (edgeFade <= 0.001f)
        {
            continue;
        }

        int proby = sy;
        if (proby < 0) proby = 0;
        if (proby >= renderheight) proby = renderheight - 1;
        uint32_t probe = surface[sx + proby * renderwidth];
        float luma = (((probe >> 16) & 0xFF) * 0.2126f + ((probe >> 8) & 0xFF) * 0.7152f + (probe & 0xFF) * 0.0722f) / 255.0f;
        float lightFactor = 0.35f + std::pow(ZG_DustSaturate(luma), 1.10f) * 0.65f;

        float finalAlpha = p.alpha * dusttuning.visibilityScale * depthFade * verticalFade * edgeFade * lightFactor;
        if (finalAlpha <= 0.002f)
        {
            continue;
        }

        float projectedRadius = (p.size * (float)focmult) / rz;
        if (projectedRadius > 6.0f) projectedRadius = 6.0f;

        uint32_t dustcol;
        ColourModify(0xF2, 0xE6, 0xD0, dustcol, GetDimPalette((int32_t)rz));

        // Darken distant dust explicitly, not only via alpha, so it visually recedes into the scene.
        uint8_t dr = (uint8_t)((float)((dustcol >> 16) & 0xFF) * distanceDarken);
        uint8_t dg = (uint8_t)((float)((dustcol >> 8) & 0xFF) * distanceDarken);
        uint8_t db = (uint8_t)((float)(dustcol & 0xFF) * distanceDarken);
        dustcol = 0xFF000000 | (dr << 16) | (dg << 8) | db;

        // Keep far particles strictly 1:1 in screen space so they never become
        // visually stretched like 1 px high and 2 px wide.
        if (projectedRadius < 0.85f)
        {
            int cx = (int)(sxf + 0.5f);
            int cy = (int)(syf + 0.5f);
            if (cx >= 0 && cx < renderwidth && cy >= 0 && cy < renderheight)
            {
                if ((int)rz <= zbuff[cx])
                {
                    uint8_t a = (uint8_t)ZG_DustClamp(finalAlpha * 255.0f, 0.0f, 255.0f);
                    ZG_BlendDustPixel(surface[cx + cy * renderwidth], dustcol, a);
                }
            }
            continue;
        }
        else if (projectedRadius < 1.65f)
        {
            int cx = (int)(sxf + 0.5f);
            int cy = (int)(syf + 0.5f);
            int x0 = cx - 1;
            int x1 = cx;
            int y0 = cy - 1;
            int y1 = cy;
            if (x0 < 0) x0 = 0;
            if (y0 < 0) y0 = 0;
            if (x1 >= renderwidth) x1 = renderwidth - 1;
            if (y1 >= renderheight) y1 = renderheight - 1;

            const uint8_t a = (uint8_t)ZG_DustClamp(finalAlpha * 0.92f * 255.0f, 0.0f, 255.0f);
            for (int py = y0; py <= y1; ++py)
            {
                for (int px = x0; px <= x1; ++px)
                {
                    if ((int)rz > zbuff[px])
                    {
                        continue;
                    }
                    ZG_BlendDustPixel(surface[px + py * renderwidth], dustcol, a);
                }
            }
            continue;
        }

        int radius = (int)(projectedRadius + 0.5f);
        if (radius < 2) radius = 2;
        if (radius > 6) radius = 6;

        int x0 = (int)(sxf + 0.5f) - radius;
        int x1 = (int)(sxf + 0.5f) + radius;
        int y0 = (int)(syf + 0.5f) - radius;
        int y1 = (int)(syf + 0.5f) + radius;
        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        if (x1 >= renderwidth) x1 = renderwidth - 1;
        if (y1 >= renderheight) y1 = renderheight - 1;

        float rr = projectedRadius * projectedRadius;
        if (rr < 1.0f) rr = 1.0f;

        for (int py = y0; py <= y1; ++py)
        {
            float oy = ((float)py + 0.5f) - syf;
            for (int px = x0; px <= x1; ++px)
            {
                if ((int)rz > zbuff[px])
                {
                    continue;
                }

                float ox = ((float)px + 0.5f) - sxf;
                float d2 = ox * ox + oy * oy;
                if (d2 > rr)
                {
                    continue;
                }

                float local = 1.0f - (d2 / rr);
                local *= local;
                uint8_t a = (uint8_t)ZG_DustClamp(finalAlpha * local * 255.0f, 0.0f, 255.0f);
                ZG_BlendDustPixel(surface[px + py * renderwidth], dustcol, a);
            }
        }
    }
}


void Renderer::DrawWallBloodSplats(Camera* camera)
{
    if (!camera || !Config::BloodPoolsEnabled() ||
        gloommap->GetWallBloodSplats().empty() ||
        !rendersurface || !rendersurface->pixels)
        return;

    struct SplatPart
    {
        float along;
        float vertical;
        float radiusAlong;
        float radiusVertical;
        float opacity;
    };

    static const int kDirX[16] = {
        256, 237, 181,  98,   0, -98, -181, -237,
       -256,-237,-181, -98,   0,  98,  181,  237
    };
    static const int kDirY[16] = {
          0,  98, 181, 237, 256, 237, 181,  98,
          0, -98,-181,-237,-256,-237,-181, -98
    };
    const auto Hash32 = [](uint32_t value) -> uint32_t
    {
        value ^= value >> 16;
        value *= 0x7FEB352Du;
        value ^= value >> 15;
        value *= 0x846CA68Bu;
        value ^= value >> 16;
        return value;
    };

    uint32_t* surface = static_cast<uint32_t*>(rendersurface->pixels);
    Quick cammatrix[4];
    GloomMaths::GetCamRot(camera->rotquick.GetInt() & 0xFF, cammatrix);
    const auto& zones = gloommap->GetZones();

    for (const auto& splat : gloommap->GetWallBloodSplats())
    {
        if (splat.zone >= zones.size() || splat.zone >= walls.size())
            continue;
        const Zone& zone = zones[splat.zone];
        if (!walls[splat.zone].valid || zone.ztype != Zone::ZT_WALL)
            continue;

        Quick wx1, wz1, wx2, wz2, tx;
        wx1.SetInt(zone.x1); wz1.SetInt(zone.z1);
        wx2.SetInt(zone.x2); wz2.SetInt(zone.z2);
        wx1 = wx1 - camera->x; wz1 = wz1 - camera->z;
        wx2 = wx2 - camera->x; wz2 = wz2 - camera->z;

        tx = wx1;
        Quick lxq = (wx1 * cammatrix[0]) + (wz1 * cammatrix[1]);
        Quick lzq = (tx * cammatrix[2]) + (wz1 * cammatrix[3]);
        tx = wx2;
        Quick rxq = (wx2 * cammatrix[0]) + (wz2 * cammatrix[1]);
        Quick rzq = (tx * cammatrix[2]) + (wz2 * cammatrix[3]);

        if (zone.open)
        {
            Quick bias; bias.SetInt(12);
            lzq = lzq + bias;
            rzq = rzq + bias;
        }

        const float lx = static_cast<float>(lxq.GetInt());
        const float lz = static_cast<float>(lzq.GetInt());
        const float rx = static_cast<float>(rxq.GetInt());
        const float rz = static_cast<float>(rzq.GetInt());
        const float dx = rx - lx;
        const float dz = rz - lz;
        const float worldDx = static_cast<float>(zone.x2 - zone.x1);
        const float worldDz = static_cast<float>(zone.z2 - zone.z1);
        const float wallLength = std::sqrt(worldDx * worldDx + worldDz * worldDz);
        if (wallLength < 1.0f)
            continue;

        const float centreT = static_cast<float>(splat.along) / 65535.0f;
        float grow = static_cast<float>(splat.age) / 14.0f;
        if (grow < 0.0f) grow = 0.0f;
        if (grow > 1.0f) grow = 1.0f;
        grow = grow * grow * (3.0f - 2.0f * grow);
        grow = 0.42f + 0.58f * grow;
        const float radius = static_cast<float>(splat.targetRadius) * grow;

        // Build a deterministic impact pattern in wall-local coordinates.
        // Several overlapping core stains make an irregular blot; small
        // satellite droplets form a loose directional fan.  A gravity streak
        // is optional rather than being stamped onto every mark.
        SplatPart parts[18];
        int partCount = 0;
        const uint32_t baseHash = Hash32(splat.seed ^ 0xA341316Cu);
        const int fanDir = static_cast<int>((baseHash >> 4) & 15u);
        const float fanX = static_cast<float>(kDirX[fanDir]) / 256.0f;
        const float fanY = static_cast<float>(kDirY[fanDir]) / 256.0f;

        const int coreCount = 3 + static_cast<int>(baseHash % 3u);
        for (int i = 0; i < coreCount && partCount < 18; ++i)
        {
            const uint32_t h = Hash32(baseHash + 0x9E3779B9u * static_cast<uint32_t>(i + 1));
            const int dir = static_cast<int>((h >> 3) & 15u);
            const float offset = radius * (0.04f + static_cast<float>((h >> 8) & 255u) / 255.0f * 0.30f);
            const float scale = 0.42f + static_cast<float>((h >> 16) & 255u) / 255.0f * 0.34f;
            const float aspect = 0.68f + static_cast<float>((h >> 24) & 127u) / 127.0f * 0.48f;
            SplatPart& p = parts[partCount++];
            p.along = static_cast<float>(kDirX[dir]) / 256.0f * offset;
            p.vertical = static_cast<float>(kDirY[dir]) / 256.0f * offset;
            p.radiusAlong = std::max(2.0f, radius * scale);
            p.radiusVertical = std::max(2.0f, radius * scale * aspect);
            p.opacity = 0.72f + static_cast<float>((h >> 20) & 15u) / 15.0f * 0.24f;
        }

        const int satelliteCount = 5 + static_cast<int>((baseHash >> 12) % 7u);
        for (int i = 0; i < satelliteCount && partCount < 18; ++i)
        {
            const uint32_t h = Hash32(baseHash ^ (0x85EBCA6Bu * static_cast<uint32_t>(i + 3)));
            const int spread = static_cast<int>((h >> 5) % 7u) - 3;
            const int dir = (fanDir + spread + 16) & 15;
            const float distance = radius * (0.72f + static_cast<float>((h >> 10) & 255u) / 255.0f * 1.25f);
            const float dropScale = 0.09f + static_cast<float>((h >> 18) & 255u) / 255.0f * 0.19f;
            const float sideJitter = (static_cast<int>((h >> 27) & 15u) - 7) * radius * 0.018f;
            SplatPart& p = parts[partCount++];
            p.along = static_cast<float>(kDirX[dir]) / 256.0f * distance - fanY * sideJitter;
            p.vertical = static_cast<float>(kDirY[dir]) / 256.0f * distance + fanX * sideJitter;
            p.radiusAlong = std::max(1.35f, radius * dropScale * (0.85f + ((h >> 2) & 3u) * 0.12f));
            p.radiusVertical = std::max(1.35f, radius * dropScale * (0.66f + ((h >> 14) & 7u) * 0.07f));
            p.opacity = 0.48f + static_cast<float>((h >> 22) & 31u) / 31.0f * 0.34f;
        }

        if (((baseHash >> 29) & 3u) == 0u && partCount < 18)
        {
            const uint32_t h = Hash32(baseHash ^ 0xC2B2AE35u);
            SplatPart& p = parts[partCount++];
            p.along = (static_cast<int>((h >> 4) & 31u) - 15) * radius * 0.015f;
            p.vertical = radius * (0.62f + static_cast<float>((h >> 12) & 63u) / 63.0f * 0.38f);
            p.radiusAlong = std::max(1.5f, radius * 0.10f);
            p.radiusVertical = std::max(3.0f, radius * (0.28f + static_cast<float>((h >> 20) & 31u) / 31.0f * 0.20f));
            p.opacity = 0.52f;
        }

        float maxAlongReach = radius;
        float maxVerticalReach = radius;
        for (int i = 0; i < partCount; ++i)
        {
            maxAlongReach = std::max(maxAlongReach,
                std::fabs(parts[i].along) + parts[i].radiusAlong);
            maxVerticalReach = std::max(maxVerticalReach,
                std::fabs(parts[i].vertical) + parts[i].radiusVertical);
        }

        const float dt = std::min(0.48f, (maxAlongReach + 2.0f) / wallLength);
        int x0 = walls[splat.zone].wl_lsx;
        int x1 = walls[splat.zone].wl_rsx;
        const auto projectT = [&](float t, int& sx) -> bool
        {
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;
            const float px = lx + dx * t;
            const float pz = lz + dz * t;
            if (pz <= 4.0f) return false;
            sx = halfrenderwidth + static_cast<int>(px * focmult / pz);
            return true;
        };
        int projected0 = x0, projected1 = x1;
        if (projectT(centreT - dt, projected0) && projectT(centreT + dt, projected1))
        {
            x0 = std::min(projected0, projected1) - 2;
            x1 = std::max(projected0, projected1) + 2;
        }
        if (x0 < walls[splat.zone].wl_lsx) x0 = walls[splat.zone].wl_lsx;
        if (x1 > walls[splat.zone].wl_rsx) x1 = walls[splat.zone].wl_rsx;
        if (x0 < 0) x0 = 0;
        if (x1 >= renderwidth) x1 = renderwidth - 1;
        if (x0 > x1)
            continue;

        for (int sx = x0; sx <= x1; ++sx)
        {
            const float grad = static_cast<float>(sx - halfrenderwidth) /
                static_cast<float>(focmult);
            const float divisor = dx - grad * dz;
            if (std::fabs(divisor) < 0.0001f)
                continue;
            const float m = (grad * lz - lx) / divisor;
            if (m < 0.0f || m > 1.0f)
                continue;

            const float depthf = lz + m * dz;
            if (depthf <= 5.0f || depthf >= 8192.0f)
                continue;
            const int depth = static_cast<int>(depthf + 0.5f);
            const int depthTolerance = std::max(3, depth / 384);
            if (std::abs(zbuff[sx] - depth) > depthTolerance)
                continue;

            const float alongWorld = (m - centreT) * wallLength;
            if (std::fabs(alongWorld) > maxAlongReach)
                continue;

            Quick texpos;
            float clampedM = m;
            if (clampedM < 0.0f) clampedM = 0.0f;
            if (clampedM > 0.9999847f) clampedM = 0.9999847f;
            texpos.SetVal(static_cast<int32_t>(clampedM * 65536.0f));
            int baseTexture = 0;
            Column* texcol = GetTexColumn(static_cast<int>(splat.zone), texpos, baseTexture);
            if (!texcol || texcol->flag)
                continue;

            int cy = halfrenderheight +
                ((camera->y + static_cast<int>(splat.y)) * focmult) / depth;
            int screenRadius = static_cast<int>((maxVerticalReach * focmult) / depth + 2.0f);
            if (screenRadius < 2) screenRadius = 2;
            if (screenRadius > 96) screenRadius = 96;
            int sy0 = cy - screenRadius;
            int sy1 = cy + screenRadius;
            if (sy0 < ceilend[sx]) sy0 = ceilend[sx];
            if (sy1 >= floorstart[sx]) sy1 = floorstart[sx] - 1;
            if (sy0 < 0) sy0 = 0;
            if (sy1 >= renderheight) sy1 = renderheight - 1;
            if (sy0 > sy1)
                continue;

            const int pal = GetDimPalette(depth);
            uint32_t bloodColour = 0;
            const uint8_t r = static_cast<uint8_t>(((splat.color & 0xF00u) >> 8) * 17u);
            const uint8_t g = static_cast<uint8_t>(((splat.color & 0x0F0u) >> 4) * 17u);
            const uint8_t b = static_cast<uint8_t>((splat.color & 0x00Fu) * 17u);
            if (fadetimer)
                ColourModifyFade(r, g, b, bloodColour, pal);
            else
                ColourModify(r, g, b, bloodColour, pal);

            float distance = 1.0f - (depthf - 128.0f) / (6144.0f - 128.0f);
            if (distance < 0.0f) distance = 0.0f;
            if (distance > 1.0f) distance = 1.0f;
            const float baseAlpha = 122.0f + 82.0f * distance;

            for (int sy = sy0; sy <= sy1; ++sy)
            {
                const float wallY =
                    (static_cast<float>(sy - halfrenderheight) * depthf /
                        static_cast<float>(focmult)) - camera->y;
                const float vertical = wallY - splat.y;

                float strength = 0.0f;
                for (int i = 0; i < partCount; ++i)
                {
                    const SplatPart& p = parts[i];
                    const float px = alongWorld - p.along;
                    const float py = vertical - p.vertical;
                    const float metric =
                        (px * px) / (p.radiusAlong * p.radiusAlong) +
                        (py * py) / (p.radiusVertical * p.radiusVertical);
                    if (metric <= 1.0f)
                    {
                        const float local = p.opacity *
                            (0.40f + 0.60f * (1.0f - metric));
                        if (local > strength) strength = local;
                    }
                }
                if (strength <= 0.0f)
                    continue;

                int alpha = static_cast<int>(baseAlpha * strength + 0.5f);
                if (alpha < 1) continue;
                if (alpha > 225) alpha = 225;
                uint32_t& dst = surface[sx + sy * renderwidth];
                dst = ZG_AlphaBlendPixel(dst, bloodColour,
                    static_cast<uint8_t>(alpha));
            }
        }
    }
}

void Renderer::DrawWallReflections()
{
    if (Config::GetReflections() < 2 || !rendersurface || !rendersurface->pixels)
        return;

    uint32_t* surface = (uint32_t*)rendersurface->pixels;

    // Mirror only the lower part of each already rendered wall column.  This
    // keeps the cost linear in screen width and avoids a second ray-cast or a
    // full-screen temporary buffer.  Alpha fades are continuous, not dithered.
    for (int x = 0; x < renderwidth; ++x)
    {
        const int depth = zbuff[x];
        if (depth <= 5 || depth >= 6144)
            continue;

        int wallTop = ceilend[x];
        int wallFoot = floorstart[x];
        if (wallTop < 0) wallTop = 0;
        if (wallFoot <= wallTop || wallFoot < 0 || wallFoot >= renderheight)
            continue;

        const int wallHeight = wallFoot - wallTop;
        if (wallHeight < 4)
            continue;

        // Use a much longer floor image than the first Update 4 pass.  The
        // former wallHeight/8 strip was often only a handful of pixels high
        // and therefore disappeared on textured floors.  About 45% of the
        // visible wall height matches the useful length of enemy reflections
        // while the hard cap keeps the pass inexpensive on close walls.
        int reflectionHeight = (wallHeight * 45) / 100;
        if (reflectionHeight < 4) reflectionHeight = 4;
        if (reflectionHeight > 96) reflectionHeight = 96;
        if (wallFoot + reflectionHeight > renderheight)
            reflectionHeight = renderheight - wallFoot;
        if (reflectionHeight <= 0)
            continue;

        float distance = 1.0f - (float)(depth - 96) / (float)(6144 - 96);
        if (distance < 0.0f) distance = 0.0f;
        if (distance > 1.0f) distance = 1.0f;
        // Deliberately visible in ALL mode.  Distance still reduces the
        // effect, but nearby wall feet now have enough contrast to survive
        // dark and noisy floor textures.
        const float baseAlpha = 64.0f + 76.0f * distance;

        for (int row = 0; row < reflectionHeight; ++row)
        {
            const int sourceY = wallFoot - 1 - row;
            const int targetY = wallFoot + row;
            if (sourceY < wallTop || targetY >= renderheight)
                break;

            float fade = 1.0f - ((float)row + 0.5f) / (float)reflectionHeight;
            if (fade <= 0.0f)
                continue;
            // Softer than a squared fade so the reflection remains visible
            // farther down the floor, but still vanishes continuously.
            fade *= 0.55f + 0.45f * fade;

            int alphaValue = (int)(baseAlpha * fade + 0.5f);
            if (alphaValue < 1)
                continue;
            if (alphaValue > 255)
                alphaValue = 255;

            const uint32_t source = surface[x + sourceY * renderwidth];
            uint32_t& target = surface[x + targetY * renderwidth];
            target = ZG_BlendReflectionPixel(target, source,
                (uint8_t)alphaValue, 190);
        }
    }
}

void Renderer::DrawObjectFloorEffects(Camera* camera)
{
    if (!camera || !rendersurface || !rendersurface->pixels)
        return;

    const int reflectionMode = Config::GetReflections();
    const bool drawPools = Config::BloodPoolsEnabled() &&
        !gloommap->GetBloodPools().empty();
    if (reflectionMode < 1 && !ZGloomPC::gBlobShadows && !drawPools)
        return;

    uint32_t* surface = (uint32_t*)rendersurface->pixels;

    // Transparent texture strips (windows, fences and half-open doors) do not
    // contribute to the opaque z-buffer.  Keep a tiny per-column cover buffer
    // so reflections behind them cannot leak onto the foreground floor.
    std::fill(reflectioncover.begin(), reflectioncover.end(), 30000);
    for (const auto& strip : strips)
    {
        if (!strip.isstrip)
            continue;
        if (strip.rotx < 0 || strip.rotx >= renderwidth || strip.rotz <= 0)
            continue;
        if (strip.rotz < reflectioncover[strip.rotx])
            reflectioncover[strip.rotx] = strip.rotz;
    }

    // Pools are the lowest object-related floor layer.  Drawing them here,
    // after transparent-strip coverage is known but before shadows, glows and
    // sprite reflections, keeps every later effect naturally in front.
    if (drawPools)
        DrawBloodPools(camera);

    if (reflectionMode < 1 && !ZGloomPC::gBlobShadows)
        return;

    // All floor effects are rendered in a separate pass before any sprite.
    // Consequently a near reflection can never paint over a farther enemy.
    for (auto o : strips)
    {
        if (o.isstrip || o.t <= 1 || o.t == 3 || !o.data.ms.render)
            continue;

        const int iz = o.rotz;
        if (iz <= 5)
            continue;

        std::vector<Shape>* shapes = o.data.ms.shape;
        if (!shapes || shapes->empty())
            continue;

        int frame = 0;
        if (o.data.ms.render == 8)
        {
            uint16_t ang = GloomMaths::CalcAngle(
                camera->x.GetInt(), camera->z.GetInt(),
                o.x.GetInt(), o.z.GetInt());
            ang += 16;
            ang -= o.data.ms.rotquick.GetInt();
            ang >>= 5;
            ang &= 7;
            frame = ang | (((o.data.ms.frame >> 16) & 7) << 3);
        }
        else
        {
            frame = o.data.ms.frame >> 16;
        }

        if (frame < 0) frame = 0;
        if ((size_t)frame >= shapes->size())
            frame = (int)shapes->size() - 1;

        const Shape& shape = (*shapes)[frame];
        const int shapeWidth = (int)shape.w;
        const int shapeHeight = (int)shape.h;
        if (shapeWidth <= 0 || shapeHeight <= 0 || shape.data.empty())
            continue;

        const int scale = o.data.ms.scale;
        int screenWidth = ((shapeWidth * scale / 0x100) * focmult) / iz;
        int screenHeight = ((shapeHeight * scale / 0x100) * focmult) / iz;
        if (screenWidth <= 0 || screenHeight <= 0)
            continue;

        int screenCenterX = (o.rotx * focmult) / iz + halfrenderwidth;
        int objectY = -o.y.GetInt() - camera->y;
        objectY -= shapeHeight - shape.yh - 1;
        objectY = (objectY * focmult) / iz;
        const int spriteTop = halfrenderheight - objectY - screenHeight;
        const int spriteBottom = spriteTop + screenHeight;
        int groundY = halfrenderheight + (camera->y * focmult) / iz;
        if (groundY < 0) groundY = 0;
        if (groundY >= renderheight) groundY = renderheight - 1;

        const bool enemy = ZG_IsEnemyLogicType(o.t);
        const bool centreCovered =
            screenCenterX >= 0 && screenCenterX < renderwidth &&
            iz > reflectioncover[screenCenterX];

        // Existing blob shadows remain independent from the reflection mode.
        if (enemy && ZGloomPC::gBlobShadows && !centreCovered)
        {
            const int kShadowFarZ = 4096;
            const int kShadowMinSize = 12;
            const bool skipShadow =
                (iz > kShadowFarZ) &&
                (screenWidth < kShadowMinSize && screenHeight < kShadowMinSize);
            if (!skipShadow)
            {
                int rx = screenWidth / 3; if (rx < 1) rx = 1;
                int ry = screenWidth / 8; if (ry < 1) ry = 1;
                ZG_DrawBlobShadow(surface, renderwidth, renderheight,
                    zbuff.data(), screenCenterX, spriteBottom, rx, ry, iz);
            }
        }

        int weaponIndex = -1;
        if (objectgraphics)
        {
            for (int wi = 0; wi < 5; ++wi)
            {
                if (o.data.ms.shape == &objectgraphics->BulletShapes[wi] ||
                    o.data.ms.shape == &objectgraphics->SparkShapes[wi])
                {
                    weaponIndex = wi;
                    break;
                }
            }
            if (weaponIndex < 0 &&
                o.t >= ObjectGraphics::OLT_WEAPON1 &&
                o.t <= ObjectGraphics::OLT_WEAPON5)
            {
                weaponIndex = o.t - ObjectGraphics::OLT_WEAPON1;
            }
        }

        // Projectiles and weapon upgrades use the existing coloured floor glow,
        // now controlled by REFLECTIONS rather than by MUZZLE FLASH.
        if (reflectionMode >= 1 && weaponIndex >= 0 && !centreCovered &&
            screenCenterX >= 0 && screenCenterX < renderwidth)
        {
            float tr = 1.0f, tg = 1.0f, tb = 1.0f;
            Hud_GetWeaponTint(weaponIndex, tr, tg, tb);
            int glowWidth = screenWidth;

            if (o.t >= ObjectGraphics::OLT_WEAPON1 &&
                o.t <= ObjectGraphics::OLT_WEAPON5)
            {
                // Restore the original pickup pulse from the pre-reflection
                // renderer: use the actual visible floor clip under the item,
                // not the mathematical horizon and not an absolute gap.  The
                // glow therefore grows for a few frames at the bottom of the
                // bob and contracts again as the weapon rises.
                int floorClip = floorstart[screenCenterX];
                if (floorClip < 0) floorClip = 0;
                if (floorClip >= renderheight) floorClip = renderheight - 1;
                int gap = floorClip - spriteBottom;
                if (gap < 0) gap = 0;
                int maxGap = screenHeight / 2;
                if (maxGap < 8) maxGap = 8;
                if (maxGap > 32) maxGap = 32;
                float touch = 1.0f - (float)gap / (float)maxGap;
                if (touch < 0.0f) touch = 0.0f;
                if (touch > 1.0f) touch = 1.0f;
                glowWidth = (int)((float)screenWidth * (1.0f + 0.5f * touch));
                if (glowWidth < 1) glowWidth = 1;
            }

            ZG_DrawProjectileGlow(surface, renderwidth, renderheight,
                zbuff.data(), floorstart.data(), screenCenterX, glowWidth,
                iz, tr, tg, tb, camera->y, focmult, halfrenderheight);
        }

        if (reflectionMode < 1 || iz >= 5120 || (o.data.ms.blood & 0x8000))
            continue;

        int gap = groundY - spriteBottom;
        if (gap < 0) gap = -gap;
        int enemyGapLimit = screenHeight / 4 + 3;
        if (enemyGapLimit < 5) enemyGapLimit = 5;
        if (enemyGapLimit > 18) enemyGapLimit = 18;
        int pickupGapLimit = screenHeight / 2 + 6;
        if (pickupGapLimit < 10) pickupGapLimit = 10;
        if (pickupGapLimit > 36) pickupGapLimit = 36;

        const bool groundedEnemy = enemy && gap <= enemyGapLimit;
        bool pickup = ZG_IsPickupLogicType(o.t);
        if (o.t >= ObjectGraphics::OLT_WEAPON1 &&
            o.t <= ObjectGraphics::OLT_WEAPON5 && gap <= pickupGapLimit)
        {
            pickup = true;
        }
        const bool groundedPickup = pickup && gap <= pickupGapLimit;
        if (!groundedEnemy && !groundedPickup)
            continue;

        int reflectionHeight = groundedEnemy
            ? (screenHeight * 55) / 100
            : (screenHeight * 42) / 100;
        if (reflectionHeight < 2) reflectionHeight = 2;
        const int maxReflectionHeight = groundedEnemy ? 72 : 40;
        if (reflectionHeight > maxReflectionHeight)
            reflectionHeight = maxReflectionHeight;
        if (groundY + reflectionHeight > renderheight)
            reflectionHeight = renderheight - groundY;
        if (reflectionHeight <= 0)
            continue;

        float distance = 1.0f - (float)(iz - 128) / (float)(5120 - 128);
        if (distance < 0.0f) distance = 0.0f;
        if (distance > 1.0f) distance = 1.0f;
        distance = 0.35f + 0.65f * distance;

        const int gapLimit = groundedEnemy ? enemyGapLimit : pickupGapLimit;
        float contact = 1.0f - (float)gap / (float)(gapLimit + 1);
        if (contact < 0.0f) contact = 0.0f;
        if (contact > 1.0f) contact = 1.0f;

        const float baseAlpha = (groundedEnemy ? 88.0f : 68.0f) * distance * contact;
        const uint8_t brightness = groundedEnemy ? 165 : 180;
        const int left = screenCenterX - screenWidth / 2;
        const int palette = GetDimPalette(iz);

        for (int sx = left; sx < left + screenWidth; ++sx)
        {
            if (sx < 0 || sx >= renderwidth)
                continue;
            if (iz > zbuff[sx] || iz > reflectioncover[sx])
                continue;

            int sourceX = ((sx - left) * shapeWidth) / screenWidth;
            if (sourceX < 0) sourceX = 0;
            if (sourceX >= shapeWidth) sourceX = shapeWidth - 1;
            const int floorClip = floorstart[sx];

            for (int row = 0; row < reflectionHeight; ++row)
            {
                const int sy = groundY + row;
                if (sy < floorClip || sy < 0)
                    continue;
                if (sy >= renderheight)
                    break;

                int sourceY = shapeHeight - 1 -
                    (row * shapeHeight) / reflectionHeight;
                if (sourceY < 0) sourceY = 0;
                if (sourceY >= shapeHeight) sourceY = shapeHeight - 1;

                const uint32_t sourceColour =
                    shape.data[sourceY + sourceX * shapeHeight];
                if (sourceColour == 1)
                    continue;

                uint32_t dimColour = 0;
                if (fadetimer)
                {
                    ColourModifyFade(
                        (sourceColour >> 16) & 0xFF,
                        (sourceColour >> 8) & 0xFF,
                        sourceColour & 0xFF,
                        dimColour, palette);
                }
                else
                {
                    ColourModify(
                        (sourceColour >> 16) & 0xFF,
                        (sourceColour >> 8) & 0xFF,
                        sourceColour & 0xFF,
                        dimColour, palette);
                }

                float fade = 1.0f -
                    ((float)row + 0.5f) / (float)reflectionHeight;
                if (fade <= 0.0f)
                    continue;
                fade *= fade;
                int alphaValue = (int)(baseAlpha * fade + 0.5f);
                if (alphaValue < 1)
                    continue;
                if (alphaValue > 255)
                    alphaValue = 255;

                uint32_t& target = surface[sx + sy * renderwidth];
                target = ZG_BlendReflectionPixel(target, dimColour,
                    (uint8_t)alphaValue, brightness);
            }
        }
    }
}

void Renderer::DrawObjects(Camera* camera)
{
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

	DrawObjectFloorEffects(camera);

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

					// Some maps contain objects whose graphics set exists but has no
					// decoded frames.  The old clamp turned size()==0 into frame -1 and
					// indexed 20 bytes before address zero on 32-bit ARM.
					if (!s || s->empty())
						continue;

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

					if (frametouse < 0)
						frametouse = 0;
					if ((size_t)frametouse >= s->size())
						frametouse = static_cast<int>(s->size()) - 1;

					const Shape& drawshape = (*s)[frametouse];
					auto scale = o.data.ms.scale;
					auto shapewidth = drawshape.w;
					auto shapeheight = drawshape.h;

					const size_t requiredPixels = static_cast<size_t>(shapewidth) *
						static_cast<size_t>(shapeheight);
					if (shapewidth == 0 || shapeheight == 0 ||
						drawshape.data.size() < requiredPixels)
						continue;

					ix *= focmult;
					ix /= iz;

					// Add handle! otherwise bullets fill screen
					iy -= drawshape.h - drawshape.yh - 1;
					iy *= focmult;
					iy /= iz;

					
int h = ((shapeheight * scale / 0x100) * focmult) / iz;
int w = ((shapewidth * scale / 0x100) * focmult) / iz;

if ((w > 0) && (h > 0))
{
    // Floor shadows, glows and reflections are rendered in the dedicated
    // pre-pass above, so every sprite remains in front of those effects.

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
										const int sourceX = tx.GetInt();
										const int sourceY = ty.GetInt();
										if (sourceX < 0 || sourceX >= shapewidth ||
											sourceY < 0 || sourceY >= shapeheight)
										{
											ty = ty + dy;
											continue;
										}

										auto col = drawshape.data[static_cast<size_t>(sourceY) +
											static_cast<size_t>(sourceX) * shapeheight];

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

	// Persistent MASSACRE splats are composited onto the already rendered wall
	// before reflections, so lower marks are naturally mirrored by ALL mode.
	DrawWallBloodSplats(camera);

	// ALL adds conservative mirrored wall columns on top of the floor.
	DrawWallReflections();

	Uint32 now = SDL_GetTicks();
	float dustdt = 0.0f;
	if (dustlastticks == 0)
	{
		dustlastticks = now;
	}
	else
	{
		dustdt = (float)(now - dustlastticks) / 1000.0f;
		dustlastticks = now;
	}
	DrawDust(camera, dustdt);
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

	ApplyTeleportPixelate();

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

