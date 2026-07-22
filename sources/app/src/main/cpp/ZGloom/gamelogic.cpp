#include "gamelogic.h"
#include "renderer.h"
#include "monsterlogic.h"
#include "soundhandler.h"
#include "hud.h"
#include "config.h"
#include "SaveSystem.h"
#include <cctype>
#include <algorithm>
#include <sstream>
#include <fstream>
#include "cheats/CheatSystem.h"

// Defender minigame implementation lives in this always-compiled translation
// unit.  Some older Android CMake/Gradle native models keep a fixed source
// list and ignore newly added .cpp files even after reconfiguration.  Keeping
// these definitions here makes the portable minigame independent of source
// glob/cache behaviour on Android, while DefenderGame.cpp remains a harmless
// compatibility stub for projects that already list it explicitly.

const DefenderGame::ShapeDef DefenderGame::kShapes[14] =
{
    {65, 36,  6,  3}, // 0 player right
    {65, 40,  6,  3}, // 1 player left
    {66, 44,  3,  1}, // 2 bullet right
    {66, 46,  3,  1}, // 3 bullet left
    {72, 37,  5,  5}, // 4 lander
    {84, 37,  1,  1}, // 5 fragment
    {86, 39,  1,  1}, // 6 fragment
    {81, 39,  1,  1}, // 7 player fragment
    {79, 37,  1,  1}, // 8 player fragment
    {71, 44,  2,  1}, // 9 life marker
    {65, 49, 15, 12}, // 10 game over
    {86, 49, 11, 10}, // 11 win
    {79, 42,  2,  5}, // 12 scanner left
    {85, 42,  2,  5}  // 13 scanner right
};

DefenderGame::DefenderGame()
{
    objects.reserve(kMaxObjects);
}

bool DefenderGame::Begin(std::vector<Column>* textureColumns, std::size_t textureBaseColumn, int episodeNumber)
{
    Reset();

    columns = textureColumns;
    baseColumn = textureBaseColumn;
    episode = std::max(1, std::min(3, episodeNumber));

    if (!ValidateTexture())
    {
        Reset();
        return false;
    }

    static const int targetTable[3] = {20, 35, 50};
    static const int delayTable[3] = {25, 20, 15};
    targetTotal = targetTable[episode - 1];
    targetsRemaining = targetTotal;
    landerDelay = delayTable[episode - 1];
    playerLives = 3;
    active = true;

    InitialiseRound();
    DrawFrame();
    return true;
}

void DefenderGame::Reset()
{
    columns = nullptr;
    baseColumn = 0;
    objects.clear();
    active = false;
    complete = false;
    won = false;
    fireHeld = false;
    playerDead = false;
    resultStarted = false;
    episode = 1;
    targetTotal = 20;
    targetsToSpawn = 20;
    targetsRemaining = 20;
    landerDelay = 25;
    landerCounter = 1;
    playerLives = 3;
    playerShape = 0;
    deadCounter = 0;
    resultCounter = 0;
    playerX = 0;
    playerY = 18 << 16;
    playerVX = 0;
}

bool DefenderGame::ValidateTexture() const
{
    // Visible monitor: base + 0..63. Defender artwork: base + 64..127.
    return columns && columns->size() >= baseColumn + 128;
}

void DefenderGame::InitialiseRound()
{
    objects.clear();
    targetsToSpawn = targetsRemaining;
    landerCounter = landerDelay;
    playerX = 0;
    playerY = 18 << 16;
    playerVX = 0;
    playerShape = 0;
    playerDead = false;
    deadCounter = 0;
    fireHeld = false;
}

void DefenderGame::Update(const InputState& input)
{
    if (!active || complete || !ValidateTexture())
        return;

    if (resultStarted)
    {
        // The original keeps drawing/updating residual explosion fragments
        // during the short WIN/GAME OVER hold, while normal play is locked.
        UpdateObjects();
        RemoveDeadObjects();
        if (resultCounter > 0)
            --resultCounter;
        if (resultCounter <= 0)
        {
            complete = true;
            active = false;
        }
        DrawFrame();
        return;
    }

    if (targetsToSpawn > 0)
    {
        --landerCounter;
        if (landerCounter <= 0)
        {
            SpawnLander();
            landerCounter = landerDelay;
        }
    }

    if (!playerDead)
        UpdatePlayer(input);
    else
    {
        ++deadCounter;
        if (deadCounter >= 96)
        {
            if (playerLives > 0)
                InitialiseRound();
            else
                StartResult(false);
        }
    }

    UpdateObjects();
    ResolveCollisions();
    RemoveDeadObjects();

    if (targetsRemaining <= 0)
        StartResult(true);
    else if (playerLives <= 0 && playerDead && deadCounter >= 96)
        StartResult(false);

    DrawFrame();
}

void DefenderGame::StartResult(bool playerWon)
{
    if (resultStarted)
        return;

    won = playerWon;
    resultStarted = true;
    resultCounter = 96;
    fireHeld = false;
}

void DefenderGame::UpdatePlayer(const InputState& input)
{
    if (input.moveX < 0)
    {
        playerShape = 1;
        playerVX -= 0x8000;
        if (playerVX >= 0)
            playerVX -= 0x4000;
        if (playerVX < -0x30000)
            playerVX = -0x30000;
    }
    else if (input.moveX > 0)
    {
        playerShape = 0;
        playerVX += 0x8000;
        if (playerVX <= 0)
            playerVX += 0x4000;
        if (playerVX > 0x30000)
            playerVX = 0x30000;
    }
    else if (playerVX < 0)
    {
        playerVX += 0x1000;
        if (playerVX > 0)
            playerVX = 0;
    }
    else if (playerVX > 0)
    {
        playerVX -= 0x1000;
        if (playerVX < 0)
            playerVX = 0;
    }

    playerX = WrapWorldX(playerX + playerVX);

    playerY += input.moveY * 0x10000;
    if (playerY < (1 << 16))
        playerY = 1 << 16;
    if (playerY > (34 << 16))
        playerY = 34 << 16;

    if (input.fire)
    {
        if (!fireHeld)
        {
            fireHeld = true;
            SpawnBullet();
        }
    }
    else
    {
        fireHeld = false;
    }
}

void DefenderGame::SpawnBullet()
{
    if (objects.size() >= kMaxObjects)
        return;

    DefObject bullet;
    bullet.type = ObjectType::Bullet;
    bullet.x = playerX;
    bullet.y = playerY + (1 << 16);
    bullet.vx = (playerShape == 0 ? 0x20000 : -0x20000) + playerVX;
    bullet.vy = 0;
    bullet.shape = playerShape + 2;
    bullet.delay = 12;
    bullet.collidable = true;
    objects.push_back(bullet);
    SoundHandler::Play(SoundHandler::SOUND_SHOOT3);
}

void DefenderGame::SpawnLander()
{
    if (targetsToSpawn <= 0 || objects.size() >= kMaxObjects)
        return;

    --targetsToSpawn;

    DefObject lander;
    lander.type = ObjectType::Lander;
    lander.x = static_cast<int32_t>(GloomMaths::RndW() & 255) << 16;
    lander.y = static_cast<int32_t>(GloomMaths::RndN(32)) << 16;
    lander.shape = 5;
    lander.delay = 32;
    lander.collidable = false;
    objects.push_back(lander);

    // The original creates a brief cloud of pixels around every incoming lander.
    SpawnFragments(lander.x, lander.y, 8, 5, false);
}

void DefenderGame::SpawnFragments(int32_t x, int32_t y, int count, int firstShape, bool gravity)
{
    for (int i = 0; i < count && objects.size() < kMaxObjects; ++i)
    {
        DefObject fragment;
        fragment.type = gravity ? ObjectType::Fragment : ObjectType::ImpactFragment;
        fragment.vx = RandomSigned16() * 2;
        fragment.vy = RandomSigned16() * 2;
        fragment.x = WrapWorldX(x - (fragment.vx * 32));
        fragment.y = y - (fragment.vy * 32);
        fragment.shape = firstShape + (i & 1);
        fragment.delay = gravity ? 15 : 32;
        fragment.collidable = false;
        objects.push_back(fragment);
    }
}

void DefenderGame::DestroyLander(std::size_t index)
{
    if (index >= objects.size() || objects[index].remove)
        return;

    const int32_t x = objects[index].x;
    const int32_t y = objects[index].y;
    objects[index].remove = true;
    --targetsRemaining;
    SoundHandler::Play(SoundHandler::SOUND_DIE);
    SpawnFragments(x, y, 8, 5, true);
}

void DefenderGame::DestroyPlayer()
{
    if (playerDead)
        return;

    playerDead = true;
    deadCounter = 0;
    --playerLives;
    SoundHandler::Play(SoundHandler::SOUND_ROBODIE);
    SpawnFragments(playerX, playerY, 32, 7, true);
}

void DefenderGame::UpdateObjects()
{
    for (DefObject& object : objects)
    {
        if (object.remove)
            continue;

        switch (object.type)
        {
            case ObjectType::Bullet:
                --object.delay;
                if (object.delay <= 0)
                {
                    object.remove = true;
                    break;
                }
                object.x = WrapWorldX(object.x + object.vx);
                break;

            case ObjectType::ImpactFragment:
                --object.delay;
                if (object.delay <= 0)
                {
                    object.remove = true;
                    break;
                }
                object.x = WrapWorldX(object.x + object.vx);
                object.y += object.vy;
                break;

            case ObjectType::Fragment:
                object.vy += 0x1000;
                object.y += object.vy;
                if ((object.y >> 16) >= kScreenHeight)
                {
                    object.remove = true;
                    break;
                }
                object.x = WrapWorldX(object.x + object.vx);
                break;

            case ObjectType::Lander:
                // In the Amiga routine landers stop immediately while the
                // player explosion/death countdown is active.
                if (playerDead)
                    break;

                if (object.delay > 0)
                {
                    --object.delay;
                    if (object.delay <= 0)
                    {
                        object.shape = 4;
                        object.collidable = true;
                    }
                    break;
                }

                if (!playerDead)
                {
                    const int landerX = object.x >> 16;
                    const int targetX = playerX >> 16;
                    const int dx = WrappedDelta(landerX, targetX);
                    if (dx < 0)
                        object.vx += 0x4000;
                    else if (dx > 0)
                        object.vx -= 0x4000;
                    object.vx = std::max(-0x10000, std::min(0x10000, object.vx));

                    const int landerY = object.y >> 16;
                    const int targetY = playerY >> 16;
                    if (landerY < targetY)
                        object.vy += 0x2000;
                    else if (landerY > targetY)
                        object.vy -= 0x2000;
                    object.vy = std::max(-0x8000, std::min(0x8000, object.vy));
                }

                object.x = WrapWorldX(object.x + object.vx);
                object.y += object.vy;
                break;
        }
    }
}

