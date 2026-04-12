#include "Dust_Particles.h"
#include <algorithm>
#include <cmath>

namespace
{
    static inline float DustClamp(float v, float mn, float mx)
    {
        if (v < mn) return mn;
        if (v > mx) return mx;
        return v;
    }

    static inline float DustSaturate(float v)
    {
        return DustClamp(v, 0.0f, 1.0f);
    }

    static inline float DustSmoothStep(float a, float b, float x)
    {
        const float span = (b - a);
        if (span <= 0.0001f)
        {
            return (x >= b) ? 1.0f : 0.0f;
        }
        float t = DustSaturate((x - a) / span);
        return t * t * (3.0f - 2.0f * t);
    }

    static int32_t DustFindSegDist(int32_t x, int32_t z, const Zone& zone)
    {
        int32_t tx, tz;

        tx = zone.x2 - x;
        tx *= zone.na;
        tz = zone.z2 - z;
        tz *= zone.nb;

        tx += tz;
        tx *= 2;

        if (((tx >> 16) < zone.ln) && (tx >= 0))
        {
            tx = zone.x2 - x;
            tx *= zone.a;
            tz = zone.z2 - z;
            tz *= zone.b;

            tx += tz;
            tx *= 2;

            if (tx < 0) tx = -tx;
            return tx >> 16;
        }

        return 0x3FFF;
    }
}

void DustParticleSystem::Reset()
{
    m_map = nullptr;
    m_zones.clear();
    m_particles.clear();
    m_elapsedTime = 0.0f;
    m_seed = 0x7A3C18D2u;
}

void DustParticleSystem::SetTuning(const DustTuning& tuning)
{
    m_tuning = tuning;
}

uint32_t DustParticleSystem::NextRand()
{
    m_seed = m_seed * 1664525u + 1013904223u;
    return m_seed;
}

float DustParticleSystem::Rand01()
{
    return float(NextRand() & 0x00FFFFFFu) / float(0x01000000u);
}

float DustParticleSystem::RandRange(float mn, float mx)
{
    return mn + (mx - mn) * Rand01();
}

float DustParticleSystem::ComputeClearanceForPoint(int32_t x, int32_t z) const
{
    if (!m_map)
    {
        return 0.0f;
    }

    int32_t best = 0x3FFF;

    for (int32_t dx = -1; dx <= 1; ++dx)
    {
        for (int32_t dz = -1; dz <= 1; ++dz)
        {
            int32_t gx = x / 256 + dx;
            int32_t gz = z / 256 + dz;

            if (gx < 0 || gx >= 32 || gz < 0 || gz >= 32)
            {
                continue;
            }

            std::vector<uint32_t>& collzones = m_map->GetCollisions(0, gx, gz);
            for (uint32_t idx : collzones)
            {
                if (idx >= m_map->GetZones().size())
                {
                    continue;
                }

                const Zone& zone = m_map->GetZones()[idx];
                if (zone.ztype != Zone::ZT_WALL)
                {
                    continue;
                }
                if (zone.x1 == -1 && zone.x2 == -1 && zone.z1 == -1 && zone.z2 == -1)
                {
                    continue;
                }

                int32_t dist = DustFindSegDist(x, z, zone);
                if (dist < best)
                {
                    best = dist;
                }
            }
        }
    }

    return (best == 0x3FFF) ? 9999.0f : float(best);
}

