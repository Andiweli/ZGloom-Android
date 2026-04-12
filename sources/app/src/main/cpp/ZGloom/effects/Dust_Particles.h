#pragma once

#include <vector>
#include <cstdint>
#include "gloommap.h"

struct DustCameraState
{
    int32_t x = 0;
    int32_t y = 0;
    int32_t z = 0;
    uint8_t rot = 0;
    int32_t dx = 0;
    int32_t dz = 0;
};

struct DustTuning
{
    bool enabled = true;

    // Overall particle budget and density scaling.
    int maxParticles = 1422;
    float densityScale = 1.0f;
    float visibilityScale = 1.0f;
    float speedScale = 1.0f;

    // Local cell / segment selection.
    int cellSize = 256;
    int cellPadding = 16;
    int minWallClearance = 22;
    int zoneFadeDistance = 18;

    // Lifetime / motion in WORLD SPACE.
    float lifeMin = 18.0f;
    float lifeMax = 42.0f;
    float driftSpeedMin = 1.8f;
    float driftSpeedMax = 5.8f;
    float verticalDriftMin = -0.24f;
    float verticalDriftMax = 0.28f;
    float swayRadiusMin = 2.1f;
    float swayRadiusMax = 6.8f;
    float verticalOscMin = 0.6f;
    float verticalOscMax = 2.8f;
    float impulseDamping = 2.7f;

    // Rendering defaults.
    float particleSizeMin = 0.75f;
    float particleSizeMax = 2.2f;
    float alphaMin = 0.58f;
    float alphaMax = 0.78f;
    float nearFadeStart = 48.0f;
    float nearFadeEnd = 120.0f;
    float farFadeStart = 1050.0f;
    float farFadeEnd = 1500.0f;

    // Optional subtle local air disturbance from player movement.
    bool enablePlayerInfluence = true;
    float playerInfluenceRadius = 148.0f;
    float playerInfluenceStrength = 26.0f;
    float playerInfluenceMinSpeed = 10.0f;
};

struct DustZone
{
    int32_t minX = 0;
    int32_t maxX = 0;
    int32_t minY = 0;
    int32_t maxY = 0;
    int32_t minZ = 0;
    int32_t maxZ = 0;
    float densityScale = 1.0f;
};

struct DustParticle
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    float driftX = 0.0f;
    float driftY = 0.0f;
    float driftZ = 0.0f;

    float impulseX = 0.0f;
    float impulseY = 0.0f;
    float impulseZ = 0.0f;

    float swayRadius = 0.0f;
    float verticalOscillation = 0.0f;
    float phaseA = 0.0f;
    float phaseB = 0.0f;
    float phaseC = 0.0f;

    float size = 1.0f;
    float alpha = 0.08f;

    float life = 0.0f;
    float maxLife = 0.0f;

    uint16_t zoneIndex = 0;
};

struct DustRenderParticle
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float size = 1.0f;
    float alpha = 0.0f;
    uint16_t zoneIndex = 0;
};

class DustParticleSystem
{
public:
    void Reset();
    void SetTuning(const DustTuning& tuning);
    const DustTuning& GetTuning() const { return m_tuning; }

    void BuildFromMap(GloomMap* map);
    void Update(const DustCameraState& camera, float dt);
    void GatherRenderParticles(std::vector<DustRenderParticle>& out) const;

    const std::vector<DustZone>& GetZones() const { return m_zones; }
    int GetZoneCount() const { return (int)m_zones.size(); }
    int GetParticleCount() const { return (int)m_particles.size(); }

private:
    GloomMap* m_map = nullptr;
    DustTuning m_tuning;
    std::vector<DustZone> m_zones;
    std::vector<DustParticle> m_particles;
    float m_elapsedTime = 0.0f;
    uint32_t m_seed = 0x7A3C18D2u;

private:
    uint32_t NextRand();
    float Rand01();
    float RandRange(float mn, float mx);
    float ComputeClearanceForPoint(int32_t x, int32_t z) const;
    bool BuildZoneForCell(int gx, int gz, DustZone& outZone) const;
    int ComputeParticleTarget(const DustZone& zone) const;
    void SpawnParticle(DustParticle& p, uint16_t zoneIndex);
    void WrapParticleToZone(DustParticle& p, const DustZone& zone);
};