void DefenderGame::ResolveCollisions()
{
    for (std::size_t li = 0; li < objects.size(); ++li)
    {
        DefObject& lander = objects[li];
        if (lander.remove || lander.type != ObjectType::Lander || !lander.collidable)
            continue;

        const int lx = lander.x >> 16;
        const int ly = lander.y >> 16;

        if (!playerDead)
        {
            const int dx = std::abs(WrappedDelta(lx, playerX >> 16));
            const int dy = std::abs(ly - (playerY >> 16));
            if (dx < 6 && dy < 4)
            {
                DestroyPlayer();
                continue;
            }
        }

        for (std::size_t bi = 0; bi < objects.size(); ++bi)
        {
            DefObject& bullet = objects[bi];
            if (bullet.remove || bullet.type != ObjectType::Bullet || !bullet.collidable)
                continue;

            const int dx = std::abs(WrappedDelta(lx, bullet.x >> 16));
            const int dy = std::abs(ly - (bullet.y >> 16));
            if (dx < 6 && dy < 4)
            {
                bullet.remove = true;
                DestroyLander(li);
                break;
            }
        }
    }
}

void DefenderGame::RemoveDeadObjects()
{
    objects.erase(std::remove_if(objects.begin(), objects.end(),
        [](const DefObject& object) { return object.remove; }), objects.end());
}

void DefenderGame::DrawFrame()
{
    if (!ValidateTexture())
        return;

    DrawMountains((playerX >> 16) - 22);

    DrawSprite(5, 3, 12);
    DrawSprite(39, 3, 13);
    DrawSprite(22, ((playerY >> 16) >> 3) + 1, 7);

    for (const DefObject& object : objects)
    {
        if (object.remove || object.type != ObjectType::Lander || object.shape != 4)
            continue;

        int scanX = WrappedDelta(object.x >> 16, playerX >> 16);
        scanX = ((scanX >> 3) + 16) & 31;
        scanX += 6;
        const int scanY = ((object.y >> 16) >> 3) + 1;
        DrawSprite(scanX, scanY, 5);
    }

    for (const DefObject& object : objects)
    {
        if (object.remove)
            continue;
        const int x = WrappedDelta(object.x >> 16, playerX >> 16) + 22;
        const int y = object.y >> 16;
        DrawSprite(x, y, object.shape);
    }

    if (resultStarted)
    {
        if ((resultCounter & 0x10) != 0)
            DrawSprite(22, 18, won ? 11 : 10);
    }
    else if (!playerDead)
    {
        if (targetsRemaining <= 0 && ((targetsRemaining - 1) & 0x10) != 0)
            DrawSprite(22, 18, 11);
        DrawSprite(22, playerY >> 16, playerShape);
    }

    if (playerLives > 0)
    {
        for (int i = 0; i < playerLives; ++i)
            DrawSprite(2, 1 + i * 2, 9);
    }
}

void DefenderGame::DrawMountains(int offset)
{
    const int topOffset = (offset >> 1) & 63;
    const int bottomOffset = offset & 63;

    for (int x = 0; x < kScreenWidth; ++x)
    {
        const int sourceTopX = kArtworkX + ((topOffset + x) & 63);
        const int sourceBottomX = kArtworkX + ((bottomOffset + x) & 63);

        for (int y = 0; y < 25; ++y)
            SetDestinationPixel(x, y, GetArtworkPixel(sourceTopX, y));
        for (int y = 25; y < kScreenHeight; ++y)
            SetDestinationPixel(x, y, GetArtworkPixel(sourceBottomX, y));
    }
}

void DefenderGame::DrawSprite(int destX, int destY, int shape)
{
    if (shape < 0 || shape >= static_cast<int>(sizeof(kShapes) / sizeof(kShapes[0])))
        return;

    const ShapeDef& s = kShapes[shape];
    const int startX = destX - s.width / 2;
    const int startY = destY - s.height / 2;

    for (int x = 0; x < s.width; ++x)
    {
        const int dx = startX + x;
        if (dx < 0 || dx >= kScreenWidth)
            continue;

        for (int y = 0; y < s.height; ++y)
        {
            const int dy = startY + y;
            if (dy < 0 || dy >= kScreenHeight)
                continue;

            const uint8_t colour = GetArtworkPixel(s.sourceX + x, s.sourceY + y);
            if (colour != 0)
                SetDestinationPixel(dx, dy, colour);
        }
    }
}

void DefenderGame::SetDestinationPixel(int x, int y, uint8_t colour)
{
    if (!ValidateTexture() || x < 0 || x >= kScreenWidth || y < 0 || y >= kScreenHeight)
        return;

    (*columns)[baseColumn + kDestinationX + static_cast<std::size_t>(x)].data[kDestinationY + y] = colour;
}

uint8_t DefenderGame::GetArtworkPixel(int x, int y) const
{
    if (!ValidateTexture() || x < kArtworkX || x >= kArtworkX + 64 || y < 0 || y >= 64)
        return 0;

    return (*columns)[baseColumn + static_cast<std::size_t>(x)].data[y];
}

int32_t DefenderGame::WrapWorldX(int32_t value)
{
    return static_cast<int32_t>(static_cast<uint32_t>(value) & 0x00FFFFFFu);
}

int DefenderGame::WrappedDelta(int lhs, int rhs)
{
    int delta = lhs - rhs;
    delta &= 255;
    if (delta >= 128)
        delta -= 256;
    return delta;
}

int DefenderGame::RandomSigned16()
{
    return static_cast<int16_t>(GloomMaths::RndW());
}



// --------- Minimal loader for cheats.txt (no dependency on Cheats::Load) ---------
static bool g_CheatsLoadedOnceGL = false;

static inline void trim_gl(std::string& s) {
    auto notspace = [](int ch){ return !std::isspace(ch); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notspace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notspace).base(), s.end());
}
static inline std::string upper_gl(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)std::toupper(c); });
    return s;
}

static void EnsureCheatsLoadedGL() {
    if (g_CheatsLoadedOnceGL) return;
    g_CheatsLoadedOnceGL = true;
    std::ifstream in("cheats.txt");
    if (!in) return;
    std::string line;
    while (std::getline(in, line)) {
        auto sc = line.find_first_of(";#");
        if (sc != std::string::npos) line = line.substr(0, sc);
        trim_gl(line);
        if (line.empty()) continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = upper_gl(line.substr(0, eq));
        std::string v = upper_gl(line.substr(eq+1));
        trim_gl(k); trim_gl(v);
        auto isOn = [&](const std::string& s)->bool { return (s=="1" || s=="ON" || s=="TRUE"); };
        auto toInt = [&](const std::string& s)->int { try { return std::stoi(s); } catch(...) { return 0; } };
        if (k == "GOD") { Cheats::SetGodMode(isOn(v)); }
        else if (k == "ONEHIT" || k == "ONEHITKILL") { Cheats::SetOneHitKill(isOn(v)); }
        else if (k == "THERMO" || k == "THERMOGOGGLES") { Cheats::SetThermoGoggles(isOn(v)); }
        else if (k == "BOUNCY" || k == "BOUNCYBULLETS") { Cheats::SetBouncyBullets(isOn(v)); }
        else if (k == "INVIS" || k == "INVISIBILITY") { Cheats::SetInvisibility(isOn(v)); }
        else if (k == "STARTWEAPON" || k == "WEAPON") { Cheats::SetStartWeapon(toInt(v)); }
    }
}


void GameLogic::Init(ObjectGraphics* ograph)
{
	// note weird order of SFX.
	wtable[0].hitpoint = 1;
	wtable[0].damage = 1;
	wtable[0].speed = 32;
	wtable[0].sound = SoundHandler::SOUND_SHOOT3;

	wtable[1].hitpoint = 5;
	wtable[1].damage = 2;
	wtable[1].speed = 36;
	wtable[1].sound = SoundHandler::SOUND_SHOOT5;

	wtable[2].hitpoint = 10;
	wtable[2].damage = 2;
	wtable[2].speed = 40;
	wtable[2].sound = SoundHandler::SOUND_SHOOT;

	wtable[3].hitpoint = 15;
	wtable[3].damage = 3;
	wtable[3].speed = 40;
	wtable[3].sound = SoundHandler::SOUND_SHOOT4;

	wtable[4].hitpoint = 20;
	wtable[4].damage = 5;
	wtable[4].speed = 24;
	wtable[4].sound = SoundHandler::SOUND_SHOOT5;

	for (auto i = 0; i < 5; i++)
	{
		wtable[i].shape = &(ograph->BulletShapes[i]);
	}

	for (auto i = 0; i < 5; i++)
	{
		wtable[i].spark = &(ograph->SparkShapes[i]);
	}

	p1lives = 3;
	p1health = 25;
	p1weapon = 0;
	p1reload = 5;

	playerhit = false;
	playerDeathActive = false;
	lifeDebitedForCurrentDeath = false;
	gameOverRequested = false;
	defenderGame.Reset();
	defenderSessionActive = false;
	defenderLocking = false;
	defenderRewardApplied = false;
	defenderOrbitSpeed = 0;
}

void GameLogic::RestoreTriggeredEvents(const std::vector<uint32_t>& events)
{
	for (const uint32_t eventId : events)
	{
		// Events below 19 are one-shot triggers in the original game logic.
		// Higher event numbers remain repeatable after loading.
		if (eventId >= 2 && eventId < 19 && eventId < 25)
			eventhit[eventId] = true;
	}
}