bool DustParticleSystem::BuildZoneForCell(int gx, int gz, DustZone& outZone) const
{
    if (!m_map)
    {
        return false;
    }

    const int32_t cellMinX = gx * m_tuning.cellSize;
    const int32_t cellMinZ = gz * m_tuning.cellSize;
    const int32_t cellMaxX = cellMinX + m_tuning.cellSize;
    const int32_t cellMaxZ = cellMinZ + m_tuning.cellSize;

    const int32_t cx = (cellMinX + cellMaxX) / 2;
    const int32_t cz = (cellMinZ + cellMaxZ) / 2;

    // Sample a 3x3 pattern inside the cell. This is intentionally more tolerant
    // than the first version because many real Gloom corridor cells are narrow and
    // the strict clearance test can easily reject every single valid corridor.
    const int32_t probe = 64;
    const int32_t samplePoints[9][2] = {
        { cx, cz },
        { cx - probe, cz },
        { cx + probe, cz },
        { cx, cz - probe },
        { cx, cz + probe },
        { cx - probe, cz - probe },
        { cx + probe, cz - probe },
        { cx - probe, cz + probe },
        { cx + probe, cz + probe }
    };

    float centerClearance = ComputeClearanceForPoint(cx, cz);
    float minClearance = centerClearance;
    float avgClearance = 0.0f;
    int validSamples = 0;

    for (const auto& pt : samplePoints)
    {
        float c = ComputeClearanceForPoint(pt[0], pt[1]);
        minClearance = std::min(minClearance, c);
        if (c < 9000.0f)
        {
            avgClearance += c;
            ++validSamples;
        }
    }

    if (validSamples > 0)
    {
        avgClearance /= (float)validSamples;
    }
    else
    {
        avgClearance = centerClearance;
    }

    // Require at least some nearby wall geometry in a wider neighborhood.
    // This keeps us inside authored map space without falsely rejecting wider rooms.
    int nearbyWalls = 0;
    for (int32_t dx = -2; dx <= 2; ++dx)
    {
        for (int32_t dz = -2; dz <= 2; ++dz)
        {
            int32_t ngx = gx + dx;
            int32_t ngz = gz + dz;
            if (ngx < 0 || ngx >= 32 || ngz < 0 || ngz >= 32)
            {
                continue;
            }

            std::vector<uint32_t>& collzones = m_map->GetCollisions(0, ngx, ngz);
            for (uint32_t idx : collzones)
            {
                if (idx >= m_map->GetZones().size())
                {
                    continue;
                }
                const Zone& zone = m_map->GetZones()[idx];
                if (zone.ztype == Zone::ZT_WALL)
                {
                    ++nearbyWalls;
                }
            }
        }
    }

    if (nearbyWalls == 0)
    {
        return false;
    }

    // Loose but practical open-space test.
    if (centerClearance < 14.0f || minClearance < 6.0f)
    {
        return false;
    }

    outZone.minX = cellMinX + m_tuning.cellPadding;
    outZone.maxX = cellMaxX - m_tuning.cellPadding;
    outZone.minZ = cellMinZ + m_tuning.cellPadding;
    outZone.maxZ = cellMaxZ - m_tuning.cellPadding;

    // ZGloom object/world Y: floor ~= 0, eye height ~= around -110, ceiling ~= about -256.
    outZone.minY = -220;
    outZone.maxY = -42;

    if (outZone.maxX <= outZone.minX || outZone.maxZ <= outZone.minZ || outZone.maxY <= outZone.minY)
    {
        return false;
    }

    float openness = DustSaturate((avgClearance - 18.0f) / 80.0f);
    outZone.densityScale = 0.90f + openness * 1.10f;
    return true;
}

int DustParticleSystem::ComputeParticleTarget(const DustZone& zone) const
{
    const float sizeX = float(zone.maxX - zone.minX);
    const float sizeZ = float(zone.maxZ - zone.minZ);
    const float base = (sizeX * sizeZ) / (256.0f * 256.0f);
    int target = int((4.75f + base * 3.57f) * zone.densityScale * m_tuning.densityScale);
    if (target < 1) target = 1;
    return target;
}

