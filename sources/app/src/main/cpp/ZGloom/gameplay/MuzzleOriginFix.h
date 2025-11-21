#pragma once
#ifndef GLOOM_ROT_UNITS
#define GLOOM_ROT_UNITS 360
#endif
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
struct Vec3i { int x, y, z; };
static constexpr int MUZZLE_FORWARD = 28;
static constexpr int MUZZLE_RIGHT   = 6;
static constexpr int MUZZLE_HEIGHT  = 24;
inline float RotToRad(int rot) { return (rot * (2.0f * (float)M_PI / (float)GLOOM_ROT_UNITS)); }
inline Vec3i ComputeMuzzleOrigin(int px, int py, int pz, int rot)
{
    const float a = RotToRad(rot);
    const float fx = cosf(a), fz = sinf(a);
    const float rx = -fz, rz = fx;
    const float ox = (float)px + fx * (float)MUZZLE_FORWARD + rx * (float)MUZZLE_RIGHT;
    const float oy = (float)py + (float)MUZZLE_HEIGHT;
    const float oz = (float)pz + fz * (float)MUZZLE_FORWARD + rz * (float)MUZZLE_RIGHT;
    return { (int)(ox + (ox>=0?0.5f:-0.5f)), (int)(oy + (oy>=0?0.5f:-0.5f)), (int)(oz + (oz>=0?0.5f:-0.5f)) };
}