void GameLogic::SetLives(int lives)
{
	p1lives = static_cast<int16_t>(std::max(0, std::min(5, lives)));
	playerDeathActive = false;
	lifeDebitedForCurrentDeath = false;
	if (p1lives > 0)
		gameOverRequested = false;
}

bool GameLogic::AwardLife()
{
	if (p1lives >= 5)
		return false;

	++p1lives;
	return true;
}

void GameLogic::NotifyPlayerDeathStarted()
{
	if (!playerDeathActive)
	{
		playerDeathActive = true;
		lifeDebitedForCurrentDeath = false;
	}
}

void GameLogic::CommitPlayerDeath()
{
	NotifyPlayerDeathStarted();
	if (lifeDebitedForCurrentDeath)
		return;

	if (p1lives > 0)
		--p1lives;

	lifeDebitedForCurrentDeath = true;
	SDL_Log("ZGloom Lives: player death committed; lives=%d", p1lives);
}

void GameLogic::RequestGameOver()
{
	gameOverRequested = true;
}

bool GameLogic::ConsumeGameOverRequest()
{
	const bool requested = gameOverRequested;
	gameOverRequested = false;
	return requested;
}

void GameLogic::ResetPlayer(MapObject& o)
{
	// reset the player on death
	o.data.ms.hitpoints = 25;
	o.data.ms.eyey = -110;
	o.data.ms.logic = NullLogic;
	//o.data.ms.colltype = 8;
	//o.data.ms.collwith = 4;
	// 3 seconds invulnrability. Curiously, code comment state 2?
	o.data.ms.delay = 75;
	o.x = origx;
	o.z = origz;
	o.data.ms.rotquick.SetInt(origrot);
	o.data.ms.invisible = 0;
	o.data.ms.thermo = 0;
	o.data.ms.bouncecnt = 0;
	playerDeathActive = false;
	lifeDebitedForCurrentDeath = false;
}

void GameLogic::InitLevel(GloomMap* gmapin, Camera* cam, ObjectGraphics* ograph)
{
	gmap = gmapin;
	objectgraphics = ograph;
	levelfinished = false;
	levelfinishednow = false;
	sucking = 0;
	sucker = 0;
	playerDeathActive = false;
	lifeDebitedForCurrentDeath = false;
	gameOverRequested = false;
	defenderGame.Reset();
	defenderSessionActive = false;
	defenderLocking = false;
	defenderRewardApplied = false;
	defenderOrbitSpeed = 0;
	std::fill(animframe, animframe + 160, 0);

	for (auto e = 0; e < 25; e++)
	{
		eventhit[e] = false;
	}

	for (auto &o : gmap->GetMapObjects())
	{
		if (o.t == 0)// player
		{
			cam->x = o.x;
			cam->y = 120; //TODO, and rotation
			cam->z = o.z;
			cam->rotquick = o.data.ms.rotquick;

			origx = o.x;
			origz = o.z;
			origrot = o.data.ms.rotquick.GetInt();

			o.data.ms.hitpoints = p1health;
			//o.data.ms.lives = p1lives; TODO
			o.data.ms.weapon = p1weapon;
			o.data.ms.reload = p1reload;
			/* CHEAT INIT OVERRIDES */
			if (Cheats::GetGodMode()) { p1health = 32767; o.data.ms.hitpoints = p1health; }
			{
				int sw = Cheats::GetStartWeapon();
				if (sw >= 0 && sw <= 4) { p1weapon = sw; o.data.ms.weapon = p1weapon; }
			}

		}
	}
}

bool GameLogic::AdjustPos(int32_t& overshoot, Quick& x, Quick& z, int32_t r, int32_t& closestzone)
{
	/*
	adjustposq;
	neg	d0
	move	d0, d1
		;
	muls	zo_a(a4), d0
	add.l	d0, d0
		;
	muls	zo_b(a4), d1
	add.l	d1, d1
		;
	sub.l	d0, d6
	sub.l	d1, d7
		;
	rts
	*/

	overshoot = -overshoot;

	int32_t xo, zo;

	xo = overshoot * gmap->GetZones()[closestzone].a;
	xo += xo;

	zo = overshoot * gmap->GetZones()[closestzone].b;
	zo += zo;

	x.SetVal(x.GetVal() - xo);
	z.SetVal(z.GetVal() - zo);

	return Collision(false, x.GetInt(), z.GetInt(), r, overshoot, closestzone);
}

void GameLogic::MoveBlood()
{
	// Floor pools and wall splats are level-local and permanent once fully
	// grown.  Saturating the age keeps the render calculation cheap and avoids
	// ever-growing counters.
	for (auto& pool : gmap->GetBloodPools())
	{
		if (pool.age < 24)
			++pool.age;
	}
	for (auto& splat : gmap->GetWallBloodSplats())
	{
		if (splat.age < 20)
			++splat.age;
	}

	for (auto &b : gmap->GetBlood())
	{
		//TODO: sucking, screen splatter

		if (b.y.GetInt() < 0)
		{
			// Normal airborne blood.  MASSACRE particles now collide with wall
			// geometry before the position is committed.  The contact point is
			// stored as a persistent wall decal and the particle stops there.
			Quick nextx = b.x + b.xvec;
			Quick nexty = b.y + b.yvec;
			Quick nextz = b.z + b.zvec;

			if (Config::BloodPoolsEnabled() && nexty.GetInt() < -4 &&
				nexty.GetInt() > -252)
			{
				int32_t overshoot = 0;
				int32_t closestzone = -1;
				// BloodSpeed3 can move by roughly eight world units per tick.  A
				// radius of eight therefore catches fast particles without making
				// parallel flights stain unrelated nearby walls.
				if (Collision(false, nextx.GetInt(), nextz.GetInt(), 8,
					overshoot, closestzone) && closestzone >= 0)
				{
					Zone& wall = gmap->GetZones()[closestzone];
					const int oldDistance = FindSegDist(
						b.x.GetInt(), b.z.GetInt(), wall);
					const int newDistance = FindSegDist(
						nextx.GetInt(), nextz.GetInt(), wall);
					if (newDistance <= oldDistance || oldDistance < 8)
					{
						AddWallBloodSplat(closestzone, nextx, nexty, nextz,
							b.color);
						b.killme = true;
						continue;
					}
				}
			}

			b.x = nextx;
			b.y = nexty;
			b.z = nextz;

			Quick temp;
			temp.SetVal(0x8000);
			b.yvec = b.yvec + temp;

			if (b.y.GetInt() >= 0)
			{
				b.killme = true;
			}
		}
		else
		{
			// deaths head suck!
			/*
			move.l	bl_xvec(a5),d0
			add.l	d0,bl_x(a5)
			move.l	bl_zvec(a5),d1
			add.l	d1,bl_z(a5)
			;
			move.l	bl_dest(a5),a0
			move	bl_x(a5),d0
			sub	ob_x(a0),d0
			muls	d0,d0
			move	bl_z(a5),d1
			sub	ob_z(a0),d1
			muls	d1,d1
			add.l	d1,d0
			cmp.l	#64*64,d0
			bcc.s	.loop
			bra.s	.kill
			*/

			if (!sucking) 
			{
				b.killme = true;
			}
			else
			{
				b.x = b.x + b.xvec;
				b.z = b.z + b.zvec;

				MapObject o = GetNamedObj(b.dest);

				int32_t dist = (o.x.GetInt() - b.x.GetInt()) *  (o.x.GetInt() - b.x.GetInt());

				dist += (o.z.GetInt() - b.z.GetInt()) *  (o.z.GetInt() - b.z.GetInt());

				if (dist < 64 * 64)
				{
					b.killme = true;
				}
			}
		}
	}

	// kill pass
	auto b = gmap->GetBlood().begin();

	while (b != gmap->GetBlood().end())
	{
		if (b->killme)
		{
			b = gmap->GetBlood().erase(b);
		}
		else
		{
			++b;
		}
	}
}

void GameLogic::AddBloodPoolAt(Quick x, Quick z, uint32_t colour,
	uint64_t sourceKey, uint64_t ownerKey, int targetRadius)
{
	if (!gmap || !Config::BloodPoolsEnabled())
		return;

	colour &= 0x0FFFu;
	if (colour == 0)
		return;

	// Each emitter may create exactly one mark.  Central pools use the enemy ID;
	// landed gore chunks use their own object IDs while retaining the enemy ID
	// as owner so the number of satellite pools can be bounded.
	for (const auto& pool : gmap->GetBloodPools())
	{
		if (sourceKey != 0 && pool.source == sourceKey)
			return;
	}

	static const std::size_t kMaxBloodPools = 64;
	auto& pools = gmap->GetBloodPools();
	if (pools.size() >= kMaxBloodPools)
		pools.pop_front();

	BloodPool pool;
	pool.x = x;
	pool.z = z;
	pool.color = colour;
	pool.source = sourceKey;
	pool.owner = ownerKey;
	pool.age = 0;
	if (targetRadius < 10) targetRadius = 10;
	if (targetRadius > 72) targetRadius = 72;
	pool.targetRadius = static_cast<uint16_t>(targetRadius);

	const uint32_t mix = static_cast<uint32_t>(sourceKey) ^
		static_cast<uint32_t>(sourceKey >> 32) ^
		static_cast<uint32_t>(ownerKey) ^
		static_cast<uint32_t>(ownerKey >> 32) ^
		static_cast<uint32_t>(x.GetInt() * 131) ^
		static_cast<uint32_t>(z.GetInt() * 313);
	pool.seed = static_cast<uint16_t>(mix ^ (mix >> 16));

	pools.push_back(pool);
}

void GameLogic::AddBloodPool(MapObject& source, int radiusPercent)
{
	if (!gmap || !Config::BloodPoolsEnabled())
		return;

	int radius = source.data.ms.rad;
	if (radius <= 0)
		radius = 36;
	radius = (radius * source.data.ms.scale) / 0x100;
	if (radius < 28) radius = 28;
	if (radius > 72) radius = 72;

	if (radiusPercent < 25) radiusPercent = 25;
	if (radiusPercent > 100) radiusPercent = 100;
	radius = (radius * radiusPercent + 50) / 100;
	if (radius < 16) radius = 16;

	AddBloodPoolAt(source.x, source.z, source.data.ms.blood,
		source.identifier, source.identifier, radius);
}