void DustParticleSystem::SpawnParticle(DustParticle& p, uint16_t zoneIndex)
{
    if (zoneIndex >= m_zones.size())
    {
        return;
    }

    const DustZone& zone = m_zones[zoneIndex];

    p.zoneIndex = zoneIndex;
    p.x = RandRange(float(zone.minX), float(zone.maxX));
    p.y = RandRange(float(zone.minY), float(zone.maxY));
    p.z = RandRange(float(zone.minZ), float(zone.maxZ));

    const float ang = RandRange(0.0f, 6.28318530718f);
    const float drift = RandRange(m_tuning.driftSpeedMin, m_tuning.driftSpeedMax) * m_tuning.speedScale;
    p.driftX = std::cos(ang) * drift;
    p.driftZ = std::sin(ang) * drift;
    p.driftY = RandRange(m_tuning.verticalDriftMin, m_tuning.verticalDriftMax) * m_tuning.speedScale;

    p.impulseX = 0.0f;
    p.impulseY = 0.0f;
    p.impulseZ = 0.0f;

    p.swayRadius = RandRange(m_tuning.swayRadiusMin, m_tuning.swayRadiusMax);
    p.verticalOscillation = RandRange(m_tuning.verticalOscMin, m_tuning.verticalOscMax);
    p.phaseA = RandRange(0.0f, 6.28318530718f);
    p.phaseB = RandRange(0.0f, 6.28318530718f);
    p.phaseC = RandRange(0.0f, 6.28318530718f);

    p.size = RandRange(m_tuning.particleSizeMin, m_tuning.particleSizeMax);

    // Roughly half the particles are intentionally more present so the dust reads
    // better on screen, while the other half stays finer and more transparent.
    if (Rand01() < 0.5f)
    {
        p.alpha = RandRange(m_tuning.alphaMin, m_tuning.alphaMax);
    }
    else
    {
        const float strongMin = std::min(0.94f, m_tuning.alphaMax * 1.08f);
        const float strongMax = 0.985f;
        p.alpha = RandRange(strongMin, strongMax);
    }

    // Slight global transparency reduction so all dust particles read about
    // 20% lighter without changing density, motion or size.
    p.alpha *= 0.80f;

    p.maxLife = RandRange(m_tuning.lifeMin, m_tuning.lifeMax);
    p.life = p.maxLife;
}

void DustParticleSystem::WrapParticleToZone(DustParticle& p, const DustZone& zone)
{
    const float sizeX = float(zone.maxX - zone.minX);
    const float sizeY = float(zone.maxY - zone.minY);
    const float sizeZ = float(zone.maxZ - zone.minZ);

    if (sizeX > 0.0f)
    {
        while (p.x < float(zone.minX)) p.x += sizeX;
        while (p.x > float(zone.maxX)) p.x -= sizeX;
    }
    if (sizeY > 0.0f)
    {
        while (p.y < float(zone.minY)) p.y += sizeY;
        while (p.y > float(zone.maxY)) p.y -= sizeY;
    }
    if (sizeZ > 0.0f)
    {
        while (p.z < float(zone.minZ)) p.z += sizeZ;
        while (p.z > float(zone.maxZ)) p.z -= sizeZ;
    }
}

void DustParticleSystem::BuildFromMap(GloomMap* map)
{
    Reset();
    m_map = map;

    if (!m_map)
    {
        return;
    }

    for (int gz = 0; gz < 32; ++gz)
    {
        for (int gx = 0; gx < 32; ++gx)
        {
            DustZone zone;
            if (BuildZoneForCell(gx, gz, zone))
            {
                m_zones.push_back(zone);
            }
        }
    }

    if (m_zones.empty())
    {
        // Fallback pass for very tight or oddly authored maps: accept any cell with
        // at least some nearby wall geometry and a minimally open center.
        for (int gz = 0; gz < 32; ++gz)
        {
            for (int gx = 0; gx < 32; ++gx)
            {
                const int32_t cx = gx * m_tuning.cellSize + m_tuning.cellSize / 2;
                const int32_t cz = gz * m_tuning.cellSize + m_tuning.cellSize / 2;
                float centerClearance = ComputeClearanceForPoint(cx, cz);
                if (centerClearance < 10.0f || centerClearance > 9000.0f)
                {
                    continue;
                }

                DustZone zone;
                zone.minX = gx * m_tuning.cellSize + 12;
                zone.maxX = (gx + 1) * m_tuning.cellSize - 12;
                zone.minZ = gz * m_tuning.cellSize + 12;
                zone.maxZ = (gz + 1) * m_tuning.cellSize - 12;
                zone.minY = -220;
                zone.maxY = -42;
                zone.densityScale = 1.0f;
                if (zone.maxX > zone.minX && zone.maxZ > zone.minZ)
                {
                    m_zones.push_back(zone);
                }
            }
        }

        if (m_zones.empty())
        {
            return;
        }
    }

    int totalRequested = 0;
    for (const DustZone& zone : m_zones)
    {
        totalRequested += ComputeParticleTarget(zone);
    }

    const float scale = (totalRequested > m_tuning.maxParticles && totalRequested > 0)
        ? (float)m_tuning.maxParticles / (float)totalRequested
        : 1.0f;

    for (uint16_t i = 0; i < m_zones.size(); ++i)
    {
        int target = int(std::floor((float)ComputeParticleTarget(m_zones[i]) * scale));
        if (target < 1) target = 1;

        for (int n = 0; n < target; ++n)
        {
            DustParticle p;
            SpawnParticle(p, i);
            m_particles.push_back(p);
        }
    }
}