void GameLogic::AddChunkBloodPool(MapObject& chunk)
{
	if (!gmap || !Config::BloodPoolsEnabled())
		return;

	const uint32_t colour = chunk.data.ms.blood & 0x0FFFu;
	if (colour == 0)
		return;

	const uint64_t owner = chunk.data.ms.washit != 0 ?
		chunk.data.ms.washit : chunk.identifier;

	// Large enemies can emit the same gore set several times.  A bounded number
	// of landed-piece pools keeps the result readable and protects slower
	// devices while still distributing the stains over the actual landing area.
	int satellites = 0;
	for (const auto& pool : gmap->GetBloodPools())
	{
		if (pool.owner == owner && pool.source != owner)
			++satellites;
	}
	static const int kMaxChunkPoolsPerEnemy = 12;
	if (satellites >= kMaxChunkPoolsPerEnemy)
		return;

	int radius = chunk.data.ms.rad;
	if (radius <= 0)
		radius = 28;
	radius = (radius * chunk.data.ms.scale) / 0x200;
	radius = radius / 3;

	// Give differently shaped chunks slightly different stain sizes without
	// introducing another RNG dependency into the simulation.
	const int frame = static_cast<int>((chunk.data.ms.frame >> 16) & 0xFFFFu);
	radius += (frame & 3) * 2;
	if (radius < 12) radius = 12;
	if (radius > 28) radius = 28;

	AddBloodPoolAt(chunk.x, chunk.z, colour,
		chunk.identifier, owner, radius);
}

void GameLogic::AddWallBloodSplat(int zoneIndex, Quick x, Quick y, Quick z,
	uint32_t colour)
{
	if (!gmap || !Config::BloodPoolsEnabled())
		return;

	colour &= 0x0FFFu;
	if (colour == 0 || zoneIndex < 0 ||
		zoneIndex >= static_cast<int>(gmap->GetZones().size()))
		return;

	Zone& zone = gmap->GetZones()[zoneIndex];
	if (zone.ztype != Zone::ZT_WALL || (zone.a == 0 && zone.b == 0))
		return;

	const int64_t dx = static_cast<int64_t>(zone.x2) - zone.x1;
	const int64_t dz = static_cast<int64_t>(zone.z2) - zone.z1;
	const int64_t len2 = dx * dx + dz * dz;
	if (len2 <= 0)
		return;

	int64_t dot = (static_cast<int64_t>(x.GetInt()) - zone.x1) * dx +
		(static_cast<int64_t>(z.GetInt()) - zone.z1) * dz;
	if (dot < 0) dot = 0;
	if (dot > len2) dot = len2;
	const uint16_t along = static_cast<uint16_t>((dot * 65535 + len2 / 2) / len2);

	int wallY = y.GetInt();
	if (wallY > -8) wallY = -8;
	if (wallY < -248) wallY = -248;

	// Nearby particles on the same wall merge into a larger irregular mark.
	// This keeps a 31-particle death burst readable while preserving isolated
	// droplets farther away.
	auto& splats = gmap->GetWallBloodSplats();
	const int wallLength = zone.ln > 0 ? zone.ln : 256;
	for (auto& splat : splats)
	{
		if (splat.zone != static_cast<uint32_t>(zoneIndex) ||
			splat.color != colour)
			continue;
		const int alongWorld =
			(static_cast<int>(along) - static_cast<int>(splat.along)) *
			wallLength / 65535;
		const int dy = wallY - splat.y;
		if (alongWorld * alongWorld + dy * dy <= 18 * 18)
		{
			if (splat.targetRadius < 34)
				splat.targetRadius = static_cast<uint16_t>(
					std::min(34, static_cast<int>(splat.targetRadius) + 2));
			if (splat.age > 4) splat.age = 4;
			return;
		}
	}

	static const std::size_t kMaxWallBloodSplats = 64;
	if (splats.size() >= kMaxWallBloodSplats)
		splats.pop_front();

	WallBloodSplat splat;
	splat.zone = static_cast<uint32_t>(zoneIndex);
	splat.along = along;
	splat.y = static_cast<int16_t>(wallY);
	splat.color = colour;
	splat.age = 0;

	const uint32_t mix = static_cast<uint32_t>(zoneIndex * 977) ^
		static_cast<uint32_t>(along * 131u) ^
		static_cast<uint32_t>((wallY + 256) * 313) ^ colour;
	splat.seed = static_cast<uint16_t>(mix ^ (mix >> 16));
	splat.targetRadius = static_cast<uint16_t>(18 + (splat.seed & 7u));
	splats.push_back(splat);
}

void GameLogic::DoDoor()
{
	for (auto &d : gmap->GetActiveDoors())
	{

		/*
		dodoors
		lea	doors(pc), a5
		;
		.loop
		move.l(a5), a5
		tst.l(a5)
		beq.done
		;
		move.l	do_poly(a5), a0
		move	do_fracadd(a5), d0
		add		d0, do_frac(a5)
		move	do_frac(a5), d0
		move	d0, d1
		add		d1, d1
		move	d1, zo_open(a0); copy frac
		;
		*/
		Zone& zone = gmap->GetZones()[d.do_poly];

		d.do_frac += d.do_fracadd;
		zone.open = d.do_frac * 2;

		/*
			move	do_rx(a5), d1
			sub		do_lx(a5), d1; width
			move	d1, d2
			muls	d0, d2
			lsl.l	#2, d2
			swap	d2
			move	do_lx(a5), d3
			sub		d2, d3
			move	d3, zo_lx(a0)
			add		d1, d3
			move	d3, zo_rx(a0)
			;

		*/

		int32_t width = d.do_rx - d.do_lx;
		int32_t origwidth = width;
		width *= d.do_frac;
		width <<= 2;
		width >>= 16;
		zone.x1 = d.do_lx - width;
		zone.x2 = zone.x1 + origwidth;

		/*
			move	do_rz(a5), d1
			sub		do_lz(a5), d1
			move	d1, d2
			muls	d0, d2
			lsl.l	#2, d2
			swap	d2
			move	do_lz(a5), d3
			sub		d2, d3
			move	d3, zo_lz(a0)
			add		d1, d3
			move	d3, zo_rz(a0)
		*/

		width = d.do_rz - d.do_lz;
		origwidth = width;
		width *= d.do_frac;
		width <<= 2;
		width >>= 16;
		zone.z1 = d.do_lz - width;
		zone.z2 = zone.z1 + origwidth;

		/*
			;
			tst		d0
			beq.s	.kill
			cmp		#$4000, d0
			bne.s	.loop
			;
			.kill
			move.l	a5, a0
			killitem	doors
			move.l	a0, a5
			bra.loop
			;
			.done
			rts
			*/
	}

	//kill pass

	auto i = gmap->GetActiveDoors().begin();

	while (i != gmap->GetActiveDoors().end())
	{
		if (i->do_frac == 0x4000)
		{
			gmap->GetZones()[i->do_poly].x1 = -1;
			gmap->GetZones()[i->do_poly].x2 = -1;
			gmap->GetZones()[i->do_poly].z1 = -1;
			gmap->GetZones()[i->do_poly].z2 = -1;

			i = gmap->GetActiveDoors().erase(i);
		}
		else
		{
			++ i;
		}
	}
}

MapObject GameLogic::GetPlayerObj()
{
	for (auto o : gmap->GetMapObjects())
	{
		if (o.t == ObjectGraphics::OLT_PLAYER1)
		{
			return o;
			break;
		}
	}

	// warning squash

	return gmap->GetMapObjects().front();
}

MapObject GameLogic::GetNamedObj(uint64_t id)
{
	for (auto o : gmap->GetMapObjects())
	{
		if (o.identifier == id)
		{
			return o;
			break;
		}
	}

	// warning squash

	return gmap->GetMapObjects().front();
}

uint8_t GameLogic::PickCalc(MapObject& o)
{
	/*
	pickcalc; pick a player and calculate angle to player!
		;
		bsr	pickplayer
		bsr	calcangle
		tst	ob_invisible(a0)
		beq.s.rts
		move	d0, -(a7)
		bsr	rndw
		and	#63, d0
		sub	#32, d0
		add(a7) + , d0
		and	#255, d0
		.rts	rts
	*/
	MapObject player = GetPlayerObj();
	uint8_t ang = GloomMaths::CalcAngle(player.x.GetInt(), player.z.GetInt(), o.x.GetInt(), o.z.GetInt());

	if (player.data.ms.invisible)
	{
		ang = ang + (GloomMaths::RndW() & 63) - 32;
	}
	return ang;
}

void  GameLogic::DoRot()
{
	std::vector<ActiveRotPoly>&rotpolys = gmap->GetActiveRotPolys();
	std::vector<Zone>&zones = gmap->GetZones();

	for (auto& r: rotpolys)
	{
		if (r.speed)
		{
			r.rot += r.speed;

			if (r.flags & 1)
			{
				// Morph
				if (r.rot < 0)
				{
					if (r.flags & 3)
					{
						r.speed = -r.speed;
					}
					else
					{
						r.speed = 0;
					}
				}
				else if (r.rot > 0x4000)
				{
					if (r.flags & 2)
					{
						r.speed = -r.speed;
					}
					else
					{
						r.speed = 0;
					}
				}

				for (int vertex = 0; vertex < r.num; vertex++)
				{
					auto thiszone = r.first + vertex;
					auto prevzone = thiszone - 1;

					if (vertex == 0) prevzone = r.first + r.num - 1;

					/*
						.loop	movem(a3) + , d0 - d3, (vx, vz, ox, oz, d4 is rot)
						muls	d4, d0
						lsl.l	#2, d0
						swap	d0
						add	d2, d0
						muls	d4, d1
						lsl.l	#2, d1
						swap	d1
						add	d3, d1
						movem	d0 - d1, zo_lx(a2)
						movem	d0 - d1, zo_rx(a1)
						move.l	a2, a1
					*/
					int32_t vx = r.vx[vertex];
					vx *= r.rot;
					vx <<= 2;
					vx >>= 16;
					vx += r.ox[vertex];

					int32_t vz= r.vz[vertex];
					vz *= r.rot;
					vz <<= 2;
					vz >>= 16;
					vz += r.oz[vertex];

					zones[thiszone].x1 = vx;
					zones[thiszone].z1 = vz;

					zones[prevzone].x2 = vx;
					zones[prevzone].z2 = vz;
				}

				// norm recalc
				for (int vertex = 0; vertex < r.num; vertex++)
				{
					auto thiszone = r.first + vertex;
					int16_t rx, rz;

					GloomMaths::CalcNormVec(zones[thiszone].x2 - zones[thiszone].x1, zones[thiszone].z2 - zones[thiszone].z1, rx, rz);

					zones[thiszone].na = rx;
					zones[thiszone].nb = rz;

					zones[thiszone].a = -rz;
					zones[thiszone].b = rx;
				}

			}
			else
			{
				//rot
				r.rot += r.speed;

				for (int vertex = 0; vertex < r.num; vertex++)
				{
					auto thiszone = r.first + vertex;
					auto prevzone = thiszone - 1;

					if (vertex == 0) prevzone = r.first + r.num - 1;

					int16_t rotmatrix[4];
					GloomMaths::GetCamRot2Raw(r.rot & 1023, rotmatrix);

					int16_t nx, nz;

					Rotter(r.lx[vertex], r.lz[vertex], nx, nz, rotmatrix);

					zones[thiszone].x1 = nx + r.cx;
					zones[thiszone].z1 = nz + r.cz;

					zones[prevzone].x2 = nx + r.cx;
					zones[prevzone].z2 = nz + r.cz;

					Rotter(r.na[vertex], r.nb[vertex], nx, nz, rotmatrix);

					zones[thiszone].na = nx;
					zones[thiszone].nb = nz;

					zones[thiszone].a = -nz;
					zones[thiszone].b = nx;
				}
			}
		}
	}
}

void GameLogic::Rotter(int16_t x, int16_t z, int16_t&nx, int16_t& nz, int16_t camrots[4])
{
	/*
		move	d0, d2
		move	d1, d3
		;
		muls(a4), d0
		muls	2(a4), d3
		add.l	d3, d0
		add.l	d0, d0
		swap	d0; new x!
		;
		muls	4(a4), d2
		muls	6(a4), d1
		add.l	d2, d1
		add.l	d1, d1
		swap	d1
	*/
	int32_t newx = (int32_t)camrots[0] * (int32_t)x + (int32_t)camrots[1] * (int32_t)z;
	int32_t newz = (int32_t)camrots[2] * (int32_t)x + (int32_t)camrots[3] * (int32_t)z;

	// compensate for 1.15 fracs
	newx += newx;
	newz += newz;

	nx = (newx >> 16);
	nz = (newz >> 16);
}

bool GameLogic::Collision(bool event, int32_t x, int32_t z, int32_t r, int32_t& overshoot, int32_t& closestzone)
{
	// do 3x3 square

	int32_t closest = 0x3fff;
	bool good = true;

	for (int32_t dx = -1; dx < 2; dx++)
	{
		for (int32_t dz = -1; dz < 2; dz++)
		{
			int32_t gx = x / 256 + dx;
			int32_t gz = z / 256 + dz;

			if ((gx >= 0) && (gx < 32) && (gz >= 0) && (gz < 32))
			{
				std::vector<uint32_t> collzones = gmap->GetCollisions(event?1:0, gx, gz);

				for (size_t checkzone = 0; checkzone < collzones.size(); checkzone++)
				{
					int32_t dist = FindSegDist(x, z, gmap->GetZones()[collzones[checkzone]]);

					if (dist < r)
					{
						good = false;

						if (dist < closest)
						{
							closest = dist;
							closestzone = collzones[checkzone];
						}
					}
				}
			}
		}
	}

	// explicit check of rotpolys as they may have gone out of their collision grid spot

	std::vector<ActiveRotPoly>& rotpolys = gmap->GetActiveRotPolys();

	for (auto &thisrot : rotpolys)
	{
		for (int16_t i = 0; i < thisrot.num; i++)
		{
			int32_t dist = FindSegDist(x, z, gmap->GetZones()[thisrot.first + i]);

			if (dist < r)
			{
				good = false;

				if (dist < closest)
				{
					closest = dist;
					closestzone = thisrot.first + i;
				}
			}
		}
	}

	overshoot = closest - r;

	return !good;
}