void DustParticleSystem::Update(const DustCameraState& camera, float dt)
{
    if (!m_tuning.enabled || m_particles.empty())
    {
        return;
    }

    if (dt <= 0.0f)
    {
        dt = 0.001f;
    }
    if (dt > 0.050f)
    {
        dt = 0.050f;
    }

    m_elapsedTime += dt;

    const float camSpeed = std::sqrt(float(camera.dx * camera.dx + camera.dz * camera.dz)) / std::max(dt, 0.001f);
    const bool applyInfluence = m_tuning.enablePlayerInfluence && camSpeed >= m_tuning.playerInfluenceMinSpeed;

    float moveDirX = 0.0f;
    float moveDirZ = 0.0f;
    if (applyInfluence)
    {
        moveDirX = float(camera.dx) / std::max(std::sqrt(float(camera.dx * camera.dx + camera.dz * camera.dz)), 0.001f);
        moveDirZ = float(camera.dz) / std::max(std::sqrt(float(camera.dx * camera.dx + camera.dz * camera.dz)), 0.001f);
    }

    for (DustParticle& p : m_particles)
    {
        if (p.zoneIndex >= m_zones.size())
        {
            continue;
        }

        const DustZone& zone = m_zones[p.zoneIndex];

        p.life -= dt;
        if (p.life <= 0.0f)
        {
            SpawnParticle(p, p.zoneIndex);
            continue;
        }

        p.x += p.driftX * dt;
        p.y += p.driftY * dt;
        p.z += p.driftZ * dt;

        if (applyInfluence)
        {
            const float dx = p.x - float(camera.x);
            const float dz = p.z - float(camera.z);
            const float dist = std::sqrt(dx * dx + dz * dz);

            if (dist > 0.001f && dist < m_tuning.playerInfluenceRadius)
            {
                const float falloff = 1.0f - DustSaturate(dist / m_tuning.playerInfluenceRadius);
                const float radialX = dx / dist;
                const float radialZ = dz / dist;
                const float swirlX = -moveDirZ;
                const float swirlZ = moveDirX;

                p.impulseX += (swirlX * 0.65f + radialX * 0.35f) * (m_tuning.playerInfluenceStrength * falloff * dt);
                p.impulseZ += (swirlZ * 0.65f + radialZ * 0.35f) * (m_tuning.playerInfluenceStrength * falloff * dt);
                p.impulseY += falloff * 1.0f * dt;
            }
        }

        p.x += p.impulseX * dt;
        p.y += p.impulseY * dt;
        p.z += p.impulseZ * dt;

        const float damp = std::exp(-m_tuning.impulseDamping * dt);
        p.impulseX *= damp;
        p.impulseY *= damp;
        p.impulseZ *= damp;

        WrapParticleToZone(p, zone);
    }
}

void DustParticleSystem::GatherRenderParticles(std::vector<DustRenderParticle>& out) const
{
    out.clear();
    if (!m_tuning.enabled || m_particles.empty())
    {
        return;
    }

    out.reserve(m_particles.size());

    for (const DustParticle& p : m_particles)
    {
        if (p.zoneIndex >= m_zones.size())
        {
            continue;
        }

        DustRenderParticle rp;
        rp.zoneIndex = p.zoneIndex;
        rp.x = p.x + std::sin(m_elapsedTime * 0.47f + p.phaseA) * p.swayRadius;
        rp.y = p.y + std::sin(m_elapsedTime * 0.71f + p.phaseB) * p.verticalOscillation;
        rp.z = p.z + std::cos(m_elapsedTime * 0.39f + p.phaseC) * p.swayRadius;
        rp.size = p.size;
        rp.alpha = p.alpha;
        out.push_back(rp);
    }
}