int32_t GameLogic::FindSegDist(int32_t x, int32_t z, Zone& zone)
{
	// tranlation of source. Some kind of cross product to determine if exceeds line length, then similar perp check
	/*
	findsegdist; find distance from d6, d7 to zone in a4...
		;
	; find end dist
		move	zo_rx(a4), d0
		sub	d6, d0
		muls	zo_na(a4), d0
		move	zo_rz(a4), d1
		sub	d7, d1
		muls	zo_nb(a4), d1
		add.l	d1, d0
		add.l	d0, d0
		swap	d0; distance from end
		;
	cmp	zo_ln(a4), d0
		bcs.s.perp; use perpendicular distance!
		;
	move	#$3fff, d0
		rts
		;
	.perp; find perpendicular dist.
		;
	move	zo_rx(a4), d0
		sub	d6, d0
		muls	zo_a(a4), d0
		move	zo_rz(a4), d1
		sub	d7, d1
		muls	zo_b(a4), d1
		add.l	d1, d0
		add.l	d0, d0
		bpl.s.pl
		neg.l	d0
		.pl	swap	d0; perpendicular dist.w
		rts

	*/

	int32_t tx, tz;

	tx = zone.x2 - x;
	tx *= zone.na;
	tz = zone.z2 - z;
	tz *= zone.nb;

	tx += tz;
	tx *= 2;

	if (((tx >> 16) < zone.ln) && (tx>=0))
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

DefenderGame::InputState GameLogic::ReadDefenderInput() const
{
	DefenderGame::InputState input;
	const Uint8* keys = SDL_GetKeyboardState(nullptr);

	const bool left = keys[Config::GetKey(Config::KEY_LEFT)] != 0 ||
		keys[Config::GetKey(Config::KEY_SLEFT)] != 0;
	const bool right = keys[Config::GetKey(Config::KEY_RIGHT)] != 0 ||
		keys[Config::GetKey(Config::KEY_SRIGHT)] != 0;
	const bool up = keys[Config::GetKey(Config::KEY_UP)] != 0;
	const bool down = keys[Config::GetKey(Config::KEY_DOWN)] != 0;

	input.moveX = (right ? 1 : 0) - (left ? 1 : 0);
	input.moveY = (down ? 1 : 0) - (up ? 1 : 0);
	input.fire = keys[Config::GetKey(Config::KEY_SHOOT)] != 0;

	if (Config::HaveController())
	{
		const Sint32 axisX = Config::GetControllerX();
		const Sint32 axisY = Config::GetControllerY();
		if (axisX < -8000 || Config::GetControllerLeft()) input.moveX = -1;
		if (axisX >  8000 || Config::GetControllerRight()) input.moveX = 1;
		if (axisY < -8000 || Config::GetControllerUp()) input.moveY = -1;
		if (axisY >  8000 || Config::GetControllerDown()) input.moveY = 1;
		if (Config::GetControllerFire() || Config::GetControllerConfirm()) input.fire = true;
	}

	if (SDL_GetMouseState(nullptr, nullptr) != 0)
		input.fire = true;

	return input;
}

bool GameLogic::BeginDefenderSession(const Teleport& tele)
{
	std::vector<Column>* textureColumns = nullptr;
	std::size_t textureBaseColumn = 0;
	if (!gmap || tele.textureIndex < 0 ||
		!gmap->ResolveTextureColumns(tele.textureIndex, textureColumns, textureBaseColumn))
	{
		SDL_Log("ZGloom Defender: monitor texture %d could not be resolved", tele.textureIndex);
		return false;
	}

	int episode = SaveSystem::GetCurrentFlat();
	if (episode < 1) episode = 1;
	if (episode > 3) episode = 3;

	if (!defenderGame.Begin(textureColumns, textureBaseColumn, episode))
	{
		SDL_Log("ZGloom Defender: texture %d has no embedded Defender artwork", tele.textureIndex);
		return false;
	}

	defenderTeleport = tele;
	defenderSessionActive = true;
	defenderLocking = true;
	defenderRewardApplied = false;
	defenderOrbitSpeed = 0;
	firedown = true;

	SDL_Log("ZGloom Defender: started event %d, texture=%d, episode=%d, target=%d",
		tele.ev, tele.textureIndex, episode, defenderGame.GetTargetsRemaining());
	return true;
}

void GameLogic::UpdateDefenderSession(Camera* cam, MapObject& playerobj)
{
	if (!defenderSessionActive || !cam)
		return;

	const DefenderGame::InputState input = ReadDefenderInput();

	if (defenderLocking)
	{
		auto StepPosition = [](int current, int target) -> int
		{
			const int delta = target - current;
			if (delta > 2) return current + 2;
			if (delta < -2) return current - 2;
			return target;
		};

		const int newX = StepPosition(playerobj.x.GetInt(), defenderTeleport.x);
		const int newZ = StepPosition(playerobj.z.GetInt(), defenderTeleport.z);
		int currentRot = playerobj.data.ms.rotquick.GetInt() & 255;
		const int targetRot = defenderTeleport.rot & 255;
		int rotDelta = (targetRot - currentRot) & 255;
		if (rotDelta >= 128) rotDelta -= 256;
		if (rotDelta > 4) rotDelta = 4;
		if (rotDelta < -4) rotDelta = -4;
		currentRot = (currentRot + rotDelta) & 255;

		playerobj.x.SetInt(newX);
		playerobj.z.SetInt(newZ);
		playerobj.data.ms.rotquick.SetInt(currentRot);
		playerobj.data.ms.rotspeed = 0;
		playerobj.data.ms.bounce = 0;
		cam->x = playerobj.x;
		cam->z = playerobj.z;
		cam->rotquick = playerobj.data.ms.rotquick;
		cam->y = -(playerobj.y.GetInt() + playerobj.data.ms.eyey);

		if (newX == defenderTeleport.x && newZ == defenderTeleport.z && currentRot == targetRot)
		{
			defenderLocking = false;
			SDL_Log("ZGloom Defender: player locked to monitor");
		}
		return;
	}

	// Match the original atmachine behaviour: horizontal Defender movement also
	// gives the camera a small, damped orbit in front of the monitor.
	if (input.moveX < 0)
		defenderOrbitSpeed = std::max(-8, defenderOrbitSpeed - 1);
	else if (input.moveX > 0)
		defenderOrbitSpeed = std::min(8, defenderOrbitSpeed + 1);
	else if (defenderOrbitSpeed < 0)
		defenderOrbitSpeed = std::min(0, defenderOrbitSpeed + 2);
	else if (defenderOrbitSpeed > 0)
		defenderOrbitSpeed = std::max(0, defenderOrbitSpeed - 2);

	const int viewRot = (defenderTeleport.rot + defenderOrbitSpeed) & 255;
	Quick orbitRot[4];
	GloomMaths::GetCamRot(static_cast<uint8_t>((viewRot - 64) & 255), orbitRot);
	Quick radius;
	radius.SetInt(defenderOrbitSpeed * 6);
	playerobj.x.SetInt(defenderTeleport.x);
	playerobj.z.SetInt(defenderTeleport.z);
	playerobj.x = playerobj.x - orbitRot[1] * radius;
	playerobj.z = playerobj.z + orbitRot[0] * radius;
	playerobj.data.ms.rotquick.SetInt(viewRot);
	playerobj.data.ms.bounce = 0;
	cam->x = playerobj.x;
	cam->z = playerobj.z;
	cam->rotquick = playerobj.data.ms.rotquick;
	cam->y = -(playerobj.y.GetInt() + playerobj.data.ms.eyey);

	defenderGame.Update(input);

	if (defenderGame.DidWin() && !defenderRewardApplied)
	{
		// Defender awards one main-game life, capped at five as requested.
		// Further wins remain valid but the surplus reward is discarded.
		const bool awarded = AwardLife();
		defenderRewardApplied = true;
		SDL_Log("ZGloom Defender: won; main-game reserve lives=%d%s",
			p1lives, awarded ? "" : " (maximum reached; reward discarded)");
	}

	if (defenderGame.IsComplete())
	{
		defenderSessionActive = false;
		defenderLocking = false;
		defenderOrbitSpeed = 0;
		firedown = false;
		SDL_Log("ZGloom Defender: session complete (%s)", defenderGame.DidWin() ? "WIN" : "LOSS");
	}
}

bool GameLogic::Update(Camera* cam)
{
	Quick inc;
	bool done = false;
	bool moved = false;

	inc.SetVal(0xd0000);
	newobjects.clear();

	Quick camrots[4], camrotstrafe[4];

	GloomMaths::GetCamRot(cam->rotquick.GetInt()&0xFF, camrots);
	GloomMaths::GetCamRot(((cam->rotquick.GetInt())+64)&0xFF, camrotstrafe);
	const Uint8 *keystate = SDL_GetKeyboardState(NULL);

	Quick newx = cam->x;
	Quick newz = cam->z;

	MapObject playerobj = GetPlayerObj();
	int16_t initialhealth = playerobj.data.ms.hitpoints;
	bool squished = false;

	// Ensure cheats are loaded on first frame
	EnsureCheatsLoadedGL();

	// --- GOD MODE RUNTIME ENFORCEMENT ---
	// God mode raises internal health to 32767.  When it is turned off again,
	// clamp any leftover huge value back to the normal visible max, otherwise
	// damage is applied but the HUD stays full for thousands of hits.
	{
		static bool sPrevGodMode = false;
		const bool cGodMode = Cheats::GetGodMode();

		if (cGodMode || sPrevGodMode)
		{
			for (auto& o : gmap->GetMapObjects())
			{
				if (o.t == ObjectGraphics::OLT_PLAYER1)
				{
					if (cGodMode)
					{
						o.data.ms.hitpoints = 32767;
						p1health = 32767;
						playerobj.data.ms.hitpoints = 32767;
					}
					else if (o.data.ms.hitpoints > 25)
					{
						o.data.ms.hitpoints = 25;
						p1health = 25;
						if (playerobj.data.ms.hitpoints > 25)
						{
							playerobj.data.ms.hitpoints = 25;
						}
					}
					break;
				}
			}
		}

		sPrevGodMode = cGodMode;
	}
	initialhealth = playerobj.data.ms.hitpoints;

	// --- CHEAT RUNTIME ENFORCEMENT (THERMO/INVIS/BOUNCY) ---
	{
		static bool sPrevThermo=false, sPrevInvis=false, sPrevBouncy=false;
		const bool cThermo = Cheats::GetThermoGoggles();
		const bool cInvis  = Cheats::GetInvisibility();
		const bool cBouncy = Cheats::GetBouncyBullets();
		if (cThermo) { playerobj.data.ms.thermo = 0x7FFF; } else if (sPrevThermo) { playerobj.data.ms.thermo = 0; }
		if (cInvis)  { playerobj.data.ms.invisible = 0x7FFF; } else if (sPrevInvis)  { playerobj.data.ms.invisible = 0; }
		if (cBouncy) { playerobj.data.ms.bouncecnt = 3; } else if (sPrevBouncy) { playerobj.data.ms.bouncecnt = 0; }
		sPrevThermo = cThermo; sPrevInvis = cInvis; sPrevBouncy = cBouncy;
	}

	// Load cheats once per run (without opening menu)
	EnsureCheatsLoadedGL();

	// Edge-apply WEAPON immediately (0..4 apply, 5 = default/no override)
	{
		static int sPrevStartWeapon = -12345;
		int sw = Cheats::GetStartWeapon();
		if (sw != sPrevStartWeapon) {
			sPrevStartWeapon = sw;
			if (sw >= 0 && sw <= 4) { playerobj.data.ms.weapon = sw; p1weapon = sw; }
		}
	}

	if (defenderSessionActive)
	{
		UpdateDefenderSession(cam, playerobj);
	}
	else if (playerobj.data.ms.logic == NullLogic)
	{
		playerobj.x = cam->x;
		playerobj.y.SetInt(0);
		playerobj.z = cam->z;
		playerobj.data.ms.rotquick = cam->rotquick;

		inc.SetVal(playerobj.data.ms.movspeed);

		//wire these up to controller as well at some point

		bool controlfire  = keystate[Config::GetKey(Config::KEY_SHOOT)] != 0;
		bool controlup = keystate[Config::GetKey(Config::KEY_UP)] != 0;
		bool controldown = keystate[Config::GetKey(Config::KEY_DOWN)] != 0;
		bool controlleft = keystate[Config::GetKey(Config::KEY_LEFT)] != 0;
		bool controlright = keystate[Config::GetKey(Config::KEY_RIGHT)] != 0;
		bool controlstrafeleft = keystate[Config::GetKey(Config::KEY_SLEFT)] != 0;
		bool controlstraferight = keystate[Config::GetKey(Config::KEY_SRIGHT)] != 0;
		bool controlstrafemod = keystate[Config::GetKey(Config::KEY_STRAFEMOD)] != 0;
		bool controlrun = keystate[Config::GetKey(Config::KEY_RUN)] != 0;

		if (Config::HaveController())
		{
			Sint32 contX = Config::GetControllerX();
			Sint32 contY = Config::GetControllerY();
			
			if (contX < -8000) controlstrafeleft = true;
			if (contX >  8000) controlstraferight = true;
			if (contY < -8000) controlup = true;
			if (contY >  8000) controldown = true;

			if (Config::GetControllerFire()) controlfire = true;
			if (Config::GetControllerRun())  controlrun  = true;
		}

		bool isRunning = controlrun;

		if (isRunning)
		{
			int32_t v = inc.GetVal();
			v += v / 2;
			inc.SetVal(v);
		}

		if (SDL_GetMouseState(NULL, NULL))
		{
			controlfire = true;
		}

		if (controlup)
		{
			// U 
			newx = cam->x - camrots[1] * inc;
			newz = cam->z + camrots[0] * inc;
			moved = true;
		}
		if (controldown)
		{
			// D
			newx = cam->x + camrots[1] * inc;
			newz = cam->z - camrots[0] * inc;
			moved = true;
		}
		if (controlleft || controlstrafeleft)
		{
			//L
			//TODO: Rotation acceleration
			if (controlstrafemod || controlstrafeleft)
			{
				//strafe
				newx = newx + camrotstrafe[1] * inc;
				newz = newz - camrotstrafe[0] * inc;
				moved = true;
			}
			else
			{
				cam->rotquick.SetInt(cam->rotquick.GetInt() - 4);
			}
		}
		if (controlright || controlstraferight)
		{
			//R
			if (controlstrafemod || controlstraferight)
			{
				//strafe
				newx = newx - camrotstrafe[1] * inc;
				newz = newz + camrotstrafe[0] * inc;
				moved = true;
			}
			else
			{
				cam->rotquick.SetInt(cam->rotquick.GetInt() + 4);
			}
		}

		if (!moved)
		{
			//unbounce
			if (playerobj.data.ms.bounce)
			{
				playerobj.data.ms.bounce += 30;

				if ((playerobj.data.ms.bounce & 127) < 30)
				{
					playerobj.data.ms.bounce = 0;
					SoundHandler::Play(SoundHandler::SOUND_FOOTSTEP);
				}
			}
		}
		else
		{
			// bounce!
			/*
			.bounce	move	ob_bounce(a5),d2
			add	#20,ob_bounce(a5)
			move	ob_bounce(a5),d1
			and	#255,d2
			cmp	#64,d2
			bcc.s	.fskip
			and	#255,d1
			cmp	#64,d1
			bcs.s	.fskip
			;
			bsr	footstep
			*/

			int16_t d2 = playerobj.data.ms.bounce;
			int16_t stepAdd = isRunning ? 30 : 20;
			playerobj.data.ms.bounce += stepAdd;
			int16_t d1 = playerobj.data.ms.bounce;

			d2 &= 255;
			d1 &= 255;

			if ((d1 >= 64) && (d2 < 64))
			{
				SoundHandler::Play(SoundHandler::SOUND_FOOTSTEP);
			}
		}

		if (controlfire)
{
            // Ensure muzzle-origin spawn independent of strafe movement
            auto MakeShotFromMuzzle = [&](MapObject p)->MapObject {
                MapObject m = p;
                Quick fwdInc; fwdInc.SetInt(28);   // forward offset
                Quick rightInc; rightInc.SetInt(0); // side offset (weapon on right)
                Quick rotF[4], rotR[4];
                GloomMaths::GetCamRot(p.data.ms.rotquick.GetInt() & 0xFF, rotF);
                GloomMaths::GetCamRot((p.data.ms.rotquick.GetInt() + 64) & 0xFF, rotR);
                m.x = cam->x - rotF[1] * fwdInc + rotR[1] * rightInc;
                m.z = cam->z + rotF[0] * fwdInc - rotR[0] * rightInc;
                // keep m.y unchanged; weapon height already encoded in rendering
                return m;
            };
//Shoot!
			if ((playerobj.data.ms.reloadcnt == 0) && (!firedown))
			{
				auto wep = playerobj.data.ms.weapon;

				if (playerobj.data.ms.mega)
				{
					if (playerobj.data.ms.mega >= (750 + 125))
					{
						// ULTRA MEGA OVERKILL
						playerobj.data.ms.rotquick.SetInt(playerobj.data.ms.rotquick.GetInt() + 8);
						Shoot(MakeShotFromMuzzle(playerobj), this, (playerobj.data.ms.collwith & 3) ^ 3, 0, wtable[wep].hitpoint, Cheats::AmplifyPlayerOutgoingDamage(wtable[wep].damage), wtable[wep].speed, wtable[wep].shape, wtable[wep].spark);
						playerobj.data.ms.rotquick.SetInt(playerobj.data.ms.rotquick.GetInt() - 16);
						Shoot(MakeShotFromMuzzle(playerobj), this, (playerobj.data.ms.collwith & 3) ^ 3, 0, wtable[wep].hitpoint, Cheats::AmplifyPlayerOutgoingDamage(wtable[wep].damage), wtable[wep].speed, wtable[wep].shape, wtable[wep].spark);
						playerobj.data.ms.rotquick.SetInt(playerobj.data.ms.rotquick.GetInt() + 8);
						Shoot(MakeShotFromMuzzle(playerobj), this, (playerobj.data.ms.collwith & 3) ^ 3, 0, wtable[wep].hitpoint, Cheats::AmplifyPlayerOutgoingDamage(wtable[wep].damage), wtable[wep].speed, wtable[wep].shape, wtable[wep].spark);
					}
					else
					{
						playerobj.data.ms.rotquick.SetInt(playerobj.data.ms.rotquick.GetInt() + 4);
						Shoot(MakeShotFromMuzzle(playerobj), this, (playerobj.data.ms.collwith & 3) ^ 3, 0, wtable[wep].hitpoint, Cheats::AmplifyPlayerOutgoingDamage(wtable[wep].damage), wtable[wep].speed, wtable[wep].shape, wtable[wep].spark);
						playerobj.data.ms.rotquick.SetInt(playerobj.data.ms.rotquick.GetInt() - 8);
						Shoot(MakeShotFromMuzzle(playerobj), this, (playerobj.data.ms.collwith & 3) ^ 3, 0, wtable[wep].hitpoint, Cheats::AmplifyPlayerOutgoingDamage(wtable[wep].damage), wtable[wep].speed, wtable[wep].shape, wtable[wep].spark);
						playerobj.data.ms.rotquick.SetInt(playerobj.data.ms.rotquick.GetInt() + 4);
					}
				}
				else
				{
					Shoot(MakeShotFromMuzzle(playerobj), this, (playerobj.data.ms.collwith & 3) ^ 3, 0, wtable[wep].hitpoint, Cheats::AmplifyPlayerOutgoingDamage(wtable[wep].damage), wtable[wep].speed, wtable[wep].shape, wtable[wep].spark);
				}
				SoundHandler::Play(wtable[wep].sound);
				playerobj.data.ms.reload = Cheats::GetCheatReloadForWeapon(playerobj.data.ms.weapon, playerobj.data.ms.reload);
				playerobj.data.ms.reloadcnt = playerobj.data.ms.reload * 2; // global fire rate -50% (slower)
				if (!Config::GetAutoFire()) firedown = true;
				playerobj.data.ms.fired = 10;
			}
		}
		else
		{
			firedown = false;
		}

		if (playerobj.data.ms.reloadcnt > 0) playerobj.data.ms.reloadcnt--;

		if (keystate[SDL_SCANCODE_F1])
		{
			// cheat for debug
			done = true;
		}


		int32_t overshoot, closestzone;

		if (!Collision(false, newx.GetInt(), newz.GetInt(), 32, overshoot, closestzone))
		{
			cam->x = newx;
			cam->z = newz;
		}
		else
		{
			// well, it's what the original seems to do...
			if (!AdjustPos(overshoot, newx, newz, 32, closestzone))
			{
				cam->x = newx;
				cam->z = newz;
			}
			else if (!AdjustPos(overshoot, newx, newz, 32, closestzone))
			{
				cam->x = newx;
				cam->z = newz;
			}
			else
			{
				if (Collision(false, cam->x.GetInt(), cam->z.GetInt(), 32, overshoot, closestzone))
				{
					squished = true;
				}
			}
		}

		// mouse control
		{
			int mx, my;
			SDL_GetRelativeMouseState(&mx, &my);

			cam->rotquick.SetVal(cam->rotquick.GetVal() + mx*Config::GetMouseSens() * 800);
		}

		//gamepad control
		if (Config::HaveController())
		{
			Sint32 rot = Config::GetControllerRot();

			if (abs(rot) < 8000) rot = 0;

			cam->rotquick.SetVal(cam->rotquick.GetVal() + rot * 10);
		}
		

		CheckSuck(cam);

		cam->y = -(playerobj.y.GetInt() + playerobj.data.ms.eyey);
		// add bounce
		int16_t camrotsraw[4];
		GloomMaths::GetCamRotRaw(playerobj.data.ms.bounce & 255, camrotsraw);
		cam->y -= (camrotsraw[1] * 20) >> 16;

		// event check
		Teleport tele;
		bool gotele = false;

		// prevent multiple sound playing!
		if (!playerobj.data.ms.pixsizeadd)
		{
			if (Collision(true, cam->x.GetInt(), cam->z.GetInt(), 32, overshoot, closestzone))
			{
				if (gmap->GetZones()[closestzone].ev > 1)
				{
					const uint32_t eventnum = gmap->GetZones()[closestzone].ev;
					const bool levelExitEvent = (eventnum == 24);

					if (levelExitEvent)
					{
						levelfinished = true;
						SoundHandler::Play(SoundHandler::SOUND_TELEPORT);
						playerobj.data.ms.pixsizeadd = 1;
					}

					if (!eventhit[eventnum])
					{
						// Event 24 is the Amiga level-exit trigger.  Execute its script,
						// but do not allow a teleport command in that script to replace
						// the slow blue beam-out with the normal fast teleport animation.
						gmap->ExecuteEvent(eventnum, gotele, tele, !levelExitEvent);
					}

					// these are one-shot
					if (eventnum < 19)
					{
						eventhit[eventnum] = true;
					}
				}
			}
		}

		if (gotele && !levelfinished)
		{
			if (tele.IsMonitorLock())
			{
				BeginDefenderSession(tele);
			}
			else
			{
				// normal teleport animation
				activetele = tele;
				playerobj.data.ms.pixsizeadd = 2;
			}
		}
	}
	else
	{
		// we're dead, jim
		//playerobj.data.ms.logic(playerobj, this);
	}

	// actually do the tele animation

	playerobj.data.ms.pixsize += playerobj.data.ms.pixsizeadd;

	if (playerobj.data.ms.pixsize >= 24)
	{
		cam->x.SetInt(activetele.x);
		cam->z.SetInt(activetele.z);
		cam->rotquick.SetInt((uint8_t)activetele.rot);
		playerobj.data.ms.pixsizeadd = -playerobj.data.ms.pixsizeadd;
		if (levelfinished) done = true;
	}

	if (levelfinishednow) done = true;

	if (playerobj.data.ms.pixsize == 0) playerobj.data.ms.pixsizeadd = 0;

	MoveBlood();

	// move any doors
	DoDoor();

	// update rots/morphs

	DoRot();

	//update the anims

	Column** tp = gmap->GetTexPointers();
	Column** to = gmap->GetTexPointersOrig();

	for (auto& a : gmap->GetAnims())
	{
		a.current--;

		if (a.current < 0)
		{
			a.current = a.delay;

			int curframe = animframe[a.first];

			curframe++;
			if (curframe >= a.frames)
			{
				curframe = 0;
			}

			animframe[a.first] = curframe;

			tp[a.first] = to[a.first + curframe];
		}
	}

	for (auto &o : gmap->GetMapObjects())
	{
		o.data.ms.logic(o, this);
	}

	ObjectCollision();
	
	//made a bit of a horlicks of this. 
	//I'm not confident about passing pointers to list members around, is that safe? I moved the kill pass to the end, so it should be OK, but erred on the side of safely
	auto playerobjupdated = GetPlayerObj();

	playerobj.data.ms.hitpoints = playerobjupdated.data.ms.hitpoints;
	playerobj.data.ms.weapon = playerobjupdated.data.ms.weapon;
	playerobj.data.ms.reload = playerobjupdated.data.ms.reload;
	playerobj.data.ms.eyey = playerobjupdated.data.ms.eyey;
	playerobj.data.ms.delay = playerobjupdated.data.ms.delay;
	playerobj.data.ms.colltype = playerobjupdated.data.ms.colltype;
	playerobj.data.ms.collwith = playerobjupdated.data.ms.collwith;
	playerobj.data.ms.mega = playerobjupdated.data.ms.mega;
	playerobj.data.ms.mess = playerobjupdated.data.ms.mess;
	playerobj.data.ms.messtimer = playerobjupdated.data.ms.messtimer;
	playerobj.data.ms.invisible = playerobjupdated.data.ms.invisible;
	playerobj.data.ms.thermo = playerobjupdated.data.ms.thermo;
	playerobj.data.ms.bouncecnt = playerobjupdated.data.ms.bouncecnt;
	// --- CHEAT ENFORCEMENT (THERMO/INVIS/BOUNCY) - post-copy ---
	{
		static bool sPrevThermo2=false, sPrevInvis2=false, sPrevBouncy2=false;
		const bool cThermo2 = Cheats::GetThermoGoggles();
		const bool cInvis2  = Cheats::GetInvisibility();
		const bool cBouncy2 = Cheats::GetBouncyBullets();
		if (cThermo2) { playerobj.data.ms.thermo = 0x7FFF; } else if (sPrevThermo2) { playerobj.data.ms.thermo = 0; }
		if (cInvis2)  { playerobj.data.ms.invisible = 0x7FFF; } else if (sPrevInvis2)  { playerobj.data.ms.invisible = 0; }
		if (cBouncy2) { playerobj.data.ms.bouncecnt = 3; } else if (sPrevBouncy2) { playerobj.data.ms.bouncecnt = 0; }
		sPrevThermo2 = cThermo2; sPrevInvis2 = cInvis2; sPrevBouncy2 = cBouncy2;
	}

	// --- ENFORCE WEAPON POST-COPY ---
	{
		int sw = Cheats::GetStartWeapon();
		if (sw >= 0 && sw <= 4) {
			if (playerobj.data.ms.weapon != sw) {
				playerobj.data.ms.weapon = sw;
				p1weapon = sw;
			}
		}
	}



	if (squished)
	{
		playerobj.data.ms.hitpoints -= Cheats::FilterDamageToPlayer(1);
	}

	if (playerobj.data.ms.mega)
	{
		playerobj.data.ms.mega--;

		if (playerobj.data.ms.mega == 0)
		{
			playerobj.data.ms.mess = Hud::MESSAGES_MEGA_WEAPON_OUT;
			playerobj.data.ms.messtimer = -127;
		}
	}
	if (playerobj.data.ms.invisible)
	{
		playerobj.data.ms.invisible--;
		if (playerobj.data.ms.invisible==0)
		{
			playerobj.data.ms.mess = Hud::MESSAGES_INVISIBILITY_OUT;
			playerobj.data.ms.messtimer = -127;
		}
	}
	if (playerobj.data.ms.thermo)
	{
		playerobj.data.ms.thermo--;
		if (playerobj.data.ms.thermo == 0)
		{
			playerobj.data.ms.mess = Hud::MESSAGES_THERMO_OUT;
			playerobj.data.ms.messtimer = -127;
		}
	}
	if (playerobj.data.ms.messtimer < 0)
	{
		playerobj.data.ms.messtimer++;
	}
	if (playerobj.data.ms.fired>0)
	{
		playerobj.data.ms.fired--;
	}

	playerhit = playerobj.data.ms.hitpoints < initialhealth;

	if (playerobj.data.ms.logic != NullLogic)
	{
		// we're still dead, jim
		cam->x = playerobj.x = playerobjupdated.x;
		playerobj.y = playerobjupdated.y;
		cam->y = -(playerobj.y.GetInt() + playerobj.data.ms.eyey);
		cam->z = playerobj.z = playerobjupdated.z;

		playerobj.data.ms.rotquick = playerobjupdated.data.ms.rotquick;
		cam->rotquick = playerobj.data.ms.rotquick;
		playerhit = true;
	}

	//do this after the above otherwise I don't pick up the player reset on death
	playerobj.data.ms.logic = playerobjupdated.data.ms.logic;

	if (playerobj.data.ms.logic == NullLogic)
	{
		//invuln timer after death
		if (playerobj.data.ms.delay)
		{
			playerobj.data.ms.delay--;

			if (playerobj.data.ms.delay == 0)
			{
				//reset collision data so can be hit again
				playerobj.data.ms.colltype = 8;
				playerobj.data.ms.collwith = 4;
			}
		}
	}

	if (squished &&  (playerobj.data.ms.hitpoints <= 0))
	{
		NotifyPlayerDeathStarted();
		playerobj.data.ms.hitpoints = 0;
		playerobj.data.ms.logic = PlayerDeath;

		playerobj.data.ms.colltype = 0;
		playerobj.data.ms.collwith = 0;
	}

	//kill pass

	auto i = gmap->GetMapObjects().begin();

	while (i != gmap->GetMapObjects().end())
	{
		if (i->killme)
		{
			i = gmap->GetMapObjects().erase(i);
		}
		else
		{
			++i;
		}
	}

	gmap->GetMapObjects().insert(gmap->GetMapObjects().end(), newobjects.begin(), newobjects.end());

	for (auto& o : gmap->GetMapObjects())
	{
		if (o.t == ObjectGraphics::OLT_PLAYER1)
		{
			o = playerobj;

			if (done)
			{
				//p1lives = 3; TODO
				p1health = o.data.ms.hitpoints;
				p1weapon = o.data.ms.weapon;
				p1reload = o.data.ms.reload;
			}

			break;
		}
	}

	return done;
}

int32_t GameLogic::GetTeleEffect()
{
	for (auto o : gmap->GetMapObjects())
	{
		if (o.t == ObjectGraphics::OLT_PLAYER1)
		{
			return o.data.ms.pixsize;
		}
	}

	return 0;
}

bool GameLogic::GetThermo()
{
	for (auto o : gmap->GetMapObjects())
	{
		if (o.t == ObjectGraphics::OLT_PLAYER1)
		{
			return o.data.ms.thermo != 0;
		}
	}

	return false;
}

void GameLogic::ObjectCollision()
{
	for (auto &o : gmap->GetMapObjects())
	{
		if (o.data.ms.collwith)
		{
			for (auto &o2 : gmap->GetMapObjects())
			{
				// don't compare with self!
				if (o.identifier == o2.identifier)
				{
					o.data.ms.washit = 0;

					// note goes back to the outer loop in the original? But that seems to mess with the asymettric nature of the collision system?
					// UPDATE: this may be why objects get added to both the back and front of the list
					break;
				}

				if ((o.data.ms.collwith & o2.data.ms.colltype) == 0)
				{
					continue;
				}

				int32_t radsum = o2.data.ms.rad + o.data.ms.rad;

				if (abs(o2.x.GetInt() - o.x.GetInt()) > radsum)
				{
					continue;
				}
				if (abs(o2.z.GetInt() - o.z.GetInt()) > radsum)
				{
					continue;
				}

				int32_t xd = abs(o2.x.GetInt() - o.x.GetInt());
				int32_t zd = abs(o2.z.GetInt() - o.z.GetInt());

				if ((xd*xd + zd*zd) < (radsum*radsum))
				{
					//printf("COLLISION %i %i\n", o.t, o2.t);
					if (o.data.ms.washit == o2.identifier)
					{
						// prevents double collision, as this part of the code hits *both* objects. I think?
						break;
					}

					o.data.ms.washit = o2.identifier;

					o.data.ms.hitpoints -= (o.t == ObjectGraphics::OLT_PLAYER1 ? Cheats::FilterDamageToPlayer(o2.data.ms.damage) : o2.data.ms.damage);
					if (o.data.ms.hitpoints <= 0)
					{
						o.data.ms.die(o, o2, this);
					}
					else
					{
						o.data.ms.hit(o, o2, this);
					}

					o2.data.ms.hitpoints -= (o2.t == ObjectGraphics::OLT_PLAYER1 ? Cheats::FilterDamageToPlayer(o.data.ms.damage) : o.data.ms.damage);
					if (o2.data.ms.hitpoints <= 0)
					{
						o2.data.ms.die(o2, o, this);
					}
					else
					{
						o2.data.ms.hit(o2, o, this);
					}

					// note break here. 
					break;
				}
			}
		}
	}
}

void GameLogic::CheckSuck(Camera* cam)
{
	/*
	checksuck	cmp.l	sucking(pc),a5
	bne.s	.nosuck
	;
	move.l	suckangle(pc),a0
	moveq	#25,d0
	move	d0,d1
	muls	2(a0),d0
	neg.l	d0
	add.l	d0,d6
	;
	muls	6(a0),d1
	add.l	d1,d7
	bsr	checknewslow
	beq.s	.newok
	bsr	adjustpos
	beq.s	.newok
	bsr	adjustpos
	beq.s	.newok
	;
	move.l	ob_x(a5),d6
	move.l	ob_z(a5),d7
	bra.s	.nosuck
	;
	.newok	move.l	d6,ob_x(a5)
	move.l	d7,ob_z(a5)
	;
	.nosuck	rts

	*/
	Quick xpos, zpos;

	xpos = cam->x;
	zpos = cam->z;

	if (!sucking) return;

	int16_t camrots[4];
	GloomMaths::GetCamRotRaw(suckangle, camrots);

	int32_t xvec = 25 * camrots[1];
	int32_t zvec = 25 * camrots[3];

	xvec = -xvec;

	xpos.SetVal(xpos.GetVal() + xvec);
	zpos.SetVal(zpos.GetVal() + zvec);

	int32_t overshoot, closestzone;

	if (!Collision(false, xpos.GetInt(), zpos.GetInt(), 32, overshoot, closestzone))
	{
		cam->x = xpos;
		cam->z = zpos;
	}
	else
	{
		if (!AdjustPos(overshoot, xpos, zpos, 32, closestzone))
		{
			cam->x = xpos;
			cam->z = zpos;
		}
		else if (!AdjustPos(overshoot, xpos, zpos, 32, closestzone))
		{
			cam->x = xpos;
			cam->z = zpos;
		}
	}

}